# Combined DT24 + Ruuvi receiver

Build with `make -C src combined-test`. This adds `src/gattclient-ruuvi`;
it does not install or replace `gattclient` or `ruuvi-mqtt`.

The optional `--ruuvi DE:4D:F8:3F:DA:91` mode uses the existing DT24
ATT connection and epoll mainloop. Separate raw HCI event and command
sockets receive legacy advertising reports on the same selected adapter.
There is no BlueZ StartDiscovery call. The startup guard reads the BlueZ
Discovering property and refuses to start when another discovery client
is active or the property cannot be checked. Do not start another scanner
while this process is running: the guard is not a controller-wide lock.

Scan settings: active, public local address, accept-all, 10 ms interval and
window, duplicate filtering OFF. Reports are validated and filtered by
sensor MAC, manufacturer 0x0499 and RAWv2 format 0x05 before decoding.
GLib/GIO remain dependencies for configuration, state and the startup guard;
received advertisements themselves do not travel through D-Bus.

The process stops on a disconnect or scanner failure rather than resetting
the adapter or reconnecting indefinitely. `--duration N` limits the event
loop after connection; an outer timeout also bounds initial connection.
Normal cleanup disables scanning. SIGKILL, power loss or an unresponsive
controller cannot guarantee cleanup. The HCI commands require elevated
permissions; on zigbeegate an unprivileged attempt returned EPERM.

## Historical isolated hardware trial on zigbeegate

The original investigation used `/home/palich/hermes/ruuvi-combined`.
This is not the current service build/install path. The commands below are
for an explicitly authorized isolated trial only: stop the current combined
service and all other readers first. Never run a second scanner beside it.

```sh
cd /home/palich/hermes/ruuvi-combined
sudo timeout --signal=TERM --kill-after=10 630 ./src/gattclient-ruuvi \
  -D -c -i hci0 -d 70:BF:48:9E:1B:42 -m 48 \
  --ruuvi DE:4D:F8:3F:DA:91 --duration 600 2>&1 | tee combined-live.log
```

`-D` disables all MQTT publishing. Ruuvi JSON is printed to stdout. DT24
frame progress and final counters are logged, including when MQTT is off.
Check changing measurement sequences, received DT24 frames and the kernel
journal over the same time window. A successful build or mocked HCI test
is not proof of concurrent reception or controller stability.

For service operation use `config/combined.ini.example` and
`systemd/ble-mqtt.service`; see [deployment](combined-service.md).
`[ruuvi] mac` enables reception without a CLI MAC. An explicit
`[ruuvi] topic_prefix` creates a second MQTT client within the SAME process,
with its own broker LWT. DT24 uses `tele/hass-batt/STATE`, Ruuvi uses
`tele/ruuvi1/SENSOR`, `tele/ruuvi1/AVAILABILITY` and `tele/ruuvi1/LWT`.
Consider Ruuvi available only when both LWT and AVAILABILITY are Online.
The MQTT password is read from a mode-0600 INI, never a compiled constant.
For backward-compatible CLI trials without a Ruuvi prefix, the original
single-connection `RUUVI/SENSOR` and `RUUVI/AVAILABILITY` layout remains.
Publish interval and stale timeout come from the shared configuration.
Set `console_json=false` for journal-friendly service logging.

## Verified boundary

Host and ARM-native builds and combined unit tests passed. Existing local
integration tests passed. An unprivileged native hardware attempt connected
to DT24, then failed to enable scanning with Operation not permitted. It
exited, released the connection and left BlueZ powered/not discovering.
The user subsequently demonstrated simultaneous DT24 frames and changing
Ruuvi sequences in a privileged run. This short run is not a sustained
stability claim. The combined service has since been deployed and fresh
telemetry from both devices was observed through MQTT. See
[deployment status](deployment-status.md) for dated evidence and limitations.
