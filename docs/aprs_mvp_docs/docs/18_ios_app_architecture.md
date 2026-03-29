# iPhone App Architecture

Date: 2026-03-28
Status: initial in-repo scaffold landed; simulator build verified

## Purpose

Define the first iPhone-only operator app for PAKT while keeping protocol compatibility with the existing desktop BLE app.

The app is an MVP operator tool, not a consumer-polished product.

## Goals

- connect and disconnect to a PAKT device over BLE
- show live APRS packets from the native `rx_packet` stream
- show live GPS, system, and device status
- send APRS transmit requests using the existing `tx_request` contract
- send advanced raw commands over `device_command`
- control SA818 settings through the same command channel
- show a session-only debug console backed by the dedicated BLE debug stream

## Compatibility rule

The iPhone app must reuse the same BLE protocol and payload contracts as the desktop app wherever possible.

That means:
- same UUID map
- same JSON payloads
- same TX result semantics
- same device status semantics
- same chunking protocol for client writes
- no separate iOS-only radio-control protocol

New protocol surface added for the app:
- `0xA024` Debug Stream notify characteristic
- expanded `device_command` payload support
- enriched `device_status` payload for dashboard/radio state

## Code layout

Current scaffold:

- `app/ios/project.yml` — XcodeGen source of truth
- `app/ios/PAKTiOS/PAKTiOSApp.swift` — app entry
- `app/ios/PAKTiOS/BLEProtocol.swift` — UUIDs, chunk split/reassembly, payload decoding helpers
- `app/ios/PAKTiOS/Models.swift` — protocol-aligned models
- `app/ios/PAKTiOS/BLEManager.swift` — CoreBluetooth transport/session state
- `app/ios/PAKTiOS/ContentView.swift` — SwiftUI tab shell and feature screens
- `app/ios/PAKTiOSTests/` — protocol-level smoke tests

## Screen set

V1 screen groups:

1. Device
- scan/connect/disconnect
- capability summary
- live device status

2. Packets
- session-only APRS monitor list from native `rx_packet`

3. GPS
- textual GPS fix and motion status

4. Transmit
- APRS TX request send
- beacon-now action
- raw command send
- TX result history

5. Radio
- RX/TX frequency
- squelch
- volume
- bandwidth

6. Debug
- toggle debug stream
- live append-only console
- clear session log

## Transport notes

- CoreBluetooth is wrapped in a single `BLEManager`
- the app subscribes to the native notify characteristics used by the desktop app
- debug stream subscription is enabled only when the user toggles it on
- chunking is required for client writes that may exceed one ATT payload, matching the desktop `chunker.py` behavior

## Session model

V1 is session-only:
- APRS packets are kept in memory only
- debug console is kept in memory only
- no local database
- no offline queueing

## Firmware dependencies

The app depends on firmware support for:
- `device_status` live notify
- `device_command` support for:
  - `radio_set`
  - `debug_stream`
  - `beacon_now`
- `debug_stream` notify characteristic

## Validation checklist

- app builds for iPhone simulator
- scan/connect flow works
- capability and config reads work
- APRS RX notify appears in the packet list
- GPS telemetry updates the GPS screen
- TX request yields `tx_result`
- radio settings write over `device_command`
- debug toggle enables/disables the dedicated debug stream
- desktop app still interoperates with the same firmware build

## Current handoff status

What is already verified in this repo state:
- firmware protocol additions build successfully with `idf.py build`
- the updated firmware was flashed successfully to the current prototype on `/dev/cu.usbmodem111101`
- the SwiftUI iPhone app scaffold generates from XcodeGen and builds for iOS Simulator
- the same iPhone app project also builds for a paired physical iPhone on this machine
- desktop Python telemetry tests pass after the protocol updates

What is still blocked:
- desktop BLE validation from this Mac was not completed in-session; host BLE scanning behaved inconsistently and should be rechecked by the next agent
- physical iPhone install/launch has not yet been verified end to end
- on-device iPhone BLE validation has not yet been run
- keep `app/ios/project.yml` and the generated `app/ios/PAKTiOS.xcodeproj` aligned on future changes; treat `project.yml` as the source of truth and regenerate deliberately
