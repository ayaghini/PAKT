import XCTest
@testable import PAKTiOS

final class PAKTiOSTests: XCTestCase {
    func testDeviceStatusDecodesExtendedFields() throws {
        let json = """
        {"radio":"idle","bonded":true,"encrypted":true,"gps_fix":true,"pending_tx":1,"rx_queue":2,"rx_freq_hz":144390000,"tx_freq_hz":144390000,"squelch":3,"volume":4,"wide_band":false,"debug_enabled":true,"uptime_s":42}
        """
        let data = try XCTUnwrap(json.data(using: .utf8))
        let status = try XCTUnwrap(ProtocolCodec.decodeJSON(DeviceStatus.self, from: data))
        XCTAssertEqual(status.radio, "idle")
        XCTAssertTrue(status.bonded)
        XCTAssertTrue(status.encrypted)
        XCTAssertEqual(status.pendingTx, 1)
        XCTAssertEqual(status.rxFreqHz, 144_390_000)
        XCTAssertFalse(status.wideBand)
        XCTAssertTrue(status.debugEnabled)
    }

    func testChunkSplitAndReassembleRoundTrip() throws {
        let payload = Data("hello over ble".utf8)
        let chunks = Chunking.split(payload: payload, messageID: 7, chunkPayloadMax: 5)
        XCTAssertEqual(chunks.count, 3)

        let expectation = expectation(description: "reassembled")
        var rebuilt = Data()
        let reassembler = Reassembler { data in
            rebuilt = data
            expectation.fulfill()
        }
        chunks.reversed().forEach { _ = reassembler.feed($0) }
        waitForExpectations(timeout: 1.0)
        XCTAssertEqual(rebuilt, payload)
    }
}
