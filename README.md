# DT24 and Ruuvi BLE-to-MQTT readers

Three C executables share configuration, logging and MQTT transport:

- `gattclient`: ATORCH DT24pw, connected GATT notifications.
- `ruuvi-mqtt`: RuuviTag/Pro RAWv2 advertising through BlueZ D-Bus.
- `gattclient-ruuvi`: DT24 GATT and Ruuvi raw-HCI advertising in one process.
  This is the receiver deployed as `ble-mqtt.service` on `zigbeegate`.

Start with [combined service operations](docs/combined-service.md). See
[receiver architecture](docs/combined.md) and [verified deployment status](docs/deployment-status.md)
for the distinction between unit tests, live observations and remaining limitations.

DT24 is based on https://github.com/syssi/esphome-atorch-dl24 and
https://github.com/gbrault/gattclient (vendored BlueZ ATT/GATT code).

## Build and tests

Build dependencies: C11 compiler, make, pkg-config, GLib/GIO development files,
libmosquitto development files. Debian package names: `build-essential`,
`pkg-config`, `libglib2.0-dev`, `libmosquitto-dev`. BlueZ is required at runtime
for Ruuvi; the adapter must already be powered on.

```sh
make -C src -j4
make -C src test
make -C src integration-test
make -C src combined-integration-test
```

Integration tests also need `mosquitto`, `mosquitto-clients`, `openssl`,
`dbus-run-session`, and system Python 3 with `python3-dbus` and `python3-gi`.
They use isolated loopback brokers and private D-Bus sessions, not your live
adapter or home MQTT broker. Temporary files use `TMPDIR`.

Individual targets: `make -C src gattclient`, `make -C src ruuvi-mqtt`,
and `make -C src gattclient-ruuvi`. The default `all` target builds the first two.
Existing `make install` installs only `gattclient`; do not run it casually on
the production DT24 host.

## Configuration and console run

```sh
install -m 600 config/ruuvi.ini.example config/ruuvi.local.ini
./src/ruuvi-mqtt -C config/ruuvi.local.ini --check-config
./src/ruuvi-mqtt -C config/ruuvi.local.ini -D --duration 60
```

Remove `--duration` for continuous foreground reception. Ctrl+C/SIGTERM stops
only this process's discovery session. JSON goes to stdout, diagnostics to
stderr; `--syslog` routes diagnostics to syslog with identity `ruuvi-mqtt`.
The example disables MQTT until explicitly configured/enabled.

Both binaries accept `-C FILE`, `-D`, `-i hciN`, `-d MAC`, `--check-config`.
CLI values override INI regardless of the position of `-C`. DT24 `-H NAME`
overrides prefix to `tele/NAME` and client ID to `gatt@NAME`.

**Migration:** credentials are no longer compiled into `gattclient`. New DT24
builds require `-C FILE` for MQTT operation. Prepare `config/dt24.ini.example`
with existing broker credentials before replacing a deployed binary. This
repository now also supports the deployed combined service. The original
DT24 binary is preserved for rollback; the old screen autostart was removed
by the owner. Do not re-enable it alongside the combined receiver.

```sh
./src/gattclient -C config/dt24.local.ini --check-config
# Run only on the intended DT24 host, after reviewing the config:
./src/gattclient -C config/dt24.local.ini -c
```

See [Ruuvi operation and verification](docs/ruuvi.md).

## systemd examples

`systemd/ruuvi-mqtt.service` and `systemd/gattclient-dt24.service` are optional
system-wide unit examples running as `palich`, not root. They are not installed
or started by the build. See [installation, logs and rollback](docs/systemd.md)
before using them, especially before migrating the existing DT24 reader.
