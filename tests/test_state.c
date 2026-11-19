#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ruuvi_state.h"
static GVariant *props(bool payload, unsigned company, int seq) {
    GVariantBuilder p, m;
    g_variant_builder_init(&p,G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&p,"{sv}","RSSI",g_variant_new_int16(-65));
    if (payload) {
        unsigned char b[24]={5,0x12,0x40,0x4f,0x9c,0xbc,0xe4}; b[17]=seq;
        g_variant_builder_init(&m,G_VARIANT_TYPE("a{qv}"));
        g_variant_builder_add(&m,"{qv}",company,g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,b,24,1));
        g_variant_builder_add(&p,"{sv}","ManufacturerData",g_variant_builder_end(&m));
    }
    return g_variant_ref_sink(g_variant_builder_end(&p));
}
int main(void) {
    struct ruuvi_state s={.rssi=32767};
    GVariant *p=props(true,0x0499,1);
    assert(ruuvi_update(&s,p,100)); g_variant_unref(p);
    assert(s.received && s.rssi==-65 && s.last_measurement==100);
    assert(ruuvi_fresh(&s,104,5) && !ruuvi_fresh(&s,105,5));
    p=props(false,0,0); assert(!ruuvi_update(&s,p,103)); g_variant_unref(p);
    assert(s.last_seen==100);
    p=props(true,0x1234,2); assert(!ruuvi_update(&s,p,104)); g_variant_unref(p);
    p=props(true,0x0499,1); assert(ruuvi_update(&s,p,104)); g_variant_unref(p);
    assert(s.last_seen==104 && s.last_measurement==100 && !ruuvi_fresh(&s,105,5));
    p=props(true,0x0499,2); assert(ruuvi_update(&s,p,106)); g_variant_unref(p);
    assert(ruuvi_fresh(&s,106,5));
    char *json=ruuvi_json(&s,"DE:4D:F8:3F:DA:91",106,5);
    assert(json && strstr(json,"23.360") && strstr(json,"\"Available\":true"));
    puts(json); g_free(json);
    puts("Live property events, duplicate sequence and freshness: OK");
}
