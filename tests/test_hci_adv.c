#include "hci_adv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;
static void sample(const uint8_t *raw, int rssi, void *data) {
    (void)data;
    assert(raw[0]==5 && raw[16]==0x82 && raw[17]==0xc5);
    assert(rssi==-70);
    calls++;
}
int main(void) {
    const uint8_t mac[]={0x91,0xda,0x3f,0xf8,0x4d,0xde};
    /* Real RAWv2 sample captured from DE:4D:F8:3F:DA:91. */
    uint8_t packet[]={4,0x3e,43,2,1,2,1,0x91,0xda,0x3f,0xf8,0x4d,0xde,31,
      2,1,6,27,0xff,0x99,4,
      5,0x11,0x62,0x55,0x2e,0xbc,0x3f,3,0x90,0xfd,0xe8,0,0x20,0xc2,0xb6,0x88,0x82,0xc5,0xde,0x4d,0xf8,0x3f,0xda,0x91,0xba};
    assert(hci_adv_parse(packet,sizeof(packet),mac,sample,NULL)==1);
    assert(calls==1);
    for (size_t n=0;n<sizeof(packet);n++) {
        calls=0;
        assert(hci_adv_parse(packet,n,mac,sample,NULL)==-1);
        assert(calls==0);
    }
    uint8_t other[6]={0};
    assert(hci_adv_parse(packet,sizeof(packet),other,sample,NULL)==0);
    packet[5]=4; /* SCAN_RSP must never become a sensor sample. */
    assert(hci_adv_parse(packet,sizeof(packet),mac,sample,NULL)==0);
    packet[5]=2;
    packet[19]=0x98; /* wrong manufacturer */
    assert(hci_adv_parse(packet,sizeof(packet),mac,sample,NULL)==0);
    packet[19]=0x99;
    packet[17]=32; /* AD length beyond report */
    assert(hci_adv_parse(packet,sizeof(packet),mac,sample,NULL)==-1);
    packet[17]=27;
    uint8_t multi[87];
    memcpy(multi,packet,sizeof(packet));
    memcpy(multi+sizeof(packet),packet+5,sizeof(packet)-5);
    multi[2]=sizeof(multi)-3;multi[4]=2;
    calls=0;
    assert(hci_adv_parse(multi,sizeof(multi),mac,sample,NULL)==2 && calls==2);
    multi[sizeof(packet)+8]=32; calls=0;
    assert(hci_adv_parse(multi,sizeof(multi),mac,sample,NULL)==-1 && calls==0);
    puts("HCI advertising framing, filters, truncations and multi-report tests passed");
    return 0;
}
