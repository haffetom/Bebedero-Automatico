// Credenciales Sinric Pro
#define APP_KEY            "26bd83ac-a6e8-4c45-bbdd-84b7b2adda3c"
#define APP_SECRET         "751b72a0-becc-49c7-9c07-81830cb65168-90da5f43-7fb8-4b0d-a0a3-5f8db7873b6d"
#define DEVICE_ID_LIMPIA   "68793f93030990a558da25ea"
#define DEVICE_ID_SUCIA    "68798d6ff64d827f96b4348a"
#define DEVICE_ID_MODE     "68868cbdedeca866fe95e0a6" 

#include <Arduino.h>
#include <apwifieeprommode.h>
#include <EEPROM.h>
#include <WebSocketsServer.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>

// Puerto de comunicación
WebSocketsServer webSocket = WebSocketsServer(8081);
bool clienteConectado = false;

// Pines de bombas de agua
#define BOMBA_LIMPIA_PIN 12
#define BOMBA_SUCIA_PIN 13

// Sensores de nivel del tanque chiquito (bebedero)
#define SENSOR_CHIQUITO_LLENO 19
#define SENSOR_CHIQUITO_VACIO 18

// Sensores del tanque de agua limpia
#define SENSOR_LIMPIA_LLENO 23
#define SENSOR_LIMPIA_VACIO 22

// Sensor del tanque de agua sucia
#define SENSOR_AGUA_SUCIA_LLENO 5

// Sensor Infrarrojo
#define IR_PIN 36  //Usarlo para testear, luego se puede eliminar
#define IR_PIN2 35

// Sensor de Turbidez
#define TURBIDITY_PIN    34    
const int UMBRAL_TURBIDEZ = 4095;

// Eventos de comunicación
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.printf("[WebSocket] Cliente #%u conectado\n", num);
    clienteConectado = true;
    delay(1000);
    break;
  case WStype_DISCONNECTED:
    Serial.printf("[WebSocket] Cliente #%u desconectado\n", num);
    clienteConectado = false;
    break;
  case WStype_TEXT:
    Serial.printf("[WebSocket] Mensaje recibido: %s\n", payload);
    // Comandos desde la app
    if (strcmp((char *)payload, "ENCENDER_LIMPIA") == 0) {
      digitalWrite(BOMBA_LIMPIA_PIN, HIGH);
      webSocket.broadcastTXT("BOMBA LIMPIA ENCENDIDA");
    } else if (strcmp((char *)payload, "APAGAR_LIMPIA") == 0) {
      digitalWrite(BOMBA_LIMPIA_PIN, LOW);
      webSocket.broadcastTXT("BOMBA LIMPIA APAGADA");
    } else if (strcmp((char *)payload, "ENCENDER_SUCIA") == 0) {
      digitalWrite(BOMBA_SUCIA_PIN, HIGH);
      webSocket.broadcastTXT("BOMBA SUCIA ENCENDIDA");
    } else if (strcmp((char *)payload, "APAGAR_SUCIA") == 0) {
      digitalWrite(BOMBA_SUCIA_PIN, LOW);
      webSocket.broadcastTXT("BOMBA SUCIA APAGADA");
    }
    break;
  }
}

//Modo Automatico/Manual
bool autoModeEnabled = true;
SinricProSwitch &myAutoMode   = SinricPro[DEVICE_ID_MODE];

// Acceso a Bombas de agua
SinricProSwitch &myBombaLimpia  = SinricPro[DEVICE_ID_LIMPIA];
SinricProSwitch &myBombaSucia   = SinricPro[DEVICE_ID_SUCIA];

// Callbacks Sinric Pro
bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId == DEVICE_ID_LIMPIA) {
    digitalWrite(BOMBA_LIMPIA_PIN, state ? HIGH : LOW);
    Serial.printf("SinricPro: BOMBA_LIMPIA %s\n", state ? "ON" : "OFF");
  } else if (deviceId == DEVICE_ID_SUCIA) {
    digitalWrite(BOMBA_SUCIA_PIN, state ? HIGH : LOW);
    Serial.printf("SinricPro: BOMBA_SUCIA %s\n", state ? "ON" : "OFF");
  }
  return true;
}

bool onModeState(const String &deviceId, bool &state) {
  if (deviceId == DEVICE_ID_MODE) {
    autoModeEnabled = state;
    if (autoModeEnabled) {
      Serial.println("SinricPro: AUTO MODE ACTIVADO");
    } else {
      Serial.println("SinricPro: MANUAL MODE ACTIVADO");
    }
  }
  return true;
}

void setup() {
    // Conexión
    Serial.begin(115200);
    intentoconexion("Bebedero-Config", "Amadeus2025");
    Serial.println(WiFi.localIP());

    // Configuracion sensor infrarrojo
    pinMode(IR_PIN, INPUT);
    pinMode(IR_PIN2, INPUT);

    // Configurarcion pines de bombas de agua
    pinMode(BOMBA_LIMPIA_PIN, OUTPUT);
    pinMode(BOMBA_SUCIA_PIN, OUTPUT);
    digitalWrite(BOMBA_LIMPIA_PIN, LOW);
    digitalWrite(BOMBA_SUCIA_PIN, LOW);
    
    // Sensores de nivel (con resistencia pull-up)
    pinMode(SENSOR_CHIQUITO_LLENO, INPUT_PULLUP);
    pinMode(SENSOR_CHIQUITO_VACIO, INPUT_PULLUP);
    pinMode(SENSOR_LIMPIA_LLENO, INPUT_PULLUP);
    pinMode(SENSOR_LIMPIA_VACIO, INPUT_PULLUP);
    pinMode(SENSOR_AGUA_SUCIA_LLENO, INPUT_PULLUP);

    // Sensor de turbidez
    pinMode(TURBIDITY_PIN, INPUT);

   //Interaccion con Alexa
    myBombaLimpia.onPowerState(onPowerState);
    myBombaSucia.onPowerState(onPowerState);
    myAutoMode.onPowerState(onModeState);

    SinricPro.onConnected([](){ Serial.println("SinricPro Connected"); });
    SinricPro.onDisconnected([](){ Serial.println("SinricPro Disconnected"); });

    SinricPro.begin(APP_KEY, APP_SECRET);
    SinricPro.handle(); 
    
    // Comunicación por red
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket listo en el puerto 81");
}


void notificarEstadoSensores() {
    delay(500);
  if (!clienteConectado) return;

  if (digitalRead(SENSOR_LIMPIA_LLENO) == LOW)
    {webSocket.broadcastTXT("Agua limpia: LLENO");}
  if (digitalRead(SENSOR_LIMPIA_VACIO) == LOW)
    {webSocket.broadcastTXT("Agua limpia: VACIO");}
  if (digitalRead(SENSOR_CHIQUITO_LLENO) == LOW)
    {webSocket.broadcastTXT("Tanque chiquito: LLENO");}
  if (digitalRead(SENSOR_CHIQUITO_VACIO) == LOW)
    {webSocket.broadcastTXT("Tanque chiquito: VACIO");}
  if (digitalRead(SENSOR_AGUA_SUCIA_LLENO) == HIGH)
    {webSocket.broadcastTXT("Tanque sucio: LLENO");}
  if (digitalRead(IR_PIN) == HIGH) {
    webSocket.broadcastTXT("Bebedero: Sucio");
  } else { webSocket.broadcastTXT("Bebedero: Limpio"); }
}

void loop() {
  SinricPro.handle();   // Maneja eventos de SinricPro
  loopAP();             // Tu rutina de WiFi/Eeprom
  webSocket.loop();     // Atiende WebSocket

  int presencia     = digitalRead(IR_PIN2);
  int valorTurbidez = analogRead(TURBIDITY_PIN);
  bool turbidezAlta = (valorTurbidez < UMBRAL_TURBIDEZ);

  // Estados de nivel
  bool chiquitoLleno = digitalRead(SENSOR_CHIQUITO_LLENO)    == HIGH;
  bool chiquitoVacio = digitalRead(SENSOR_CHIQUITO_VACIO)    == LOW;
  bool limpiaVacio   = digitalRead(SENSOR_LIMPIA_VACIO)      == LOW;
  bool suciaLlena    = digitalRead(SENSOR_AGUA_SUCIA_LLENO)  == HIGH;
  bool limpiaLleno = digitalRead(SENSOR_LIMPIA_LLENO) == HIGH;
  //Imprime señal de infrarrojo
  static int prevPresencia = LOW;
  if (presencia == HIGH && prevPresencia == LOW) {
    Serial.println("Señal HIGH: Movimiento detectado");
  }
  prevPresencia = presencia;

  //Imprime señal de turbidez
  //if (valorTurbidez == 4095) {
  //  Serial.print("Limpio: ");
  //  Serial.println(valorTurbidez);
  //}

// Activacion de bombas de agua
  if (!clienteConectado) {
    if (autoModeEnabled) {
      // Modo AUTOMÁTICO: sólo corre la lógica sensórica
      if (presencia == HIGH || turbidezAlta) {
        // 1) VACIADO PRIORITARIO si hay agua turbia
      Serial.print("Reservorio limpio   - LLENO?   "); Serial.println(limpiaLleno    ? "Sí" : "No");
      Serial.print("Reservorio limpio   - VACÍO?   "); Serial.println(limpiaVacio    ? "Sí" : "No");
      Serial.print("Recipiente chiquito - LLENO? "); Serial.println(chiquitoLleno  ? "Sí" : "No");
      Serial.print("Recipiente chiquito - VACÍO?  "); Serial.println(chiquitoVacio  ? "Sí" : "No");
      Serial.print("Reservorio sucio    - LLENO?   "); Serial.println(suciaLlena     ? "Sí" : "No");
        Serial.print(turbidezAlta);
        if (chiquitoLleno && !suciaLlena) {
          digitalWrite(BOMBA_LIMPIA_PIN, LOW);
          Serial.println("Vaciando agua turbia...");
          delay(1000);
          digitalWrite(BOMBA_SUCIA_PIN, HIGH);
          while (digitalRead(SENSOR_CHIQUITO_VACIO)  != LOW &&
                 digitalRead(SENSOR_AGUA_SUCIA_LLENO) != HIGH) {
            /* Espera hasta que termine */
          }
          digitalWrite(BOMBA_SUCIA_PIN, LOW);
          Serial.println("Vaciado completado");
        } else {
          digitalWrite(BOMBA_SUCIA_PIN, LOW);
        }
        // 2) RELLENADO SÓLO SI FALTA AGUA LIMPIA
        if (!chiquitoLleno && !limpiaVacio) {
          digitalWrite(BOMBA_SUCIA_PIN, LOW);
          Serial.println("Llenando con agua limpia...");
          digitalWrite(BOMBA_LIMPIA_PIN, HIGH);
        } else {
          digitalWrite(BOMBA_LIMPIA_PIN, LOW);
          Serial.println("Llenado completado");
        }
      } else {
        // No hay presencia ni turbidez: todo apagado
        digitalWrite(BOMBA_LIMPIA_PIN, LOW);
        digitalWrite(BOMBA_SUCIA_PIN,  LOW);
        Serial.println("Descanso: esperando detección...");
        delay(500);
      }
    }
    else {
      // Modo MANUAL: no ejecutar nada automático
    }
  } else {
    // Cliente conectado -> notifico sensores
    delay(1);
    notificarEstadoSensores();
  }

  delay(500);
}