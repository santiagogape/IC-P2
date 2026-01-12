#ifndef SRF02_I2C_H
#define SRF02_I2C_H

#include <Wire.h>
#include "protocol.h"   // Unit, ErrorCode

// SRF02 registers
#define SRF02_COMMAND_REGISTER   0x00
#define SRF02_RANGE_HIGH_BYTE    0x02
#define SRF02_RANGE_LOW_BYTE     0x03

#define SRF02_HW_DELAY_MS     70
#define SRF02_INIT_DELAY_MS  100

typedef struct {
  uint8_t dir;
  Unit    unit;
} I2CJob;

typedef struct {
  uint8_t   dir;
  ErrorCode status;
  uint16_t  value;
} I2CResult;

void srf02_i2c_init(void);
bool srf02_is_valid_dir(uint8_t dir);
I2CResult srf02_execute_job(const I2CJob *job);

#endif
