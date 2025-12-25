#include "srf02_i2c.h"

static inline uint8_t srf02_dir_to_i2c(uint8_t dir) // symbol # used
{
  return (uint8_t)(dir >> 1);
}

static ErrorCode srf02_write_command(uint8_t dir, uint8_t command) // symbol # used
{
  Wire.beginTransmission(srf02_dir_to_i2c(dir));
  Wire.write(SRF02_COMMAND_REGISTER);
  Wire.write(command);

  if (Wire.endTransmission() != 0)
    return ERR_I2C_NACK;

  return ERR_OK;
}

static ErrorCode srf02_read_register(uint8_t dir, uint8_t reg, uint8_t *out) // symbol # used
{
  Wire.beginTransmission(srf02_dir_to_i2c(dir));
  Wire.write(reg);

  if (Wire.endTransmission() != 0)
    return ERR_I2C_NACK;

  if (Wire.requestFrom(srf02_dir_to_i2c(dir), (uint8_t)1) != 1)
    return ERR_I2C_TIMEOUT;

  *out = Wire.read();
  return ERR_OK;
}

void srf02_i2c_init(void) // symbol # used
{
  Wire.begin();
  delay(SRF02_INIT_DELAY_MS);
}

bool srf02_is_valid_dir(uint8_t dir) // symbol # used
{
  return (dir == US_1) || (dir == US_2);
}

I2CResult srf02_execute_job(const I2CJob *job) // symbol # used
{
  I2CResult res = { job->dir, ERR_OK, 0 };

  if (!srf02_is_valid_dir(job->dir)) {
    res.status = ERR_BAD_DIR;
    return res;
  }

  ErrorCode err = srf02_write_command(job->dir, (uint8_t)job->unit);
  if (err != ERR_OK) {
    res.status = err;
    return res;
  }

  chThdSleepMilliseconds(SRF02_HW_DELAY_MS);

  uint8_t hi, lo;

  err = srf02_read_register(job->dir, SRF02_RANGE_HIGH_BYTE, &hi);
  if (err != ERR_OK) { res.status = err; return res; }

  err = srf02_read_register(job->dir, SRF02_RANGE_LOW_BYTE, &lo);
  if (err != ERR_OK) { res.status = err; return res; }

  res.value = ((uint16_t)hi << 8) | lo;
  return res;
}
