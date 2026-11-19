#define _POSIX_C_SOURCE 200809L
#include "mqtt.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
int main(int argc,char **argv) {
    assert(argc>=2);
    struct app_config c;app_config_defaults(&c);c.port=atoi(argv[1]);
    strcpy(c.client_id,"dual-test");strcpy(c.topic_prefix,"test/dt24");
    strcpy(c.ruuvi_mac,"DE:4D:F8:3F:DA:91");strcpy(c.ruuvi_topic_prefix,"test/ruuvi1");
    assert(mosq_init(&c));
    struct timespec delay={1,0};nanosleep(&delay,NULL);
    mosq_gather_data(2.5,12.0);mosq_ruuvi("{\"MeasurementSequence\":123}",true);
    nanosleep(&delay,NULL);
    if(argc>2) {puts("ready");fflush(stdout);for(;;)pause();}
    mosq_ruuvi(NULL,false);mosq_destroy();puts("Dual MQTT clients stopped");
}
