#ifndef SRC_MQTT_H
#define SRC_MQTT_H
#include "app_config.h"
bool mosq_init(const struct app_config *config);
void mosq_destroy(void);
bool mosq_connected(bool ruuvi);
void mosq_ruuvi(const char *json, bool available);
void mosq_gather_data(double current, double voltage);
#endif
