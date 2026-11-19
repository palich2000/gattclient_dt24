#include "mqtt.h"
#include "mqtt_transport.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned messages;
struct mqtt_transport *mqtt_transport_start(const struct app_config *c) {(void)c;return (struct mqtt_transport *)1;}
void mqtt_transport_stop(struct mqtt_transport *m) {(void)m;}
bool mqtt_transport_connected(struct mqtt_transport *m) {return m!=NULL;}
unsigned mqtt_transport_generation(struct mqtt_transport *m) {return m?1:0;}
bool mqtt_transport_publish(struct mqtt_transport *m,const char *suffix,const char *payload,bool retain) {
    assert(m);
    if(messages==0) assert(!strcmp(suffix,"RUUVI/AVAILABILITY") && !strcmp(payload,"Online") && retain);
    if(messages==1) assert(!strcmp(suffix,"RUUVI/SENSOR") && !strcmp(payload,"{}") && !retain);
    if(messages==2) assert(!strcmp(suffix,"RUUVI/AVAILABILITY") && !strcmp(payload,"Offline") && retain);
    messages++;return true;
}
int main(void) {
    struct app_config c;app_config_defaults(&c);assert(mosq_init(&c));
    mosq_ruuvi("{}",true);mosq_ruuvi(NULL,true);mosq_ruuvi(NULL,false);
    assert(messages==3);mosq_destroy();puts("Shared MQTT Ruuvi topics passed");
}
