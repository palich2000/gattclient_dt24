#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mqtt.h"
int main(int argc,char **argv) {
    assert(argc==2);
    struct app_config c; app_config_defaults(&c);
    c.port=atoi(argv[1]);
    snprintf(c.client_id,sizeof(c.client_id),"dt24-test");
    snprintf(c.topic_prefix,sizeof(c.topic_prefix),"test/transport");
    assert(mosq_init(&c));
    struct timespec delay={1,0}; nanosleep(&delay,NULL);
    mosq_gather_data(2.5,12.0);
    mosq_destroy();
    puts("DT24 MQTT wrapper: OK");
}
