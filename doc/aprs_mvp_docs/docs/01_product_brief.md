# Product brief

## One-sentence
A pocket APRS tracker + message TNC for 2m with BLE clients and MVP KISS TNC interoperability: a Windows desktop test app first, then phone app UX, eliminating audio cables and manual level tuning.

## Target user
- Hams who want APRS position + basic messaging with a modern phone UI.
- Hams who want a reliable "packet modem" they can use with a phone at events, hikes, or in vehicles.

## Core value proposition
- Phone provides map + UX.
- Windows desktop app provides fast bench validation and troubleshooting during development.
- Device provides RF, GPS, and reliable real-time packet processing.
- Existing APRS software can integrate through KISS-over-BLE without custom RF/audio wiring.
- No audio cables, no VOX hacks, no "which sound device?" troubleshooting.

## Constraints
- 2m APRS 1200 AFSK, AX.25 UI frames
- Small, battery powered
- BLE-first connectivity (desktop test app first in development, phone app for user UX)
- Support both native PAKT BLE control and MVP KISS TNC interoperability over BLE
- Configurable for regional APRS frequency plans

## Non-goals (MVP)
- Digipeater / iGate (optional later)
- High-power RF stages (keep within SA818 characteristics in MVP)

> ### Feedback
>
> This is a strong, focused product brief that clearly defines the project's vision. The value proposition is compelling and addresses common pain points for APRS users.
>
> A few thoughts:
>
> *   **Target User:** Consider explicitly adding **SOTA/POTA/hiker hams** to your target user list. The small, battery-powered, cable-free nature of this device is a perfect fit for portable operators who value lightweight and simple setups.
> *   **Core Value:** The "no audio cables, no VOX hacks" is a significant selling point. It's the primary differentiator from many existing solutions and is worth highlighting in any user-facing material.
> *   **Configuration Constraint:** The requirement for configurable regional frequencies is important. It would be beneficial to start thinking about *how* this configuration will be managed (e.g., via the BLE app, a physical switch, a config file on the device). This will have implications for both the firmware and mobile app architecture.
> *   **KISS TNC:** This is now in MVP because third-party APRS software compatibility is a project goal. Keep the MVP implementation narrow: data frames on port 0, raw frame transport over BLE, and compatibility validation with at least one existing KISS client.
