#include "app_config.h"
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

enum field_type { STRING, INTEGER, BOOLEAN };
struct field { const char *group, *key; size_t offset, size; enum field_type type; };
#define FIELD(g,k,m,t) {g,k,offsetof(struct app_config,m),sizeof(((struct app_config *)0)->m),t}
static const struct field fields[] = {
    FIELD("bluetooth","adapter",adapter,STRING), FIELD("bluetooth","mac",mac,STRING),
    FIELD("bluetooth","address_type",address_type,STRING), FIELD("bluetooth","mtu",mtu,INTEGER),
    FIELD("bluetooth","security",security,STRING),
    FIELD("mqtt","enabled",mqtt_enabled,BOOLEAN), FIELD("mqtt","host",host,STRING),
    FIELD("mqtt","port",port,INTEGER), FIELD("mqtt","client_id",client_id,STRING),
    FIELD("mqtt","username",username,STRING), FIELD("mqtt","password",password,STRING),
    FIELD("mqtt","topic_prefix",topic_prefix,STRING), FIELD("mqtt","keepalive",keepalive,INTEGER),
    FIELD("mqtt","tls",tls,BOOLEAN), FIELD("mqtt","ca_file",ca_file,STRING),
    FIELD("mqtt","cert_file",cert_file,STRING), FIELD("mqtt","key_file",key_file,STRING),
    FIELD("ruuvi","mac",ruuvi_mac,STRING), FIELD("ruuvi","topic_prefix",ruuvi_topic_prefix,STRING),
    FIELD("app","console_json",console_json,BOOLEAN),
    FIELD("app","publish_interval",publish_interval,INTEGER), FIELD("app","stale_timeout",stale_timeout,INTEGER),
    FIELD("app","log_level",log_level,INTEGER), FIELD("app","thermal_zone",thermal_zone,INTEGER)
};
static bool invalid(GError **error, const char *message) {
    g_set_error_literal(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE, message);
    return false;
}
void app_config_defaults(struct app_config *c) {
    *c = (struct app_config){.port=1883, .keepalive=60, .publish_interval=10,
        .stale_timeout=120, .log_level=6, .mqtt_enabled=true, .console_json=true};
    g_strlcpy(c->adapter,"hci0",sizeof(c->adapter));
    g_strlcpy(c->host,"localhost",sizeof(c->host));
    g_strlcpy(c->address_type,"public",sizeof(c->address_type));
    g_strlcpy(c->security,"low",sizeof(c->security));
}
bool app_config_load(struct app_config *c, const char *path, GError **error) {
    GKeyFile *kf = g_key_file_new();
    GError *load_error=NULL;
    if (!g_key_file_load_from_file(kf,path,G_KEY_FILE_NONE,&load_error)) {
        g_clear_error(&load_error);
        g_key_file_unref(kf);
        return invalid(error,"Cannot read configuration or invalid INI syntax");
    }
    struct app_config next = *c;
    bool ok = true;
    gsize ng;
    gchar **groups = g_key_file_get_groups(kf,&ng);
    for (gsize i=0; i<ng && ok; i++) {
        gsize nk;
        gchar **keys = g_key_file_get_keys(kf,groups[i],&nk,NULL);
        for (gsize j=0; j<nk && ok; j++) {
            const struct field *f = NULL;
            for (size_t n=0; n<G_N_ELEMENTS(fields); n++)
                if (!strcmp(groups[i],fields[n].group) && !strcmp(keys[j],fields[n].key)) f=&fields[n];
            if (!f) { ok=invalid(error,"Unknown configuration section/key"); break; }
            void *dst = (char *)&next + f->offset;
            GError *e = NULL;
            if (f->type == STRING) {
                char *s = g_key_file_get_string(kf,f->group,f->key,&e);
                if (s && strlen(s) >= f->size) ok=invalid(error,"Configuration string too long");
                else if (s) g_strlcpy(dst,s,f->size);
                g_free(s);
            } else if (f->type == INTEGER) {
                *(int *)dst = g_key_file_get_integer(kf,f->group,f->key,&e);
            } else {
                *(bool *)dst = g_key_file_get_boolean(kf,f->group,f->key,&e);
            }
            if (e) {
                /* Do not include values in diagnostics (may contain credentials). */
                g_error_free(e); ok=invalid(error,"Invalid configuration value type");
            }
        }
        g_strfreev(keys);
    }
    g_strfreev(groups);
    g_key_file_unref(kf);
    if (ok && next.password[0]) {
        struct stat st;
        if (stat(path,&st) || !S_ISREG(st.st_mode) || (st.st_mode & 077))
            ok=invalid(error,"Configuration with password must have mode 0600 (or stricter)");
    }
    if (ok) *c = next;
    return ok;
}
bool app_config_validate(const struct app_config *c, GError **error) {
    if (!g_regex_match_simple("^hci[0-9]+$",c->adapter,0,0)) return invalid(error,"Invalid adapter");
    if (!g_regex_match_simple("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$",c->mac,0,0)) return invalid(error,"Invalid MAC address");
    if (strcmp(c->address_type,"public") && strcmp(c->address_type,"random")) return invalid(error,"Invalid address_type");
    if (strcmp(c->security,"low") && strcmp(c->security,"medium") && strcmp(c->security,"high")) return invalid(error,"Invalid security");
    if (c->mtu < 0 || c->mtu > 65535 || (c->mtu && c->mtu < 23)) return invalid(error,"Invalid MTU");
    if (c->port < 1 || c->port > 65535 || c->keepalive < 5 || c->keepalive > 65535) return invalid(error,"Invalid MQTT port/keepalive");
    if (c->publish_interval < 1 || c->publish_interval > 86400 || c->stale_timeout < c->publish_interval || c->stale_timeout > 604800) return invalid(error,"Invalid publish_interval/stale_timeout");
    if (c->log_level < 0 || c->log_level > 7 || c->thermal_zone < 0) return invalid(error,"Invalid log_level/thermal_zone");
    if (!c->host[0] || !c->client_id[0] || !c->topic_prefix[0]) return invalid(error,"host, client_id and topic_prefix are required");
    if (strpbrk(c->topic_prefix,"+#\r\n") || c->topic_prefix[strlen(c->topic_prefix)-1]=='/') return invalid(error,"Invalid MQTT topic_prefix");
    if (c->ruuvi_mac[0] && !g_regex_match_simple("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$",c->ruuvi_mac,0,0)) return invalid(error,"Invalid Ruuvi MAC");
    if (c->ruuvi_topic_prefix[0]) {
        if (!c->ruuvi_mac[0] || strlen(c->client_id)>sizeof(c->client_id)-7 ||
            strpbrk(c->ruuvi_topic_prefix,"+#\r\n") ||
            c->ruuvi_topic_prefix[strlen(c->ruuvi_topic_prefix)-1]=='/' ||
            !strcmp(c->ruuvi_topic_prefix,c->topic_prefix)) return invalid(error,"Invalid Ruuvi topic prefix or client ID");
    }
    if (c->password[0] && !c->username[0]) return invalid(error,"Password requires username");
    if (c->tls && !c->ca_file[0]) return invalid(error,"TLS requires ca_file");
    if (!c->tls && (c->ca_file[0] || c->cert_file[0] || c->key_file[0])) return invalid(error,"TLS files require tls=true");
    if (!!c->cert_file[0] != !!c->key_file[0]) return invalid(error,"cert_file and key_file must be provided together");
    return true;
}
