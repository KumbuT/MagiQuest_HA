# MagiQuest_HA

Turn a physical MagiQuest wand into a Home Assistant light controller using a Seeed XIAO ESP32-C3.

This firmware:
- decodes MagiQuest IR swings (wand ID + magnitude),
- lets you map wand IDs to Home Assistant light actions,
- hosts a captive setup portal for first boot,
- and serves a configuration UI for ongoing management.

## Features

- MagiQuest protocol decoding via `IRremote` (full wand ID + swing magnitude)
- Up to 10 configurable wand mappings
- Home Assistant action support: `toggle`, `turn_on`, `turn_off`
- Captive portal for first-time Wi-Fi provisioning
- Persisted settings using ESP32 `Preferences`
- LittleFS-hosted web UI
- Last-seen wand capture endpoint (`/wand/last`) to speed setup
- Home Assistant validation flag workflow:
  - validate once,
  - submit remains enabled,
  - validation resets only when HA URL/token changes
- Auto-load HA light entities when credentials are already validated

---

## Hardware Requirements

- Seeed XIAO ESP32-C3
- IR receiver module (configured on `D2`)
- LED (configured on `D6`, optional but recommended for feedback)

Pin definitions are in `include/Globals.h`:
- `IR_RECV_PIN D2`
- `LED_FEEDBACK_PIN D6`

---

## Software Requirements

- [VS Code](https://code.visualstudio.com/) + PlatformIO extension **or**
- PlatformIO Core CLI
- Git

Project framework/dependencies are defined in `platformio.ini`:
- `framework = arduino`
- `board = seeed_xiao_esp32c3`
- `board_build.filesystem = littlefs`
- `AsyncTCP`
- `ESPAsyncWebServer`
- `IRremote`
- `Arduino_JSON`

---

## Installation

### 1) Clone the repository

```bash
git clone https://github.com/KumbuT/MagiQuest_HA.git
cd MagiQuest_HA
```

### 2) Build firmware

Using PlatformIO CLI:

```bash
pio run
```

### 3) Upload firmware

```bash
pio run --target upload
```

### 4) Upload web assets (LittleFS)

This uploads files from the `Data/` folder (`index.html`, `sta-index.html`, jQuery):

```bash
pio run --target uploadfs
```

### 5) Open serial monitor (optional, recommended)

```bash
pio device monitor --baud 115200
```

---

## Initial Setup and Usage

### 1) First boot (no saved Wi-Fi)

If `is_setup_done` is false, the device starts in AP/captive mode:
- Connect to the device AP
- Open any webpage (captive redirect) or browse to the AP IP
- Use the Wi-Fi setup page (`sta-index.html`) to submit SSID/password

After successful join:
- credentials are saved to `Preferences`
- mDNS starts
- device can transition to station-only mode

### 2) Open configuration UI

In station mode:
- Browse to device IP or mDNS hostname
- Authenticate for config routes (default credentials in code):
  - username: `admin`
  - password: `admin123`

The main page (`index.html`) lets you:
- set device hostname
- enter Home Assistant URL + token
- test HA connectivity
- load/select light entities
- map wand IDs to actions

### 3) Configure Home Assistant credentials

1. Enter HA URL and long-lived access token
2. Click **Test Token and URL**
3. On success:
   - HA credentials are considered validated
   - light entities are fetched/populated
   - submit/save is enabled

Validation behavior:
- Save stays enabled after a successful test
- If HA URL or token changes, validation is invalidated and test is required again
- If saved credentials were validated previously, lights auto-load on page load

### 4) Configure wand mappings

For each mapping row (up to 10):
- set `wand_id`
- choose `action` (`toggle`, `turn_on`, `turn_off`)
- choose Home Assistant light entity

Use **Use last seen** to copy the most recently detected wand ID from `/wand/last`.

### 5) Trigger actions with wand swings

At runtime:
- firmware decodes IR packets
- only `MAGIQUEST` protocol is processed
- last seen wand data is refreshed
- if magnitude threshold is met (`> 10`), configured HA action is executed

---

## Configuration Persistence

Settings are stored in `Preferences`, including:
- Wi-Fi credentials and setup state
- hostname
- Home Assistant URL/token
- `ha_validated` flag
- wand mapping slots (`wand_id_N`, `wand_action_N`, `wand_entity_N`)

---

## Event Flow

### A) Boot and Network Flow

1. `setup()` initializes serial, storage, LittleFS, and preferences.
2. If not configured, `StartCaptivePortal()` starts AP + DNS captive behavior.
3. User submits Wi-Fi credentials (`/wifi/connect`).
4. `NetworkTick()` drives async Wi-Fi join state machine.
5. On success, credentials and setup flag are persisted; server remains available.
6. In configured mode, device serves the main config UI and APIs.

### B) Home Assistant Validation + Save Flow

1. UI loads `/getconfigs`.
2. If `ha_validated` true and credentials unchanged, UI auto-loads lights via `/ha/lights`.
3. User can run `/ha/test` to validate credentials.
4. On `/savesettings`:
   - backend compares submitted vs saved HA credentials (normalized),
   - `ha_validated` is kept only when appropriate,
   - mapping slots are rewritten,
   - optional hostname update + mDNS restart occurs.

### C) Wand Runtime Action Flow

1. `loop()` calls `IrDecodeAvailable()`.
2. Non-MagiQuest signals are ignored.
3. MagiQuest signal:
   - wand ID and magnitude extracted,
   - last-seen wand snapshot updated,
   - if magnitude > 10, `callHA()` runs.
4. `callHA()`:
   - finds matching wand mapping,
   - validates action,
   - fetches current entity state for toggle logic,
   - posts to HA service endpoint (`/api/services/light/...`).

---

## HTTP Endpoints (high level)

- `GET /` - serves captive setup page or config UI based on setup state
- `GET /getconfigs` - returns current config + mappings + validation flag
- `POST /savesettings` - persists hostname, HA config, mappings
- `POST /ha/test` - tests HA URL/token
- `POST /ha/lights` - fetches light entities from HA
- `GET /wand/last` - last detected wand ID, magnitude, age
- `POST /wifi/connect` - start Wi-Fi join flow
- `GET /wifi/status` - current Wi-Fi join progress
- `POST /wifi/finalize` - disables AP when station mode is stable

---

## Troubleshooting

- **No lights loaded**
  - verify HA URL/token via **Test Token and URL**
  - ensure HA host is reachable from the ESP32 network
- **Save button disabled**
  - HA credentials likely changed; re-run HA test
- **Wand not detected**
  - verify IR receiver wiring on `D2`
  - confirm wand is MagiQuest protocol
  - check `/wand/last` and serial logs
- **Captive portal not appearing**
  - connect to AP manually and browse to AP gateway IP

---

## License

This project is licensed under GNU GPL v3.0. See `LICENSE`.
