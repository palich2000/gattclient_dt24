#include "hci_adv.h"
#include <string.h>
int hci_adv_parse(const uint8_t *p, size_t n, const uint8_t target[6],
                  hci_adv_callback cb, void *data) {
    if (!p || !target || !cb || n<3) return -1;
    if (p[0]!=4 || p[1]!=0x3e) return 0;
    if (n!=(size_t)p[2]+3 || n<5) return -1;
    if (p[3]!=2) return 0;
    size_t pos=5;
    /* Validate complete framing before dispatching any callbacks. */
    for (unsigned i=0;i<p[4];i++) {
        if (n-pos<10) return -1;
        size_t length=p[pos+8];
        if (length>31 || length+10>n-pos) return -1;
        size_t ad=pos+9, end=ad+length;
        while (ad<end && p[ad]) {
            if ((size_t)p[ad]+1>end-ad) return -1;
            ad+=(size_t)p[ad]+1;
        }
        pos+=length+10;
    }
    if (pos!=n) return -1;
    int count=0;
    pos=5;
    for (unsigned i=0;i<p[4];i++) {
        size_t length=p[pos+8], ad=pos+9, end=ad+length;
        if ((p[pos]==0 || p[pos]==2 || p[pos]==3) && p[pos+1]<=1 &&
            !memcmp(p+pos+2,target,6)) {
            while (ad<end && p[ad]) {
                if (p[ad]==27 && p[ad+1]==0xff && p[ad+2]==0x99 &&
                    p[ad+3]==4 && p[ad+4]==5) {
                    cb(p+ad+4,p[end]==127?32767:(int)(int8_t)p[end],data);
                    count++;
                }
                ad+=(size_t)p[ad]+1;
            }
        }
        pos+=length+10;
    }
    return count;
}
