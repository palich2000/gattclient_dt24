# Running the readers

Build from here with `make`, or use `make -C src` from the repository root.

Ruuvi console-only:

```sh
./ruuvi-mqtt -C ../config/ruuvi.ini.example -D --duration 60
```

DT24 console-only (existing GATT commands remain available):

```sh
./gattclient -d DEVICE_MAC -D
```

DT24 MQTT now requires a private INI configuration; credentials are not compiled
in. Use `--check-config` before connecting and see `../README.md` and
`../docs/ruuvi.md` for migration and deployment boundaries.

Interactive DT24 commands include `help`, `rssi [interval_ms]`, and
`batt [interval_ms]`. Ctrl+C exits. No service is installed by building.
