#define _POSIX_C_SOURCE 200809L
#include "mqtt_transport.h"
#include "dlog.h"
#include <mosquitto.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* libmosquitto is process-global; keep it alive until the last client stops. */
static pthread_mutex_t library_lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned library_users;
static bool library_acquire(void) {
    pthread_mutex_lock(&library_lock);
    bool ok=library_users || mosquitto_lib_init()==MOSQ_ERR_SUCCESS;
    if (ok) library_users++;
    pthread_mutex_unlock(&library_lock);
    return ok;
}
static void library_release(void) {
    pthread_mutex_lock(&library_lock);
    if (--library_users==0) mosquitto_lib_cleanup();
    pthread_mutex_unlock(&library_lock);
}
struct mqtt_transport {
    struct mosquitto *mosq;
    char prefix[256];
    atomic_bool connected;
    atomic_uint generation;
    pthread_mutex_t lock;
    pthread_cond_t ack;
    int shutdown_mid;
    bool shutdown_acked;
};
static int send_message(struct mqtt_transport *m,const char *suffix,const char *payload,int qos,bool retain,int *mid) {
    char *topic=g_strdup_printf("%s/%s",m->prefix,suffix);
    int rc=mosquitto_publish(m->mosq,mid,topic,(int)strlen(payload),payload,qos,retain);
    g_free(topic);
    return rc;
}
static void connected(struct mosquitto *mosq,void *data,int rc) {
    (void)mosq;
    struct mqtt_transport *m=data;
    if (rc) { daemon_log(LOG_WARNING,"MQTT connection refused (%d)",rc); return; }
    send_message(m,"LWT","Online",1,true,NULL);
    atomic_fetch_add(&m->generation,1);
    atomic_store(&m->connected,true);
    daemon_log(LOG_INFO,"MQTT connected: %s",m->prefix);
}
static void disconnected(struct mosquitto *mosq,void *data,int rc) {
    (void)mosq;
    struct mqtt_transport *m=data;
    atomic_store(&m->connected,false);
    if (rc) daemon_log(LOG_WARNING,"MQTT disconnected: %s (%d), reconnecting",m->prefix,rc);
}
static void published(struct mosquitto *mosq,void *data,int mid) {
    (void)mosq;
    struct mqtt_transport *m=data;
    pthread_mutex_lock(&m->lock);
    if (mid==m->shutdown_mid) {
        m->shutdown_acked=true;
        pthread_cond_signal(&m->ack);
    }
    pthread_mutex_unlock(&m->lock);
}
struct mqtt_transport *mqtt_transport_start(const struct app_config *c) {
    if (!library_acquire()) return NULL;
    struct mqtt_transport *m=calloc(1,sizeof(*m));
    if (!m) { library_release(); return NULL; }
    pthread_mutex_init(&m->lock,NULL);
    pthread_cond_init(&m->ack,NULL);
    atomic_init(&m->connected,false);
    atomic_init(&m->generation,0);
    g_strlcpy(m->prefix,c->topic_prefix,sizeof(m->prefix));
    m->mosq=mosquitto_new(c->client_id,true,m);
    int rc=MOSQ_ERR_NOMEM;
    if (!m->mosq) goto fail;
    mosquitto_connect_callback_set(m->mosq,connected);
    mosquitto_disconnect_callback_set(m->mosq,disconnected);
    mosquitto_publish_callback_set(m->mosq,published);
    mosquitto_reconnect_delay_set(m->mosq,1,30,true);
    if (c->username[0]) {
        rc=mosquitto_username_pw_set(m->mosq,c->username,c->password);
        if (rc) goto fail;
    }
    if (c->tls) {
        rc=mosquitto_tls_set(m->mosq,c->ca_file,NULL,
            c->cert_file[0]?c->cert_file:NULL,c->key_file[0]?c->key_file:NULL,NULL);
        if (rc) goto fail;
        rc=mosquitto_tls_opts_set(m->mosq,1,"tlsv1.2",NULL);
        if (rc) goto fail;
        rc=mosquitto_tls_insecure_set(m->mosq,false);
        if (rc) goto fail;
    }
    char *topic=g_strdup_printf("%s/LWT",m->prefix);
    rc=mosquitto_will_set(m->mosq,topic,7,"Offline",1,true);
    g_free(topic);
    if (rc) goto fail;
    rc=mosquitto_connect_async(m->mosq,c->host,c->port,c->keepalive);
    if (rc) goto fail;
    rc=mosquitto_loop_start(m->mosq);
    if (rc) goto fail;
    return m;
fail:
    daemon_log(LOG_ERR,"MQTT setup failed: %s",mosquitto_strerror(rc));
    if (m->mosq) mosquitto_destroy(m->mosq);
    pthread_cond_destroy(&m->ack); pthread_mutex_destroy(&m->lock);
    free(m); library_release(); return NULL;
}
bool mqtt_transport_connected(struct mqtt_transport *m) {
    return m && atomic_load(&m->connected);
}
unsigned mqtt_transport_generation(struct mqtt_transport *m) {
    return m ? atomic_load(&m->generation) : 0;
}
bool mqtt_transport_publish(struct mqtt_transport *m,const char *suffix,const char *payload,bool retain) {
    /* Do not queue stale sensor values while disconnected. */
    if (!mqtt_transport_connected(m)) return false;
    int rc=send_message(m,suffix,payload,0,retain,NULL);
    if (rc) daemon_log(LOG_WARNING,"MQTT publish failed: %s",mosquitto_strerror(rc));
    return rc==MOSQ_ERR_SUCCESS;
}
void mqtt_transport_stop(struct mqtt_transport *m) {
    if (!m) return;
    if (mqtt_transport_connected(m)) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME,&deadline); deadline.tv_sec+=2;
        pthread_mutex_lock(&m->lock);
        if (!send_message(m,"LWT","Offline",1,true,&m->shutdown_mid)) {
            while (!m->shutdown_acked)
                if (pthread_cond_timedwait(&m->ack,&m->lock,&deadline)==ETIMEDOUT) break;
        }
        pthread_mutex_unlock(&m->lock);
    }
    mosquitto_disconnect(m->mosq);
    mosquitto_loop_stop(m->mosq,true);
    mosquitto_destroy(m->mosq);
    pthread_cond_destroy(&m->ack); pthread_mutex_destroy(&m->lock);
    free(m); library_release();
}
