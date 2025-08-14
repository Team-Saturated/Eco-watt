# EcoWatt Device – Milestone 1 (Arduino‑compatible C++)

This repo contains a minimal scaffold that models:
- periodic polling of an *Inverter SIM* (simulated here)
- buffering samples in RAM
- a periodic upload window (every 15 seconds, simulated)

It is structured for easy porting to ESP8266/NodeMCU (Arduino core).

## How to use (Arduino IDE / PlatformIO)
- **Board**: NodeMCU 1.0 (ESP‑12E Module) or any ESP8266 board
- **Framework**: Arduino
- Open `src/EcoWatt_M1.ino`
- Flash. Open Serial Monitor at 115200 baud.
- You should see samples polled every 2s and an upload printout every 15s.

## Petri Net
See `PetriNet.png` or `PetriNet.pdf` in the root. It maps directly to code:
- Places: *Idle*, *ReadyToPoll*, *Buffering*, *UploadWindow*, *Ack*
- Transitions: `t_poll`, `t_buf`, `t_tick15`, `t_send`, `t_ack`

## File structure
- `src/Acquisition.*` – fake inverter reads (voltage/current) and domain `Sample`
- `src/Buffer.*` – simple ring buffer
- `src/Uploader.*` – simulated uploader (prints JSON-ish data)
- `src/Config.h` – tunables (poll/upload intervals, buffer size)
- `src/EcoWatt_M1.ino` – glue logic (scheduler) using `millis()`

## Notes
- No networking/security/compression yet — this is *Milestone 1* only.
- Replace `Acquisition::acquire()` with the real Inverter SIM calls later.
- Upload interval is **15 seconds** for demo (project requires 15 minutes, but allows simulating 15s).
