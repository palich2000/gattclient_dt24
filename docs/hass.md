# Native build on hass

Workspace: `/home/palich/hermes/ruuvi-hass` on hass. The target userland is
32-bit ARM (`armhf`); do not copy the arm64 executable from rasp4-test.

## Build without upgrading runtime libraries

The existing GLib runtime is 2.58.3. Only development headers are missing.
Download and unpack (do not install) the matching release-series development
package inside the workspace:

```sh
mkdir -p build-deps
cd build-deps
apt-get download libglib2.0-dev=2.58.3-2+deb10u6
printf '%s  %s\n' \
  c364f77043c0f829ab077e2ff2dd94a0f73477e8632ea3596078dffb075e60a8 \
  libglib2.0-dev_2.58.3-2+deb10u6_armhf.deb | sha256sum -c -
dpkg-deb -x libglib2.0-dev_2.58.3-2+deb10u6_armhf.deb glib-headers
cd ..
sh scripts/build-hass.sh
```

This uses the native compiler and installed runtime sonames, not the unpacked
static libraries. No sudo, package installation, runtime upgrade or reboot is
required. The script assumes the existing libmosquitto development files.

## Bluetooth access and compatibility

The palich user must be in the bluetooth group. If a new SSH session still gets
D-Bus AccessDenied after group membership changes, the administrator can run
`sudo systemctl reload dbus.service` to reload policy and cached group data.
Do not restart the system bus. Check `bluetoothctl show` from the ordinary user.

BlueZ 5.50 on this host reports only UUIDs, RSSI, Pathloss, Transport and
DuplicateData via Adapter1.GetDiscoveryFilters. The receiver requests only
Transport=le and DuplicateData=true. Discoverable and Pattern are not sent;
exact adapter/device MAC filtering remains in the receiver's event handlers.
The private-bus regression fixture rejects unsupported keys.

## Bounded reception check

```sh
./src/ruuvi-mqtt -C config/ruuvi.ini.example -D --duration 65
```

Verification on 2026-11-12:
- Native build and decoder/config/state/CLI tests passed.
- The original filter set failed on the actual host; the compatibility fix
  passed the private-bus fixture and recovery/MAC-filtering tests locally.
- After rebuilding on hass, discovery started without filter errors.
- Neither the 65-second, subsequent 45-second, nor later 300-second run
  received the target DE:4D:F8:3F:DA:91. During the 45-second run, D-Bus signals
  showed three other devices,
  but no target-device event. Reception at this placement remains unverified.
- Console and radio captures are in the remote workspace as
  console-hass-fixed.*, console-hass-radio.* and scan-signals.log.
- MQTT was disabled. No permanent service was installed or started.

Do not deploy unattended until fresh target measurements have been observed.
Systemd examples and installation/rollback commands are in systemd.md;
examples are not evidence of an installed service. The existing DT24 installation
is unchanged.
