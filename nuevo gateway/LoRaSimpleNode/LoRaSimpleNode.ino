#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <Wire.h>
#include <ESP32Time.h>
#include <ArduinoJson.h>

#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)
#define BAND    915E6

// String rssi = "RSSI --";
// String packSize = "--";
int id = 1;
int period=1;
String packet ;

struct NodeInfo{
  int id;
  float value; 
};
struct NodeInfo node;

ESP32Time rtc(-21600);

// void LoRa_onReceive(void *parameter) {
//   TickType_t xLastWakeTime = xTaskGetTickCount();
//   const TickType_t xDelay = pdMS_TO_TICKS(100); // Esperar 1 segundo
//   while (true) {
//     LoRa_rxMode();
//     int packetSize = LoRa.parsePacket();
//     if (packetSize) { 
//       readMessage(packetSize);
//       Serial.println(packet);
//       // if (packet == node.id) {
//       //   LoRa_sendMessage((String)value);

//       // }
//       // LoRa_rxMode();
//     }
//     vTaskDelayUntil(&xLastWakeTime, xDelay);
//     // Dormir durante un breve período para evitar sobrecargar el procesador
//   }
// }


void setup() {
  Serial.begin(115200);                   // initialize serial
  while (!Serial);
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS, RST, DI0);

  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  LoRa.setSignalBandwidth(125E3);
  LoRa.setSpreadingFactor(7);
  LoRa.setCodingRate4(7);
  LoRa.enableCrc();
  // xTaskCreatePinnedToCore(
  //   LoRa_onReceive,            // Función de la tarea
  //   "LoRaTask",                // Nombre de la tarea
  //   4096,                      // Tamaño del stack de la tarea
  //   NULL,                      // Parámetro de entrada de la tarea
  //   3,                         // Prioridad de la tarea (0 es la más baja)
  //   NULL,                      // Manejador de la tarea
  //   0                          // Núcleo en el que se ejecutará la tarea (0 o 1)
  // );
  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Simple Node");
  Serial.println("Only receive messages from gateways");
  Serial.println("Tx: invertIQ disable");
  Serial.println("Rx: invertIQ enable");
  Serial.println();

  //LoRa.onReceive(onReceive);
  //LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
  node.id=id;
  Serial.println(node.id);
}


void loop() {
  int packetSize = LoRa.parsePacket();
    if (packetSize) { 
      readMessage(packetSize);
      Serial.println(packet);
      // String input;

      JsonDocument doc;

      DeserializationError error = deserializeJson(doc, packet);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        // return;
      }
      else{
        const char* command = doc["command"]; // "setTime"
        long arg = doc["arg"]; // 1351824120
        int id = doc["nodeId"];
        if (id==node.id){
          if (String(command)=="setTime"){
            rtc.setTime(arg); 
            Serial.println(String(rtc.getLocalEpoch()));
          }
          else if(String(command)=="setPeriod"){

          }
          else if (String(command)=="getValues"){

          }
          else if(String(command)=="getInfo"){

          }
          else{

          }

        }
        
       
      }

      

      // Serial.println(packet.substring(0, packet.indexOf(":")));
      // Serial.println(packet.substring(packet.indexOf(":")+1));
      // if (packet == node.id) {
      //   LoRa_sendMessage((String)value);

      // }
      // LoRa_rxMode();
    }
  delay(10);
}

void LoRa_rxMode(){
  LoRa.enableInvertIQ();                // active invert I and Q signals
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  LoRa.disableInvertIQ();               // normal mode
}

void LoRa_sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(false);                 // finish packet and send it
}

void readMessage(int packetSize) {
  packet ="";
  // packSize = String(packetSize,DEC);
//   String origin = (String)LoRa.read();
//   String destination=nodes[node].value;
  for (int i = 0; i < packetSize; i++) { packet +=  (char)LoRa.read(); }
  // rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
//   Serial.println(packet);
//   if (destination == origin){
//     nodes[node].value=packet;
//   }
  // Serial.println(rssi);
  //Serial.println((String)packet);
}

// boolean runEvery(unsigned long interval)
// {
//   static unsigned long previousMillis = 0;
//   unsigned long currentMillis = millis();
//   if (currentMillis - previousMillis >= interval)
//   {
//     previousMillis = currentMillis;
//     return true;
//   }
//   return false;
// }

