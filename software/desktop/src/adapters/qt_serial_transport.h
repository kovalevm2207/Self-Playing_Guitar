#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTimer>

#include "application/ports.h"

namespace guitar {

class QtSerialTransport : public QObject, public DeviceTransport {
    Q_OBJECT

public:
    explicit QtSerialTransport(QObject* parent = nullptr);
    ~QtSerialTransport() override;
    void setEvents(DeviceEvents events) override;
    std::vector<std::string> ports() const override;
    void open(const std::string& port) override;
    void close() override;
    bool write(std::string_view bytes) override;
    void startTimeout(int milliseconds) override;
    void stopTimeout() override;

private:
    void reportFailure(const QString& message);

    QSerialPort m_serial;
    QTimer m_resetTimer;
    QTimer m_responseTimer;
    DeviceEvents m_events;
    bool m_closing = false;
    bool m_opening = false;
};

} // namespace guitar
