#include "command.h"

bool exec_build_message_from_console(const ConsoleCommand *cmd,
                                     Message *out_msg)
{
  out_msg->flags  = 0;
  out_msg->cmd    = (uint8_t)cmd->cmd;
  out_msg->dir    = cmd->dir;
  out_msg->status = 0;       // requests: 0
  out_msg->plen   = 0;

  switch (cmd->cmd) {

    case CMD_RANGE_ONCE:
    case CMD_STREAM_OFF:
    case CMD_GET_STATUS:
    case CMD_LIST_SENSORS:
      return true;

    case CMD_STREAM_ON:
    case CMD_SET_DELAY:
      out_msg->plen = exec_encode_u16(cmd->value, out_msg->payload);
      return true;

    case CMD_SET_UNIT:
      out_msg->plen = 1;
      out_msg->payload[0] = (uint8_t)cmd->value; // Unit enum value
      return true;

    case CMD_HELP:
    default:
      return false;
  }
}
