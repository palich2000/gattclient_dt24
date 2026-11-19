#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
int main(int argc, char **argv) {
    assert(argc == 2);
    struct app_config c;
    app_config_defaults(&c);
    GError *err = NULL;
    assert(app_config_load(&c, argv[1], &err));
    assert(!err && strcmp(c.mac, "DE:4D:F8:3F:DA:91") == 0);
    assert(c.port == 1884 && c.publish_interval == 2 && !c.mqtt_enabled);
    assert(app_config_validate(&c, &err));
    struct app_config good = c;
    c.port = 0;
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; strcpy(c.mac,"DE:4D:F8:3F:DA:9Z");
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; c.publish_interval=0;
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; c.stale_timeout=1;
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; strcpy(c.topic_prefix,"test/+");
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; c.tls=true;
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    c=good; strcpy(c.ca_file,"ca.pem");
    assert(!app_config_validate(&c, &err)); g_clear_error(&err);
    puts("INI configuration and validation: OK");
}
