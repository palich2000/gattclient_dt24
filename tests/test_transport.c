#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mqtt_transport.h"
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);
    struct app_config c; app_config_defaults(&c);
    c.port=atoi(argv[1]);
    if (argc==3) { c.tls=true; snprintf(c.ca_file,sizeof(c.ca_file),"%s",argv[2]); }
    snprintf(c.client_id,sizeof(c.client_id),"transport-test");
    snprintf(c.topic_prefix,sizeof(c.topic_prefix),"test/transport");
    struct mqtt_transport *m=mqtt_transport_start(&c);
    assert(m);
    struct timespec delay={0,100000000};
    for (int i=0;i<50 && !mqtt_transport_connected(m);i++) nanosleep(&delay,NULL);
    assert(mqtt_transport_connected(m));
    assert(mqtt_transport_generation(m)>0);
    assert(mqtt_transport_publish(m,"SENSOR","{\"test\":true}",false));
    mqtt_transport_stop(m);
    puts("MQTT connect/publish/graceful shutdown: OK");
}
