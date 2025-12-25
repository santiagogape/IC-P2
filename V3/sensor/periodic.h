#ifndef SENSOR_PERIODIC_H
#define SENSOR_PERIODIC_H

#include "srf02_i2c.h"
#include "sensor_state.h"
#include "protocol.h"


void periodic_thread(void *arg);

bool periodic_should_run(const SensorState *st);
uint32_t periodic_time_until_next(const SensorState *st, systime_t now);

void periodic_handle_i2c_result(const I2CResult *res,
                                ExecResponse *out_event);

#endif
