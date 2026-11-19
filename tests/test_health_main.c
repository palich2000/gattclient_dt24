/* Exercise the actual combined health callback without touching Bluetooth. */
#define GATT_RUUVI
#define main combined_application_main
#include "../src/btgattclient.c"
#undef main
int main(void) {
    struct app_config config;app_config_defaults(&config);
    disable_mqtt=true;combined_console_json=false;
    combined_dt24_frames=60;combined_last_dt24=g_get_monotonic_time()/1000000.0;
    combined_next_health=1;
    combined_publish(NULL,true,&config);
    combined_last_dt24-=121;combined_next_health=1;
    combined_publish(NULL,false,&config);
    /* Real parser: routine readings are debug-only, health stays INFO. */
    uint8_t frame[36]={0xff,0x55};
    frame[5]=1;frame[6]=6; /* 26.2 V */
    frame[8]=1;frame[9]=244; /* 0.500 A */
    daemon_log_upto(LOG_INFO);
    notify_cb(0,frame,sizeof(frame),NULL);
    daemon_log_upto(LOG_DEBUG);
    frame[8]=2;frame[9]=88; /* 0.600 A */
    notify_cb(0,frame,sizeof(frame),NULL);
    return 0;
}
