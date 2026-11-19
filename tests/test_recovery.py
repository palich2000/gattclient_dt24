#!/usr/bin/python3
"""Private BlueZ bus + loopback broker: silence, daemon/broker recovery, SIGTERM."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib
from test_mqtt import free_port, wait_port

DBusGMainLoop(set_as_default=True)
loop = GLib.MainLoop()
bus = dbus.SessionBus()
name = dbus.service.BusName("org.bluez", bus)
started = time.monotonic()

class Device(dbus.service.Object):
    @dbus.service.signal("org.freedesktop.DBus.Properties", signature="sa{sv}as")
    def PropertiesChanged(self, interface, changes, invalidated):
        pass

class Adapter(dbus.service.Object):
    starts = 0
    stops = 0
    active = False
    @dbus.service.method("org.bluez.Adapter1", in_signature="a{sv}", out_signature="")
    def SetDiscoveryFilter(self, filters):
        assert filters["DuplicateData"]
    @dbus.service.method("org.bluez.Adapter1", in_signature="", out_signature="")
    def StartDiscovery(self):
        self.active = True
        self.starts += 1
    @dbus.service.method("org.bluez.Adapter1", in_signature="", out_signature="")
    def StopDiscovery(self):
        self.active = False
        self.stops += 1

adapter = Adapter(bus, "/org/bluez/hci0")
device = Device(bus, "/org/bluez/hci0/dev_DE_4D_F8_3F_DA_91")
other = Device(bus, "/org/bluez/hci0/dev_11_22_33_44_55_66")
sequence = 0

def emit():
    global sequence
    elapsed = time.monotonic() - started
    if not adapter.active:
        return True
    # Measurements cease between 2s and 8s, but RSSI events keep arriving.
    if 2 < elapsed < 8:
        device.PropertiesChanged("org.bluez.Device1", {"RSSI": dbus.Int16(-60)}, [])
        return True
    sequence += 1
    data = bytearray.fromhex("0512404f9cbce4000cfff8fc10cf7601002ede4df83fda91")
    data[17] = sequence
    props = {"ManufacturerData": dbus.Dictionary({dbus.UInt16(0x0499): dbus.ByteArray(data)}, signature="qv"), "RSSI": dbus.Int16(-65)}
    device.PropertiesChanged("org.bluez.Device1", props, [])
    # A different MAC must never overwrite the sample.
    data[1:3] = b"\x00\x00"
    props["ManufacturerData"] = dbus.Dictionary({dbus.UInt16(0x0499): dbus.ByteArray(data)}, signature="qv")
    other.PropertiesChanged("org.bluez.Device1", props, [])
    return True

def drop_bluez():
    bus.release_name("org.bluez")
    adapter.active = False
    return False

def restore_bluez():
    bus.request_name("org.bluez")
    return False

with tempfile.TemporaryDirectory(prefix="ruuvi-recovery-") as tmp:
    port = free_port()
    broker_cfg = Path(tmp) / "broker.conf"
    broker_cfg.write_text(f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n")
    children = []
    def broker_start():
        p = subprocess.Popen(["mosquitto", "-c", str(broker_cfg)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        children.append(p)
        wait_port(port)
        return p
    broker = broker_start()
    sub = subprocess.Popen(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", "test/recovery/#", "-v"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    children.append(sub)
    time.sleep(0.3)
    cfg = Path(tmp) / "config.ini"
    cfg.write_text(f"[bluetooth]\nmac=DE:4D:F8:3F:DA:91\n[mqtt]\nenabled=true\nhost=127.0.0.1\nport={port}\nclient_id=test-recovery\ntopic_prefix=test/recovery\n[app]\npublish_interval=1\nstale_timeout=2\n")
    env = dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=os.environ["DBUS_SESSION_BUS_ADDRESS"])
    child = subprocess.Popen([sys.argv[1], "-C", str(cfg)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    children.append(child)
    started = time.monotonic()
    def restart_broker():
        global broker
        broker.kill()
        broker.wait(timeout=3)
        broker = broker_start()
        return False
    def terminate():
        child.send_signal(signal.SIGTERM)
        return False
    def poll():
        if child.poll() is not None:
            loop.quit()
            return False
        if time.monotonic() - started > 18:
            child.kill()
            loop.quit()
            return False
        return True
    try:
        GLib.timeout_add(200, emit)
        GLib.timeout_add(4500, drop_bluez)
        GLib.timeout_add(5500, restore_bluez)
        GLib.timeout_add(6500, restart_broker)
        GLib.timeout_add(13000, terminate)
        GLib.timeout_add(100, poll)
        loop.run()
        stdout, stderr = child.communicate(timeout=3)
        assert child.returncode == 0, (stdout, stderr)
        # Stop only our subscriber; its stdout buffer contains the full capture.
        sub.kill()
        messages = sub.communicate(timeout=3)[0].splitlines()
        rows = [json.loads(line) for line in stdout.splitlines()]
        assert len(rows) >= 4 and all(r["Temperature"] == 23.36 for r in rows), (rows, stderr)
        assert adapter.starts >= 2 and adapter.stops == 1, (adapter.starts, adapter.stops, stderr)
        assert messages.count("test/recovery/LWT Online") >= 2, (messages, stderr)
        availability = [s.split()[-1] for s in messages if s.startswith("test/recovery/AVAILABILITY ")]
        # At least fresh -> stale -> recovered -> stopped, possibly initial offline.
        collapsed = [x for i, x in enumerate(availability) if i == 0 or x != availability[i-1]]
        assert collapsed[-4:] == ["Online", "Offline", "Online", "Offline"], (collapsed, messages, stderr)
        payloads = [json.loads(s.split(" ", 1)[1]) for s in messages if s.startswith("test/recovery/SENSOR ")]
        assert len(payloads) >= 3 and all(p["Available"] for p in payloads), messages
        assert payloads[-1]["MeasurementSequence"] > payloads[0]["MeasurementSequence"], messages
        for suffix in ("LWT", "AVAILABILITY"):
            value = subprocess.check_output(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", f"test/recovery/{suffix}", "-C", "1", "-W", "3"], text=True)
            assert value.strip() == "Offline", value
        print("MQTT end-to-end, sensor silence, MAC filtering, BlueZ/broker reconnect, SIGTERM: OK")
    finally:
        for p in reversed(children):
            if p.poll() is None:
                p.kill()
            p.wait(timeout=3)
