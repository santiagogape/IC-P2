#ifndef SENSOR_STATE_H
#define SENSOR_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <ChRt.h>
#include "protocol.h"

// =====================
// Estado real del sensor
// =====================
typedef struct {
  uint8_t   dir;
  uint16_t  min_delay_ms;
  bool      periodic_enabled;
  uint16_t  period_ms;
  Unit      unit;
  systime_t last_measurement;
} SensorState;

// =====================
// Mutex del estado (ÚNICO)
// =====================
static mutex_t sensors_mtx;

// =====================
// Sensores disponibles
// =====================
#define US_1 0xE2
#define US_2 0xF2

static SensorState sensors[] = {
  { US_1, 70, false, 0, UNIT_CM, 0 },
  { US_2, 70, false, 0, UNIT_CM, 0 }
};

static constexpr uint8_t NUM_SENSORS =
  sizeof(sensors) / sizeof(sensors[0]);

#endif
