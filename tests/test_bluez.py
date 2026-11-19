#!/usr/bin/python3
"""BlueZ protocol fixture on a PRIVATE session bus, never the real adapter."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

DBusGMainLoop(set_as_default=True)
loop = GLib.MainLoop()
bus = dbus.SessionBus()
name = dbus.service.BusName("org.bluez", bus)
packet = bytes.fromhex("0512404f9cbce4000cfff8fc10cf7601002ede4df83fda91")

class Device(dbus.service.Object):
    @dbus.service.signal("org.freedesktop.DBus.Properties", signature="sa{sv}as")
    def PropertiesChanged(self, interface, changes, invalidated):
        pass

class Adapter(dbus.service.Object):
    started = 0
    stopped = 0
    sequence = 0

    @dbus.service.method("org.bluez.Adapter1", in_signature="a{sv}", out_signature="")
    def SetDiscoveryFilter(self, filters):
        # GetDiscoveryFilters on hass (BlueZ 5.50), captured from the real host.
        supported = {"UUIDs", "RSSI", "Pathloss", "Transport", "DuplicateData"}
        if set(filters) - supported:
            raise dbus.exceptions.DBusException(
                "Invalid arguments in method call", name="org.bluez.Error.InvalidArguments")
        assert filters["Transport"] == "le"
        assert filters["DuplicateData"]
        assert not filters.get("Discoverable", False)

    @dbus.service.method("org.bluez.Adapter1", in_signature="", out_signature="")
    def StartDiscovery(self):
        self.started += 1
        GLib.timeout_add(200, self.emit)

    @dbus.service.method("org.bluez.Adapter1", in_signature="", out_signature="")
    def StopDiscovery(self):
        self.stopped += 1

    def emit(self):
        self.sequence += 1
        data = bytearray(packet)
        data[17] = self.sequence
        props = {"ManufacturerData": dbus.Dictionary({dbus.UInt16(0x0499): dbus.ByteArray(data)}, signature="qv"), "RSSI": dbus.Int16(-65)}
        device.PropertiesChanged("org.bluez.Device1", props, [])
        return self.stopped == 0

adapter = Adapter(bus, "/org/bluez/hci0")
device = Device(bus, "/org/bluez/hci0/dev_DE_4D_F8_3F_DA_91")
with tempfile.TemporaryDirectory(prefix="ruuvi-dbus-test-") as tmp:
    config = Path(tmp) / "ruuvi.ini"
    config.write_text("[bluetooth]\nmac=DE:4D:F8:3F:DA:91\n[mqtt]\nenabled=false\nclient_id=fixture\ntopic_prefix=test/ruuvi\n[app]\npublish_interval=1\nstale_timeout=3\n")
    env = dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=os.environ["DBUS_SESSION_BUS_ADDRESS"])
    assert Path(sys.argv[1]).is_file(), "ruuvi-mqtt binary is missing"
    child = subprocess.Popen([sys.argv[1], "-C", str(config), "--duration", "4"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    started = time.monotonic()
    def poll():
        if child.poll() is not None:
            loop.quit()
            return False
        if time.monotonic() - started > 12:
            child.kill()
            loop.quit()
            return False
        return True
    GLib.timeout_add(100, poll)
    loop.run()
    stdout, stderr = child.communicate(timeout=3)
    assert child.returncode == 0, (stdout, stderr)
    rows = [json.loads(line) for line in stdout.splitlines()]
    assert len(rows) >= 2, (stdout, stderr)
    assert all(r["MAC"] == "DE:4D:F8:3F:DA:91" and r["Temperature"] == 23.36 for r in rows), rows
    assert rows[-1]["MeasurementSequence"] > rows[0]["MeasurementSequence"], rows
    assert adapter.started == 1 and adapter.stopped == 1, (adapter.started, adapter.stopped)
    print("BlueZ D-Bus -> decoder -> console, timed exit and StopDiscovery: OK")
