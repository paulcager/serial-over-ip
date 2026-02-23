# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266-based bridge providing remote access to a Raspberry Pi over WiFi: serial console proxy (TCP port 8001), HTTP-triggered shutdown/reset via GPIO pins. Built on Mongoose OS.

## Build & Deploy

```bash
# Build firmware (outputs build/fw.zip)
mos build

# Sign firmware for OTA updates
mos create-fw-bundle -i build/fw.zip -o build/fw-signed.zip --sign-key=k0.pem

# Deploy OTA to device
curl -v -F file=@build/fw-signed.zip http://<esp-ip>/update
```

Platform target is ESP8266 (ESP-12F). Requires `mos` CLI tool (Mongoose OS).

## Architecture

**`src/main.c`** — Single-file application with these components:

- **UART dispatcher**: Reads serial data from the Pi and broadcasts to all connected TCP clients
- **TCP server** (port 8001): Bidirectional proxy between TCP clients and Pi's UART
- **HTTP endpoints**: `/reset`, `/shutdown`, `/wake-up` (toggle GPIO pins), `/reset-esp`
- **WiFi watchdog**: Auto-restarts ESP if WiFi lost for >30 seconds

Data flow: `Pi serial ↔ ESP UART ↔ TCP socket ↔ remote client`

**`mos.yml`** — Build config and runtime settings schema. Configurable pins and UART selection:
- `serial.uart` (default 0), `serial.shutdown.pin` (default 4), `serial.reset.pin` (default 5)

**`serial-over-ip.ino`** — Legacy Arduino version, kept for reference. The active codebase uses Mongoose OS.

## Hardware Wiring

**Pi to ESP8266:**
- Pi Tx (GPIO 14) → ESP Rx
- Pi Rx (GPIO 15) → ESP Tx
- Pi Run Pin → ESP GPIO 5 (reset pin)
- Pi GPIO 3 → ESP GPIO 4 (shutdown pin)
- Common ground

ESP8266 is 3.3V — compatible with Pi's 3.3V GPIO directly.

**Pi configuration** (`/boot/config.txt` or `/boot/firmware/config.txt` on Bookworm):
```
enable_uart=1
dtoverlay=gpio-shutdown
```

**Pi serial console** (`/boot/cmdline.txt`):
```
console=serial0,115200 console=tty1 ...
```

## Connecting to the Serial Console

The ESP exposes the Pi's serial port on TCP port 8001. Several connection methods:

**`socat` in raw mode** — simplest option, CTRL-] to disconnect:
```bash
socat TCP:esp-ip:8001 STDIO,raw,echo=0,escape=0x1d
```

**`socat` PTY + `screen`** — recommended, documented in README:
```bash
socat pty,raw,echo=0,link=/tmp/esp-serial TCP:esp-ip:8001 &
screen /tmp/esp-serial 115200
```

**`nc`** — works but CTRL-C kills the connection and terminal control chars don't pass through.

**`screen //telnet`** — `screen //telnet esp-ip 8001 115200` — works but less reliable than the socat PTY approach.

## Secrets (not in git)

- `secrets.h` — WiFi credentials (`MY_SSID`, `MY_SSID_PSK`)
- `k0.pem` — ECDSA private key for firmware signing

## ADS-B Integration

Used to manage Raspberry Pis running ADS-B receivers (dump1090) in the attic.

- **Pi hostname**: `pi-zero-flights-N`
- **ESP8266 hostname**: `pi-zero-flights-N-monitor`

See main project suite: [1090/CLAUDE.md](../1090/CLAUDE.md)
