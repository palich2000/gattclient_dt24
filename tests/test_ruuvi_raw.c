#define _GNU_SOURCE
#include "ruuvi_raw.h"
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "mainloop.h"
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
static int sockets[2], opened, enabled, disabled, fail_enable;
int __wrap_hci_open_dev(int id) { assert(id==0); opened++; return dup(sockets[0]); }
int __wrap_setsockopt(int fd,int level,int option,const void *value,socklen_t len) {
    (void)fd;(void)value;(void)len; assert(level==SOL_HCI && option==HCI_FILTER); return 0;
}
int __wrap_hci_le_set_scan_parameters(int fd,uint8_t type,uint16_t interval,uint16_t window,uint8_t own,uint8_t policy,int timeout) {
    (void)fd; assert(type==1 && interval==0x10 && window==0x10 && own==0 && policy==0 && timeout>0);return 0;
}
int __wrap_hci_le_set_scan_enable(int fd,uint8_t enable,uint8_t duplicates,int timeout) {
    (void)fd;(void)timeout; assert(duplicates==0);
    if(enable) {enabled++;if(fail_enable){errno=EIO;return -1;}} else disabled++;
    return 0;
}
/* Advance monotonic time without waiting a full health interval. */
static gint64 time_offset;
gint64 __real_g_get_monotonic_time(void);
gint64 __wrap_g_get_monotonic_time(void) {return __real_g_get_monotonic_time()+time_offset;}
static unsigned delivered;
static void publish(const char *json,bool fresh,void *data) {
    (void)data;
    if (json) {
        assert(fresh && strstr(json,"DE:4D:F8:3F:DA:91") && strstr(json,"\"MeasurementSequence\":33477.000"));
        delivered++; mainloop_exit_success();
    }
}
static void watchdog(int id,void *data) {(void)id;(void)data;assert(!"epoll callback timed out");}
int main(void) {
    assert(socketpair(AF_UNIX,SOCK_DGRAM,0,sockets)==0);
    struct app_config c;app_config_defaults(&c);
    mainloop_init();
    struct ruuvi_raw *r=ruuvi_raw_start(0,"DE:4D:F8:3F:DA:91",&c,publish,NULL);
    assert(r && opened==2 && enabled==1 && disabled==0);
    const uint8_t packet[]={4,0x3e,43,2,1,2,1,0x91,0xda,0x3f,0xf8,0x4d,0xde,31,
      2,1,6,27,0xff,0x99,4,
      5,0x11,0x62,0x55,0x2e,0xbc,0x3f,3,0x90,0xfd,0xe8,0,0x20,0xc2,0xb6,0x88,0x82,0xc5,0xde,0x4d,0xf8,0x3f,0xda,0x91,0xba};
    time_offset=61*G_USEC_PER_SEC;
    assert(write(sockets[1],packet,sizeof(packet))==(ssize_t)sizeof(packet));
    mainloop_add_timeout(3000,watchdog,NULL,NULL);
    assert(mainloop_run()==0 && delivered==1);
    ruuvi_raw_stop(r);
    assert(disabled==1);
    fail_enable=1;
    mainloop_init();
    r=ruuvi_raw_start(0,"DE:4D:F8:3F:DA:91",&c,publish,NULL);
    assert(!r && disabled==2); /* attempt cleanup even if enable acknowledgement failed */
    mainloop_quit();mainloop_run();
    close(sockets[0]);close(sockets[1]);
    puts("Raw scanner lifecycle passed");
}
