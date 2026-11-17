#pragma once

#include <QObject>
#include <QStringList>

#include "application/upload_arrangement.h"

namespace guitar {

class TransferViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList ports READ ports NOTIFY portsChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY stateChanged)
    Q_PROPERTY(int supportedStringMask READ supportedStringMask NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString loadedTitle READ loadedTitle NOTIFY stateChanged)

public:
    TransferViewModel(TransferSession& session, DeviceTransport& transport,
                      UploadArrangement& upload, QObject* parent = nullptr);
    ~TransferViewModel() override;

    QStringList ports() const { return m_ports; }
    QString status() const;
    bool connected() const;
    bool busy() const;
    bool playing() const;
    bool canPlay() const;
    int supportedStringMask() const { return m_session.state().supportedStringMask; }
    double progress() const { return m_session.state().progress; }
    QString loadedTitle() const;

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void connectPort(const QString& port);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE void upload(const QString& arrangementId, int tempo);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();

signals:
    void portsChanged();
    void stateChanged();

private:
    TransferSession& m_session;
    DeviceTransport& m_transport;
    UploadArrangement& m_upload;
    QStringList m_ports;
};

} // namespace guitar
