# Code review: combined DT24/Ruuvi receiver

Date: 2026-11-19. Repository: `/home/palich/src2/gattclient_dt24`.
Comparison base: `f3d4bfd32c4b4a6973bcf8bba391fe27736e43c6`.
Scope: current tracked diff and relevant untracked combined-receiver sources,
configuration/MQTT transport, decoder/state/raw-HCI modules, deployment scripts,
service unit and tests. Not a whole-history audit or a vendor BlueZ audit.
No commit, push, production restart or adapter reset was performed for this review.

## Result

The quiet logging regression is covered by an actual parser test and the native
build has been deployed by the operator. Subsequent read-only journal inspection
found zero individual voltage/current reading lines after the 19:22:38 UTC restart,
while DT24 and Ruuvi health summaries continued. Both streams were fresh in the
observed summaries; long-term adapter stability remains unproven.

An independent read-only reviewer raised four findings. The dispositions below
are the main reviewer's verification, not an assertion that the independent
reviewer approved every change. Three residual issues remain open; one finding
was rejected using installed primary documentation and observed runtime behavior.

## Findings and disposition

### R1: Rejected - mosquitto_sub configuration allegedly ignored

Location: `scripts/check-combined-mqtt.py`, temporary client config and subprocess.
The script creates `$XDG_CONFIG_HOME/mosquitto_sub` and explicitly passes that
XDG_CONFIG_HOME to the child. The installed mosquitto_sub manual documents exactly
this path and one option/value pair per line. Host/authentication/TLS options do
not need to appear in argv. Keeping credentials out of argv is intentional.
The observer also authenticated and received fresh data from the configured
production broker during read-only verification. Do not replace this mechanism
with passwords on the command line.

### R2: Open, medium - rollback itself is best-effort

Location: `scripts/update-quiet.py`, exception handler after installation.
The updater restores the saved executable and attempts a service restart, but
it does not check the restored hash or verify successful restart. Restoration
I/O can itself fail; restart uses check=False. The script reports failure rather
than claiming a successful rollback, so this is a recovery robustness gap, not
a demonstrated false success in the normal installation path.

New sandbox tests cover success, wrong artifact, already-installed refusal,
startup failure with successful rollback and wrong running binary with successful
rollback. Privileged commands are mocked; these are not production service tests.
A follow-up should preserve the primary error, report rollback failure separately,
verify the restored hash and running executable, and add restoration-I/O and
rollback-restart fault injection. No claim is made that arbitrary storage failure
can always be recovered automatically.

The reviewer's concern that the pinned artifact is absent from scripts/ is not a
repository defect: this is a one-time staged updater, documented to run beside its
verified native artifact in a private deployment directory. Rebuilding arbitrary
source does not yield the pinned artifact hash.

### R3: Open, medium - scan-disable failure only warns

Location: `src/ruuvi_raw.c`, `ruuvi_raw_stop`.
If the controller rejects the scan-disable command, cleanup logs a warning and
closes descriptors; it has no return value to propagate cleanup status. Descriptor
closure is not proof that controller-global scanning stopped. The BlueZ discovery
check is also not an exclusive lock or a raw-HCI scan-state check.

This is a documented cleanup limitation, not evidence that it caused the observed
DT24 timeout. There was no live failure injection. A follow-up can propagate a
cleanup result and test disable failures without introducing automatic controller
resets or interference with another scanner. Forced termination/power loss remain
outside normal cleanup guarantees.

### R4: Open, low - undefined sequence weakens freshness semantics

Location: `src/ruuvi_state.c:21-30`.
When the decoder reports an unavailable sequence (`sample.sequence < 0`), every
valid advertisement updates last_measurement, even with unchanged payload data.
For those frames the advertised measurement age means packet reception age, not
proof of a new measurement. A follow-up needs an explicit unknown-sequence policy
and regression coverage; comparing only values can also misclassify genuine
unchanged measurements as stale.

The observed production Ruuvi supplied advancing valid measurement sequences,
so this edge case does not invalidate the recorded freshness observation. It has
not been reproduced on the physical tag. Do not attribute the earlier reception
failure to this condition without evidence.

## Verification

- `make -C src integration-test`: PASS, including isolated MQTT/TLS, CLI,
  private D-Bus/BlueZ lifecycle and recovery tests.
- `make -C src combined-integration-test`: PASS, including HCI/parser/lifecycle,
  real-parser INFO/DEBUG regression, five sandbox updater cases and dual MQTT
  delivery/clean-shutdown/crash-LWT cases against a local isolated broker.
- Quiet change previously passed `combined-test` on both host and native ARM.
- Python syntax compilation and `git diff --check`: PASS.
- Bounded scans of current C/header/Python/example/unit sources found no literal
  secret assignments, shell=True, os.system or pickle deserialization patterns.
  Pattern scanning is not a complete security audit and does not erase historical
  credentials; the existing credential-rotation warning remains applicable.
- Test logs: `/home/palich/hermes/ble-mqtt-deploy/review-20261119/`.

No clean-HEAD full-suite comparison was performed: the new suites and modules are
untracked additions absent from that base. The quiet change does have an observed
RED-before/GREEN-after regression. Installer rollback tests use relocated paths
and mocked commands. No fresh hardware failure tests or sustained soak test were
performed for this review.

## Repository housekeeping

README now describes all three executables and points to current combined-service
operations. Historical hardware-trial commands are labelled as historical and
must not run beside production. Service docs distinguish initial install, pinned
binary update, legacy fallback, broker LWT and sensor freshness. The pinned quiet
updater and its sandbox regression tests now live in the repository. Existing
working-tree changes are preserved; no files were staged or committed.
