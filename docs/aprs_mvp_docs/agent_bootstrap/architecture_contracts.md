# Architecture Contracts (Implementation Constraints)

## A. Firmware layer contracts
- `IAudioIO`: deterministic sample IO; no dynamic allocation in hot path.
- `IRadioControl`: idempotent `set_freq`, explicit `ptt(on/off)` state.
- `IPacketLink`: AX.25 frames in/out; no BLE concerns in this layer.
- `IStorage`: versioned config, atomic writes.

## B. BLE contract
- Use current UUID map from `../docs/05_ble_gatt_spec.md`. Base UUID: `544E4332-8A48-4328-9844-3F5C00000000`.
- Writes for config/command/TX require encrypted+bonded link.
- Chunking required above `(mtu-3)` with `msg_id/chunk_idx/chunk_total`.
- Firmware must function at default MTU.
- Notify paths must be rate-limited and must not starve modem/audio tasks.

## C. Audio/clock contract (SGTL5000 baseline)
- SGTL5000 requires valid `SYS_MCLK`; firmware and board must agree on clock source and ratio.
- Sample rate configuration is invalid unless SGTL5000 clock-ratio checks pass.
- Audio/modem path has priority over BLE notify bursts.
- BLE notify must be rate-limited to protect demod stability.
- TX state machine must fail safe to `PTT=off`.
- Startup/shutdown sequencing must avoid pop/click events on AF path where practical.

## D. App contract
- Separate transport (BLE) from mode logic (APRS/HF profiles).
- UI never sends raw radio commands without validation.
- Show explicit state for pending/ack/timeout.
- Desktop test app must validate GATT behavior before phone UX features depend on it.

## E. Compatibility contract
- New protocol behavior must be versioned.
- Unknown fields must be ignored (forward compatibility).
- Breaking changes require capability negotiation.

## F. Power/telemetry contract
- Charger path baseline is MCP73831/2; gauge baseline is MAX17048.
- Battery telemetry must expose both voltage and percentage if available.
- Telemetry reads must degrade gracefully when gauge data is temporarily unavailable.

## G. Error and recovery contract
- Any unrecoverable TX/audio/radio error forces `PTT=off`.
- Reinit paths must be idempotent (safe to call more than once).
- Recovery logic must use bounded retries: maximum 3 attempts with at least 1 s between attempts; escalate to error state if all retries fail.
