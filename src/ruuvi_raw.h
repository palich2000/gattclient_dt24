#ifndef RUUVI_RAW_H
#define RUUVI_RAW_H
#include "app_config.h"
struct ruuvi_raw;
typedef void (*ruuvi_raw_publish)(const char *json, bool available, void *data);
struct ruuvi_raw *ruuvi_raw_start(int adapter,const char *mac,const struct app_config *cfg,
                                 ruuvi_raw_publish publish,void *data);
void ruuvi_raw_stop(struct ruuvi_raw *s);
#endif
