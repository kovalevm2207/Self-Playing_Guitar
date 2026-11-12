#pragma once
#include <QObject>
#include <QVariantList>
#include <QSerialPort>
#include <QTimer>
#include <QSettings>
#include <QVector>
#include "core/score.h"
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList songs READ songs NOTIFY catalogChanged)
    Q_PROPERTY(QStringList ports READ ports NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString loadedTitle READ loadedTitle NOTIFY changed)
public:
    explicit Backend(QObject *parent = nullptr);
    QVariantList songs() const { return m_songs; }
    QStringList ports() const { return m_ports; }
    QString status() const { return m_status; }
    bool connected() const { return m_ready; }
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString loadedTitle() const { return m_loadedTitle; }
    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void connectPort(const QString &port);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE void upload(int songIndex, int tempo);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void favorite(int index);
signals:
    void changed();
    void catalogChanged();
private:
    void send(const QByteArray &line, const QByteArray &expected);
    void receive();
    void handle(const QByteArray &line);
    void next();
    void fail(const QString &message);
    void loadCatalog();
    QVariantList m_songs;
    QStringList m_ports;
    QString m_status = QStringLiteral("Подключите Arduino Uno по USB"), m_title, m_loadedTitle;
    QSerialPort m_serial;
    QTimer m_timeout, m_boot;
    QByteArray m_input, m_expected;
    QVector<guitar::Note> m_notes;
    bool m_ready = false, m_busy = false;
    int m_index = -1, m_capacity = 0;
    double m_progress = 0;
    QSettings m_settings;
};
