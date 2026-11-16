#include "backend.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSerialPortInfo>
#include <QSet>
#include <cmath>

namespace {

constexpr int RESPONSE_TIMEOUT_MS = 3000;
constexpr int BOARD_RESET_DELAY_MS = 2200;
constexpr int MAX_RESPONSE_BYTES = 4096;

} // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    loadCatalog();
    refreshPorts();

    m_responseTimeout.setSingleShot(true);
    m_responseTimeout.setInterval(RESPONSE_TIMEOUT_MS);
    m_resetTimer.setSingleShot(true);
    m_resetTimer.setInterval(BOARD_RESET_DELAY_MS);

    connect(&m_responseTimeout, &QTimer::timeout, this, [this] {
        fail("Нет ответа Uno. Проверьте USB и прошивку song_receiver.");
    });
    connect(
        &m_resetTimer, &QTimer::timeout, this, [this] { send("HELLO 1", "READY"); });
    connect(&m_serial, &QSerialPort::readyRead, this, &Backend::receive);
    connect(&m_serial,
            &QSerialPort::errorOccurred,
            this,
            [this](QSerialPort::SerialPortError e) {
                if (e != QSerialPort::NoError) {
                    fail("Ошибка USB: " + m_serial.errorString());
                }
            });
}

void Backend::loadCatalog() {
    QFile catalogFile(":/data/songs.json");
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        m_status = "Каталог не найден";
        return;
    }

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(catalogFile.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        m_status = "Ошибка формата каталога";
        return;
    }

    const auto favorites = m_settings.value("favorites").toStringList();
    for (const auto& value : doc.array()) {
        auto song = value.toObject().toVariantMap();
        const auto events = song.value("notes").toList();

        QSet<int> usedStringNumbers;
        int duration = 0;
        bool valid = !events.isEmpty() && events.size() <= guitar::MaxNotes;
        const int beatsPerMinute = song.value("bpm").toInt();
        valid &= beatsPerMinute >= 40 && beatsPerMinute <= 220 &&
                 !song.value("id").toString().isEmpty();

        for (const auto& event : events) {
            auto fields = event.toList();
            if (fields.size() != 3) {
                valid = false;
                break;
            }

            const int stringNumber = fields[0].toInt();
            const int fret = fields[1].toInt();
            const int durationMs = fields[2].toInt();
            if (stringNumber < 1 || stringNumber > 6 || fret < 0 || fret > 24 ||
                durationMs < 50 || durationMs > 10000) {
                valid = false;
            }
            usedStringNumbers.insert(stringNumber);
            duration += durationMs;
        }

        if (!valid) {
            continue;
        }

        QVariantList usedStrings;
        for (int stringNumber = 1; stringNumber <= 6; ++stringNumber) {
            if (usedStringNumbers.contains(stringNumber)) {
                usedStrings.append(stringNumber);
            }
        }

        song["strings"] = usedStrings;
        song["stringCount"] = usedStrings.size();
        song["durationMs"] = duration;
        song["favorite"] = favorites.contains(song.value("id").toString());
        song["index"] = m_songs.size();
        m_songs.append(song);
    }
}

void Backend::refreshPorts() {
    m_ports.clear();
    for (const auto& port : QSerialPortInfo::availablePorts()) {
        m_ports.append(port.portName());
    }
    emit changed();
}

void Backend::connectPort(const QString& port) {
    disconnectPort();
    if (port.isEmpty()) {
        m_status = "Выберите USB-порт";
        emit changed();
        return;
    }

    m_serial.setPortName(port);
    m_serial.setBaudRate(115200);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial.open(QIODevice::ReadWrite)) {
        fail("Не удалось открыть порт: " + m_serial.errorString());
        return;
    }

    m_serial.setDataTerminalReady(true);
    m_busy = true;
    m_status = "Ожидаем перезапуска Uno…";
    m_resetTimer.start();
    emit changed();
}

void Backend::disconnectPort() {
    m_resetTimer.stop();
    m_responseTimeout.stop();
    m_expectedResponse.clear();

    m_connected = false;
    m_busy = false;
    m_progress = 0;
    m_loadedTitle.clear();
    m_receiveBuffer.clear();

    if (m_serial.isOpen()) {
        m_serial.close();
    }
    m_status = "Подключите Arduino Uno по USB";
    emit changed();
}

void Backend::fail(const QString& message) {
    disconnectPort();
    m_status = message;
    emit changed();
}

void Backend::send(const QByteArray& line, const QByteArray& expected) {
    m_expectedResponse = expected;
    const auto packet = line + '\n';
    if (m_serial.write(packet) != packet.size()) {
        fail("Не удалось отправить данные");
        return;
    }
    m_responseTimeout.start();
}

void Backend::receive() {
    m_receiveBuffer += m_serial.readAll();
    if (m_receiveBuffer.size() > MAX_RESPONSE_BYTES) {
        fail("Некорректный ответ устройства");
        return;
    }

    while (m_receiveBuffer.contains('\n')) {
        int end = m_receiveBuffer.indexOf('\n');
        auto line = m_receiveBuffer.left(end).trimmed();
        m_receiveBuffer.remove(0, end + 1);
        handle(line);
    }
}

void Backend::handle(const QByteArray& line) {
    if (line == "BOOT 1") {
        return;
    }
    if (line.startsWith("ERR ")) {
        fail("Uno отклонила загрузку: " + QString::fromLatin1(line));
        return;
    }
    if (m_expectedResponse.isEmpty()) {
        return;
    }

    if (m_expectedResponse == "READY") {
        auto fields = line.split(' ');
        bool validCapacity = false;
        int capacity = fields.value(2).toInt(&validCapacity);
        if (fields.size() != 4 || fields[0] != "READY" || fields[1] != "1" ||
            fields[3] != "DRYRUN" || !validCapacity || capacity < 1) {
            fail("Нужна прошивка song_receiver версии 1");
            return;
        }
        m_responseTimeout.stop();
        m_expectedResponse.clear();
        m_noteCapacity = capacity;
        m_connected = true;
        m_busy = false;
        m_status = "Uno подключена · приём нот без движения";
        emit changed();
        return;
    }

    if (line != m_expectedResponse) {
        fail("Нарушен порядок подтверждений Uno");
        return;
    }

    m_responseTimeout.stop();
    auto acknowledged = m_expectedResponse;
    m_expectedResponse.clear();

    if (acknowledged == "OK ABORT") {
        m_busy = false;
        m_progress = 0;
        m_status = "Загрузка отменена";
        emit changed();
        return;
    }
    if (acknowledged == "OK END") {
        m_busy = false;
        m_progress = 1;
        m_loadedTitle = m_uploadTitle;
        m_status = "Ноты загружены в RAM Uno · воспроизведение ещё не реализовано";
        emit changed();
        return;
    }

    if (acknowledged == "OK BEGIN") {
        m_nextNoteIndex = 0;
    } else {
        ++m_nextNoteIndex;
    }

    m_progress = double(m_nextNoteIndex) / m_pendingNotes.size();
    next();
    emit changed();
}

void Backend::upload(int songIndex, int tempo) {
    if (!m_connected || m_busy || songIndex < 0 || songIndex >= m_songs.size() ||
        tempo < 40 || tempo > 220) {
        return;
    }

    auto song = m_songs[songIndex].toMap();
    m_pendingNotes.clear();
    uint16_t crc = 0xffff;
    for (auto event : song["notes"].toList()) {
        auto fields = event.toList();
        int durationMs =
            std::lround(fields[2].toInt() * double(song["bpm"].toInt()) / tempo);
        if (durationMs < 50 || durationMs > 10000) {
            m_status = "Этот темп выходит за допустимую длительность нот";
            emit changed();
            return;
        }
        guitar::Note note{uint8_t(fields[0].toInt()),
                          uint8_t(fields[1].toInt()),
                          uint16_t(durationMs)};
        m_pendingNotes.append(note);
        crc = guitar::addNote(crc, note);
    }

    if (m_pendingNotes.size() > m_noteCapacity) {
        m_status = "В песне больше нот, чем вмещает Uno";
        emit changed();
        return;
    }

    m_uploadTitle = song["title"].toString();
    m_loadedTitle.clear();
    m_busy = true;
    m_progress = 0;
    m_nextNoteIndex = -1;
    m_status = "Передаём ноты на Uno…";

    send("BEGIN " + QByteArray::number(m_pendingNotes.size()) + ' ' +
             QByteArray::number(crc),
         "OK BEGIN");
    emit changed();
}

void Backend::next() {
    if (m_nextNoteIndex == m_pendingNotes.size()) {
        send("END", "OK END");
        return;
    }
    const auto note = m_pendingNotes[m_nextNoteIndex];
    send("NOTE " + QByteArray::number(m_nextNoteIndex) + ' ' +
             QByteArray::number(note.string) + ' ' + QByteArray::number(note.fret) +
             ' ' + QByteArray::number(note.duration),
         "OK " + QByteArray::number(m_nextNoteIndex));
}

void Backend::cancel() {
    // Close the session rather than mixing ABORT with an outstanding
    // acknowledgement. Receiver timeout invalidates incomplete data; a new BEGIN
    // also resets it.
    if (m_busy) {
        disconnectPort();
        m_status = "Загрузка отменена. Подключитесь снова.";
        emit changed();
    }
}

void Backend::favorite(int index) {
    if (index < 0 || index >= m_songs.size()) {
        return;
    }

    auto song = m_songs[index].toMap();
    song["favorite"] = !song["favorite"].toBool();
    m_songs[index] = song;

    QStringList favoriteIds;
    for (const auto& value : m_songs) {
        auto entry = value.toMap();
        if (entry["favorite"].toBool()) {
            favoriteIds << entry["id"].toString();
        }
    }

    m_settings.setValue("favorites", favoriteIds);
    emit catalogChanged();
}
