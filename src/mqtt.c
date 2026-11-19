#define _POSIX_C_SOURCE 200809L
/* DT24 payload/aggregation; connection handling is shared with ruuvi-mqtt. */
#include "mqtt.h"
#include "mqtt_transport.h"
#include "dlog.h"
#include <math.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <time.h>

char *hostname="main-batt"; /* Legacy -H option, used by btgattclient.c. */
static struct mqtt_transport *transport, *ruuvi_transport;
static struct app_config cfg;
static double total_current,total_voltage;
static unsigned total_count;
static gint64 next_publish;
static unsigned ruuvi_generation;
static bool ruuvi_available;

bool mosq_init(const struct app_config *config) {
    cfg=*config;
    total_current=total_voltage=0;
    total_count=0; next_publish=0;
    ruuvi_generation=0; ruuvi_available=false;
    transport=mqtt_transport_start(&cfg);
    if (!transport) return false;
    if (cfg.ruuvi_topic_prefix[0]) {
        struct app_config sensor=cfg;
        g_strlcpy(sensor.topic_prefix,cfg.ruuvi_topic_prefix,sizeof(sensor.topic_prefix));
        if (g_strlcat(sensor.client_id,"-ruuvi",sizeof(sensor.client_id))>=sizeof(sensor.client_id)) {
            mosq_destroy();return false;
        }
        ruuvi_transport=mqtt_transport_start(&sensor);
        if (!ruuvi_transport) {mosq_destroy();return false;}
    }
    return true;
}
void mosq_destroy(void) {
    mqtt_transport_stop(ruuvi_transport); ruuvi_transport=NULL;
    mqtt_transport_stop(transport); transport=NULL;
}
bool mosq_connected(bool ruuvi) {
    return mqtt_transport_connected(ruuvi && ruuvi_transport?ruuvi_transport:transport);
}
void mosq_ruuvi(const char *json,bool available) {
    struct mqtt_transport *target=ruuvi_transport?ruuvi_transport:transport;
    if (!mqtt_transport_connected(target)) return;
    unsigned generation=mqtt_transport_generation(target);
    if (generation!=ruuvi_generation || available!=ruuvi_available) {
        if (mqtt_transport_publish(target,ruuvi_transport?"AVAILABILITY":"RUUVI/AVAILABILITY",available?"Online":"Offline",true)) {
            ruuvi_generation=generation; ruuvi_available=available;
        }
    }
    if (json) mqtt_transport_publish(target,ruuvi_transport?"SENSOR":"RUUVI/SENSOR",json,false);
}
void mosq_gather_data(double current,double voltage) {
    if (!isfinite(current) || !isfinite(voltage)) return;
    /* Do not blend measurements accumulated during a broker outage into recovery. */
    if (!mqtt_transport_connected(transport)) {
        total_current=total_voltage=0; total_count=0; next_publish=0; return;
    }
    total_current+=current; total_voltage+=voltage; total_count++;
    gint64 now=g_get_monotonic_time();
    if (now<next_publish) return;
    struct sysinfo info;
    if (sysinfo(&info)) return;
    current=total_current/total_count; voltage=total_voltage/total_count;
    char timestamp[32],path[128],payload[512];
    time_t stamp=time(NULL);
    struct tm tm_info;
    localtime_r(&stamp,&tm_info);
    strftime(timestamp,sizeof(timestamp),"%Y-%m-%dT%H:%M:%S",&tm_info);
    snprintf(path,sizeof(path),"/sys/class/thermal/thermal_zone%d/temp",cfg.thermal_zone);
    FILE *f=fopen(path,"r");
    int temperature=0;
    if (f) { if (fscanf(f,"%d",&temperature)!=1) temperature=0; fclose(f); }
    snprintf(payload,sizeof(payload),
        "{\"Time\":\"%s\", \"Uptime\": %ld, \"LoadAverage\":%.2f, \"CPUTemp\":%d, \"Current\":%.2f, \"Voltage\":%.2f, \"Power\":%.2f}",
        timestamp,info.uptime/3600,info.loads[0]/65536.0,temperature/1000,current,voltage,current*voltage);
    if (mqtt_transport_publish(transport,"STATE",payload,false)) {
        daemon_log(LOG_DEBUG,"%s/STATE %s",cfg.topic_prefix,payload);
        next_publish=now+(gint64)cfg.publish_interval*G_USEC_PER_SEC;
        total_current=total_voltage=0; total_count=0;
    }
}
