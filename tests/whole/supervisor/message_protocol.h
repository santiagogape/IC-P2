#define COM_MASC 0xF0
#define DEV_MASC 0x0F
#define DEV_MIN  0xE0

#define UNDER_MIN_DELAY     10
#define SENSOR_DISCONECTED  11

inline uint8_t mask_error(uint8_t errorID, uint8_t code);

typedef struct __attribute__((packed)) {
  uint8_t  code;
  uint16_t param;
} Message;

const Message empty_message = {0, 0};

inline uint8_t hexToDevId(uint8_t dev);
inline uint8_t codeToDev(uint8_t code);

// =======================================================
// I/O de bajo nivel unificada para Serial1
// =======================================================
inline size_t sendCode(Stream &port, uint8_t code);

inline bool receiveCode(Stream &port, uint8_t *outCode, uint32_t timeout_ms);
size_t sendMessage(Stream &port, const Message &msg);

bool receiveMessage(Stream &port, Message *outMsg, uint32_t timeout_ms);

bool verifyReply(uint8_t sentCode, const Message &reply);