# Deployment status and verification boundary

This is a dated observation, not a continuous health monitor. Times below are UTC.

## Observed on zigbeegate, 2026-11-19

- Adapter identifies as CSR8510 A10, USB `0a12:0001`, HCI Bluetooth 4.0,
  address `00:1A:7D:DA:71:09`. Identification alone does not establish chipset
  authenticity, RF performance or compatibility under sustained load.
- The first DT24 session received 618 frames, then timed out at 19:06:35.
  Reconnection initially failed. No corresponding USB removal was recorded.
  This does not isolate the cause to hardware, radio conditions or software.
- Subsequent reception recovered. A 45-second read-only MQTT observation
  received four distinct DT24 timestamps and five distinct Ruuvi sequences.
  Ruuvi availability was Online; latest measurement age was 0.858 seconds,
  RSSI -89 dBm. This observation preceded the quiet-log update.
- The quiet-log service was started at 19:22:38. The installed executable
  SHA-256 matches the tested native artifact below. At the subsequent check,
  MainPID was 3348 and NRestarts was zero. Journal summaries at 19:28-19:32
  showed both streams fresh, DT24 64 frames per interval and advancing Ruuvi
  measurements. There is no long-duration stability guarantee.

## Native artifacts

| Build | SHA-256 |
| --- | --- |
| Original combined | `12d892bd15451a5c33e304ff38231839c731155b18a024bae7cdc100a867918b` |
| Minute health logs | `09fc4f1ab0ac0e444ea65bd79027b33c970bf53a80c0af631ed8d053aaa62a6a` |
| Quiet INFO readings | `13ae44b7849a3d8c0a5e488f65585816417d87f39d2cd8ad952c5ef557c5aaee` |

Hashes identify these exact ARM builds, not reproducible hashes of arbitrary
future builds. The quiet change moves individual DT24 reading diagnostics
from INFO to DEBUG. It does not change BLE scan parameters, reconnect logic,
MQTT cadence or payloads. It is not a Bluetooth timeout fix.

The source tree has uncommitted tracked and untracked changes. A Git HEAD
alone is not an identifier for these binaries. Do not publish a release from
HEAD without explicitly reviewing and including the intended working tree.

## Verification performed for the quiet change

- A regression using the real DT24 parser failed before the change: routine
  readings appeared at INFO.
- With the change, the reading is absent at INFO and present at DEBUG;
  fresh/stale minute health summaries remain visible.
- `make -C src combined-test` passed locally and on ARM zigbeegate, including
  advertising framing, raw scanner lifecycle, health and MQTT routing tests.
- The new native binary passed `--check-config` against the existing private INI.
- Native build logs and pinned installer are staged under
  `/home/palich/hermes/ble-mqtt-deploy`. Credentials are not in this repository.

For operator commands and safe update/rollback boundaries see
[combined-service.md](combined-service.md). Review findings and any remaining
coverage limitations are recorded separately in [code-review.md](code-review.md).
