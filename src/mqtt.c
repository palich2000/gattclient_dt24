/**
* @file mqtt.c
* @author palich (y.palich.t@gmail.com)
*
* @brief
*
*/
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <mosquitto.h>
#include <pthread.h>
#include <alloca.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>

#include "mqtt.h"
#include "dlog.h"
#include "dfork.h"
#include "dmem.h"

#define HOSTNAME_SIZE 256

#define FD_SYSTEM_TEMP_TMPL  "/sys/class/thermal/thermal_zone%d/temp"

#define MQTT_LWT_TOPIC "tele/%s/LWT"
#define MQTT_SENSOR_TOPIC "tele/%s/SENSOR"
#define MQTT_STATE_TOPIC "tele/%s/STATE"
#define ONLINE "Online"
#define OFFLINE "Offline"

#define STATE_PUBLISH_INTERVAL 60000   // 60 sec

typedef struct _client_info_t {
    struct mosquitto * m;
    bool do_exit;
} t_client_info;

char * mqtt_host = "mosquitto";
char * mqtt_username = "owntracks";
char * mqtt_password = "zhopa";
int mqtt_port = 8883;
int mqtt_keepalive = 60;

static struct mosquitto * mosq = NULL;
static pthread_t mosq_th = 0;
static char * hostname = "main-batt";
static t_client_info client_info = {0};
static int thermal_zone = 0;

uint64_t timeMillis(void) {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000UL + time.tv_usec / 1000UL;
}

static const char * create_topic(const char * template) {
    static __thread char buf[255] = {0};
    snprintf(buf, sizeof(buf) - 1, template, hostname);
    return buf;
}

static void mqtt_publish_lwt(bool online) {
    const char * msg = online ? ONLINE : OFFLINE;
    int res;
    const char * topic = create_topic(MQTT_LWT_TOPIC);
    daemon_log(LOG_INFO, "publish %s: %s", topic, msg);
    if ((res = mosquitto_publish(mosq, NULL, topic, (int) strlen(msg), msg, 0, true)) != 0) {
        DLOG_ERR("Can't publish to Mosquitto server %s", mosquitto_strerror(res));
    }
}

static void on_log(struct mosquitto * UNUSED(mosq),
                   void * UNUSED(userdata),
                   int level,
                   const char * str
                  ) {
    switch (level) {
//    case MOSQ_LOG_DEBUG:
//    case MOSQ_LOG_INFO:
//    case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR: {
        daemon_log(LOG_ERR,
                   "%i:%s", level, str);
    }
    }
}

static void mosq_sleep(t_client_info * info, int secs) {
    int s = secs;
    while (s > 0 && (info == NULL || (info != NULL && info->do_exit == false))) {
        sleep(1);
        s--;
    }
}

static bool mosq_publish_state(double current, double voltage) {

    static uint64_t timer_publish_state = 0;

    if (timer_publish_state > timeMillis()) {
        return false;
    } else {
        timer_publish_state = timeMillis() + STATE_PUBLISH_INTERVAL;
    }

    time_t timer;
    char tm_buffer[26] = {};
    char buf[255] = {};
    struct tm * tm_info;
    struct sysinfo info;
    int res;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(tm_buffer, 26, "%Y-%m-%dT%H:%M:%S", tm_info);

    if (!sysinfo(&info)) {
        int fd;
        char tmp_buf[20];
        memset(tmp_buf, ' ', sizeof(tmp_buf));
        char * f_name = NULL;
        asprintf(&f_name, FD_SYSTEM_TEMP_TMPL, thermal_zone);
        if ((fd = open(f_name, O_RDONLY)) < 0) {
            daemon_log(LOG_ERR, "%s : file open error!", __func__);
        } else {
            read(fd, buf, sizeof(tmp_buf));
            close(fd);
        }
        FREE(f_name);
        int temp_C = atoi(buf) / 1000;
        const char * topic = create_topic(MQTT_STATE_TOPIC);
        if (isnan(voltage) || isnan(current)) {
            snprintf(buf, sizeof(buf) - 1,
                     "{\"Time\":\"%s\", \"Uptime\": %ld, \"LoadAverage\":%.2f, \"CPUTemp\":%d}",
                     tm_buffer, info.uptime / 3600, info.loads[0] / 65536.0, temp_C);
        } else {
            snprintf(buf, sizeof(buf) - 1,
                     "{\"Time\":\"%s\", \"Uptime\": %ld, \"LoadAverage\":%.2f, \"CPUTemp\":%d, \"Current\":%0.2f, \"Voltage\":%0.2f, \"Power\":%0.2f}",
                     tm_buffer, info.uptime / 3600, info.loads[0] / 65536.0, temp_C, current, voltage,
                     current * voltage);
        }
        daemon_log(LOG_INFO, "%s %s", topic, buf);

        if ((res = mosquitto_publish(mosq, NULL, topic, (int) strlen(buf), buf, 0, false)) != 0) {
            daemon_log(LOG_ERR, "Can't publish to Mosquitto server %s", mosquitto_strerror(res));
        }
    }
    return true;
}

static
void on_connect(struct mosquitto * m, void * udata, int res) {
    t_client_info * info = (t_client_info *) udata;
    daemon_log(LOG_INFO, "%s", __FUNCTION__);
    switch (res) {
    case 0:
        mosquitto_subscribe(m, NULL, "stat/+/POWER", 0);
        mqtt_publish_lwt(true);
        mosq_publish_state(NAN, NAN);
        break;
    case 1:
        DLOG_ERR("Connection refused (unacceptable protocol version).");
        break;
    case 2:
        DLOG_ERR("Connection refused (identifier rejected).");
        break;
    case 3:
        DLOG_ERR("Connection refused (broker unavailable).");
        break;
    default:
        DLOG_ERR("Unknown connection error. (%d)", res);
        break;
    }
    if (res != 0) {
        mosq_sleep(info, 10);
    }
}

static
void on_publish(struct mosquitto * UNUSED(m), void * UNUSED(udata), int UNUSED(m_id)) {
    //daemon_log(LOG_ERR, "-- published successfully");
}

static
void on_subscribe(struct mosquitto * UNUSED(m), void * UNUSED(udata), int UNUSED(mid),
                  int UNUSED(qos_count), const int * UNUSED(granted_qos)) {
    daemon_log(LOG_INFO, "-- subscribed successfully");
}

static
void on_message(struct mosquitto * UNUSED(m), void * UNUSED(udata),
                const struct mosquitto_message * msg) {

    if (msg == NULL) {
        return;
    }

    daemon_log(LOG_INFO, "-- got message @ %s: (%d, QoS %d, %s) '%s'",
               msg->topic, msg->payloadlen, msg->qos, msg->retain ? "R" : "!r",
               (char *) msg->payload);
}

static
void * mosq_thread_loop(void * p) {
    t_client_info * info = (t_client_info *) p;
    daemon_log(LOG_INFO, "%s", __FUNCTION__);
    while (!info->do_exit) {
        int res = mosquitto_loop(info->m, 1000, 1);
        switch (res) {
        case MOSQ_ERR_SUCCESS:
            break;
        case MOSQ_ERR_NO_CONN: {
            int res = mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive);
            if (res) {
                daemon_log(LOG_ERR, "Can't connect to Mosquitto server %s", mosquitto_strerror(res));
                sleep(30);
            }
            break;
        }
        case MOSQ_ERR_INVAL:
        case MOSQ_ERR_NOMEM:
        case MOSQ_ERR_CONN_LOST:
        case MOSQ_ERR_PROTOCOL:
        case MOSQ_ERR_ERRNO:
            daemon_log(LOG_ERR, "%s %s %s", __FUNCTION__, strerror(errno), mosquitto_strerror(res));
            mosquitto_disconnect(mosq);
            daemon_log(LOG_ERR, "%s disconnected", __FUNCTION__);
            sleep(10);
            daemon_log(LOG_ERR, "%s Try to reconnect", __FUNCTION__);
            int res = mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive);
            if (res) {
                daemon_log(LOG_ERR, "%s Can't connect to Mosquitto server %s", __FUNCTION__,
                           mosquitto_strerror(res));
            } else {
                daemon_log(LOG_ERR, "%s Connected", __FUNCTION__);
            }

            break;
        default:
            daemon_log(LOG_ERR, "%s unkown error (%d) from mosquitto_loop", __FUNCTION__, res);
            break;
        }
    }
    daemon_log(LOG_INFO, "%s finished", __FUNCTION__);
    pthread_exit(NULL);
}

void mosq_init(const char * progname) {

//    hostname = calloc(1, HOSTNAME_SIZE);
//    gethostname(hostname, HOSTNAME_SIZE - 1);

    bool clean_session = true;

    mosquitto_lib_init();
    char * tmp = alloca(strlen(progname) + strlen(hostname) + 2);
    strcpy(tmp, progname);
    strcat(tmp, "@");
    strcat(tmp, hostname);
    mosq = mosquitto_new(tmp, clean_session, &client_info);
    if (!mosq) {
        daemon_log(LOG_ERR, "mosq Error: Out of memory.");
    } else {
        client_info.m = mosq;
        client_info.do_exit = false;
        mosquitto_log_callback_set(mosq, on_log);

        mosquitto_connect_callback_set(mosq, on_connect);
        mosquitto_publish_callback_set(mosq, on_publish);
        mosquitto_subscribe_callback_set(mosq, on_subscribe);
        mosquitto_message_callback_set(mosq, on_message);

        mosquitto_username_pw_set(mosq, mqtt_username, mqtt_password);
        mosquitto_will_set(mosq, create_topic(MQTT_LWT_TOPIC), strlen(OFFLINE), OFFLINE, 0, true);
        daemon_log(LOG_INFO, "Try connect to Mosquitto server as %s", tmp);
        int res = mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive);
        if (res) {
            daemon_log(LOG_ERR, "Can't connect to Mosquitto server %s", mosquitto_strerror(res));
        }
        pthread_create(&mosq_th, NULL, mosq_thread_loop, &client_info);
    }

}

void mosq_destroy(void) {
    mqtt_publish_lwt(false);
    client_info.do_exit = true;
    pthread_join(mosq_th, NULL);
    if (mosq) {
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
    }
    mosquitto_lib_cleanup();
}

void mosq_gather_data(double current, double voltage) {
    static double total_current = 0.0f;
    static double total_voltage = 0.0f;
    static int total_count = 0.0f;
    total_current += current;
    total_voltage += voltage;
    total_count++;
    if (mosq_publish_state(total_current / total_count, total_voltage / total_count)) {
        total_current = 0;
        total_voltage = 0;
        total_count = 0;
    }
}