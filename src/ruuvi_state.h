#ifndef RUUVI_STATE_H
#define RUUVI_STATE_H
#include "ruuvi.h"
#include <glib.h>
struct ruuvi_state {
    struct ruuvi_sample sample;
    bool received;
    double last_seen, last_measurement;
    int rssi;
    char timestamp[32];
};
bool ruuvi_update(struct ruuvi_state *s, GVariant *properties, double now);
bool ruuvi_fresh(const struct ruuvi_state *s, double now, int timeout);
char *ruuvi_json(const struct ruuvi_state *s, const char *mac, double now, int timeout);
#endif
