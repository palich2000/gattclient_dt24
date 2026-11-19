#include "ruuvi_state.h"
#include <math.h>
#include <string.h>

bool ruuvi_update(struct ruuvi_state *s, GVariant *properties, double now) {
    gint16 rssi;
    if (g_variant_lookup(properties,"RSSI","n",&rssi)) s->rssi=rssi;
    GVariant *m=g_variant_lookup_value(properties,"ManufacturerData",G_VARIANT_TYPE("a{qv}"));
    if (!m) return false;
    GVariantIter it;
    guint16 company;
    GVariant *v;
    bool valid=false;
    g_variant_iter_init(&it,m);
    while (g_variant_iter_next(&it,"{qv}",&company,&v)) {
        if (company==0x0499 && g_variant_is_of_type(v,G_VARIANT_TYPE("ay"))) {
            gsize len;
            const uint8_t *bytes=g_variant_get_fixed_array(v,&len,1);
            struct ruuvi_sample sample;
            if (ruuvi_decode(bytes,len,&sample)) {
                bool changed=!s->received || sample.sequence < 0 || sample.sequence!=s->sample.sequence;
                s->sample=sample;
                s->received=true;
                s->last_seen=now;
                if (changed) {
                    s->last_measurement=now;
                    GDateTime *dt=g_date_time_new_now_utc();
                    char *iso=g_date_time_format(dt,"%Y-%m-%dT%H:%M:%SZ");
                    g_strlcpy(s->timestamp,iso,sizeof(s->timestamp));
                    g_free(iso); g_date_time_unref(dt);
                }
                valid=true;
            }
        }
        g_variant_unref(v);
    }
    g_variant_unref(m);
    return valid;
}
bool ruuvi_fresh(const struct ruuvi_state *s, double now, int timeout) {
    return s->received && now-s->last_seen < timeout && now-s->last_measurement < timeout;
}
static void number(GString *out,const char *key,double value) {
    char buf[G_ASCII_DTOSTR_BUF_SIZE];
    if (isfinite(value)) g_ascii_formatd(buf,sizeof(buf),"%.3f",value);
    else g_strlcpy(buf,"null",sizeof(buf));
    g_string_append_printf(out,",\"%s\":%s",key,buf);
}
char *ruuvi_json(const struct ruuvi_state *s, const char *mac, double now, int timeout) {
    GString *out=g_string_new(NULL);
    /* mac is validated by configuration, timestamp is generated internally. */
    g_string_append_printf(out,"{\"Time\":\"%s\",\"MAC\":\"%s\",\"Available\":%s",
        s->timestamp,mac,ruuvi_fresh(s,now,timeout)?"true":"false");
    number(out,"Temperature",s->sample.temperature);
    number(out,"Humidity",s->sample.humidity);
    number(out,"Pressure",s->sample.pressure);
    number(out,"BatteryVoltage",s->sample.battery);
    number(out,"AccelerationX",s->sample.acceleration[0]);
    number(out,"AccelerationY",s->sample.acceleration[1]);
    number(out,"AccelerationZ",s->sample.acceleration[2]);
    number(out,"TxPower",s->sample.tx_power);
    number(out,"RSSI",s->rssi==32767?NAN:s->rssi);
    number(out,"MovementCounter",s->sample.movement < 0?NAN:s->sample.movement);
    number(out,"MeasurementSequence",s->sample.sequence < 0?NAN:s->sample.sequence);
    number(out,"AgeSeconds",MAX(0,now-s->last_seen));
    number(out,"MeasurementAgeSeconds",MAX(0,now-s->last_measurement));
    g_string_append_c(out,'}');
    return g_string_free(out,FALSE);
}
