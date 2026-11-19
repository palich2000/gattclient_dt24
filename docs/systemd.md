# systemd service examples

These are OPTIONAL system-wide units, both running as `palich`, not root:

- `systemd/ruuvi-mqtt.service`
- `systemd/gattclient-dt24.service`

They are examples only. Adding them to the repository does not install, enable,
start, or replace any service. Adjust `User=` if the target account is different.
Do not install them under `~/.config/systemd/user`: they are system units with
system-level Bluetooth dependencies and, for DT24, an ambient capability.

## Paths and permissions

| Purpose | Ruuvi | DT24 |
| --- | --- | --- |
| Executable | /usr/local/libexec/ble-mqtt/ruuvi-mqtt | /usr/local/libexec/ble-mqtt/gattclient |
| Configuration | /etc/ble-mqtt/ruuvi.ini | /etc/ble-mqtt/dt24.ini |
| Unit | /etc/systemd/system/ruuvi-mqtt.service | /etc/systemd/system/gattclient-dt24.service |

The separate libexec directory deliberately leaves an existing
`/usr/local/bin/gattclient` untouched. Build on the target architecture. Configuration
files must be readable by the service user and mode 0600 when containing passwords.
Use absolute TLS certificate/key paths, with files readable by that user.

Ruuvi uses the system D-Bus and receives no Linux capabilities. Its user must be
permitted by the target's BlueZ/D-Bus policy to start discovery. DT24 uses direct
HCI/L2CAP sockets and the example grants only CAP_NET_RAW through systemd; do not
apply `setcap` to a shared binary or make the whole service root as a first fix.
The DT24 capability/sandbox combination still needs a controlled hardware test
on its intended host. Static unit validation does not establish runtime access.

The adapter must already be powered and unblocked. Wants/After=bluetooth.service
orders startup, but does not issue a power-on command. Both units can retry a
failed process after five seconds. The Ruuvi reader handles BlueZ and MQTT
reconnection internally. Invalid configuration must be corrected, not left in
an endless restart cycle; ExecStartPre validates it before every start.

## Install Ruuvi on the selected host, when explicitly ready

Run from the repository root on that host. First prepare `config/ruuvi.local.ini`
from the example: set broker credentials, enable MQTT if desired, and choose a
unique client ID/topic prefix. Do not use `config/probe.local.ini`, which points
to the temporary test SSH forward. Stop any foreground instance of this same
reader before starting the service.

```sh
make -C src ruuvi-mqtt
./src/ruuvi-mqtt -C config/ruuvi.local.ini --check-config
sudo install -d -m 755 /usr/local/libexec/ble-mqtt /etc/ble-mqtt

# These guards refuse to overwrite an existing installation.
test ! -e /usr/local/libexec/ble-mqtt/ruuvi-mqtt && \
  sudo install -m 755 src/ruuvi-mqtt /usr/local/libexec/ble-mqtt/ruuvi-mqtt
test ! -e /etc/ble-mqtt/ruuvi.ini && \
  sudo install -o palich -m 600 config/ruuvi.local.ini /etc/ble-mqtt/ruuvi.ini
test ! -e /etc/systemd/system/ruuvi-mqtt.service && \
  sudo install -m 644 systemd/ruuvi-mqtt.service /etc/systemd/system/ruuvi-mqtt.service
```

If a guard fails, stop and inspect/back up the existing installation before
continuing. Do not interpret a later successful command as success of all steps.
After checking that all three intended files were installed:

```sh
sudo -u palich /usr/local/libexec/ble-mqtt/ruuvi-mqtt -C /etc/ble-mqtt/ruuvi.ini --check-config
sudo systemd-analyze verify /etc/systemd/system/ruuvi-mqtt.service
sudo systemctl daemon-reload
sudo systemctl enable --now ruuvi-mqtt.service
systemctl show ruuvi-mqtt.service -p ActiveState -p SubState -p MainPID -p NRestarts -p UnitFileState
journalctl -u ruuvi-mqtt.service -n 30 --no-pager
```

Check advancing MeasurementSequence and fresh SENSOR payloads on the configured
broker, plus Online LWT/AVAILABILITY. `active` alone does not prove reception.
Boot behavior is only verified after an actual reboot, not by enablement.

## DT24 migration is a separate operation

Do not start a second DT24 reader alongside the existing screen/script on
zigbeegate. First prepare a private `config/dt24.local.ini`, preserve the original
binary/script/startup configuration, then arrange a controlled stop and migration.
Use the same guarded installation procedure with:

- `src/gattclient` to `/usr/local/libexec/ble-mqtt/gattclient`;
- `config/dt24.local.ini` to `/etc/ble-mqtt/dt24.ini`, owner palich, mode 0600;
- `systemd/gattclient-dt24.service` to `/etc/systemd/system/gattclient-dt24.service`.

Validate the installed config and unit before daemon-reload/start. The unit uses
`-c` (no interactive console) and does not fork or wrap the process in screen.
Verify fresh Voltage/Current/Power MQTT messages and logs before enabling it at
boot. The rebuilt DT24 executable and service were not tested against the live
DT24 during Ruuvi development.

## Control, logs and rollback

For Ruuvi (replace the unit name with `gattclient-dt24` for DT24):

```sh
sudo systemctl stop ruuvi-mqtt.service
sudo systemctl start ruuvi-mqtt.service
sudo systemctl restart ruuvi-mqtt.service
journalctl -u ruuvi-mqtt.service -f
sudo systemctl disable --now ruuvi-mqtt.service
```

The last command removes autostart and stops the service; files are preserved.
To fully uninstall a unit after stopping/disabling, remove only its exact unit
file and run daemon-reload. Keep private configuration and binary backups until
rollback has been verified. DT24 rollback means restoring its original startup
path only AFTER the new service has stopped, never running both concurrently.
Do not stop the shared Bluetooth service as part of stopping either reader.

Ruuvi JSON stdout and diagnostics are captured by journald. SyslogIdentifier
labels captured output; it does not override the application's own identifier
for messages sent directly to syslog. Use `journalctl -u` for service attribution.
Stop allows 15 seconds for cleanup, then systemd terminates remaining processes
in this service's control group; Ruuvi attempts StopDiscovery and Offline MQTT
publication before exiting.

## Validation boundary

Both examples passed `systemd-analyze verify` with systemd 245 using temporary
copies whose ExecStart/ExecStartPre executable paths alone were substituted with
`/usr/bin/true`, because the example installation paths were not installed.
Nothing was executed by this check. The unmodified files still need verification
after installing the real binaries on the target host. No service installation,
startup, capability sandbox, auto-restart or post-reboot test is claimed here.
