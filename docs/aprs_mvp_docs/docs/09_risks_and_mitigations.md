# Risks and mitigations

## Audio level/deviation calibration
**Risk:** Packets not decodable due to wrong deviation/clipping.
**Mitigation:** I2S codec, calibration modes, configurable gain, simple limiter.

## RF desense from digital noise
**Risk:** ESP32/codec noise reduces RX performance.
**Mitigation:** RF shielding, layout separation, filtering, good grounding, LC filters.

## iOS background BLE limitations
**Risk:** iOS may throttle background streams.
**Mitigation:** Keep BLE payload small, session-based operation, app design that expects foreground use for continuous RX.

## Support burden (antennas, local APRS infrastructure)
**Risk:** Users blame device for coverage/path issues.
**Mitigation:** In-app diagnostics (last heard digi, path presets), clear docs, “RF basics” troubleshooting guide.

## Regulatory / regional frequency differences
**Risk:** Wrong default frequency for user region.
**Mitigation:** Region presets during setup; store user selection.

