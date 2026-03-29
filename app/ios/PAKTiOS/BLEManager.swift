import CoreBluetooth
import Foundation

@MainActor
final class BLEManager: NSObject, ObservableObject {
    enum ConnectionState: String {
        case unavailable
        case idle
        case scanning
        case connecting
        case connected
        case disconnecting
        case failed
    }

    @Published var bluetoothReady = false
    @Published var state: ConnectionState = .unavailable
    @Published var discovered: [PeripheralDescriptor] = []
    @Published var connectedName: String = "Not connected"
    @Published var lastError: String?
    @Published var status = DeviceStatus()
    @Published var gps = GpsTelemetry()
    @Published var power = PowerTelemetry()
    @Published var system = SystemTelemetry()
    @Published var capabilities = DeviceCapabilities()
    @Published var configJSON: String = ""
    @Published var packets: [PacketEvent] = []
    @Published var debugLines: [DebugEvent] = []
    @Published var txResults: [TxResult] = []
    @Published var radioSettings = RadioSettings()
    @Published var debugStreaming = false

    private lazy var central = CBCentralManager(delegate: self, queue: nil)
    private var peripherals: [UUID: CBPeripheral] = [:]
    private var connectedPeripheral: CBPeripheral?
    private var characteristics: [CBUUID: CBCharacteristic] = [:]
    private var reassemblers: [CBUUID: Reassembler] = [:]
    private var messageID: UInt8 = 0
    private var characteristicReadHandlers: [CBUUID: (Data) -> Void] = [:]
    private var pendingServiceCount = 0
    private var discoveredServiceCount = 0

    func startScan() {
        guard central.state == .poweredOn else {
            state = .unavailable
            return
        }
        discovered.removeAll()
        peripherals.removeAll()
        state = .scanning
        central.scanForPeripherals(withServices: [PaktUUIDs.aprsService, PaktUUIDs.telemetryService], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])
    }

    func stopScan() {
        central.stopScan()
        if state == .scanning {
            state = .idle
        }
    }

    func connect(to descriptor: PeripheralDescriptor) {
        guard let peripheral = peripherals[descriptor.id] else { return }
        stopScan()
        state = .connecting
        connectedName = descriptor.name
        connectedPeripheral = peripheral
        peripheral.delegate = self
        central.connect(peripheral, options: nil)
    }

    func disconnect() {
        guard let peripheral = connectedPeripheral else { return }
        state = .disconnecting
        central.cancelPeripheralConnection(peripheral)
    }

    func refreshConfig() {
        readValue(for: PaktUUIDs.deviceConfig) { [weak self] data in
            self?.configJSON = ProtocolCodec.decodeText(from: data)
        }
    }

    func refreshCapabilities() {
        readValue(for: PaktUUIDs.deviceCaps) { [weak self] data in
            guard let caps = ProtocolCodec.decodeJSON(DeviceCapabilities.self, from: data) else { return }
            self?.capabilities = caps
        }
    }

    func sendTXRequest(dest: String, text: String, ssid: Int = 0) {
        let payload: [String: Any] = ["dest": dest, "text": text, "ssid": ssid]
        writeChunkedJSON(payload, to: PaktUUIDs.txRequest, withResponse: true)
    }

    func sendRawCommand(_ json: String) {
        guard let data = json.data(using: .utf8) else {
            lastError = "Raw command is not valid UTF-8."
            return
        }
        write(data, to: PaktUUIDs.deviceCommand, withResponse: false)
    }

    func applyRadioSettings(_ settings: RadioSettings) {
        let payload: [String: Any] = [
            "cmd": "radio_set",
            "rx_freq_hz": settings.rxFreqHz,
            "tx_freq_hz": settings.txFreqHz,
            "squelch": settings.squelch,
            "volume": settings.volume,
            "wide_band": settings.wideBand
        ]
        writeJSON(payload, to: PaktUUIDs.deviceCommand, withResponse: false)
    }

    func triggerBeaconNow() {
        writeJSON(["cmd": "beacon_now"], to: PaktUUIDs.deviceCommand, withResponse: false)
    }

    func setDebugStreaming(_ enabled: Bool) {
        debugStreaming = enabled
        if let characteristic = characteristics[PaktUUIDs.debugStream], let peripheral = connectedPeripheral {
            peripheral.setNotifyValue(enabled, for: characteristic)
        }
        writeJSON(["cmd": "debug_stream", "enabled": enabled], to: PaktUUIDs.deviceCommand, withResponse: false)
    }

    func clearDebugConsole() {
        debugLines.removeAll()
    }

    private func nextMessageID() -> UInt8 {
        messageID &+= 1
        return messageID == 0 ? 1 : messageID
    }

    private func readValue(for uuid: CBUUID, handler: @escaping (Data) -> Void) {
        guard let peripheral = connectedPeripheral, let characteristic = characteristics[uuid] else { return }
        characteristicReadHandlers[uuid] = handler
        peripheral.readValue(for: characteristic)
    }

    private func writeJSON(_ object: [String: Any], to uuid: CBUUID, withResponse: Bool) {
        guard JSONSerialization.isValidJSONObject(object),
              let data = try? JSONSerialization.data(withJSONObject: object) else {
            lastError = "Failed to encode JSON payload."
            return
        }
        write(data, to: uuid, withResponse: withResponse)
    }

    private func writeChunkedJSON(_ object: [String: Any], to uuid: CBUUID, withResponse: Bool) {
        guard JSONSerialization.isValidJSONObject(object),
              let data = try? JSONSerialization.data(withJSONObject: object) else {
            lastError = "Failed to encode JSON payload."
            return
        }
        guard let peripheral = connectedPeripheral, let characteristic = characteristics[uuid] else { return }
        let chunkPayloadMax = 17
        let chunks = Chunking.split(payload: data, messageID: nextMessageID(), chunkPayloadMax: chunkPayloadMax)
        for chunk in chunks {
            peripheral.writeValue(chunk, for: characteristic, type: withResponse ? .withResponse : .withoutResponse)
        }
    }

    private func write(_ data: Data, to uuid: CBUUID, withResponse: Bool) {
        guard let peripheral = connectedPeripheral, let characteristic = characteristics[uuid] else { return }
        peripheral.writeValue(data, for: characteristic, type: withResponse ? .withResponse : .withoutResponse)
    }

    private func subscribeDefaultNotifications(on peripheral: CBPeripheral) {
        for uuid in PaktUUIDs.notifyCharacteristics {
            if let characteristic = characteristics[uuid] {
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
    }

    private func resetSession() {
        characteristics.removeAll()
        reassemblers.removeAll()
        characteristicReadHandlers.removeAll()
        connectedPeripheral = nil
        connectedName = "Not connected"
        configJSON = ""
        debugStreaming = false
        pendingServiceCount = 0
        discoveredServiceCount = 0
    }

    private func ensureReassembler(for uuid: CBUUID) -> Reassembler {
        if let existing = reassemblers[uuid] { return existing }
        let reassembler = Reassembler { [weak self] data in
            Task { @MainActor [weak self] in
                self?.routeNotification(uuid: uuid, data: data)
            }
        }
        reassemblers[uuid] = reassembler
        return reassembler
    }

    private func handleIncoming(uuid: CBUUID, data: Data) {
        if Chunking.looksChunked(data) {
            _ = ensureReassembler(for: uuid).feed(data)
        } else {
            routeNotification(uuid: uuid, data: data)
        }
    }

    private func routeNotification(uuid: CBUUID, data: Data) {
        if let reader = characteristicReadHandlers.removeValue(forKey: uuid) {
            reader(data)
            return
        }

        switch uuid {
        case PaktUUIDs.deviceStatus:
            if let value = ProtocolCodec.decodeJSON(DeviceStatus.self, from: data) {
                status = value
                radioSettings = RadioSettings(status: value)
                debugStreaming = value.debugEnabled
            }
        case PaktUUIDs.gpsTelemetry:
            if let value = ProtocolCodec.decodeJSON(GpsTelemetry.self, from: data) {
                gps = value
            }
        case PaktUUIDs.powerTelemetry:
            if let value = ProtocolCodec.decodeJSON(PowerTelemetry.self, from: data) {
                power = value
            }
        case PaktUUIDs.systemTelemetry:
            if let value = ProtocolCodec.decodeJSON(SystemTelemetry.self, from: data) {
                system = value
            }
        case PaktUUIDs.txResult:
            if let value = ProtocolCodec.decodeJSON(TxResult.self, from: data) {
                txResults.insert(value, at: 0)
                txResults = Array(txResults.prefix(30))
            }
        case PaktUUIDs.rxPacket:
            let text = ProtocolCodec.decodeText(from: data)
            packets.insert(PacketEvent(text: text, receivedAt: .now), at: 0)
            packets = Array(packets.prefix(200))
        case PaktUUIDs.debugStream:
            let text = ProtocolCodec.decodeText(from: data)
            debugLines.append(DebugEvent(text: text, receivedAt: .now))
            if debugLines.count > 500 {
                debugLines.removeFirst(debugLines.count - 500)
            }
        default:
            break
        }
    }
}

extension BLEManager: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            bluetoothReady = (central.state == .poweredOn)
            state = bluetoothReady ? .idle : .unavailable
            if !bluetoothReady {
                lastError = "Bluetooth unavailable: \(central.state.rawValue)"
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any],
                                    rssi RSSI: NSNumber) {
        Task { @MainActor in
            let name = peripheral.name
                ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
                ?? "PAKT"
            peripherals[peripheral.identifier] = peripheral
            let descriptor = PeripheralDescriptor(id: peripheral.identifier, name: name, rssi: RSSI.intValue)
            if let index = discovered.firstIndex(where: { $0.id == descriptor.id }) {
                discovered[index] = descriptor
            } else {
                discovered.append(descriptor)
                discovered.sort { $0.name < $1.name }
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            state = .connected
            connectedName = peripheral.name ?? "PAKT"
            peripheral.discoverServices([PaktUUIDs.aprsService, PaktUUIDs.telemetryService, PaktUUIDs.kissService])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            state = .failed
            lastError = error?.localizedDescription ?? "Failed to connect."
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            resetSession()
            state = .idle
            if let error {
                lastError = error.localizedDescription
            }
        }
    }
}

extension BLEManager: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
                return
            }
            let services = peripheral.services ?? []
            // Set pending count from actual discovered services so the
            // didDiscoverCharacteristicsFor counter reaches the right target.
            pendingServiceCount = max(services.count, 1)
            discoveredServiceCount = 0
            services.forEach { peripheral.discoverCharacteristics(nil, for: $0) }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
                return
            }
            service.characteristics?.forEach { characteristics[$0.uuid] = $0 }
            discoveredServiceCount += 1
            if discoveredServiceCount >= pendingServiceCount {
                // All expected services have reported their characteristics — safe to subscribe.
                subscribeDefaultNotifications(on: peripheral)
                refreshConfig()
                refreshCapabilities()
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didUpdateValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
                return
            }
            guard let value = characteristic.value else { return }
            handleIncoming(uuid: characteristic.uuid, data: value)
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didWriteValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didUpdateNotificationStateFor characteristic: CBCharacteristic,
                                error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
            }
        }
    }
}
