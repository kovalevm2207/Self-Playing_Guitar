#include "presentation/transfer_view_model.h"

namespace guitar {

TransferViewModel::TransferViewModel(TransferSession& session,
                                     DeviceTransport& transport,
                                     UploadArrangement& upload, QObject* parent)
    : QObject(parent), m_session(session), m_transport(transport), m_upload(upload) {
    m_session.setOnChanged([this] { emit stateChanged(); });
    refreshPorts();
}

TransferViewModel::~TransferViewModel() {
    m_session.setOnChanged({});
}

bool TransferViewModel::connected() const {
    return m_session.state().phase == TransferPhase::Ready ||
           m_session.state().phase == TransferPhase::Uploading;
}

bool TransferViewModel::busy() const {
    return m_session.state().phase == TransferPhase::Connecting ||
           m_session.state().phase == TransferPhase::Uploading;
}

QString TransferViewModel::loadedTitle() const {
    return QString::fromStdString(m_session.state().loadedTitle);
}

QString TransferViewModel::status() const {
    const auto& state = m_session.state();
    switch (state.error) {
        case TransferError::PortRequired:
            return "Выберите USB-порт";
        case TransferError::Connection:
            return "Ошибка USB: " + QString::fromStdString(state.details);
        case TransferError::Timeout:
            return "Нет ответа Uno. Проверьте USB и прошивку song_receiver.";
        case TransferError::Protocol:
            return "Некорректный ответ Uno. Нужна прошивка song_receiver версии 1.";
        case TransferError::DeviceRejected:
            return "Uno отклонила загрузку: " +
                   QString::fromStdString(state.details);
        case TransferError::Capacity:
            return "В аранжировке больше нот, чем вмещает текущая прошивка Uno";
        case TransferError::InvalidScore:
            return "Не удалось подготовить ноты. Проверьте аранжировку и темп.";
        case TransferError::None:
            break;
    }
    if (state.result == TransferResult::Cancelled) {
        return "Загрузка отменена. Подключитесь снова.";
    }
    if (state.result == TransferResult::Loaded) {
        return "Ноты загружены в RAM Uno · воспроизведение ещё не реализовано";
    }
    switch (state.phase) {
        case TransferPhase::Connecting:
            return "Ожидаем перезапуска и ответа Uno…";
        case TransferPhase::Uploading:
            return "Передаём ноты на Uno…";
        case TransferPhase::Ready:
            return "Uno подключена · приём нот без движения";
        case TransferPhase::Disconnected:
            return "Подключите Arduino Uno по USB";
    }
    return {};
}

void TransferViewModel::refreshPorts() {
    m_ports.clear();
    for (const auto& port : m_transport.ports()) {
        m_ports.append(QString::fromStdString(port));
    }
    emit portsChanged();
}

void TransferViewModel::connectPort(const QString& port) {
    m_session.connectPort(port.toStdString());
}

void TransferViewModel::disconnectPort() {
    m_session.disconnect();
}

void TransferViewModel::upload(const QString& arrangementId, int tempo) {
    m_upload.execute(arrangementId.toStdString(), tempo);
}

void TransferViewModel::cancel() {
    m_session.cancel();
}

} // namespace guitar
