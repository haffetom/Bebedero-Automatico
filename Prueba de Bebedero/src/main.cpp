// Credenciales Sinric Pro
#define APP_KEY        "26bd83ac-a6e8-4c45-bbdd-84b7b2adda3c"
#define APP_SECRET     "751b72a0-becc-49c7-9c07-81830cb65168-90da5f43-7fb8-4b0d-a0a3-5f8db7873b6d"
#define DEVICE_ID_LIMPIA  "68793f93030990a558da25ea"
#define DEVICE_ID_SUCIA   "68798d6ff64d827f96b4348a"

#include <Arduino.h>
#include <apwifieeprommode.h>
#include <EEPROM.h>
#include <WebSocketsServer.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>

// Puerto de comunicación
WebSocketsServer webSocket = WebSocketsServer(81);
bool clienteConectado = false;
bool estadoSalida = false;

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

// Sensor Ultrasonico
#define TRIG_PIN 27
#define ECHO_PIN 14

// Sensor Infrarrojo
#define IR_PIN 25
#define IR_PIN2 35

// Sensor de Turbidez
#define TURBIDITY_PIN    34    
const int UMBRAL_TURBIDEZ = 400;

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

void setup() {
    // Conexión
    Serial.begin(115200);
    intentoconexion("Bebedero-Config", "Amadeus2025");
    Serial.println(WiFi.localIP());

    // Configuracion sensor ultrasonico 
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

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

    // Acceso a Bombas de agua
    SinricProSwitch &myBombaLimpia  = SinricPro[DEVICE_ID_LIMPIA];
    SinricProSwitch &myBombaSucia   = SinricPro[DEVICE_ID_SUCIA];

    myBombaLimpia.onPowerState(onPowerState);
    myBombaSucia.onPowerState(onPowerState);

    SinricPro.onConnected([](){ Serial.println("SinricPro Connected"); });
    SinricPro.onDisconnected([](){ Serial.println("SinricPro Disconnected"); });

    SinricPro.begin(APP_KEY, APP_SECRET);
    SinricPro.handle(); 
    
    // Comunicación por red
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket listo en el puerto 81");
}

int medirDistanciaCm() {
    // Generar pulso de trigger de 10µs
    digitalWrite(TRIG_PIN, HIGH);
    delay(1);
    digitalWrite(TRIG_PIN, LOW);
    int duracion = pulseIn(ECHO_PIN, HIGH, 50000);
    Serial.println(duracion);
    if (duracion == 0) return -1;
    return ((duracion * 0.0343) / 2);
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
    SinricPro.handle(); // Maneja eventos de SinricPro

    loopAP();
    webSocket.loop();

    int presencia = digitalRead(IR_PIN2);
    int valorTurbidez = analogRead(TURBIDITY_PIN);
    bool turbidezAlta = (valorTurbidez > UMBRAL_TURBIDEZ);

    // Flags estáticas para control único de impresión
    static bool startedVaciado             = false;
    static bool finishedVaciadoNormal      = false;
    static bool finishedVaciadoPorLlenadoSucia = false;
    static bool startedLlenado               = false;
    static bool finishedLlenado             = false;

    if (presencia == LOW) {
        // Leer sensores
        bool chiquitoLleno = digitalRead(SENSOR_CHIQUITO_LLENO) == HIGH; //Lógica invertida
        bool chiquitoVacio = digitalRead(SENSOR_CHIQUITO_VACIO) == LOW;
        bool limpiaVacio   = digitalRead(SENSOR_LIMPIA_VACIO) == LOW;
        bool suciaLlena    = digitalRead(SENSOR_AGUA_SUCIA_LLENO) == HIGH; //Lógica invertida
        bool Sucio         = digitalRead(IR_PIN) == LOW; //Lógica invertida

        if (!clienteConectado) {
            // ——— Llenado automático con flags ———
            if (!chiquitoLleno && !limpiaVacio) {
                if (!startedLlenado) {
                    Serial.println("Llenando");
                    //startedLlenado = true;
                }
                digitalWrite(BOMBA_LIMPIA_PIN, HIGH);
            } else {
                digitalWrite(BOMBA_LIMPIA_PIN, LOW);
                if (!finishedLlenado) {
                    Serial.println("Recipiente Lleno");
                    //finishedLlenado = true;
                }
            }

            // Vaciado automático con Sensor de Turbidez
            //if (chiquitoLleno && turbidezAlta && !suciaLlena) {

            // Vaciado automático con Sensor Infrarrojo
            if (chiquitoLleno && !suciaLlena && Sucio) {
                if (!startedVaciado) {
                    Serial.println("Vaciando...");
                    digitalWrite(BOMBA_SUCIA_PIN, HIGH);
                    //startedVaciado = true;
                } digitalWrite(BOMBA_SUCIA_PIN, HIGH);
                // Espera dentro del while con el break original
                while (!chiquitoVacio && !suciaLlena) {
                    chiquitoVacio = digitalRead(SENSOR_CHIQUITO_VACIO) == LOW;
                    suciaLlena    = digitalRead(SENSOR_AGUA_SUCIA_LLENO) == HIGH;
                    
                    // Si se llena el tanque sucio, se ejecuta tu bloque original:
                    if (suciaLlena) {
                        if (!finishedVaciadoPorLlenadoSucia) {
                            Serial.println("Se detuvo el vaciado: tanque sucio lleno.");
                            //finishedVaciadoPorLlenadoSucia = true;
                        }
                        break;
                    }
                }

                // Apagar bomba y luego tu bloque de vaciado normal:
                digitalWrite(BOMBA_SUCIA_PIN, LOW);
                if (!finishedVaciadoNormal) {
                    Serial.println("Vaciado completado: recipiente limpio.");
                    finishedVaciadoNormal = true;
                }
            }
        } else {
           // webSocket.broadcastTXT("Objeto detectado a " + String(distancia) + " cm");
            delay(1000);
            notificarEstadoSensores();
        }
        delay(500);
    } else {
       // Reset de flags cuando el gato se va
        startedLlenado               = false;
        finishedLlenado              = false;
        startedVaciado                = false;
        finishedVaciadoNormal         = false;
        finishedVaciadoPorLlenadoSucia = false;
        digitalWrite(BOMBA_LIMPIA_PIN, LOW);
        digitalWrite(BOMBA_SUCIA_PIN, LOW);
        Serial.println("Descanso: esperando detección...");
        delay(2000);
    }

}
