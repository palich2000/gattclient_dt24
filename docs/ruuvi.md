# Ruuvi reader: operation and verification

## Design

`ruuvi_main.c` subscribes to BlueZ D-Bus `PropertiesChanged` and
`InterfacesAdded`. It requests LE discovery with `DuplicateData=true`, filters
by the exact adapter/device object path, and decodes only manufacturer `0x0499`,
RAWv2 version `0x05`, 24-byte payload. It does not pair, connect, change adapter
power, remove devices, or read cached `GetManagedObjects` as fresh samples.
BlueZ manages the scan session; StopDiscovery releases only our session.
BlueZ may use active LE scanning internally; this is not a claim of HCI passive
scan mode. Other BlueZ clients can merge discovery filters, so local MAC and
manufacturer checks are mandatory.

Modules:

- `ruuvi.c`: pure decoder, signed values and unavailable sentinels.
- `ruuvi_state.c`: live property parsing, sample timestamps, freshness, JSON.
- `ruuvi_main.c`: CLI, D-Bus subscriptions, timers, process lifecycle.
- `app_config.c`: shared validated INI configuration.
- `mqtt_transport.c`: shared Mosquitto connection, reconnect, TLS, LWT.
- `mqtt.c`: DT24-specific averaging and original STATE field names/units.
- `dlog.c`: existing logging infrastructure, unchanged.

One Ruuvi sensor per process. Multiple instances require unique client IDs and
topic prefixes. RAWv2 is supported; other formats/history/GATT commands and Home
Assistant discovery are intentionally not implemented.

## Configuration

The complete examples are in `config/`. Keys are case-sensitive. Unknown keys,
bad types, overlong values, invalid MACs/adapters/ranges and inconsistent TLS
options fail before opening Bluetooth or MQTT. Files containing a password must
be regular files with no group/other permission bits (0600 or stricter). Parser
errors do not echo INI lines or credential values. Keep private files outside
Git; `*.local.ini` is ignored. Credentials previously in Git history are not
removed by editing the current source.

| Section | Keys |
| --- | --- |
| bluetooth | adapter, mac, address_type, mtu, security |
| mqtt | enabled, host, port, client_id, username, password, topic_prefix, keepalive, tls, ca_file, cert_file, key_file |
| app | publish_interval, stale_timeout, log_level, thermal_zone |

`address_type`, `mtu`, `security` and `thermal_zone` apply to DT24, not Ruuvi.
`stale_timeout` applies to Ruuvi. All time intervals are seconds.
Defaults: hci0, localhost:1883, keepalive 60, publish 10, stale 120, log priority 6.
Ruuvi requires explicit MAC/client_id/topic_prefix. DT24 can derive its prefix
and client ID from `-H`, or legacy name `main-batt` if not supplied.

TLS is explicit: a port number never enables encryption. `tls=true` requires
`ca_file`; server certificate and hostname verification remain enabled. Optional
client cert/key must be supplied together. Use absolute paths for TLS files.
No custom insecure-verification option is provided. The test suite exercises
server-authenticated TLS with a disposable trusted certificate; production
certificates and client-certificate authentication have not been exercised.

## Output and freshness

For prefix `test/ruuvi/de4df83fda91`:

| Suffix | Payload | Retained |
| --- | --- | --- |
| SENSOR | JSON with latest valid sample | no |
| LWT | Online/Offline: MQTT process connection | yes |
| AVAILABILITY | Online/Offline: sensor measurement freshness | yes |

Consumers must require BOTH LWT and AVAILABILITY to be Online. A crash has an
MQTT will for LWT, not a second will for AVAILABILITY. Broker restart/loss is
handled by the Mosquitto loop; availability is re-published after reconnect.
Orderly shutdown publishes Offline availability, then QoS1 Offline LWT and waits
up to two seconds for its acknowledgement before disconnecting.

SENSOR fields: Time (UTC reception time of last new sequence), MAC, Available,
Temperature (C), Humidity (% RH), Pressure (hPa, absolute/unadjusted),
BatteryVoltage (V), AccelerationX/Y/Z (g), TxPower/RSSI (dBm), MovementCounter,
MeasurementSequence, AgeSeconds (last valid advertisement),
MeasurementAgeSeconds (last new measurement). Unavailable values are JSON null,
never NaN or invented zero. Time is receiver time, not a timestamp from the tag.

The latest measurement is published periodically without averaging; repeated
sequence values are allowed in periodic output while fresh. Same-sequence
advertisements update radio age but not measurement age. RSSI-only updates and
wrong manufacturer/MAC packets never refresh measurement freshness. Publication
stops after stale_timeout; availability becomes Offline. Fresh reception restores
it. Wraparound/reset of sequence is accepted as a new measurement.

BlueZ disappearance clears freshness and retries after the name reappears.
Powered-off adapters are not automatically powered on. Failed scan setup is
retried at five-second intervals. A closed system D-Bus connection exits nonzero
for a future supervisor to restart. Fatal MQTT setup errors also exit nonzero.

## DT24 compatibility boundary

The DT24 frame decoder and vendored ATT/GATT stack were not changed. The new
binary keeps Current, Voltage, Power, Time, Uptime, LoadAverage and CPUTemp in
`<prefix>/STATE`; current and voltage are averaged at the configured interval.
Temperature/Ah/Wh remain log-only, as before. Default interval remains 10 seconds.

Intentional differences: credentials require configuration, the default adapter
is explicitly hci0, MQTT connection lifecycle uses the shared client, publication
interval uses monotonic time, samples during broker disconnection are discarded
rather than blended into recovery, and the old unused POWER subscription and
connect-time host-only STATE publication are not carried over. LWT is retained
and delivered at QoS1. Live DT24 hardware behavior of the rebuilt binary has not
been tested, because its running production instance was deliberately untouched.

## Verified development hosts

- Source: `/home/palich/src2/gattclient_dt24`.
- Test copy: `rasp4-test:/home/palich/hermes/ruuvi-dev`.
- Bluetooth was enabled separately with user approval. No receiver systemd unit
  is installed or enabled. The program runs as user `palich` in the foreground.
- The existing DT24 executable and process on `zigbeegate` remain unchanged.
- Later host bring-up enabled Bluetooth on `hass` and added `palich` to the
  bluetooth group on both `hass` and `zigbeegate`, with D-Bus policy reloads
  performed by the owner. Native builds are staged on both hosts; see
  `hass.md` for the legacy armhf build recipe.

Console command on rasp4-test:

```sh
cd /home/palich/hermes/ruuvi-dev
./src/ruuvi-mqtt -C config/ruuvi.ini.example -D --duration 60
```

For continuous console reception omit `--duration`; stop with Ctrl+C. To restart,
run the same command after the old process exits. Do not start duplicate clients
with the same MQTT client ID. No permanent process is left by the finite probes.

The private `config/probe.local.ini` from the live test points to a temporary SSH
forward, not a permanent broker; do not use it for normal operation.

## Repeatable live acceptance probe

`scripts/live_probe.py` uses the real tag on the explicitly named test host and
forwards MQTT through SSH to a disposable loopback-only broker on the desktop.
It reads back the uploaded config checksum, captures console and MQTT output,
compares the samples, checks sequence progress and retained Offline states, then
checks Discovering=no. It never publishes to the home broker or installs a
service. Requires SSH forwarding, local mosquitto/clients and a staged executable.
Choose a new output directory for each run; existing output is not overwritten.

```sh
python3 scripts/live_probe.py --host rasp4-test \
  --remote-dir /home/palich/hermes/ruuvi-dev \
  --output /home/palich/hermes/ruuvi-investigation/live-another \
  --duration 70
```

Use a working SSH agent in the invoking environment. If remote port 18884 is
occupied, specify another free `--remote-port`; forwarding fails closed rather
than reusing an existing listener.

## Evidence and limits

- Clean x86_64 and ARM64 builds with -Wall -Wextra -Werror.
- Decoder tests: real captured packet, official extrema, unavailable fields,
  malformed length/version and null input; decoder also run with ASan/UBSan.
- State tests: actual D-Bus value types, manufacturer filtering, partial
  properties, repeated sequences, stale/new measurement transitions.
- CLI tests for both executables: config errors, private credential-file mode,
  secret-safe errors, command-line precedence, offline validation.
- Isolated MQTT tests verify actual subscriber payloads, retained lifecycle and
  DT24 Current/Voltage/Power fields. Separate TLS smoke test exercises encryption.
- Private-bus end-to-end tests cover live signal parsing, MAC filtering, sensor
  silence with continuing RSSI, BlueZ disappearance, broker restart, timed exit,
  SIGTERM and StopDiscovery cleanup.
- Real 70-second BLE-to-MQTT probe: 14 published samples, 12 distinct sequences,
  RSSI -93..-82 dBm. Subscriber payloads exactly matched console output; both
  retained statuses were Offline after exit, and scanning stopped.
- Evidence: `/home/palich/hermes/ruuvi-investigation/live-final/summary.json`,
  `console.jsonl`, `mqtt.log`, `receiver.log`, `adapter-after.txt`.

This is short-run validation, not a long-duration RF reliability claim. Placement
should be improved or reception measured at the intended final location before
choosing a production host. A subsequent five-minute test on zigbeegate ran
Ruuvi discovery alongside the unchanged DT24 process on hci0. DT24 continued
with 29 new STATE timestamps (maximum spacing 11 seconds), 644 additional RX
ACL packets and the same PID 1140/LE connection. No Ruuvi measurements were
received. This verifies discovery did not interrupt DT24 in that window, NOT
successful simultaneous receipt of both sensors. The receiver exited normally
and Discovering returned to no. Evidence on zigbeegate:
`/home/palich/hermes/ruuvi-dev/coexistence-ot6kzbw_/report.json` and snapshots.
A prior 18-second attempt was interrupted by a screen-hardcopy capture race;
the monitoring helper was corrected and the full test repeated. That first
attempt also logged a StopDiscovery timeout, followed by Discovering=no; the
completed five-minute attempt had no such timeout.

The helper is `/home/palich/hermes/ruuvi-investigation/coexistence_probe.py`
(locally), also staged in the remote workspace. It verifies the exact DT24 PID
and command, monitors fresh screen output, LE connection and HCI counters, and
stops only its Ruuvi child if DT24 timestamps become stale. Revalidate the
hard-coded PID and screen session before reuse. No Home Assistant configuration,
production broker deployment, permanent Ruuvi service or reboot test was done.

## Protocol references

- https://docs.ruuvi.com/communication/bluetooth-advertisements/data-format-5-rawv2
- https://github.com/ruuvi/ruuvi-sensor-protocols/blob/master/dataformat_05.md
- https://github.com/bluez/bluez/blob/master/doc/org.bluez.Adapter.rst
- https://mosquitto.org/man/libmosquitto-3.html
