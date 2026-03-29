import CoreBluetooth
import Foundation

enum PaktUUIDs {
    private static let base = "544E4332-8A48-4328-9844-3F5C"

    static let deviceConfig = CBUUID(string: "\(base)A0010000")
    static let deviceCommand = CBUUID(string: "\(base)A0020000")
    static let deviceStatus = CBUUID(string: "\(base)A0030000")
    static let deviceCaps = CBUUID(string: "\(base)A0040000")
    static let rxPacket = CBUUID(string: "\(base)A0100000")
    static let txRequest = CBUUID(string: "\(base)A0110000")
    static let txResult = CBUUID(string: "\(base)A0120000")

    static let telemetryService = CBUUID(string: "\(base)A0200000")
    static let gpsTelemetry = CBUUID(string: "\(base)A0210000")
    static let powerTelemetry = CBUUID(string: "\(base)A0220000")
    static let systemTelemetry = CBUUID(string: "\(base)A0230000")
    static let debugStream = CBUUID(string: "\(base)A0240000")

    static let aprsService = CBUUID(string: "\(base)A0000000")
    static let kissService = CBUUID(string: "\(base)A0500000")
    static let kissRX = CBUUID(string: "\(base)A0510000")
    static let kissTX = CBUUID(string: "\(base)A0520000")

    static let notifyCharacteristics: [CBUUID] = [
        deviceStatus, rxPacket, txResult, gpsTelemetry, powerTelemetry, systemTelemetry
    ]
}

enum Chunking {
    static let headerSize = 3
    static let maxChunks = 64

    static func split(payload: Data, messageID: UInt8, chunkPayloadMax: Int) -> [Data] {
        guard !payload.isEmpty, chunkPayloadMax > 0 else { return [] }

        let slices = stride(from: 0, to: payload.count, by: chunkPayloadMax).map {
            payload.subdata(in: $0..<min($0 + chunkPayloadMax, payload.count))
        }
        guard slices.count <= maxChunks else { return [] }

        return slices.enumerated().map { index, slice in
            var data = Data([messageID, UInt8(index & 0xFF), UInt8(slices.count & 0xFF)])
            data.append(slice)
            return data
        }
    }

    static func looksChunked(_ data: Data) -> Bool {
        guard data.count >= headerSize else { return false }
        let chunkIndex = Int(data[1])
        let chunkTotal = Int(data[2])
        guard chunkTotal > 0, chunkTotal <= maxChunks, chunkIndex < chunkTotal else { return false }
        return chunkTotal > 1 || chunkIndex > 0
    }
}

final class Reassembler {
    private struct Slot {
        var total: Int
        var parts: [Int: Data]
        var startedAt: Date
    }

    private let timeout: TimeInterval
    private let callback: (Data) -> Void
    private var slots: [UInt8: Slot] = [:]

    init(timeout: TimeInterval = 5.0, callback: @escaping (Data) -> Void) {
        self.timeout = timeout
        self.callback = callback
    }

    func reset() {
        slots.removeAll()
    }

    func feed(_ data: Data) -> Bool {
        guard data.count >= Chunking.headerSize else { return false }
        expire()

        let messageID = data[0]
        let chunkIndex = Int(data[1])
        let chunkTotal = Int(data[2])
        let payload = data.dropFirst(Chunking.headerSize)

        guard chunkTotal > 0, chunkTotal <= Chunking.maxChunks, chunkIndex < chunkTotal else {
            return false
        }

        if slots[messageID] == nil {
            slots[messageID] = Slot(total: chunkTotal, parts: [:], startedAt: Date())
        }
        guard var slot = slots[messageID], slot.total == chunkTotal else { return false }
        if slot.parts[chunkIndex] != nil { return true }

        slot.parts[chunkIndex] = Data(payload)
        slots[messageID] = slot

        if slot.parts.count == chunkTotal {
            let assembled = (0..<chunkTotal).reduce(into: Data()) { result, idx in
                if let part = slot.parts[idx] { result.append(part) }
            }
            slots.removeValue(forKey: messageID)
            callback(assembled)
        }
        return true
    }

    private func expire() {
        let now = Date()
        slots = slots.filter { now.timeIntervalSince($0.value.startedAt) <= timeout }
    }
}

enum ProtocolCodec {
    static func decodeJSON<T: Decodable>(_ type: T.Type, from data: Data) -> T? {
        let decoder = JSONDecoder()
        return try? decoder.decode(T.self, from: data)
    }

    static func decodeText(from data: Data) -> String {
        String(data: data, encoding: .utf8) ?? data.map { String(format: "%02x", $0) }.joined()
    }
}
