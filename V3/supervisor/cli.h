#ifndef SUPERVISOR_CLI_H
#define SUPERVISOR_CLI_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"  // CommandId, Unit

typedef struct {
  CommandId cmd;
  uint8_t   dir;
  uint16_t  value;   // delay/period/unit según comando
} ConsoleCommand;

typedef enum {
  CLI_ERR_NONE = 0,
  CLI_ERR_SYNTAX,
  CLI_ERR_BAD_NUMBER,
  CLI_ERR_OVERFLOW,
  CLI_ERR_UNKNOWN_CMD,
  CLI_ERR_BAD_PARAM,
  CLI_ERR_MAX_DELAY,
  CLI_ERR_MAX_PERIOD,
  CLI_ERR_BUSY
} CliError;

void cli_init(void);
bool cli(ConsoleCommand *out_cmd);
CliError cli_last_error(void);

#define MAX_DELAY_MS   1024
#define MAX_PERIOD_MS  2048   // o 5000

void printer_print_cli_error(CliError err);

#endif
