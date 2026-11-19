#!/bin/sh
# Desktop entry point for the approved, already staged ARM release.
set -eu
stage=/home/palich/hermes/ble-mqtt-deploy/review-GHfp9p
binary=3f2972258eed1162bfa6de993eedb424246c64ff436a0381b2da776d13fe3202
updater=6447af84a450e4f99f324bc07f2c532e5ac9d599e0bfb814b955c8106944eb8e
case ${1-} in
check)
    exec ssh -o BatchMode=yes zigbeegate "set -eu; cd '$stage'; printf '%s\n' '$binary  src/gattclient-ruuvi' '$updater  update-3f297225.py' | sha256sum -c -; ./src/gattclient-ruuvi -C /etc/ble-mqtt/combined.ini --check-config"
    ;;
install)
    # Only local constants are interpolated. sudo reads the password from SSH's tty.
    exec ssh -t zigbeegate "sudo /bin/sh -eu -c '
        d=/root/ble-mqtt-release
        install -d -m 0700 \"\$d\"
        cd \"\$d\"
        install -m 0600 $stage/update-3f297225.py update-3f297225.py
        install -m 0700 $stage/src/gattclient-ruuvi gattclient-ruuvi-3f297225
        printf \"%s\\n\" \"$binary  gattclient-ruuvi-3f297225\" \"$updater  update-3f297225.py\" | sha256sum -c -
        exec /usr/bin/python3 -I ./update-3f297225.py
    '"
    ;;
*) printf 'Usage: %s check|install\n' "$0" >&2; exit 2;;
esac
