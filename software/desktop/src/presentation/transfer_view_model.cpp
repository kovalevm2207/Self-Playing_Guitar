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
    return m_session.state().phase != TransferPhase::Disconnected &&
           m_session.state().phase != TransferPhase::Connecting;
}

bool TransferViewModel::busy() const {
    return m_session.state().phase != TransferPhase::Disconnected &&
           m_session.state().phase != TransferPhase::Ready;
}

bool TransferViewModel::playing() const {
    const auto phase = m_session.state().phase;
    return phase == TransferPhase::Starting || phase == TransferPhase::Playing ||
           phase == TransferPhase::Stopping;
}

bool TransferViewModel::canPlay() const {
    return m_session.state().phase == TransferPhase::Ready &&
           m_session.state().supportsPlayback && !loadedTitle().isEmpty();
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
        case TransferError::Playback:
            if (state.details == "PLAN") {
                return "Переходы между нотами не укладываются в заданный темп";
            }
            if (state.details == "NOT_LOADED") {
                return "Сначала загрузите ноты на контроллер";
            }
            return "Симуляция остановлена: контроллер пропустил срок действия";
        case TransferError::None:
            break;
    }
    if (state.result == TransferResult::Cancelled) {
        return "Загрузка отменена. Подключитесь снова.";
    }
    if (state.result == TransferResult::Loaded) {
        return state.simulated
                   ? "Загружено: " + loadedTitle() + " · симуляция"
                   : "Ноты загружены · прошивка поддерживает только приём";
    }
    if (state.result == TransferResult::Played) {
        return "Симуляция исполнения завершена";
    }
    if (state.result == TransferResult::Stopped) {
        return "Исполнение остановлено";
    }
    switch (state.phase) {
        case TransferPhase::Connecting:
            return "Ожидаем перезапуска и ответа Uno…";
        case TransferPhase::Uploading:
            return "Передаём ноты на Uno…";
        case TransferPhase::Ready:
            return state.simulated ? "Uno подключена · симуляция приводов"
                                   : "Uno подключена · приём нот без движения";
        case TransferPhase::Starting:
            return "Запускаем симуляцию…";
        case TransferPhase::Playing:
            return "Симуляция исполнения · физические выходы отключены";
        case TransferPhase::Stopping:
            return "Останавливаем исполнение…";
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

void TransferViewModel::play() {
    m_session.play();
}

void TransferViewModel::stop() {
    m_session.stop();
}

} // namespace guitar
