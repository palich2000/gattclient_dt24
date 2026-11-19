#ifndef RUUVI_H
#define RUUVI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct ruuvi_sample {
    double temperature, humidity, pressure, battery, acceleration[3], tx_power;
    int movement, sequence;
    uint8_t mac[6];
    bool mac_valid;
};
bool ruuvi_decode(const uint8_t *data, size_t length, struct ruuvi_sample *out);
#endif
