// =============================================================
// Constantes de SRF02
// =============================================================
#define SRF02_I2C_INIT_DELAY      100   // ms
#define SRF02_RANGING_DELAY       70    // ms (mínimo entre disparos)

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define SOFTWARE_REVISION byte(0x00)
#define RANGE_HIGH_BYTE byte(2)
#define RANGE_LOW_BYTE byte(3)
#define AUTOTUNE_MINIMUM_HIGH_BYTE byte(4)
#define AUTOTUNE_MINIMUM_LOW_BYTE byte(5)

// === Modos de disparo ===
#define REAL_RANGING_MODE_INCHES   byte(80)
#define REAL_RANGING_MODE_CMS      byte(81)
#define REAL_RANGING_MODE_USECS    byte(82)

// =============================================================
// Funciones de bajo nivel
// =============================================================

/**
 * Envía un comando al registro 0x00 del SRF02.
 * address = dirección I2C (7 bits)
 * command = código de comando SRF02 (p. ej. REAL_RANGING_MODE_CMS)
 */
inline void srf02_writeCommand(uint8_t address, uint8_t command);

/**
 * Lee un registro del SRF02 (1 byte)
 */
inline uint8_t srf02_readRegister(uint8_t address, uint8_t reg);

/**
 * Realiza un disparo de medición (one-shot) con la unidad especificada.
 * Retorna el valor de distancia como uint16_t (high<<8 | low)
 * 
 * units:
 *   4 → pulgadas (inches)
 *   5 → centímetros (cm)
 *   6 → microsegundos (µs)
 * en terminal.ino  /supervisor se indica que 
 * las unidades se identifican por 4,5,6
 */
 ;
void srf02_oneShot(uint8_t address, uint8_t unit);
uint16_t srf02_read_result(uint8_t address);

/**
 * Lee información de estado del SRF02.
 * Devuelve software_revision y valor autotune mínimo (uint16_t)
 */
void srf02_status(uint8_t address, uint8_t *rev, uint16_t *autoMin);



// ************************************************************************************************************************
// sensor
// ************************************************************************************************************************
// ============================================================
// Estructura de configuración local del sensor
// ============================================================

typedef struct {
  uint8_t addr;       // Dirección lógica del sensor (0xE0, 0xF2…)
  uint8_t i2c_addr;   // Dirección de 7 bits para Wire
  uint16_t delay_ms;  // Retardo mínimo entre disparos
} SensorConfig;

// ============================================================
// Configuración de sensores conectados
// ============================================================

#define N_SENSORS 2
SensorConfig sensors[N_SENSORS] = {
  {0xE2, byte(0xE2 >> 1), SRF02_RANGING_DELAY},
  {0xF2, byte(0xF2 >> 1), SRF02_RANGING_DELAY}
};
