#include "adapters/qt_serial_transport.h"

#include <QSerialPortInfo>
#include <utility>

namespace guitar {
namespace {

constexpr int BOARD_RESET_DELAY_MS = 2200;
constexpr int SERIAL_BAUD_RATE = 115200;

} // namespace

QtSerialTransport::QtSerialTransport(QObject* parent) : QObject(parent) {
    m_resetTimer.setSingleShot(true);
    m_resetTimer.setInterval(BOARD_RESET_DELAY_MS);
    m_responseTimer.setSingleShot(true);

    connect(&m_resetTimer, &QTimer::timeout, this, [this] {
        if (m_serial.isOpen() && m_events.ready) {
            m_events.ready();
        }
    });
    connect(&m_responseTimer, &QTimer::timeout, this, [this] {
        if (m_events.timedOut) {
            m_events.timedOut();
        }
    });
    connect(&m_serial, &QSerialPort::readyRead, this, [this] {
        const auto bytes = m_serial.readAll().toStdString();
        if (m_events.received) {
            m_events.received(bytes);
        }
    });
    connect(&m_serial,
            &QSerialPort::errorOccurred,
            this,
            [this](QSerialPort::SerialPortError error) {
                if (!m_closing && !m_opening && error != QSerialPort::NoError) {
                    reportFailure(m_serial.errorString());
                }
            });
}

QtSerialTransport::~QtSerialTransport() {
    m_events = {};
    close();
    disconnect(&m_serial, nullptr, this, nullptr);
}

void QtSerialTransport::setEvents(DeviceEvents events) {
    m_events = std::move(events);
}

std::vector<std::string> QtSerialTransport::ports() const {
    std::vector<std::string> result;
    for (const auto& port : QSerialPortInfo::availablePorts()) {
        result.push_back(port.portName().toStdString());
    }
    return result;
}

void QtSerialTransport::reportFailure(const QString& message) {
    if (m_events.failed) {
        m_events.failed(message.toStdString());
    }
}

void QtSerialTransport::open(const std::string& port) {
    close();
    m_serial.setPortName(QString::fromStdString(port));
    m_serial.setBaudRate(SERIAL_BAUD_RATE);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    m_opening = true;
    const auto opened = m_serial.open(QIODevice::ReadWrite);
    m_opening = false;
    if (!opened) {
        reportFailure(m_serial.errorString());
        return;
    }
    m_serial.setDataTerminalReady(true);
    if (m_serial.isOpen()) {
        m_resetTimer.start();
    }
}

void QtSerialTransport::close() {
    m_closing = true;
    m_resetTimer.stop();
    m_responseTimer.stop();
    if (m_serial.isOpen()) {
        m_serial.close();
    }
    m_closing = false;
}

bool QtSerialTransport::write(std::string_view bytes) {
    return m_serial.isOpen() &&
           m_serial.write(bytes.data(), static_cast<qint64>(bytes.size())) ==
               static_cast<qint64>(bytes.size());
}

void QtSerialTransport::startTimeout(int milliseconds) {
    m_responseTimer.start(milliseconds);
}

void QtSerialTransport::stopTimeout() {
    m_responseTimer.stop();
}

} // namespace guitar
