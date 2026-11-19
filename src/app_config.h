#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <glib.h>
#include <stdbool.h>
struct app_config {
    char adapter[32], mac[18], address_type[8], security[8];
    char ruuvi_mac[18], ruuvi_topic_prefix[256];
    char host[256], client_id[128], username[256], password[512], topic_prefix[256];
    char ca_file[512], cert_file[512], key_file[512];
    int port, keepalive, publish_interval, stale_timeout, mtu, thermal_zone, log_level;
    bool mqtt_enabled, tls, console_json;
};
void app_config_defaults(struct app_config *c);
bool app_config_load(struct app_config *c, const char *path, GError **error);
bool app_config_validate(const struct app_config *c, GError **error);
#endif
