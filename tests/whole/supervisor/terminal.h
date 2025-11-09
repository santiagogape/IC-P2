#define HELP "help"
#define MAX_ARGS 4
#define COM_NUM 7

typedef struct { char name[9]; uint8_t id; } Argument;
const Argument commands[COM_NUM] = {
  {"us", 9}, {"one-shot",1}, {"on",2}, {"off",3},
  {"unit",4}, {"delay",7}, {"status",8}
};
const char UNITS[3][4] = {"inc","cm","ms"};

bool isHex(const char *s);
bool isNumber(const char *s);

uint8_t parseHex(char *hex);

uint16_t parseInt(const char *num, bool *error);

inline uint8_t codeFromCommandIdAndDevId(uint8_t id, uint8_t dev_id);

// ============================================================
// CLI parser
// ============================================================
inline void argument_error();

Message processCommand(uint8_t argc, char *argv[]);
Message CLI();