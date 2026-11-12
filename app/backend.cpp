#include "backend.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSerialPortInfo>
#include <QSet>
#include <cmath>

Backend::Backend(QObject *parent) : QObject(parent) {
    loadCatalog(); refreshPorts();
    m_timeout.setSingleShot(true); m_timeout.setInterval(3000);
    m_boot.setSingleShot(true); m_boot.setInterval(2200);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail("Нет ответа Uno. Проверьте USB и прошивку song_receiver."); });
    connect(&m_boot, &QTimer::timeout, this, [this] { send("HELLO 1", "READY"); });
    connect(&m_serial, &QSerialPort::readyRead, this, &Backend::receive);
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        if (e != QSerialPort::NoError) fail("Ошибка USB: " + m_serial.errorString());
    });
}
void Backend::loadCatalog() {
    QFile f(":/data/songs.json");
    if (!f.open(QIODevice::ReadOnly)) { m_status = "Каталог не найден"; return; }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) { m_status = "Ошибка формата каталога"; return; }
    const auto favorites = m_settings.value("favorites").toStringList();
    for (const auto &value : doc.array()) {
        auto song = value.toObject().toVariantMap();
        const auto events = song.value("notes").toList();
        QSet<int> strings; int duration = 0; bool valid = !events.isEmpty() && events.size() <= guitar::MaxNotes;
        const int bpm = song.value("bpm").toInt();
        valid &= bpm >= 40 && bpm <= 220 && !song.value("id").toString().isEmpty();
        for (const auto &event : events) {
            auto n = event.toList();
            if (n.size() != 3) { valid = false; break; }
            int s = n[0].toInt(), fret = n[1].toInt(), ms = n[2].toInt();
            if (s < 1 || s > 6 || fret < 0 || fret > 24 || ms < 50 || ms > 10000) valid = false;
            strings.insert(s); duration += ms;
        }
        if (!valid) continue;
        QVariantList used; for (int s = 1; s <= 6; ++s) if (strings.contains(s)) used.append(s);
        song["strings"] = used; song["stringCount"] = used.size(); song["durationMs"] = duration;
        song["favorite"] = favorites.contains(song.value("id").toString());
        song["index"] = m_songs.size(); m_songs.append(song);
    }
}
void Backend::refreshPorts() {
    m_ports.clear();
    for (const auto &p : QSerialPortInfo::availablePorts()) m_ports.append(p.portName());
    emit changed();
}
void Backend::connectPort(const QString &port) {
    disconnectPort();
    if (port.isEmpty()) { m_status = "Выберите USB-порт"; emit changed(); return; }
    m_serial.setPortName(port); m_serial.setBaudRate(115200);
    m_serial.setDataBits(QSerialPort::Data8); m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop); m_serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial.open(QIODevice::ReadWrite)) { fail("Не удалось открыть порт: " + m_serial.errorString()); return; }
    m_serial.setDataTerminalReady(true);
    m_busy = true; m_status = "Ожидаем перезапуска Uno…"; m_boot.start(); emit changed();
}
void Backend::disconnectPort() {
    m_boot.stop(); m_timeout.stop(); m_expected.clear();
    m_ready = m_busy = false; m_progress = 0; m_loadedTitle.clear(); m_input.clear();
    if (m_serial.isOpen()) m_serial.close();
    m_status = "Подключите Arduino Uno по USB"; emit changed();
}
void Backend::fail(const QString &message) { disconnectPort(); m_status = message; emit changed(); }
void Backend::send(const QByteArray &line, const QByteArray &expected) {
    m_expected = expected;
    const auto packet = line + '\n';
    if (m_serial.write(packet) != packet.size()) { fail("Не удалось отправить данные"); return; }
    m_timeout.start();
}
void Backend::receive() {
    m_input += m_serial.readAll();
    if (m_input.size() > 4096) { fail("Некорректный ответ устройства"); return; }
    while (m_input.contains('\n')) {
        int end = m_input.indexOf('\n'); auto line = m_input.left(end).trimmed(); m_input.remove(0, end + 1);
        handle(line);
    }
}
void Backend::handle(const QByteArray &line) {
    if (line == "BOOT 1") return;
    if (line.startsWith("ERR ")) { fail("Uno отклонила загрузку: " + QString::fromLatin1(line)); return; }
    if (m_expected.isEmpty()) return;
    if (m_expected == "READY") {
        auto parts = line.split(' '); bool ok = false;
        int capacity = parts.value(2).toInt(&ok);
        if (parts.size() != 4 || parts[0] != "READY" || parts[1] != "1" || parts[3] != "DRYRUN" || !ok || capacity < 1) {
            fail("Нужна прошивка song_receiver версии 1"); return;
        }
        m_timeout.stop(); m_expected.clear(); m_capacity = capacity; m_ready = true; m_busy = false;
        m_status = "Uno подключена · приём нот без движения"; emit changed(); return;
    }
    if (line != m_expected) { fail("Нарушен порядок подтверждений Uno"); return; }
    m_timeout.stop(); auto acknowledged = m_expected; m_expected.clear();
    if (acknowledged == "OK ABORT") {
        m_busy = false; m_progress = 0; m_status = "Загрузка отменена"; emit changed(); return;
    }
    if (acknowledged == "OK END") {
        m_busy = false; m_progress = 1; m_loadedTitle = m_title;
        m_status = "Ноты загружены в RAM Uno · воспроизведение ещё не реализовано"; emit changed(); return;
    }
    if (acknowledged == "OK BEGIN") m_index = 0;
    else ++m_index;
    m_progress = double(m_index) / m_notes.size(); next(); emit changed();
}
void Backend::upload(int songIndex, int tempo) {
    if (!m_ready || m_busy || songIndex < 0 || songIndex >= m_songs.size() || tempo < 40 || tempo > 220) return;
    auto song = m_songs[songIndex].toMap(); m_notes.clear(); uint16_t crc = 0xffff;
    for (auto v : song["notes"].toList()) {
        auto a = v.toList(); int duration = std::lround(a[2].toInt() * double(song["bpm"].toInt()) / tempo);
        if (duration < 50 || duration > 10000) { m_status = "Этот темп выходит за допустимую длительность нот"; emit changed(); return; }
        guitar::Note n{uint8_t(a[0].toInt()), uint8_t(a[1].toInt()), uint16_t(duration)};
        m_notes.append(n); crc = guitar::addNote(crc, n);
    }
    if (m_notes.size() > m_capacity) { m_status = "В песне больше нот, чем вмещает Uno"; emit changed(); return; }
    m_title = song["title"].toString(); m_loadedTitle.clear(); m_busy = true; m_progress = 0; m_index = -1;
    m_status = "Передаём ноты на Uno…";
    send("BEGIN " + QByteArray::number(m_notes.size()) + ' ' + QByteArray::number(crc), "OK BEGIN"); emit changed();
}
void Backend::next() {
    if (m_index == m_notes.size()) { send("END", "OK END"); return; }
    const auto n = m_notes[m_index];
    send("NOTE " + QByteArray::number(m_index) + ' ' + QByteArray::number(n.string) + ' ' + QByteArray::number(n.fret) + ' ' + QByteArray::number(n.duration),
         "OK " + QByteArray::number(m_index));
}
void Backend::cancel() {
    // Close the session rather than mixing ABORT with an outstanding acknowledgement.
    // Receiver timeout invalidates incomplete data; a new BEGIN also resets it.
    if (m_busy) { disconnectPort(); m_status = "Загрузка отменена. Подключитесь снова."; emit changed(); }
}
void Backend::favorite(int index) {
    if (index < 0 || index >= m_songs.size()) return;
    auto song = m_songs[index].toMap(); song["favorite"] = !song["favorite"].toBool(); m_songs[index] = song;
    QStringList ids; for (const auto &v : m_songs) { auto s = v.toMap(); if (s["favorite"].toBool()) ids << s["id"].toString(); }
    m_settings.setValue("favorites", ids); emit catalogChanged();
}
