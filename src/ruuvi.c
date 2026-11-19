#include "ruuvi.h"
#include <math.h>
#include <string.h>

static unsigned be16(const uint8_t *p) {
    return ((unsigned)p[0] << 8) | p[1];
}
static double signed16(const uint8_t *p) {
    unsigned n = be16(p);
    if (n == 0x8000) return NAN;
    return n & 0x8000 ? (int)n - 65536 : (int)n;
}

bool ruuvi_decode(const uint8_t *data, size_t length, struct ruuvi_sample *out) {
    if (!data || !out || length != 24 || data[0] != 5) return false;
    *out = (struct ruuvi_sample){0};
    out->temperature = signed16(data + 1) * 0.005;
    unsigned humidity = be16(data + 3), pressure = be16(data + 5);
    out->humidity = humidity == 65535 ? NAN : humidity * 0.0025;
    out->pressure = pressure == 65535 ? NAN : (pressure + 50000) / 100.0;
    for (int i = 0; i < 3; i++) out->acceleration[i] = signed16(data + 7 + 2*i) / 1000.0;
    unsigned power = be16(data + 13), battery = power >> 5, tx = power & 31;
    out->battery = battery == 2047 ? NAN : (battery + 1600) / 1000.0;
    out->tx_power = tx == 31 ? NAN : (int)tx * 2 - 40;
    unsigned sequence = be16(data + 16);
    out->sequence = sequence == 65535 ? -1 : (int)sequence;
    out->movement = data[15] == 255 ? -1 : data[15];
    memcpy(out->mac, data + 18, 6);
    out->mac_valid = false;
    for (int i = 0; i < 6; i++) if (out->mac[i] != 255) out->mac_valid = true;
    return true;
}
