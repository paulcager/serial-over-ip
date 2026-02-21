# serial-over-ip

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

ESP8266-based remote serial console and power management for Raspberry Pi. Provides remote access to a Raspberry Pi's serial console and enables remote shutdown, hard reset, and wake-up capabilities over the network.

## Purpose

Enables remote management of Raspberry Pi devices in physically inaccessible locations (e.g., attic installations):
- Remote serial console access via TCP/IP
- Safe shutdown via GPIO trigger
- Hard reset via run pin toggle
- Wake-up from powered-off state
- Eliminates need for physical access (ladders, dust, etc.)

## Architecture

### Hardware Platform
- **MCU**: ESP8266 (ESP-12F module)
- **Firmware**: Arduino-based (built with Mongoose OS)
- **Connectivity**: WiFi (station mode or AP mode)

### Main Components

#### serial-over-ip.ino
Core Arduino sketch implementing three main functions:

1. **Serial Console Proxy** (TCP port 8001)
   - Bidirectional bridge between ESP UART and TCP socket
   - Supports up to 2 concurrent clients (MAX_CLIENTS)
   - 115200 baud serial connection to Pi
   - Oldest client disconnected when limit exceeded

2. **HTTP Control Interface** (TCP port 80)
   - `/shutdown`: Triggers GPIO 3 on Pi for safe shutdown
   - `/reset`: Toggles run pin for hard reset
   - `/wake-up`: Toggles GPIO 3 to wake from shutdown
   - `/reset-esp32`: Reboots the ESP8266 itself

3. **GPIO Control**
   - **RESET_PIN (GPIO 4)**: Connected to Pi's run pin
   - **SHUTDOWN_PIN (GPIO 5)**: Connected to Pi's GPIO 3
   - Pin toggle: 250ms low pulse

### Raspberry Pi Configuration

The Pi must be configured to respond to GPIO signals:

#### `/boot/config.txt` (or `/boot/firmware/config.txt` on Bookworm)
```
gpu_mem=16                    # Minimal GPU memory
enable_uart=1                 # Enable serial console
disable_splash=1              # Skip boot splash
dtoverlay=gpio-shutdown       # Enable GPIO 3 shutdown/wake
```

#### `/boot/cmdline.txt` (or `/boot/firmware/cmdline.txt`)
```
console=serial0,115200 console=tty1 root=...
```

#### `/etc/rc.local`
```bash
echo cpu >/sys/class/leds/led0/trigger  # Activity LED shows CPU usage
tvservice -o                             # Disable HDMI to save power
```

### Interconnections

**Pi to ESP8266 Wiring:**
- Pi Tx (GPIO 14) → ESP Rx
- Pi Rx (GPIO 15) → ESP Tx
- Pi Run Pin → ESP GPIO 4 (RESET_PIN)
- Pi GPIO 3 → ESP GPIO 5 (SHUTDOWN_PIN)
- Common ground between Pi and ESP

**Note:** ESP8266 is 3.3V - ensure voltage level compatibility with Pi's 3.3V GPIO.

## Usage

### Accessing Serial Console

Using netcat:
```bash
nc -v 192.168.0.X 8001
```

Using screen (better terminal emulation):
```bash
screen //telnet 192.168.0.X 8001 115200
```

### Power Management

**Safe Shutdown:**
```bash
curl http://192.168.0.X/shutdown
# Pi will cleanly shut down via GPIO 3 trigger
```

**Hard Reset:**
```bash
curl http://192.168.0.X/reset
# Forces immediate reboot via run pin toggle
```

**Wake Up:**
```bash
curl http://192.168.0.X/wake-up
# Wakes Pi from shutdown state
```

**Restart ESP8266:**
```bash
curl http://192.168.0.X/reset-esp32
# Reboots the ESP8266 module itself
```

## Configuration

### secrets.h

Not checked into Git - contains WiFi credentials:

```c
#define MY_SSID your-wifi-ssid
#define MY_SSID_PSK your-wifi-password
```

**Optional:**
```c
#define CREATE_AP  // Makes ESP create its own access point instead of joining one
```

### Pin Configuration

Modify in `serial-over-ip.ino` if needed:
```c
#define RESET_PIN 4       // ESP GPIO connected to Pi run pin
#define SHUTDOWN_PIN 5    // ESP GPIO connected to Pi GPIO 3
#define MAX_CLIENTS 2     // Maximum concurrent telnet connections
```

## Build and Deployment

### Building Firmware

Uses Mongoose OS build system:

```bash
mos build
```

Creates `build/fw.zip`

### Signing Firmware

Prevents unauthorized updates (recommended for security):

```bash
# Generate signing key (first time only)
openssl genrsa -out k0.pem 2048

# Sign firmware
mos create-fw-bundle -i build/fw.zip -o build/fw-signed.zip --sign-key=k0.pem
```

### Deploying Firmware

**Over-the-air (OTA) update:**
```bash
curl -v -F file=@build/fw-signed.zip http://192.168.0.X/update
```

**Initial flash via USB:**
```bash
mos flash
```

## Security Considerations

### Important Security Notes

⚠️ **The ESP8266 interface is NOT secured**:
- No authentication on HTTP endpoints
- No encryption on telnet console
- Anyone on the network can shutdown/reset the Pi
- Serial console provides root access if auto-login enabled

### Security Mitigation Options

1. **VLAN Isolation**: Place ESP on isolated management VLAN
2. **Reverse Proxy**: Use nginx/Apache with authentication in front of ESP
3. **AP Mode**: Use `CREATE_AP` to create isolated access point
4. **Firewall Rules**: Restrict access to ESP's IP to specific management hosts
5. **VPN**: Access ESP only through VPN connection

**Recommended for production:** Combine VLAN isolation with firewall rules.

## Integration with ADS-B Project

This project is used to remotely manage Raspberry Pi devices running ADS-B receivers (dump1090) in the attic:

### Typical Deployment
```
┌─────────────────────────────────────┐
│  Attic (Physically Inaccessible)    │
│                                     │
│  ┌──────────────┐  ┌──────────────┐│
│  │ Raspberry Pi │──│  ESP8266     ││
│  │ (dump1090)   │  │  (serial-    ││
│  │              │  │   over-ip)   ││
│  └──────────────┘  └───────┬──────┘│
│                            │       │
└────────────────────────────┼───────┘
                             │ WiFi
                             ▼
                    ┌─────────────────┐
                    │  Home Network   │
                    │  Management PC  │
                    └─────────────────┘
```

### Naming Convention
- **Pi hostname**: `pi-zero-flights-N` (where N = 1, 2, 3, 4, etc.)
- **ESP8266 hostname**: `pi-zero-flights-N-monitor`

### Related Projects
- **dump1090_exporter**: Metrics from ADS-B receiver
- **dump1090-proxy**: Aggregates multiple receivers
- **Feeder containers**: Upload to FlightRadar24, FlightAware, RadarBox

See main project suite: [1090/CLAUDE.md](../1090/CLAUDE.md)

## Troubleshooting

### ESP8266 Not Connecting to WiFi
- Check `secrets.h` credentials
- Verify 2.4GHz WiFi (ESP8266 doesn't support 5GHz)
- Check serial output at 115200 baud for error messages
- Try `/reset-esp32` endpoint if already connected

### Cannot Connect to Serial Console
- Verify Pi's UART is enabled (`enable_uart=1` in config.txt)
- Check wiring: Pi Tx → ESP Rx, Pi Rx → ESP Tx
- Ensure common ground between Pi and ESP
- Test with `nc -v <ip> 8001`

### Shutdown/Reset Not Working
- Verify `dtoverlay=gpio-shutdown` in Pi's config.txt
- Check GPIO wiring (run pin and GPIO 3)
- Test with curl commands
- Verify ESP GPIO pins are correctly defined

### Serial Console Shows Garbage
- Baud rate mismatch: Ensure both Pi and ESP use 115200
- Check `/boot/cmdline.txt` has `console=serial0,115200`
- Verify clean ground connection

## Development Notes

### Code Structure
- Single-file Arduino sketch (simple and maintainable)
- Non-blocking loop design
- Client management with fixed array
- Minimal dependencies (ESP8266WiFi, ESP8266WebServer)

### Memory Considerations
- ESP8266 has limited RAM (~80KB usable)
- MAX_CLIENTS limited to 2 to prevent memory exhaustion
- WiFiClient objects dynamically allocated/deallocated

### Pin Selection
- GPIO 4 and 5 recommended (avoid boot-sensitive pins)
- Avoid GPIO 0, 2, 15 (affect boot mode)
- GPIO 1 and 3 are default UART (used for serial)

### Future Enhancements
- Authentication on HTTP endpoints
- TLS/SSL support for serial console
- MQTT integration for monitoring
- Multi-Pi support from single ESP
- Web dashboard for status monitoring

## References

- **ESP8266 Arduino Core**: https://github.com/esp8266/Arduino
- **Mongoose OS**: https://mongoose-os.com/
- **Pi GPIO Shutdown**: https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README
- **Pi Run Pin**: https://forums.raspberrypi.com/viewtopic.php?t=218600

## License

MIT License - See LICENSE file for details
