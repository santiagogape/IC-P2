#include "execution.h"

static SensorState* find_state(uint8_t dir) // symbol # used
{
  for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    if (sensors[i].dir == dir)
      return &sensors[i];
  return nullptr;
}


static bool delay_elapsed(const SensorState *st, systime_t now) // symbol # used
{
  if (st->last_measurement == 0)
    return true;

  return TIME_I2MS(now - st->last_measurement) >= st->min_delay_ms;
}


bool execution_handle_command(const ExecCommand *cmd,
                              ExecResponse *out_resp,
                              I2CJob *out_job) // symbol # used
{
  out_resp->cmd = cmd->cmd;
  out_resp->dir = cmd->dir;
  out_resp->status = ERR_OK;
  out_resp->plen = 0;
  out_resp->is_event = false;
  SensorState *st = find_state(cmd->dir);
  if (!st) {
    out_resp->status = ERR_BAD_DIR;
    return false;
  }

  systime_t now = chVTGetSystemTime();

  switch (cmd->cmd) {
    case CMD_RANGE_ONCE:
      if (!delay_elapsed(st, now)) {
        out_resp->status = ERR_SENSOR_BUSY;
        return false;
      }
      out_job->dir = st->dir;
      out_job->unit = st->unit;
      return true;

    case CMD_STREAM_ON: {
      if (cmd->plen != 2) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }
      uint16_t p = ((uint16_t)cmd->payload[0] << 8) | cmd->payload[1];
      if (p == 0 || p < st->min_delay_ms) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }
      st->period_ms = p;
      st->periodic_enabled = true;
      return false;
    }

    case CMD_STREAM_OFF:
      st->periodic_enabled = false;
      return false;

    case CMD_SET_UNIT:
      if (cmd->plen != 1) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }
      st->unit = (Unit)cmd->payload[0];
      return false;

    case CMD_SET_DELAY: {
      if (cmd->plen != 2) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }
      uint16_t d = ((uint16_t)cmd->payload[0] << 8) | cmd->payload[1];
      if (d < SRF02_HW_DELAY_MS) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }
      if (st->periodic_enabled && d > st->period_ms) {
        out_resp->status = ERR_BAD_PARAM;
        return false;
      }

      st->min_delay_ms = d;
      return false;
    }

    case CMD_GET_STATUS:
      execution_get_status(cmd->dir, out_resp);
      return false;

    case CMD_LIST_SENSORS:
      execution_list_sensors(out_resp);
      return false;

    default:
      out_resp->status = ERR_BAD_CMD;
      return false;
  }
}

void execution_handle_i2c_result(const I2CResult *res,
                                 ExecResponse *out_resp) // symbol # used
{
  out_resp->cmd = CMD_RANGE_ONCE;
  out_resp->dir = res->dir;
  out_resp->status = res->status;
  out_resp->is_event = false;

  if (res->status == ERR_OK) {
    out_resp->plen = 2;
    out_resp->payload[0] = (res->value >> 8) & 0xFF;
    out_resp->payload[1] = res->value & 0xFF;
  } else {
    out_resp->plen = 0;
  }
}

void execution_get_status(uint8_t dir, ExecResponse *out) // symbol # used
{
  SensorState *st = find_state(dir);

  out->cmd = CMD_GET_STATUS;
  out->dir = dir;
  out->is_event = false;

  if (!st) {
    out->status = ERR_BAD_DIR;
    out->plen = 0;
    return;
  }

  out->status = ERR_OK;
  out->plen = 7;

  out->payload[0] = st->dir;
  out->payload[1] = (uint8_t)st->unit;
  out->payload[2] = (st->min_delay_ms >> 8) & 0xFF;
  out->payload[3] = st->min_delay_ms & 0xFF;
  out->payload[4] = st->periodic_enabled ? 1 : 0;
  out->payload[5] = (st->period_ms >> 8) & 0xFF;
  out->payload[6] = st->period_ms & 0xFF;
}

void execution_list_sensors(ExecResponse *out) // symbol # used
{
  out->cmd = CMD_LIST_SENSORS;
  out->dir = 0;
  out->status = ERR_OK;
  out->is_event = false;

  out->plen = NUM_SENSORS;
  for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    out->payload[i] = sensors[i].dir;
}


bool message_to_exec_command(const Message *msg, ExecCommand *out) // symbol # used
{
  // Rechazar respuestas o eventos
  if (msg->flags & (MSG_FLAG_RESPONSE | MSG_FLAG_EVENT))
    return false;

  out->cmd  = (CommandId)msg->cmd;
  out->dir  = msg->dir;
  out->plen = msg->plen;

  if (msg->plen > MAX_PAYLOAD_SIZE)
    return false;

  // Validación por comando
  switch (out->cmd) {

    case CMD_RANGE_ONCE:
    case CMD_STREAM_OFF:
    case CMD_GET_STATUS:
    case CMD_LIST_SENSORS:
      if (msg->plen != 0) return false;
      break;

    case CMD_STREAM_ON:
    case CMD_SET_DELAY:
      if (msg->plen != 2) return false;
      break;

    case CMD_SET_UNIT:
      if (msg->plen != 1) return false;
      break;

    default:
      return false;
  }

  // Copiar payload
  for (uint8_t i = 0; i < msg->plen; ++i)
    out->payload[i] = msg->payload[i];

  return true;
}

