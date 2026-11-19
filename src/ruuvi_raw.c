#define _GNU_SOURCE
#include "ruuvi_raw.h"
#include "ruuvi_state.h"
#include "hci_adv.h"
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "mainloop.h"
#include "dlog.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct ruuvi_raw {
    int events, control, timer;
    bool scan_attempted;
    bdaddr_t target;
    char mac[18];
    struct app_config cfg;
    struct ruuvi_state state;
    ruuvi_raw_publish publish;
    void *data;
    double next_publish, next_health;
    bool availability_known, last_fresh;
    unsigned long reports, measurements, health_reports, health_measurements;
};
static double now(void) { return g_get_monotonic_time()/1000000.0; }
static void received(const uint8_t raw[24],int rssi,void *data) {
    struct ruuvi_raw *s=data;
    int previous=s->state.received?s->state.sample.sequence:-1;
    GVariantBuilder m,p;
    g_variant_builder_init(&m,G_VARIANT_TYPE("a{qv}"));
    g_variant_builder_add(&m,"{qv}",0x0499,g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,raw,24,1));
    g_variant_builder_init(&p,G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&p,"{sv}","ManufacturerData",g_variant_builder_end(&m));
    g_variant_builder_add(&p,"{sv}","RSSI",g_variant_new_int16(rssi));
    GVariant *properties=g_variant_ref_sink(g_variant_builder_end(&p));
    if (ruuvi_update(&s->state,properties,now())) {
        s->reports++;
        if (previous<0 || previous!=s->state.sample.sequence) s->measurements++;
    }
    g_variant_unref(properties);
}
static void input(int fd,uint32_t events,void *data) {
    struct ruuvi_raw *s=data;
    if (events&(EPOLLHUP|EPOLLERR)) {
        daemon_log(LOG_ERR,"Ruuvi HCI socket lost; exiting combined session");
        mainloop_exit_failure(); return;
    }
    /* Bound each dispatch so a busy radio cannot starve the DT24 ATT socket. */
    for (unsigned i=0;i<64;i++) {
        uint8_t packet[HCI_MAX_EVENT_SIZE];
        ssize_t n=recv(fd,packet,sizeof(packet),MSG_DONTWAIT);
        if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return;
        if (n<0 && errno==EINTR) continue;
        if (n<=0) { mainloop_exit_failure(); return; }
        hci_adv_parse(packet,(size_t)n,s->target.b,received,s);
    }
}
static void timer_destroy(void *data) { ((struct ruuvi_raw *)data)->timer=-1; }
static void tick(int id,void *data) {
    struct ruuvi_raw *s=data;
    double t=now();
    bool fresh=ruuvi_fresh(&s->state,t,s->cfg.stale_timeout);
    if (!s->availability_known || fresh!=s->last_fresh) {
        daemon_log(fresh?LOG_INFO:LOG_WARNING,"Ruuvi %s: %s",s->mac,fresh?"fresh data":"no fresh data");
        s->availability_known=true;s->last_fresh=fresh;
    }
    if (t>=s->next_health) {
        daemon_log(fresh?LOG_INFO:LOG_WARNING,
            "Ruuvi health: reports=%lu new_measurements=%lu measurement_age=%.1fs RSSI=%d fresh=%s",
            s->reports-s->health_reports,s->measurements-s->health_measurements,
            s->state.received?t-s->state.last_measurement:-1.0,
            s->state.rssi,fresh?"yes":"no");
        s->health_reports=s->reports;s->health_measurements=s->measurements;
        s->next_health=t+60;
    }
    char *json=NULL;
    if (fresh && t>=s->next_publish) {
        json=ruuvi_json(&s->state,s->mac,t,s->cfg.stale_timeout);
        s->next_publish=t+s->cfg.publish_interval;
    }
    s->publish(json,fresh,s->data);
    g_free(json);
    mainloop_modify_timeout(id,1000);
}
struct ruuvi_raw *ruuvi_raw_start(int adapter,const char *mac,const struct app_config *cfg,
                                 ruuvi_raw_publish publish,void *data) {
    struct ruuvi_raw *s=calloc(1,sizeof(*s));
    if (!s) return NULL;
    s->events=s->control=s->timer=-1;
    if (!mac || !cfg || !publish || str2ba(mac,&s->target)<0) goto fail;
    s->next_health=now()+60;
    s->cfg=*cfg;s->publish=publish;s->data=data;s->state.rssi=32767;
    g_strlcpy(s->mac,mac,sizeof(s->mac));
    s->control=hci_open_dev(adapter);
    s->events=hci_open_dev(adapter);
    if (s->control<0 || s->events<0) goto fail;
    fcntl(s->control,F_SETFD,FD_CLOEXEC);
    fcntl(s->events,F_SETFD,FD_CLOEXEC);
    struct hci_filter filter;
    hci_filter_clear(&filter);
    hci_filter_set_ptype(HCI_EVENT_PKT,&filter);
    hci_filter_set_event(EVT_LE_META_EVENT,&filter);
    if (setsockopt(s->events,SOL_HCI,HCI_FILTER,&filter,sizeof(filter))<0) goto fail;
    if (mainloop_add_fd(s->events,EPOLLIN|EPOLLERR|EPOLLHUP,input,s,NULL)<0) goto fail;
    /* No preemptive scan-disable: refuse an already scanning controller rather
     * than taking over another client's discovery. Command errors are fatal. */
    if (hci_le_set_scan_parameters(s->control,1,htobs(0x10),htobs(0x10),0,0,1000)<0) goto fail;
    s->scan_attempted=true;
    if (hci_le_set_scan_enable(s->control,1,0,1000)<0) goto fail;
    s->timer=mainloop_add_timeout(1000,tick,s,timer_destroy);
    if (s->timer<0) goto fail;
    daemon_log(LOG_INFO,"Ruuvi raw HCI scan hci%d: active, public, 10ms/10ms, duplicates off, MAC %s",adapter,mac);
    return s;
fail:
    daemon_log(LOG_ERR,"Ruuvi raw scan setup failed: %s",strerror(errno));
    ruuvi_raw_stop(s);return NULL;
}
void ruuvi_raw_stop(struct ruuvi_raw *s) {
    if (!s) return;
    if (s->timer>=0) mainloop_remove_timeout(s->timer);
    if (s->events>=0) { mainloop_remove_fd(s->events); close(s->events); }
    if (s->scan_attempted && s->control>=0 && hci_le_set_scan_enable(s->control,0,0,1000)<0)
        daemon_log(LOG_WARNING,"Ruuvi scan disable failed: %s",strerror(errno));
    if (s->control>=0) close(s->control);
    if (s->publish) s->publish(NULL,false,s->data);
    daemon_log(LOG_INFO,"Ruuvi session: %lu valid reports, %lu measurement changes",s->reports,s->measurements);
    free(s);
}
