#include "app_config.h"
#include "mqtt_transport.h"
#include "ruuvi_state.h"
#include "dlog.h"
#include <gio/gio.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

struct app {
    struct app_config cfg;
    struct ruuvi_state state;
    struct mqtt_transport *mqtt;
    GDBusConnection *bus;
    GMainLoop *loop;
    char *adapter_path, *device_path;
    bool scanning, bluez_present, available;
    int exit_code;
    unsigned generation;
    double next_publish, next_scan;
};
static double monotonic(void) { return g_get_monotonic_time()/1000000.0; }
static bool adapter_call(struct app *a,const char *method,GVariant *args) {
    GError *error=NULL;
    GVariant *reply=g_dbus_connection_call_sync(a->bus,"org.bluez",a->adapter_path,
        "org.bluez.Adapter1",method,args,NULL,G_DBUS_CALL_FLAGS_NONE,3000,NULL,&error);
    if (!reply) {
        daemon_log(LOG_WARNING,"%s: %s",method,error->message);
        g_clear_error(&error); return false;
    }
    g_variant_unref(reply); return true;
}
static void start_scan(struct app *a) {
    GVariantBuilder f;
    g_variant_builder_init(&f,G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&f,"{sv}","Transport",g_variant_new_string("le"));
    g_variant_builder_add(&f,"{sv}","DuplicateData",g_variant_new_boolean(true));
    /* BlueZ 5.50 rejects Discoverable and Pattern. Only use the common
     * filter set; exact adapter/MAC filtering is done on incoming events. */
    if (adapter_call(a,"SetDiscoveryFilter",g_variant_new("(a{sv})",&f)) &&
        adapter_call(a,"StartDiscovery",NULL)) {
        a->scanning=true;
        daemon_log(LOG_INFO,"Scanning %s for %s",a->cfg.adapter,a->cfg.mac);
    }
    a->next_scan=monotonic()+5;
}
static void properties(GDBusConnection *bus,const char *sender,const char *path,
        const char *interface,const char *signal,GVariant *parameters,gpointer data) {
    (void)bus;(void)sender;(void)interface;(void)signal;
    struct app *a=data;
    const char *changed_interface;
    GVariant *changes,*invalidated;
    g_variant_get(parameters,"(&s@a{sv}@as)",&changed_interface,&changes,&invalidated);
    if (!strcmp(path,a->device_path) && !strcmp(changed_interface,"org.bluez.Device1"))
        ruuvi_update(&a->state,changes,monotonic());
    if (!strcmp(path,a->adapter_path) && !strcmp(changed_interface,"org.bluez.Adapter1")) {
        gboolean powered,discovering;
        if ((g_variant_lookup(changes,"Powered","b",&powered) && !powered) ||
            (g_variant_lookup(changes,"Discovering","b",&discovering) && !discovering)) {
            a->scanning=false;
            a->state.received=false;
        }
    }
    g_variant_unref(changes); g_variant_unref(invalidated);
}
static void added(GDBusConnection *bus,const char *sender,const char *path,
        const char *interface,const char *signal,GVariant *parameters,gpointer data) {
    (void)bus;(void)sender;(void)path;(void)interface;(void)signal;
    struct app *a=data;
    const char *object;
    GVariant *interfaces;
    g_variant_get(parameters,"(&o@a{sa{sv}})",&object,&interfaces);
    if (!strcmp(object,a->device_path)) {
        GVariant *p=g_variant_lookup_value(interfaces,"org.bluez.Device1",G_VARIANT_TYPE("a{sv}"));
        if (p) { ruuvi_update(&a->state,p,monotonic()); g_variant_unref(p); }
    }
    g_variant_unref(interfaces);
}
static void appeared(GDBusConnection *bus,const char *name,const char *owner,gpointer data) {
    (void)bus;(void)name;(void)owner;
    struct app *a=data;
    a->bluez_present=true; a->next_scan=0;
}
static void vanished(GDBusConnection *bus,const char *name,gpointer data) {
    (void)bus;(void)name;
    struct app *a=data;
    a->bluez_present=false; a->scanning=false; a->state.received=false;
    daemon_log(LOG_WARNING,"BlueZ unavailable; waiting for service");
}
static gboolean tick(gpointer data) {
    struct app *a=data;
    double now=monotonic();
    if (a->bluez_present && !a->scanning && now>=a->next_scan) start_scan(a);
    bool fresh=ruuvi_fresh(&a->state,now,a->cfg.stale_timeout);
    unsigned generation=mqtt_transport_generation(a->mqtt);
    if (a->mqtt && mqtt_transport_connected(a->mqtt) &&
        (generation!=a->generation || fresh!=a->available)) {
        if (mqtt_transport_publish(a->mqtt,"AVAILABILITY",fresh?"Online":"Offline",true)) {
            a->available=fresh; a->generation=generation; a->next_publish=0;
        }
    }
    if (fresh && now>=a->next_publish) {
        char *json=ruuvi_json(&a->state,a->cfg.mac,now,a->cfg.stale_timeout);
        puts(json); fflush(stdout);
        if (a->mqtt) mqtt_transport_publish(a->mqtt,"SENSOR",json,false);
        g_free(json);
        a->next_publish=now+a->cfg.publish_interval;
    }
    return G_SOURCE_CONTINUE;
}
static gboolean stop(gpointer data) {
    struct app *a=data;
    g_main_loop_quit(a->loop); return G_SOURCE_REMOVE;
}
static void bus_closed(GDBusConnection *bus,gboolean vanished_remote,GError *error,gpointer data) {
    (void)bus;(void)vanished_remote;(void)error;
    struct app *a=data;
    daemon_log(LOG_ERR,"System D-Bus connection closed");
    a->exit_code=1; g_main_loop_quit(a->loop);
}
int main(int argc,char **argv) {
    char *config=NULL,*adapter=NULL,*mac=NULL;
    gboolean no_mqtt=false,check=false,syslog_output=false;
    gint duration=0;
    GOptionEntry options[]={
        {"config",'C',0,G_OPTION_ARG_FILENAME,&config,"INI configuration (required)","FILE"},
        {"adapter",'i',0,G_OPTION_ARG_STRING,&adapter,"Override adapter","hciN"},
        {"device",'d',0,G_OPTION_ARG_STRING,&mac,"Override sensor MAC","MAC"},
        {"no-mqtt",'D',0,G_OPTION_ARG_NONE,&no_mqtt,"Disable MQTT",NULL},
        {"check-config",0,0,G_OPTION_ARG_NONE,&check,"Validate config without connecting",NULL},
        {"duration",0,0,G_OPTION_ARG_INT,&duration,"Stop after N seconds (0: unlimited)","N"},
        {"syslog",0,0,G_OPTION_ARG_NONE,&syslog_output,"Send diagnostics to syslog",NULL},
        {NULL,0,0,0,NULL,NULL,NULL}
    };
    GOptionContext *context=g_option_context_new("- Ruuvi advertising to MQTT");
    g_option_context_add_main_entries(context,options,NULL);
    GError *error=NULL;
    struct app a={.state={.rssi=32767}};
    app_config_defaults(&a.cfg);
    if (!g_option_context_parse(context,&argc,&argv,&error) || argc!=1 || !config || duration<0 ||
        !app_config_load(&a.cfg,config,&error)) goto invalid;
    if (adapter) {
        if (strlen(adapter)>=sizeof(a.cfg.adapter)) goto invalid;
        g_strlcpy(a.cfg.adapter,adapter,sizeof(a.cfg.adapter));
    }
    if (mac) {
        if (strlen(mac)>=sizeof(a.cfg.mac)) goto invalid;
        g_strlcpy(a.cfg.mac,mac,sizeof(a.cfg.mac));
    }
    if (no_mqtt) a.cfg.mqtt_enabled=false;
    if (!app_config_validate(&a.cfg,&error)) goto invalid;
    if (check) { puts("Configuration valid"); return 0; }
    daemon_log_ident="ruuvi-mqtt";
    daemon_log_use=syslog_output?DAEMON_LOG_SYSLOG:DAEMON_LOG_STDERR;
    daemon_log_upto(a.cfg.log_level);
    a.adapter_path=g_strdup_printf("/org/bluez/%s",a.cfg.adapter);
    char *upper=g_ascii_strup(a.cfg.mac,-1);
    for (char *p=upper; *p; p++) if (*p==':') *p='_';
    a.device_path=g_strdup_printf("%s/dev_%s",a.adapter_path,upper); g_free(upper);
    a.bus=g_bus_get_sync(G_BUS_TYPE_SYSTEM,NULL,&error);
    if (!a.bus) {
        daemon_log(LOG_ERR,"D-Bus: %s",error->message); a.exit_code=1; goto cleanup;
    }
    g_dbus_connection_set_exit_on_close(a.bus,FALSE);
    if (a.cfg.mqtt_enabled) {
        a.mqtt=mqtt_transport_start(&a.cfg);
        if (!a.mqtt) { a.exit_code=1; goto cleanup; }
    }
    a.loop=g_main_loop_new(NULL,FALSE);
    g_signal_connect(a.bus,"closed",G_CALLBACK(bus_closed),&a);
    guint prop_id=g_dbus_connection_signal_subscribe(a.bus,"org.bluez",
        "org.freedesktop.DBus.Properties","PropertiesChanged",NULL,NULL,0,properties,&a,NULL);
    guint add_id=g_dbus_connection_signal_subscribe(a.bus,"org.bluez",
        "org.freedesktop.DBus.ObjectManager","InterfacesAdded",NULL,NULL,0,added,&a,NULL);
    guint watch=g_bus_watch_name_on_connection(a.bus,"org.bluez",0,appeared,vanished,&a,NULL);
    guint timer=g_timeout_add(200,tick,&a);
    guint sigint=g_unix_signal_add(SIGINT,stop,&a),sigterm=g_unix_signal_add(SIGTERM,stop,&a);
    guint duration_id=duration?g_timeout_add_seconds(duration,stop,&a):0;
    g_main_loop_run(a.loop);
    if (a.scanning && !g_dbus_connection_is_closed(a.bus)) adapter_call(&a,"StopDiscovery",NULL);
    mqtt_transport_publish(a.mqtt,"AVAILABILITY","Offline",true);
    g_source_remove(timer);
    if (g_main_context_find_source_by_id(NULL,sigint)) g_source_remove(sigint);
    if (g_main_context_find_source_by_id(NULL,sigterm)) g_source_remove(sigterm);
    if (duration_id && g_main_context_find_source_by_id(NULL,duration_id)) g_source_remove(duration_id);
    g_bus_unwatch_name(watch);
    g_dbus_connection_signal_unsubscribe(a.bus,prop_id);
    g_dbus_connection_signal_unsubscribe(a.bus,add_id);
    g_main_loop_unref(a.loop);
cleanup:
    mqtt_transport_stop(a.mqtt);
    if (a.bus) g_object_unref(a.bus);
    g_free(a.adapter_path); g_free(a.device_path);
    g_clear_error(&error); g_option_context_free(context);
    g_free(config); g_free(adapter); g_free(mac);
    return a.exit_code;
invalid:
    fprintf(stderr,"Invalid configuration: %s\n",error?error->message:"use -C FILE; see --help");
    g_clear_error(&error); g_option_context_free(context);
    g_free(config); g_free(adapter); g_free(mac);
    return 2;
}
