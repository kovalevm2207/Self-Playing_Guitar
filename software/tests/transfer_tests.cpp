#undef NDEBUG
#include <cassert>
#include <iostream>

#include "application/upload_arrangement.h"
#include "fakes.h"

using namespace guitar;

void connect(test::FakeTransport& transport, TransferSession& session,
             int capacity = 96) {
    session.connectPort("TEST");
    assert(session.state().phase == TransferPhase::Connecting);
    assert(transport.openedPort == "TEST");
    transport.ready();
    assert(transport.sent.back() == "HELLO 1\n");
    assert(transport.timeoutMs == 3000);
    transport.receive("BOOT 1\r\nREA");
    assert(session.state().phase == TransferPhase::Connecting);
    transport.receive("DY 1 " + std::to_string(capacity) + " DRYRUN\r\n");
    assert(session.state().phase == TransferPhase::Ready);
    assert(transport.timeoutMs == 0);
}

PreparedScore sampleScore() {
    return prepareScore(
        test::sampleCatalog().arrangements[0].music, "Test song", 120);
}

void checkUpload() {
    test::FakeTransport transport;
    TransferSession session(transport);
    connect(transport, session);
    const auto score = sampleScore();
    session.upload(score);
    assert(transport.sent.back() == "BEGIN 2 38991\n");
    assert(session.state().phase == TransferPhase::Uploading);
    transport.receive("OK BEGIN\n");
    assert(transport.sent.back() == "NOTE 0 1 0 500\n");
    transport.receive("OK 0\n");
    assert(transport.sent.back() == "NOTE 1 1 3 250\n");
    assert(session.state().progress == 0.5);
    transport.receive("OK 1\n");
    assert(transport.sent.back() == "END\n");
    assert(session.state().result != TransferResult::Loaded);
    transport.receive("OK END\n");
    assert(session.state().result == TransferResult::Loaded);
    assert(session.state().loadedTitle == "Test song");
    assert(session.state().progress == 1);
    assert(transport.timeoutMs == 0);
}

void checkFailures() {
    test::FakeTransport transport;
    TransferSession session(transport);
    session.connectPort("");
    assert(session.state().error == TransferError::PortRequired);
    connect(transport, session);
    session.upload(sampleScore());
    transport.receive("OK BEGIN\nOK 1\n");
    assert(session.state().error == TransferError::Protocol);
    assert(!transport.opened);

    connect(transport, session);
    session.upload(sampleScore());
    transport.events.timedOut();
    assert(session.state().error == TransferError::Timeout);
    assert(session.state().loadedTitle.empty());

    connect(transport, session);
    session.upload(sampleScore());
    transport.receive("ERR CHECKSUM\n");
    assert(session.state().error == TransferError::DeviceRejected);
    assert(session.state().details == "CHECKSUM");

    connect(transport, session);
    session.upload(sampleScore());
    session.cancel();
    const auto count = transport.sent.size();
    transport.ready();
    transport.events.timedOut();
    transport.receive("OK BEGIN\n");
    assert(transport.sent.size() == count);
    assert(session.state().result == TransferResult::Cancelled);
    assert(session.state().phase == TransferPhase::Disconnected);

    connect(transport, session);
    session.upload(sampleScore());
    transport.receive("BOOT 1\n");
    assert(session.state().error == TransferError::Protocol);

    connect(transport, session);
    transport.events.failed("Cable removed");
    assert(session.state().error == TransferError::Connection);
    assert(session.state().details == "Cable removed");

    connect(transport, session);
    transport.failWrite = true;
    session.upload(sampleScore());
    assert(session.state().error == TransferError::Connection);
    transport.failWrite = false;

    session.connectPort("TEST");
    transport.ready();
    transport.receive("READY 2 96 DRYRUN\n");
    assert(session.state().error == TransferError::Protocol);
}

void checkCapacityAndUseCase() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    CatalogService catalog(repository, favorites);
    test::FakeTransport transport;
    TransferSession session(transport);
    UploadArrangement upload(catalog, session);
    connect(transport, session, 1);
    const auto count = transport.sent.size();
    upload.execute("one", 120);
    assert(session.state().error == TransferError::Capacity);
    assert(transport.sent.size() == count);
    assert(session.state().phase == TransferPhase::Ready);
    upload.execute("unknown", 120);
    assert(session.state().error == TransferError::InvalidScore);
    upload.execute("one", 0);
    assert(session.state().error == TransferError::InvalidScore);
    assert(transport.sent.size() == count);
}

void checkReentrantConnection() {
    test::FakeTransport transport;
    TransferSession session(transport);
    session.setOnChanged([&] {
        if (session.state().phase == TransferPhase::Connecting) {
            session.cancel();
        }
    });
    session.connectPort("TEST");
    assert(session.state().phase == TransferPhase::Disconnected);
    assert(session.state().result == TransferResult::Cancelled);
    assert(!transport.opened);
    transport.ready();
    assert(transport.sent.empty());

    session.setOnChanged([&] {
        if (session.state().phase == TransferPhase::Connecting) {
            session.disconnect();
        }
    });
    session.connectPort("TEST");
    assert(session.state().phase == TransferPhase::Disconnected);
    assert(!transport.opened);

    // Both attempts have the same phase; only the newer one may open its port.
    bool replaced = false;
    session.setOnChanged([&] {
        if (!replaced && session.state().phase == TransferPhase::Connecting) {
            replaced = true;
            session.connectPort("SECOND");
        }
    });
    session.connectPort("FIRST");
    assert(transport.opened);
    assert(transport.openedPort == "SECOND");
    transport.ready();
    assert(transport.sent.back() == "HELLO 1\n");
    transport.receive("READY 1 96 DRYRUN\n");
    assert(session.state().phase == TransferPhase::Ready);
}

int main() {
    checkUpload();
    checkFailures();
    checkCapacityAndUseCase();
    checkReentrantConnection();
    std::cout << "PASS: fragmented handshake, upload transcript, errors, timeout, "
                 "cancellation, capacity, reentrant connection\n";
}
