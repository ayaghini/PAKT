import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var ble: BLEManager
    @State private var txDest = "APRS"
    @State private var txText = ""
    @State private var rawCommand = "{\"cmd\":\"beacon_now\"}"

    var body: some View {
        TabView {
            NavigationStack {
                DashboardView()
            }
            .tabItem { Label("Device", systemImage: "dot.radiowaves.left.and.right") }

            NavigationStack {
                PacketsView()
            }
            .tabItem { Label("Packets", systemImage: "tray.full") }

            NavigationStack {
                GPSView()
            }
            .tabItem { Label("GPS", systemImage: "location") }

            NavigationStack {
                TransmitView(txDest: $txDest, txText: $txText, rawCommand: $rawCommand)
            }
            .tabItem { Label("TX", systemImage: "paperplane") }

            NavigationStack {
                RadioView()
            }
            .tabItem { Label("Radio", systemImage: "slider.horizontal.3") }

            NavigationStack {
                DebugView()
            }
            .tabItem { Label("Debug", systemImage: "ladybug") }
        }
        .alert("BLE Error", isPresented: Binding(
            get: { ble.lastError != nil },
            set: { if !$0 { ble.lastError = nil } }
        )) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(ble.lastError ?? "")
        }
    }
}

private struct DashboardView: View {
    @EnvironmentObject private var ble: BLEManager

    var body: some View {
        List {
            Section {
                LabeledContent("State", value: ble.state.rawValue)
                LabeledContent("Device", value: ble.connectedName)
                HStack(spacing: 12) {
                    Button("Scan") { ble.startScan() }
                        .disabled(ble.state == .scanning || ble.state == .connected)
                    Button("Stop") { ble.stopScan() }
                        .disabled(ble.state != .scanning)
                    Spacer()
                    Button("Disconnect", role: .destructive) { ble.disconnect() }
                        .disabled(ble.state != .connected)
                }
            } header: {
                Text("Connection")
            } footer: {
                if ble.state == .scanning {
                    Text("Scanning… tap a device below to connect.")
                } else if ble.discovered.isEmpty && ble.state == .idle {
                    Text("Press Scan to find nearby PAKT devices.")
                }
            }

            if !ble.discovered.isEmpty {
                Section("Discovered Devices") {
                    ForEach(ble.discovered) { item in
                        Button {
                            ble.connect(to: item)
                        } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.name)
                                        .foregroundStyle(.primary)
                                    Text("RSSI \(item.rssi) dBm")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                Image(systemName: "chevron.right")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .disabled(ble.state == .connecting || ble.state == .connected)
                    }
                }
            }

            Section("Capabilities") {
                LabeledContent("Firmware", value: ble.capabilities.fwVer)
                LabeledContent("Hardware", value: ble.capabilities.hwRev)
                LabeledContent("Protocol", value: "\(ble.capabilities.protocolVersion)")
                if ble.capabilities.features.isEmpty {
                    Text("No capability record read yet.")
                        .foregroundStyle(.secondary)
                } else {
                    Text(ble.capabilities.features.joined(separator: ", "))
                        .font(.caption)
                }
            }

            Section("Device Status") {
                LabeledContent("Radio", value: ble.status.radio)
                LabeledContent("Bonded", value: ble.status.bonded ? "Yes" : "No")
                LabeledContent("Encrypted", value: ble.status.encrypted ? "Yes" : "No")
                LabeledContent("GPS Fix", value: ble.status.gpsFix ? "Yes" : "No")
                LabeledContent("Pending TX", value: "\(ble.status.pendingTx)")
                LabeledContent("RX Queue", value: "\(ble.status.rxQueue)")
                LabeledContent("RX/TX", value: "\(ble.status.rxFreqHz)/\(ble.status.txFreqHz)")
                LabeledContent("Squelch / Volume", value: "\(ble.status.squelch) / \(ble.status.volume)")
                LabeledContent("Bandwidth", value: ble.status.wideBand ? "Wide" : "Narrow")
                LabeledContent("Debug Stream", value: ble.status.debugEnabled ? "Enabled" : "Disabled")
                LabeledContent("Uptime", value: uptimeString(ble.status.uptimeS))
            }

            Section("Telemetry") {
                LabeledContent("System Uptime", value: uptimeString(ble.system.uptimeS))
                LabeledContent("Heap", value: "\(ble.system.freeHeap) free / \(ble.system.minHeap) min")
                LabeledContent("Packets", value: "TX \(ble.system.txPkts)  RX \(ble.system.rxPkts)")
                LabeledContent("Errors", value: "TX \(ble.system.txErrs)  RX \(ble.system.rxErrs)")
            }
        }
        .navigationTitle("PAKTiOS")
    }
}

private struct PacketsView: View {
    @EnvironmentObject private var ble: BLEManager

    var body: some View {
        List(ble.packets) { packet in
            VStack(alignment: .leading, spacing: 4) {
                Text(packet.text)
                    .font(.body.monospaced())
                Text(packet.receivedAt.formatted(date: .omitted, time: .standard))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle("Received APRS")
    }
}

private struct GPSView: View {
    @EnvironmentObject private var ble: BLEManager

    var body: some View {
        List {
            Section("Fix") {
                LabeledContent("State", value: gpsFixLabel(ble.gps.fix))
                LabeledContent("Satellites", value: "\(ble.gps.sats)")
                LabeledContent("Timestamp", value: ble.gps.ts == 0 ? "Unknown" : "\(ble.gps.ts)")
            }
            Section("Position") {
                LabeledContent("Latitude", value: String(format: "%.5f", ble.gps.lat))
                LabeledContent("Longitude", value: String(format: "%.5f", ble.gps.lon))
                LabeledContent("Altitude", value: String(format: "%.1f m", ble.gps.altM))
            }
            Section("Motion") {
                LabeledContent("Speed", value: String(format: "%.1f km/h", ble.gps.speedKmh))
                LabeledContent("Course", value: String(format: "%.1f°", ble.gps.course))
            }
        }
        .navigationTitle("GPS")
    }
}

private struct TransmitView: View {
    @EnvironmentObject private var ble: BLEManager
    @Binding var txDest: String
    @Binding var txText: String
    @State private var txSSID = 0
    @Binding var rawCommand: String
    @FocusState private var focused: Field?

    enum Field { case dest, text, raw }

    var body: some View {
        List {
            if !ble.status.bonded || !ble.status.encrypted {
                Section {
                    Label {
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Device not paired").fontWeight(.semibold)
                            Text("The device requires an encrypted, bonded connection before accepting transmit commands. Disconnect, then reconnect — iOS will prompt to pair on first write.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    } icon: {
                        Image(systemName: "lock.trianglebadge.exclamationmark")
                            .foregroundStyle(.orange)
                    }
                }
            }

            Section("APRS Message") {
                TextField("Destination (e.g. APRS)", text: $txDest)
                    .textInputAutocapitalization(.characters)
                    .autocorrectionDisabled()
                    .focused($focused, equals: .dest)
                    .submitLabel(.next)
                    .onSubmit { focused = .text }
                Stepper("SSID: \(txSSID)", value: $txSSID, in: 0...15)
                TextField("Message text", text: $txText, axis: .vertical)
                    .lineLimit(2...5)
                    .focused($focused, equals: .text)
                    .submitLabel(.done)
                    .onSubmit { focused = nil }
                Button("Send APRS Message") {
                    focused = nil
                    ble.sendTXRequest(dest: txDest, text: txText, ssid: txSSID)
                }
                .disabled(txDest.isEmpty || txText.isEmpty || ble.state != .connected)
            }

            Section("One-shot Actions") {
                Button("Beacon Now") {
                    ble.triggerBeaconNow()
                }
                .disabled(ble.state != .connected)
            }

            Section {
                TextField("JSON command", text: $rawCommand, axis: .vertical)
                    .lineLimit(3...6)
                    .font(.body.monospaced())
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    .focused($focused, equals: .raw)
                    .submitLabel(.done)
                    .onSubmit { focused = nil }
                Button("Send Raw Command") {
                    focused = nil
                    ble.sendRawCommand(rawCommand)
                }
                .disabled(ble.state != .connected)
            } header: {
                Text("Advanced Raw Command")
            } footer: {
                Text("Write raw JSON to the Device Command characteristic (write-no-rsp; errors are silent if not paired).")
                    .font(.caption)
            }

            if !ble.txResults.isEmpty {
                Section("Recent TX Results") {
                    ForEach(ble.txResults) { result in
                        HStack {
                            Text(result.msgID)
                                .foregroundStyle(.secondary)
                            Spacer()
                            Text(result.status)
                                .foregroundStyle(statusColor(result.status))
                        }
                    }
                }
            }
        }
        .navigationTitle("Transmit")
        .toolbar {
            ToolbarItemGroup(placement: .keyboard) {
                Spacer()
                Button("Done") { focused = nil }
            }
        }
    }

    private func statusColor(_ status: String) -> Color {
        switch status {
        case "acked": return .green
        case "timeout", "error": return .red
        default: return .secondary
        }
    }
}

private struct RadioView: View {
    @EnvironmentObject private var ble: BLEManager
    @State private var localSettings = RadioSettings()

    var body: some View {
        List {
            Section("Radio Controls") {
                TextField("RX Frequency (Hz)", value: $localSettings.rxFreqHz, format: .number)
                    .keyboardType(.numberPad)
                TextField("TX Frequency (Hz)", value: $localSettings.txFreqHz, format: .number)
                    .keyboardType(.numberPad)
                Stepper("Squelch: \(localSettings.squelch)", value: $localSettings.squelch, in: 0...8)
                Stepper("Volume: \(localSettings.volume)", value: $localSettings.volume, in: 1...8)
                Toggle("Wide Band", isOn: $localSettings.wideBand)
                Button("Apply Settings") {
                    ble.applyRadioSettings(localSettings)
                }
            }

            Section("Current Device State") {
                LabeledContent("RX", value: "\(ble.status.rxFreqHz)")
                LabeledContent("TX", value: "\(ble.status.txFreqHz)")
                LabeledContent("Squelch", value: "\(ble.status.squelch)")
                LabeledContent("Volume", value: "\(ble.status.volume)")
                LabeledContent("Bandwidth", value: ble.status.wideBand ? "Wide" : "Narrow")
            }
        }
        .navigationTitle("Radio")
        .onAppear { localSettings = ble.radioSettings }
        .onChange(of: ble.radioSettings) { _, newValue in
            localSettings = newValue
        }
    }
}

private struct DebugView: View {
    @EnvironmentObject private var ble: BLEManager

    var body: some View {
        List {
            Section {
                Toggle("Enable Debug Stream", isOn: Binding(
                    get: { ble.debugStreaming },
                    set: { ble.setDebugStreaming($0) }
                ))
                Button("Clear Console", role: .destructive) {
                    ble.clearDebugConsole()
                }
            } footer: {
                Text("Debug lines are session-only and come from the dedicated BLE debug stream.")
            }

            Section("Live Debug") {
                ForEach(ble.debugLines) { line in
                    VStack(alignment: .leading, spacing: 4) {
                        Text(line.text)
                            .font(.caption.monospaced())
                        Text(line.receivedAt.formatted(date: .omitted, time: .standard))
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }
            }
        }
        .navigationTitle("Debug")
    }
}

private func uptimeString(_ seconds: Int) -> String {
    let hours = seconds / 3600
    let minutes = (seconds % 3600) / 60
    let secs = seconds % 60
    if hours > 0 { return "\(hours)h \(minutes)m \(secs)s" }
    if minutes > 0 { return "\(minutes)m \(secs)s" }
    return "\(secs)s"
}

private func gpsFixLabel(_ fix: Int) -> String {
    switch fix {
    case 1: return "GPS"
    case 2: return "DGPS"
    default: return "No fix"
    }
}
