#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H
#include "app_config.h"
struct mqtt_transport;
struct mqtt_transport *mqtt_transport_start(const struct app_config *c);
bool mqtt_transport_connected(struct mqtt_transport *m);
unsigned mqtt_transport_generation(struct mqtt_transport *m);
bool mqtt_transport_publish(struct mqtt_transport *m,const char *suffix,const char *payload,bool retain);
void mqtt_transport_stop(struct mqtt_transport *m);
#endif
