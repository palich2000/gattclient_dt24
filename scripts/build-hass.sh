#!/bin/sh
# Build natively on hass without installing/upgrading system GLib packages.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
headers="$root/build-deps/glib-headers"
test -r "$headers/usr/include/glib-2.0/glib.h"
test -r "$headers/usr/lib/arm-linux-gnueabihf/glib-2.0/include/glibconfig.h"
exec make -C "$root/src" -j2 "${@:-ruuvi-mqtt}" \
  INCLUDES="-pthread -I$headers/usr/include/glib-2.0 -I$headers/usr/lib/arm-linux-gnueabihf/glib-2.0/include" \
  EXTRA_LIBS="-l:libgio-2.0.so.0 -l:libgobject-2.0.so.0 -l:libglib-2.0.so.0 -lmosquitto -lpthread -lm"
