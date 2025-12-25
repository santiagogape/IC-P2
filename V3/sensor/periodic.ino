#include "periodic.h"
#include "execution.h"



bool periodic_should_run(const SensorState *st) // symbol
{
  if (!st->periodic_enabled)
    return false;

  if (st->last_measurement == 0)
    return true;

  systime_t now = chVTGetSystemTime();
  return TIME_I2MS(now - st->last_measurement) >= st->period_ms;
}

uint32_t periodic_time_until_next(const SensorState *st, systime_t now) // symbol
{
  if (!st->periodic_enabled)
    return TIME_INFINITE;

  if (st->last_measurement == 0)
    return 0;

  uint32_t elapsed = TIME_I2MS(now - st->last_measurement);
  if (elapsed >= st->period_ms)
    return 0;

  return st->period_ms - elapsed;
}

void periodic_handle_i2c_result(const I2CResult *res,
                                ExecResponse *out) // symbol
{
  out->cmd = CMD_RANGE_ONCE;
  out->dir = res->dir;
  out->status = res->status;
  out->is_event = true;

  if (res->status == ERR_OK) {
    out->plen = 2;
    out->payload[0] = (res->value >> 8) & 0xFF;
    out->payload[1] = res->value & 0xFF;
  } else {
    out->plen = 0;
  }
}
