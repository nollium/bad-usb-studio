# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bad USB Studio is a Flipper Zero app + companion PWA. The Flipper app creates, edits, saves, and executes DuckyScript Bad USB payloads on-device. The PWA connects to the Flipper over BLE serial to remotely type payloads, control a virtual keyboard/mouse, and manage saved scripts.

## Build & Deploy

Requires `ufbt` (micro Flipper Build Tool). Install: `pipx install ufbt && ufbt update`

```bash
# Build (from bad_usb_studio/ directory)
cd bad_usb_studio
ufbt

# Build and deploy to connected Flipper
ufbt launch

# Deploy will fail if Flipper is in Phone Input mode (USB becomes HID).
# User must exit the app on Flipper first.
```

The PWA is static files in `docs/` — deployed via GitHub Pages from the main branch. Changes to `docs/` go live ~30s after push.

## Architecture

### Flipper App (`bad_usb_studio/`)

C app targeting Flipper Zero OFW SDK (API 87.1, firmware 1.4.3). Uses the **Scene Manager + View Dispatcher** pattern.

**Scene flow** (see `scenes/scene_config.h` for the X-macro list):
- `MainMenu` → `PayloadList` → `PayloadActions` → `Execute` or `Editor`
- `MainMenu` → `Editor` → `EditorAddCmd` / `EditorEditLine` → `Save`
- `MainMenu` → `QuickType` → `QuickTypeResult`
- `MainMenu` → `PhoneInput` (BLE serial bridge)
- `MainMenu` → `Settings`

**Key abstractions:**
- `HidTransport` (`helpers/hid_transport.h`) — vtable for USB vs BLE HID. USB uses `furi_hal_hid_kb_*`, BLE uses `ble_profile_hid_kb_*`. Modifier keys must use `KEY_MOD_LEFT_*` bits (not `HID_KEYBOARD_L_*` keycodes).
- `DuckyState` / `ducky_execute_line()` (`helpers/ducky_script.c`) — DuckyScript parser. Called both for payload execution and live keyboard commands from the PWA.
- `KeyboardLayout` (`helpers/keyboard_layout.c`) — ASCII→HID keycode tables for US/FR/DE/ES. Uses SDK's `hid_asciimap[]` for US, custom tables for others.
- `payload_storage` (`helpers/payload_storage.c`) — file I/O to `/ext/apps_data/bad_usb_studio/*.txt`.

**Phone Input BLE setup** (`scenes/scene_phone_input.c`):
The BLE profile switch is fragile. The sequence that works: `bt_disconnect` → `bt_profile_start(HID)` → 1s delay → `bt_profile_start(serial)` → set callback → disable RPC. Must run in a worker thread. The Bt service re-sets the RPC callback on connection, so `phone_take_over_serial()` is called again in the `PhoneConnected` event handler.

**Phone Input protocol** (null-terminated strings over BLE serial):
- `PING` → `PONG`
- `LIST` → newline-separated filenames
- `GET:<filename>` → file contents
- `SAVE:<filename>:<content>` → `OK` or `ERR`
- `EXEC:<ducky lines>` → `RUNNING` then `DONE`
- `LAYOUT:<US|FR|DE|ES>` → `OK`
- `KBKEY:<ducky line>` → `OK` (single keystroke, auto-inits HID)
- `KBTYPE:<text>` → `OK` (types string, auto-inits HID)
- `MMOVE:<dx>,<dy>` / `MDOWN:<btn>` / `MUP:<btn>` / `MSCROLL:<delta>` (fire-and-forget, no response)

### PWA (`docs/`)

Single `index.html` with inline CSS/JS. Connects via Web Bluetooth to the Flipper's serial service (UUID `8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000`). Writes to characteristic `...62fe0000`, listens on `...63fe0000`.

Tabs: Editor, Saved, Quick Type, Keyboard (Gboard-style with shift/symbols/popups), Mouse (trackpad with acceleration/momentum/two-finger scroll).

Mouse uses `sendFast()` (fire-and-forget BLE write) instead of `sendCommand()` (waits for response) for low latency.

## SDK Pitfalls

- `strtok_r` and `strcat` are disabled in the Flipper API — use manual tokenization and `memcpy`.
- `view_dispatcher_enable_queue()` is deprecated — omit it.
- BLE HID profile symbols (`ble_profile_hid_*`) require `fap_libs=["ble_profile"]` in `application.fam`.
- `bt_profile_start()` with the same profile already active returns NULL and crashes. Always switch away first.
- `furi_hal_bt_change_app()` cannot be called while the Bt service owns the stack — causes `furi_check` crash.
- Modifier keys in DuckyScript combos must use `KEY_MOD_LEFT_CTRL` etc. (bits 8-15), not `HID_KEYBOARD_L_CTRL` (keycode 0xE0). The OS ignores modifiers in the HID key array.
- `-Werror` is on: fix all format-truncation warnings (use adequately sized buffers for `snprintf`).

## GitHub

- Repo: `git@github.com:nollium/bad-usb-studio.git`
- PWA URL: `https://nollium.github.io/bad-usb-studio/`
- GitHub Pages serves from `/docs` on main branch.
