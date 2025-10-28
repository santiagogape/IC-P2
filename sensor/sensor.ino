/* ----------------------------------------------------------------------
 *  Ejemplo echo.ino 
 *    Este ejemplo muestra como utilizar el puerto serie uart (Serial1) 
 *    para comunicarse con otro dispositivo.
 *    
 *  Asignatura (GII-IC)
 * ---------------------------------------------------------------------- 
 */

 /*
  8. Ejercicio propuesto.
  + dos dispositivos MKR WAN 1310.
    - supervisor
    - sensor
  Ambos van a comunicarse entre ellos a través del puerto serie asíncrono,
  análogamente a lo realizado en el apartado 4. 
  El dispositivo supervisor enviará comandos a través del puerto serie al dispositivo sensor.
  El dispositivo sensor será responsable 
   1. acceso a través del puerto I2C al menos dos sensores de ultrasonidos SRF02
   2. atenderá a los comandos que le lleguen por el puerto serie desde el supervisor
   3. los llevará a cabo y confirmará su realización, o bien, notificará cualquier incidencia en caso de error.
   
  El dispositivo supervisor aceptará comandos introducidos a través del monitor serial por el
  usuario, los interpretará y enviará el comando correspondiente al dispositivo sensor, 
  esperando su respuesta, y mostrando a través del monitor serial el resultado de su ejecución. 
  El formato de los comandos a aceptar por el dispositivo supervisor serán cadenas escritas por el usuario. 
  Al menos deben implementarse los siguientes comandos:

  - help: se muestra información acerca de los comandos aceptados y la operativa correcta del sistema. 
  - us <srf02> {one-shot | on <period_ms> | off}: 
    se comanda un único disparo del sensor de ultrasonidos (one-shot), 
    o bien se establece que se dispare con un periodo específico de manera continuada (on <period_ms>),
    o que se cese de disparar el sensor de manera periódica si lo estuviera (off). 
    En el comando debe identificarse qué sensor SRF02 quiere dispararse (<srf02>). 
  - us <srf02> unit {inc | cm | ms}: este comando permite modificar la unidad de medida devuelta
    por un sensor SRF02 específico (<srf02>). 
  - us <srf02> delay <ms>: este comando establece el tiempo de espera o retardo mínimo que debe 
    haber entre dos disparos consecutivos del sensor (<srf02>). 
  - us <srf02> status: este comando debe proporcionar información de configuración del sensor, 
    en concreto, su dirección I2C, retardo mínimo entre disparos, su configuración de unidades de medida, 
    y su estado de disparo periódico, en el caso de que éste esté activado o no. 
  - us: este comando debe proporcionar la relación de sensores de ultrasonidos disponibles en el dispositivo sensor. 
  
  Todos estos comandos deben tener un mensaje de respuesta, o bien, de confirmación, o bien de error, 
    o con la información requerida en cada comando específico, en su caso.

  Formato de los mensajes:
  El formato de mensajes a intercambiar a través del puerto serie, así como sus respuestas, 
  es binario y debe seguir el siguiente esquema: 
    - primer byte: código de operación del mensaje, 
      los valores específicos de este código de operación se dejan abiertos a los que quieran proponer. 
    - resto de bytes del cuerpo del mensaje: se deja abierto al formato que quieran proponer.
   [ 1 byte | resto   ]
   [ codigo | mensaje ]

   Sugerencias y puntos de especial atención:

   - Tengan en cuenta que los sensores de ultrasonidos cuándo se activen de manera periódica 
    enviaran al supervisor de manera asíncrona mensajes con el periodo especificado, 
    indicando la medida tomada en cada disparo. Esta información debe mostrarse en el supervisor, 
    en el monitor serial, sin interferir con lo que en ese momento el supervisor esté realizando. 
  - Para tener dos o más sensores de ultrasonidos en el bus I2C deben modificar la dirección I2C de 
    al menos uno de ellos, puesto que de fábrica vienen con la misma dirección preestablecida.

  Apartado opcional pantalla OLED:
    Incorporar en el bus I2C del dispositivo sensor un dispositivo de pantalla OLED o LCD.
    Incorpore al sistema los comandos que considere necesarios para mostrar información en el dispositivo 
    de pantalla durante la ejecución del sistema. Como sugerencia podría siempre mostrarse lo que el dispositivo 
    sensor está haciendo en cada momento, así como su configuración y estado mostrada en pantalla en todo momento.
 */

constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t serial1_bauds=9600;

constexpr const uint32_t pseudo_period_ms=1000;

uint8_t led_state=LOW;

void setup()
{
  // Configuración del LED incluido en placa
  // Inicialmente apagado
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
  
  // Inicialización del puerto para el serial monitor 
  Serial.begin(serial_monitor_bauds);
  while (!Serial);

  // Inicialización del puerto de comunicaciones con el otro dispositivo MKR 
  Serial1.begin(serial1_bauds);
}

void loop()
{
  Serial.println("******************** echo example *********************"); 

  uint32_t last_ms=millis();
  while(millis()-last_ms<pseudo_period_ms) 
  { 
    if(Serial1.available()>0) 
    {
      uint8_t data=Serial1.read();
      Serial.print("<-- received: "); Serial.println(static_cast<int>(data)); 
      Serial.print("--> sending back: "); Serial.println(static_cast<int>(data)); 
      Serial1.write(data);
      break;
    }
  }

  if(millis()-last_ms<pseudo_period_ms) delay(pseudo_period_ms-(millis()-last_ms));
  else Serial.println("<-- received: TIMEOUT!!"); 

  Serial.println("*******************************************************"); 

  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
}
