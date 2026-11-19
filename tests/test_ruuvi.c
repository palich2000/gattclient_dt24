#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ruuvi.h"

int main(void) {
    /* Captured on rasp4-test from the user's tag, manufacturer 0x0499. */
    const unsigned char data[] = {
        5,0x12,0x40,0x4f,0x9c,0xbc,0xe4,0,0x0c,0xff,0xf8,0xfc,
        0x10,0xcf,0x76,1,0,0x2e,0xde,0x4d,0xf8,0x3f,0xda,0x91
    };
    struct ruuvi_sample s;
    assert(ruuvi_decode(data, sizeof(data), &s));
    assert(fabs(s.temperature - 23.36) < 1e-8);
    assert(fabs(s.humidity - 50.95) < 1e-8);
    assert(fabs(s.pressure - 983.56) < 1e-8);
    assert(fabs(s.battery - 3.259) < 1e-8);
    assert(s.sequence == 46 && s.movement == 1);
    assert(fabs(s.acceleration[1] - (-0.008)) < 1e-8);
    assert(fabs(s.acceleration[2] - (-1.008)) < 1e-8);
    assert(s.tx_power == 4 && s.mac_valid && s.mac[5] == 0x91);
    assert(!ruuvi_decode(data, 23, &s));
    assert(!ruuvi_decode(data, 25, &s));
    assert(!ruuvi_decode(NULL, 24, &s));
    unsigned char invalid[24] = {5,0x80,0,0xff,0xff,0xff,0xff,
        0x80,0,0x80,0,0x80,0,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff};
    assert(ruuvi_decode(invalid, sizeof(invalid), &s));
    assert(isnan(s.temperature) && isnan(s.humidity) && isnan(s.pressure));
    assert(isnan(s.battery) && isnan(s.tx_power) && isnan(s.acceleration[0]));
    assert(s.sequence == -1 && s.movement == -1 && !s.mac_valid);
    invalid[0] = 6;
    assert(!ruuvi_decode(invalid, sizeof(invalid), &s));
    /* Official RAWv2 minimum and maximum vectors. */
    unsigned char minimum[24] = {5,0x80,1,0,0,0,0,0x80,1,0x80,1,0x80,1};
    assert(ruuvi_decode(minimum, sizeof(minimum), &s));
    assert(fabs(s.temperature + 163.835) < 1e-8);
    assert(s.pressure == 500 && s.humidity == 0 && s.tx_power == -40);
    assert(fabs(s.acceleration[0] + 32.767) < 1e-8);
    unsigned char maximum[24] = {5,0x7f,0xff,0xff,0xfe,0xff,0xfe,
        0x7f,0xff,0x7f,0xff,0x7f,0xff,0xff,0xde,0xfe,0xff,0xfe};
    assert(ruuvi_decode(maximum, sizeof(maximum), &s));
    assert(fabs(s.temperature - 163.835) < 1e-8);
    assert(fabs(s.humidity - 163.835) < 1e-8);
    assert(fabs(s.battery - 3.646) < 1e-8 && s.tx_power == 20);
    assert(s.sequence == 65534 && s.movement == 254);
    puts("RAWv2: captured packet, bounds, unavailable fields, malformed inputs OK");
}
