import Foundation

struct PeripheralDescriptor: Identifiable, Equatable {
    let id: UUID
    let name: String
    let rssi: Int
}

struct DeviceCapabilities: Codable, Equatable {
    var fwVer: String = "unknown"
    var hwRev: String = "unknown"
    var protocolVersion: Int = 0
    var features: [String] = []

    enum CodingKeys: String, CodingKey {
        case fwVer = "fw_ver"
        case hwRev = "hw_rev"
        case protocolVersion = "protocol"
        case features
    }
}

struct DeviceStatus: Codable, Equatable {
    var radio: String = "unknown"
    var bonded: Bool = false
    var encrypted: Bool = false
    var gpsFix: Bool = false
    var pendingTx: Int = 0
    var rxQueue: Int = 0
    var rxFreqHz: Int = 0
    var txFreqHz: Int = 0
    var squelch: Int = 0
    var volume: Int = 0
    var wideBand: Bool = true
    var debugEnabled: Bool = false
    var uptimeS: Int = 0

    enum CodingKeys: String, CodingKey {
        case radio
        case bonded
        case encrypted
        case gpsFix = "gps_fix"
        case pendingTx = "pending_tx"
        case rxQueue = "rx_queue"
        case rxFreqHz = "rx_freq_hz"
        case txFreqHz = "tx_freq_hz"
        case squelch
        case volume
        case wideBand = "wide_band"
        case debugEnabled = "debug_enabled"
        case uptimeS = "uptime_s"
    }
}

struct GpsTelemetry: Codable, Equatable {
    var lat: Double = 0
    var lon: Double = 0
    var altM: Double = 0
    var speedKmh: Double = 0
    var course: Double = 0
    var sats: Int = 0
    var fix: Int = 0
    var ts: Int = 0

    enum CodingKeys: String, CodingKey {
        case lat, lon, sats, fix, ts
        case altM = "alt_m"
        case speedKmh = "speed_kmh"
        case course
    }
}

struct PowerTelemetry: Codable, Equatable {
    var battV: Double = 0
    var battPct: Int = 0
    var txDbm: Double = 0
    var vswr: Double = 0
    var tempC: Double = 0

    enum CodingKeys: String, CodingKey {
        case battV = "batt_v"
        case battPct = "batt_pct"
        case txDbm = "tx_dbm"
        case vswr
        case tempC = "temp_c"
    }
}

struct SystemTelemetry: Codable, Equatable {
    var freeHeap: Int = 0
    var minHeap: Int = 0
    var cpuPct: Int = 0
    var txPkts: Int = 0
    var rxPkts: Int = 0
    var txErrs: Int = 0
    var rxErrs: Int = 0
    var uptimeS: Int = 0

    enum CodingKeys: String, CodingKey {
        case freeHeap = "free_heap"
        case minHeap = "min_heap"
        case cpuPct = "cpu_pct"
        case txPkts = "tx_pkts"
        case rxPkts = "rx_pkts"
        case txErrs = "tx_errs"
        case rxErrs = "rx_errs"
        case uptimeS = "uptime_s"
    }
}

struct TxResult: Codable, Equatable, Identifiable {
    var msgID: String = ""
    var status: String = ""

    enum CodingKeys: String, CodingKey {
        case msgID = "msg_id"
        case status
    }

    var id: String { "\(msgID)-\(status)" }
}

struct PacketEvent: Identifiable, Equatable {
    let id = UUID()
    let text: String
    let receivedAt: Date
}

struct DebugEvent: Identifiable, Equatable {
    let id = UUID()
    let text: String
    let receivedAt: Date
}

struct RadioSettings: Equatable {
    var rxFreqHz: Int = 144_390_000
    var txFreqHz: Int = 144_390_000
    var squelch: Int = 1
    var volume: Int = 4
    var wideBand: Bool = true

    init() {}

    init(status: DeviceStatus) {
        rxFreqHz = status.rxFreqHz
        txFreqHz = status.txFreqHz
        squelch = status.squelch
        volume = status.volume
        wideBand = status.wideBand
    }
}
