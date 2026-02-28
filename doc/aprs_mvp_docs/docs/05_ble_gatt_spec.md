# BLE GATT specification (draft)

This is a pragmatic BLE profile for:
- Provisioning/config
- Streaming RX packets
- Submitting TX requests
- Telemetry

## Services overview
1. Device Info Service (standard)
2. APRS Control Service (custom)
3. APRS Data Service (custom)
4. Telemetry Service (custom)
5. DFU Service (optional later)

### UUID conventions
Use a base UUID and derive 16-bit short IDs:
- Base: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` (choose once for the project)
- Example shown below uses placeholder values.

---

## 1) APRS Control Service (custom)
**Service UUID:** `A000`

### Characteristics
#### 1.1 Config (RW, JSON)
- **UUID:** `A001`
- **Properties:** Read, Write
- **Format:** UTF-8 JSON
- **Fields (MVP):**
  - `callsign`: string (e.g., "VA7XXX")
  - `ssid`: int (0-15)
  - `aprs_freq_hz`: int (default region preset)
  - `path`: string (e.g., "WIDE1-1,WIDE2-1")
  - `beacon_interval_s`: int
  - `symbol_table`: string ("/" or "\\")
  - `symbol_code`: string (e.g., ">")
  - `comment`: string
  - `tx_power_profile`: string (e.g., "low","mid","high" depending on SA818 options)
  - `mic_gain`: int (codec/SA818 audio scaling)
  - `rx_gain`: int

#### 1.2 Command (W)
- **UUID:** `A002`
- **Properties:** Write
- **Format:** UTF-8 JSON
- **Commands:**
  - `{"cmd":"beacon_now"}`
  - `{"cmd":"ptt_test","ms":500}`
  - `{"cmd":"radio_set","freq_hz":144390000,"ctcss_hz":0}`

#### 1.3 Status (N)
- **UUID:** `A003`
- **Properties:** Notify
- **Format:** UTF-8 JSON (rate limited, e.g., 1–2 Hz)
- **Fields:**
  - `state`: "idle"|"rx"|"tx"
  - `gps`: { `fix`:0/1, `lat`:float, `lon`:float, `alt_m`:float, `speed_kmh`:float, `course_deg`:float, `age_s`:int }
  - `battery`: { `v`:float, `pct`:int, `charging`:0/1 }
  - `queue`: { `tx_pending`:int, `last_tx_ok`:0/1, `last_tx_age_s`:int }

---

## 2) APRS Data Service (custom)
### 2.1 RX Packet Stream (N, binary)
- **UUID:** `A010`
- **Properties:** Notify
- **Format:** binary frame (simple TLV)
  - byte0: version (0x01)
  - byte1: type (0x01 = RX_PACKET)
  - uint16: payload_len
  - payload (UTF‑8 text line or packed structure)
- MVP payload recommendation: **UTF‑8 TNC2 monitor line**, e.g.
  - `CALL1>APRS,WIDE1-1:!4903.50N/12310.00W>comment`

### 2.2 TX Request (W, JSON)
- **UUID:** `A011`
- **Properties:** Write
- **Format:** UTF‑8 JSON
- Example:
  - `{"type":"message","to":"CALL-9","text":"On my way","msg_id":"01"}`
  - `{"type":"position","immediate":true}`

### 2.3 TX Result (N, JSON)
- **UUID:** `A012`
- **Properties:** Notify
- Fields:
  - `msg_id`: string
  - `accepted`: 0/1
  - `sent`: 0/1
  - `acked`: 0/1
  - `retries`: int
  - `error`: string|null

---

## 3) Telemetry Service (custom)
### 3.1 Telemetry (N, JSON)
- **UUID:** `A020`
- **Properties:** Notify (0.2–1 Hz)
- Fields:
  - `uptime_s`
  - `heap_free`
  - `temperature_c` (optional)
  - `rssi_proxy` (optional; SA818 doesn't expose true RSSI reliably)

---

## Security notes
- MVP: BLE pairing optional; allow a “setup mode” window for provisioning.
- Consider a PIN or “press button to pair” for production units.
