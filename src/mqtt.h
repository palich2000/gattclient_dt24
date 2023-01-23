/**
* @file mqtt.h
* @author palich (y.palich.t@gmail.com)
*
* @brief
*
*/
#ifndef SRC_MQTT_H
#define SRC_MQTT_H

void mosq_init(const char * progname);

void mosq_destroy(void);

void mosq_gather_data(double current, double voltage);

#endif //SRC_MQTT_H