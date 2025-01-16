/*Gateway en modo offline.
Recibe y envia informacion usando el puerto serial*/

#include <ArduinoJson.h>
#include <Wire.h>
#include <LoRa.h>
#include <ESP32Time.h>
#include <SPI.h> 
#include "config.h"

#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)
#define BAND    915E6

// WiFiClient espClient;
// PubSubClient client(espClient);

ESP32Time rtc(-21600);
String packet;

// Configuración
int period;
int numMeasurements;
int numNodes;
unsigned long startTime;
unsigned long stopTime;
bool isSetConfig=false;
bool isSetTime=false;
int lastTime;
String mess="";

void setup(){
    Serial.begin(115200);
    rtc.setTime(1609459200);
    // setup_wifi();
    // client.setServer(MQTT_SERVER, PORT);
    // client.setCallback(callback);
    SPI.begin(SCK,MISO,MOSI,SS);
    LoRa.setPins(SS, RST, DI0);
    if (!LoRa.begin(BAND)) {
        Serial.println("No se pudo iniciar la conexion LoRa");
        while (true);
    }
    // configuracion de la conexion LoRa
    LoRa.setSignalBandwidth(125E3);
    LoRa.setTxPower(20);
    LoRa.setSpreadingFactor(7);
    LoRa.setCodingRate4(7);
    LoRa.enableCrc();
    LoRa_rxMode();
}
String inputBuffer = "";  // Buffer para acumular datos recibidos

void loop() {
  // Verifica si hay datos en el puerto serial
  while (Serial.available() > 0) {
    char incomingByte = Serial.read();  // Lee un byte del serial
    if (incomingByte == '}') {
      inputBuffer += incomingByte;
      // Si se detecta un delimitador de línea, procesa el comando completo
      Serial.println("M" + inputBuffer);  // Debug
      callback(inputBuffer);  // Procesa el comando recibido
      inputBuffer = "";  // Limpia el buffer después de procesar
    } else {
      // Acumula los datos en el buffer
      inputBuffer += incomingByte;
    }
    // Procesa comandos pendientes
    if (mess != "") {
      String response = sendCommandToNode(mess);  // Envía el comando y espera la respuesta
      if (!response.isEmpty()) {
        Serial.println(response);  // Envía la respuesta al serial
      } else {
        Serial.println("No response from node.");  // Manejo en caso de no recibir respuesta
      }
      mess = "";  // Limpia el mensaje pendiente
    }
  }

}

// Funcion que se ejecuta cuando llega un mensaje via
// serial
void callback(String message) {
    // Serial.println(message);
    JsonDocument doc;
    deserializeJson(doc, message);
    String command = doc["command"];
    if(command=="setTime"){
        isSetTime=true;
        rtc.setTime((unsigned long)doc["time"]);// Establecer el tiempo del gateway
        // mandar un mensaje a todos los nodos para que configuren su rtc
        // {"command":"setTime","time":rtc.getEpochTime, "nodeId":0}
        LoRa_sendMessage(message);   // mandar a todos nos nodos la hora actual
        //delay(10);
        LoRa_rxMode();
    }
    else if(command=="setConfig"){
        isSetConfig=true;
        // en el mensaje viene el json con la  configuracion del nodo que LoRa a todos los nodos
        numMeasurements = doc["numMeasurements"];
        period = doc["period"];
        numNodes = doc["numNodes"];
        LoRa_sendMessage(message);
        //delay(10);
        LoRa_rxMode();
        isSetConfig = true;
    }
    else if(command=="getValues"){
      // LoRa_sendMessage(message);
      //delay(10);
      mess = message;
      LoRa_rxMode();
      // lastTime=millis();
    }
    else if(command=="getInfo"){
      LoRa_sendMessage(message);
      //delay(10);
      mess = message;
      LoRa_rxMode();
      // lastTime=millis();
    }
    else if (command=="setStartTime"){
      startTime = doc["startTime"];
      LoRa_sendMessage(message);
      LoRa_rxMode();
    }
    else if (command == "startMeasurements"||command == "stopMeasurements"){
      LoRa_sendMessage(message);
      LoRa_rxMode();
    }
}

String sendCommandToNode(const String &command) {
  unsigned long lastTime = millis();
  int nTry = 0;

  while (nTry < 3) {  // Intenta enviar hasta 3 veces
    if (millis() - lastTime > 700) {
      LoRa_sendMessage(command);  // Envía el comando al nodo
      LoRa_rxMode();              // Cambia a modo recepción
      lastTime = millis();
      nTry++;
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {  // Si se recibe un paquete
      String packet = "";
      for (int i = 0; i < packetSize; i++) {
        packet += (char)LoRa.read();
      }

      // Intenta deserializar el paquete como JSON
      StaticJsonDocument<256> doc;  // Ajusta el tamaño según sea necesario
      DeserializationError error = deserializeJson(doc, packet);

      if (!error) {
        return packet;  // Devuelve la respuesta como JSON
      } else {
        Serial.println("Invalid JSON received. Retrying...");
      }
    }
    delay(5);  // Pequeña pausa para evitar bloqueos
  }

  return "";  // Devuelve una cadena vacía si no hay respuesta
}

void LoRa_sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(false);                 // finish packet and send it
}
void LoRa_rxMode(){
  LoRa.disableInvertIQ();               // normal mode
  LoRa.receive();                       // set receive mode
}
void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  LoRa.enableInvertIQ();                // active invert I and Q signals
}