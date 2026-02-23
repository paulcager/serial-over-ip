# serial-over-ip
ESP8266 code to implement serial console, shutdown and emergency reset for Pi.

## Introduction

I have a Raspberry Pi running [dump1090](https://github.com/adsbxchange/dump1090-mutability) in my attic. Rebooting it involves
ladders, dust, fibreglass rash and, occasionally, wasps. This project uses an [ESP-12F](https://en.wikipedia.org/wiki/ESP8266)
to provide a remote serial console, together with a way to shutdown or force-reset the Pi.

## RPI Configuration

* Enable shutdown by [driving GPIO pin 3 low](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README).
* Enable the serial console.
* Turn off the unused HDMI port (optional).

##### `/boot/config.txt`

```
gpu_mem=16
enable_uart=1
disable_splash=1
dtoverlay=gpio-shutdown
```

##### `/boot/cmdline.txt`

```
console=serial0,115200 console=tty1 root=....
```

##### `/etc/rc.local`

```
echo cpu >/sys/class/leds/led0/trigger
tvservice -o
```

## Interconnections

* Pi's `Tx` to ESP's `Rx`.
* Pi's `Rx` to ESP's `Tx`.
* Pi's  [run pin](https://forums.raspberrypi.com/viewtopic.php?t=218600) to an ESP pin (GPIO 4 or 5 recommended).
* Pi's  GPIO 3 to an ESP pin.

## Usage

To **connect to the Pi's serial port** (assuming the ESP has obtained IP address `192.168.0.28`),
use `socat` to create a virtual serial port and `screen` to connect to it:

```
$ socat pty,raw,echo=0,link=/tmp/esp-serial TCP:192.168.0.28:8001 &
$ screen /tmp/esp-serial 115200
```

This gives you a proper terminal with working control characters, scrollback, and
the ability to detach (`CTRL-A d`) and reattach (`screen -r`) without dropping the
connection.

To **initiate a shutdown** of the Pi:

```
$ curl http://192.168.0.28/shutdown
```

To **force a hard-reset** of the Pi:

```
$ curl http://192.168.0.28/reset
```

To **wake up** the Pi:

```
$ curl http://192.168.0.28/wake-up
```

## Security

The interface to the ESP **is not secured**: anyone on your network can use it to connect or restart the Pi. Security may be implemented by
locking the ESP into a secure VLAN; by using a reverse proxy in front of the ESP; or by building using `CREATE_AP` mode.

## secrets.h

This header file (which is not checked into Git) contains secrets, such as the credentials to use to connect to your WiFi:

```
#define MY_SSID my-wifi
#define MY_SSID_PSK very-secret
```

You can also `#define CREATE_AP`, in which case the ESP will _create_ an access point rather than join one.

### Build and Deploy

```
mos build
```

Sign the firmware to prevent unauthorised updates (see https://github.com/mongoose-os-libs/ota-common)

```
mos create-fw-bundle -i build/fw.zip -o build/fw-signed.zip --sign-key=k0.pem
curl -v -F file=@build/fw-signed.zip http://<<ip>>/update
```
