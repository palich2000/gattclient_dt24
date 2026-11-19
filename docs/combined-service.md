# Combined BLE MQTT service on zigbeegate

## Files and identity

- Service: `ble-mqtt.service`, a system service running as `palich`, not root.
- Binary: `/usr/local/libexec/ble-mqtt/gattclient-ruuvi`, root-owned 0755.
- Configuration: `/etc/ble-mqtt/combined.ini`, owned by palich, mode 0600.
- Unit: `/etc/systemd/system/ble-mqtt.service`, root-owned 0644.
- Staging: `/home/palich/hermes/ble-mqtt-deploy`, mode 0700.
- Journal identifier: `ble-mqtt`. Logs stream to stderr/journald, no screen,
  daemon fork, log file or password in arguments.

The application uses CAP_NET_RAW supplied by systemd, with no file
capabilities and no full-root runtime. ProtectSystem=strict, ProtectHome=true
and NoNewPrivileges=true apply. This is a system unit, so no user linger or
interactive login is required. Static unit validation is not proof of a
successful boot or runtime capability handling.

## Configuration and MQTT

Use `config/combined.ini.example`. Production credentials are migrated into
the staged private INI without printing them. Never commit that INI.
Historical Git revisions may still contain the previous hardcoded password;
removing it from current source does not rotate it or erase Git history.
Broker host/port/TLS are explicit. Existing port 8883 is used without TLS,
matching the old client; changing that requires broker-side verification.

| Device | Telemetry | Broker connection availability |
| --- | --- | --- |
| DT24 | `tele/hass-batt/STATE` | `tele/hass-batt/LWT` |
| Ruuvi | `tele/ruuvi1/SENSOR` | `tele/ruuvi1/LWT` |

Ruuvi also publishes retained `tele/ruuvi1/AVAILABILITY`, Online/Offline
according to sample freshness. Require BOTH LWT and AVAILABILITY to be
Online; an abrupt process death only updates the broker LWT. Telemetry is
non-retained. Existing DT24 JSON fields are preserved. Two MQTT clients
(one process) have distinct IDs and wills, using shared credentials.

`log_level=6` is informational. `console_json=false` suppresses raw JSON in
journal; it does not control text diagnostics. Debug level 7 additionally
logs individual DT24 readings and MQTT payloads. At level 6, changing current
does not produce a reading line for every frame. Startup,
connection errors, Ruuvi freshness transitions, periodic counters and
shutdown remain visible. Every 60 seconds, two health lines show interval
DT24 frame counts, DT24 data age, interval Ruuvi reports/new measurements,
Ruuvi measurement age/RSSI/freshness and both MQTT connection states.
An age of -1 means no sample has been received. MQTT connected describes
the connection, not an end-to-end delivery acknowledgement; use the MQTT
observer for delivery verification. Stale data produces warning-level health
lines. SIGINT/SIGTERM are blocked before worker-thread
creation and delivered through the existing signalfd event loop for cleanup.

## Install and operate

The user removed the old screen launch from `/etc/rc.local`. The installer
does not modify rc.local, old binaries, Bluetooth services or other readers.
It refuses existing destination files and running readers; it checks staged
hashes, config permissions and unit syntax before writing. Installation
failure stops/disables the new unit and removes only files it created.

Privileged commands require an administrator-prepared release in
`/root/ble-mqtt-release` (root-owned, mode 0700). Make fresh root-owned
copies, not links, of the reviewed scripts and artifacts. For initial install
include the unit, private INI (0600) and manifest. Verify **the copies**
against hashes obtained independently from the approved build and config,
not just a manifest supplied by the writable staging directory. Never run a
Python script from user staging with sudo. Before activation, the executable
validates its configuration as the service user.

```sh
python3 /home/palich/hermes/ble-mqtt-deploy/install-combined.py --check
sudo /usr/bin/python3 -I /root/ble-mqtt-release/install-combined.py
journalctl -u ble-mqtt.service -f
sudo systemctl restart ble-mqtt.service
sudo systemctl stop ble-mqtt.service
```

The installer enables startup at boot. `Restart=on-failure` waits 10 seconds
between attempts; there is no hidden shell reconnect loop or USB reset.
The startup check rejects active BlueZ discovery. An unrelated raw scanner
cannot always be detected; do not run multiple readers on this adapter.

After install, verify fresh DT24 STATE and advancing Ruuvi sequences via
MQTT, not only `active`. Check journal and kernel log for HCI/USB failures.
Test stop/start and verify both LWTs Offline/Online. Post-reboot behavior
and sustained stability require separate live observation.

## Updating an existing installation

Do not rerun `install-combined.py` over an existing installation. It intentionally
refuses existing destinations. From the repository root on the desktop, `make install-check` verifies the prepared ARM release on zigbeegate without restarting anything. `make install` installs that pinned release, prompts for remote sudo, and restarts ble-mqtt.service with rollback on startup failure. These targets do not rebuild the working tree. `make -C src install` still targets the legacy executable.

The repository contains `scripts/update-quiet.py`, the pinned one-time updater
for the minute-health ARM build to the quiet-INFO ARM build. It is not a generic
build/deploy command: it requires the exact old/new hashes documented in
[deployment-status.md](deployment-status.md). Stage it beside the verified
`gattclient-ruuvi-quiet` artifact in the root-owned release directory described above, then run
on zigbeegate:

```sh
sudo /usr/bin/python3 -I /root/ble-mqtt-release/update-quiet.py
sudo journalctl -u ble-mqtt.service -n 30 -f
```

It preserves `/etc/ble-mqtt/combined.ini`, backs up the old binary as
`/usr/local/libexec/ble-mqtt/gattclient-ruuvi.before-quiet`, atomically replaces
the executable and restarts the service. It refuses unexpected hashes or
pre-existing backup/staging files. A startup-check failure restores the old
binary and attempts a restart. Do not delete a backup simply to bypass a refusal.
An already-installed message is a safe refusal, not a reason to force reinstall.

The initial three-second check proves only that the expected process started.
Observe at least a full health interval, then verify data delivery separately:

```sh
python3 /home/palich/hermes/ble-mqtt-deploy/check-combined-mqtt.py \
  -C /etc/ble-mqtt/combined.ini --seconds 60 --require-data
```

Require advancing DT24 timestamps, advancing Ruuvi measurement sequences and
fresh measurement ages. LWT Online means MQTT connected, not sensor reception.
The observer reads credentials from the INI using a private temporary file,
not command arguments. It does not publish test messages.

For a future build, use the source and tests, retain a new artifact manifest
and create a separately reviewed updater; do not alter pinned hashes casually.
Do not grant broad passwordless sudo to automate these occasional updates.

## Rollback

The quiet updater's automatic rollback is a binary-only rollback within the
combined service. The commands below are a different, legacy DT24-only fallback.
Use that fallback only with explicit operator approval; it removes Ruuvi service.

Stop/disable the new unit before starting the preserved old reader:

```sh
sudo systemctl disable --now ble-mqtt.service
screen -dmS hass_bat bash -c '/usr/local/bin/gattclient2.sh; exec bash'
```

This restores the old DT24-only runtime, not its removed boot startup.
Do not run old and new receivers together. The old executable still contains
historical credentials and is retained only for rollback. Keep the new
private config outside Git and rotate the historical broker password when
all users of that credential can be migrated safely.
