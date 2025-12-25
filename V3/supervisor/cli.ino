#include "cli.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#define CLI_BUF_SIZE 64

static char cli_buf[CLI_BUF_SIZE];
static uint8_t cli_len = 0;
static CliError last_err = CLI_ERR_NONE;

static void cli_reset(void)
{
  cli_len = 0;
  last_err = CLI_ERR_NONE;
}

static bool parse_hex_byte(const char *s, uint8_t *out)
{
  // formato: 0xNN (4 chars)
  if (!s || strlen(s) != 4 || s[0] != '0' || s[1] != 'x')
    return false;

  char *end = nullptr;
  long v = strtol(s, &end, 16);
  if (*end != '\0' || v < 0 || v > 0xFF)
    return false;

  *out = (uint8_t)v;
  return true;
}

static bool parse_u16(const char *s, uint16_t *out)
{
  if (!s) return false;

  char *end = nullptr;
  long v = strtol(s, &end, 10);
  if (*end != '\0' || v < 0 || v > 0xFFFF)
    return false;

  *out = (uint16_t)v;
  return true;
}

void cli_init(void)
{
  cli_reset();
}

CliError cli_last_error(void)
{
  return last_err;
}

static const char* cli_error_to_string(CliError err)
{
  switch (err) {
    case CLI_ERR_SYNTAX:        return "syntax error";
    case CLI_ERR_BAD_NUMBER:   return "invalid number";
    case CLI_ERR_OVERFLOW:     return "number overflow";
    case CLI_ERR_UNKNOWN_CMD:  return "unknown command";
    case CLI_ERR_BAD_PARAM:    return "bad parameter";
    case CLI_ERR_MAX_DELAY:    return "delay exceeds maximum";
    case CLI_ERR_MAX_PERIOD:   return "period exceeds maximum";
    case CLI_ERR_BUSY:         return "busy, still expecting another response";
    default:                   return "unknown cli error";
  }
}

void printer_print_busy(){
  SerialUSB.print("[DESIGN ERROR] ");
  SerialUSB.println(cli_error_to_string(CLI_ERR_BUSY));
}

void printer_print_cli_error(CliError err)
{
  if (err == CLI_ERR_NONE)
    return;
  last_err = CLI_ERR_NONE;
  SerialUSB.print("[CLI ERROR] ");
  SerialUSB.println(cli_error_to_string(err));
}

bool cli(ConsoleCommand *out_cmd)
{
  while (SerialUSB.available()) {
    char c = (char)SerialUSB.read();

    if (c == '\r') continue;

    if (c == '\n') {
      cli_buf[cli_len] = '\0';
      cli_len = 0;
      last_err = CLI_ERR_NONE;

      // Tokenizar (máximo 6 tokens)
      char *argv[6];
      int argc = 0;

      char *tok = strtok(cli_buf, " ");
      while (tok && argc < 6) {
        argv[argc++] = tok;
        tok = strtok(nullptr, " ");
      }

      if (argc == 0) return false;

      // help
      if (strcmp(argv[0], "help") == 0) {
        out_cmd->cmd = CMD_HELP;
        out_cmd->dir = 0;
        out_cmd->value = 0;
        return true;
      }

      // us ...
      if (strcmp(argv[0], "us") != 0) {
        last_err = CLI_ERR_UNKNOWN_CMD;
        return false;
      }

      // us  (list sensors)
      if (argc == 1) {
        out_cmd->cmd = CMD_LIST_SENSORS;
        out_cmd->dir = 0;
        out_cmd->value = 0;
        return true;
      }

      // us <0xNN> ...
      uint8_t dir;
      if (!parse_hex_byte(argv[1], &dir)) {
        last_err = CLI_ERR_BAD_NUMBER;
        return false;
      }
      out_cmd->dir = dir;
      out_cmd->value = 0;

      // us <dir> one-shot
      if (argc == 3 && strcmp(argv[2], "one-shot") == 0) {
        out_cmd->cmd = CMD_RANGE_ONCE;
        return true;
      }

      // us <dir> on <period_ms>
      if (argc == 4 && strcmp(argv[2], "on") == 0) {
        uint16_t period;
        if (!parse_u16(argv[3], &period)) {
          last_err = CLI_ERR_OVERFLOW;
          return false;
        }
        if (period > MAX_PERIOD_MS){
          last_err = CLI_ERR_MAX_PERIOD;
          return false;
        }
        out_cmd->cmd = CMD_STREAM_ON;
        out_cmd->value = period;
        return true;
      }

      // us <dir> off
      if (argc == 3 && strcmp(argv[2], "off") == 0) {
        out_cmd->cmd = CMD_STREAM_OFF;
        return true;
      }

      // us <dir> unit {inc|cm|ms}
      if (argc == 4 && strcmp(argv[2], "unit") == 0) {
        out_cmd->cmd = CMD_SET_UNIT;

        if (strcmp(argv[3], "inc") == 0)      out_cmd->value = (uint16_t)UNIT_INCHES;
        else if (strcmp(argv[3], "cm") == 0)  out_cmd->value = (uint16_t)UNIT_CM;
        else if (strcmp(argv[3], "ms") == 0)  out_cmd->value = (uint16_t)UNIT_USEC;
        else {
          last_err = CLI_ERR_BAD_PARAM;
          return false;
        }
        return true;
      }

      // us <dir> delay <ms>
      if (argc == 4 && strcmp(argv[2], "delay") == 0) {
        uint16_t dly;
        if (!parse_u16(argv[3], &dly)) {
          last_err = CLI_ERR_OVERFLOW;
          return false;
        }
        if (dly > MAX_DELAY_MS) {
          last_err = CLI_ERR_MAX_DELAY;
          return false;
        }
        out_cmd->cmd = CMD_SET_DELAY;
        out_cmd->value = dly;
        return true;
      }

      // us <dir> status
      if (argc == 3 && strcmp(argv[2], "status") == 0) {
        out_cmd->cmd = CMD_GET_STATUS;
        return true;
      }

      last_err = CLI_ERR_SYNTAX;
      return false;
    }

    // acumulación
    if (cli_len < CLI_BUF_SIZE - 1) {
      cli_buf[cli_len++] = c;
    } else {
      last_err = CLI_ERR_OVERFLOW;
      cli_len = 0;
      return false;
    }
  }
  return false;
}
