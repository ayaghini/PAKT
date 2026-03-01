# HF + BLE Feasibility Study (Digirig-like Direction)

Date: 2026-02-28
Scope: Evaluate adding an HF-capable hardware variant that pairs with phone/tablet apps over BLE, while keeping a unified software architecture with the current APRS Pocket TNC direction.

## Executive Summary
- A unified architecture is feasible.
- Highest-confidence path: BLE for control/telemetry + on-device modem for packet-style modes where practical.
- Medium-confidence path: BLE bidirectional audio streaming for phone-side modem engines (FT8/PSK/RTTY/JS8 style workflows). It is possible, but this is the highest risk area due to latency/jitter/background behavior and app integration complexity.
- Recommended decision: proceed with a staged program and treat BLE audio streaming as a gated experiment, not a core assumption.

## What Digirig Pattern Means for This Project
Digirig-style value is an external interface between radio and computing device, providing:
- Audio in/out path
- CAT/PTT/control signaling
- Cross-radio adaptability

From Digirig project materials and ecosystem references:
- Digirig hardware itself is an interface, not a digital-mode modem implementation.
- Existing projects show users combine external interface hardware with app/software digital modem stacks.

Implication for PAKT:
- You can copy the integration pattern (interface-first accessory), but product differentiation should be in BLE-native UX, profile automation, and integrated firmware/app behavior.

## Feasibility Assessment

### 1. Hardware Feasibility
Status: High

What is straightforward:
- Add HF radio interface options (audio in/out, PTT line, CAT serial adapters, level selection).
- Keep your ESP32-S3 control plane, telemetry, and BLE stack model.
- Reuse existing power, safety, and bring-up patterns from current prototyping docs.

Main hardware risks:
- Radio-to-radio variability in connector pinouts and CAT/PTT logic.
- Ground loops/noise between phone-powered and radio-powered systems.
- Need for configurable isolation/attenuation footprints for robust interoperability.

### 2. BLE Transport Feasibility
Status: Mixed

Control/telemetry/config over BLE:
- High feasibility. Your current GATT approach already matches this.

Packet/frame transport over BLE (KISS-like, message-oriented):
- High feasibility for APRS/AX.25 style data and command channels.

Continuous bidirectional audio over BLE for phone-side modem processing:
- Medium feasibility technically, lower operational confidence.
- ESP32-class BLE throughput can be high enough in lab conditions, but real mobile behavior (latency, scheduling, background execution, coexistence) is the main risk.

### 3. Unified App Architecture Feasibility
Status: High

You can unify by separating the app into stable layers:
- `Transport Layer`: BLE GATT (and optional USB in future) with common session model.
- `Radio Profile Layer`: per-radio wiring/CAT/PTT templates.
- `Mode Service Layer`: APRS packet mode, HF packet-like modes, and optional experimental audio mode bridge.
- `UX Layer`: same pairing, status, logging, and workflow patterns across VHF APRS and HF usage.

This yields one codebase with mode/profile plug-ins rather than separate apps.

## User Value and Ecosystem Value

### Direct User Value
- One app across multiple radio workflows (APRS today, HF expansion tomorrow).
- Fewer dongles/cables for portable operations.
- Better mobile/tablet-first experience versus laptop-dependent setups.
- Faster setup via profile presets and guided radio pairing.

### Ecosystem Value
- Shared BLE protocol and profile catalog enable third-party mode/app integrations.
- Stronger community contribution potential if protocol/profile docs are published.
- Accessory network effects: each added radio profile increases utility for all users.

## Key Risks and Mitigations

1. BLE audio stability risk
- Mitigation: define two paths early:
  - Path A (production): packet/control-first modes.
  - Path B (experimental): audio bridge with strict acceptance gates.

2. Compatibility matrix explosion (radios/apps/cables)
- Mitigation: start with a narrow certified matrix (for example 2-3 HF radios + 1-2 mobile workflows), then expand incrementally.

3. Mobile OS behavior in background/low-power states
- Mitigation: design UX for explicit session states; surface connection quality and mode capability clearly; avoid hidden auto-magic for critical operations.

4. Support burden
- Mitigation: ship profile-based diagnostics (audio level check, PTT test, CAT test, loopback test) directly in-app.

## Recommended Target Architecture

### Firmware (device)
- Keep BLE control plane aligned with existing custom GATT philosophy.
- Add radio profile abstraction:
  - Audio routing params
  - PTT polarity/method
  - CAT bridge settings
- Implement deterministic state machine for:
  - Idle / RX-ready / TX-arming / TX / fault
- Expose quality metrics over telemetry:
  - RSSI proxy, audio clipping counters, buffer underruns, link quality.

### Mobile App
- Single app with feature flags:
  - `APRS native mode` (current core)
  - `HF interface mode` (new)
- Plugin-style mode engines:
  - Native packet workflows first
  - Optional bridge integration for phone-side modem workflows
- Session recorder for troubleshooting and support.

### Protocol Strategy
- Keep current JSON control endpoints for fast iteration.
- Add binary-framed high-rate channel only if audio bridge proves necessary.
- Version protocol explicitly (`major.minor`) and support capability negotiation.

## Suggested Delivery Plan

### Phase 0 - Discovery Prototype (2-4 weeks)
- Build bench prototype with one HF radio profile.
- Validate CAT/PTT control and AF level calibration.
- Run BLE stress tests for control + packet paths.
- Run exploratory BLE audio streaming tests and capture latency/jitter/power metrics.

Exit criteria:
- Stable control session >30 minutes with no unsafe TX behavior.
- Repeatable RX/TX command reliability under realistic RF environment.

### Phase 1 - Unified App Beta Slice (4-8 weeks)
- Integrate HF interface mode into the existing app architecture.
- Ship profile wizard + diagnostics for a small supported matrix.
- Keep audio-bridge workflows behind beta/experimental flag.

Exit criteria:
- User can configure radio, run PTT test, and complete at least one end-to-end digital workflow without desktop tools.

### Phase 2 - Productization
- Expand profile catalog based on telemetry and support data.
- Decide go/no-go on production BLE audio bridge based on field metrics.
- Publish interoperability matrix and known-good settings.

## Go / No-Go Recommendation
- Go for unified architecture and HF-capable interface variant.
- Go with control/packet-first scope.
- Conditional go for BLE audio streaming only if Phase 0 metrics pass acceptance thresholds (latency stability, failure rate, battery impact, app background behavior).

## Research Sources
- Digirig-Mobile repository: https://github.com/softcomplex/Digirig-Mobile
- Digirig project site: https://digirig.net/
- APRS BLE KISS transport draft/spec context: https://github.com/hessu/aprs-specs/blob/master/BLE-KISS-Protocol.md
- Espressif BLE throughput FAQ (throughput reference point): https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/bt/ble.html
- FT8CN Android project (mobile digital mode ecosystem example): https://github.com/N0BOY/FT8CN
