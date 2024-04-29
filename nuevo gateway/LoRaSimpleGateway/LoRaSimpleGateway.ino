#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <Wire.h>
#include "EspMQTTClient.h"
#include "config.h"


#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)
#define BAND    915E6


EspMQTTClient client(
  SSID,
  PASSWORD,
  MQTT_IP,  // MQTT Broker server ip
  // "MQTTUsername",   // Can be omitted if not needed
  // "MQTTPassword",   // Can be omitted if not needed
  "Sonometro2",     // Client name that uniquely identify your device
  MQTT_PORT              // The MQTT port, default to 1883. this line can be omitted
);

String rssi = "RSSI --";
String packSize = "--";
String packet ;
byte nodesId[] = {0x00,0x01, 0x02}; // Definir la direccion de los nodos. Maximo 4 nodos.

struct NodeInfo{
  String id;
  String value;
};

struct NodeInfo nodes[sizeof(nodesId)];

byte numNodes=sizeof(nodesId);

void setup() {
  Serial.begin(115200);                   
  while (!Serial);
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS, RST, DI0);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Simple Gateway");
  Serial.println("Only receive messages from nodes");
  Serial.println("Tx: invertIQ enable");
  Serial.println("Rx: invertIQ disable");
  Serial.println();

  //LoRa.onReceive(onReceive);
  //LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
  for (byte i=0;i<numNodes;i++){
    nodes[i].id=nodesId[i];
  }
}

void loop() {
  int packetSize=0;

  if (runEvery(1000)) { // repeat every 1000 millis
    for(byte i =0;numNodes;i++){
      String destination=nodes[i].id;
      String message= destination+"send data";
      sendMessage(message);
      unsigned long lastTime=millis();
      do {
        packetSize = LoRa.parsePacket();
        if (packetSize) { readMessage(packetSize, i);  }
        delay(10);
      }
      while((millis()-lastTime)<200||packetSize);// intenta leer el mensaje del nodo  por hasta 200 ms 
      if (!packetSize){
        nodes[i].value="Nan";
      }
    }
    sendMqttBroker();

  }

  // LoRa.enableInvertIQ(); 
  // LoRa.receive();
}

void LoRa_rxMode(){
  LoRa.disableInvertIQ();               // normal mode
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  LoRa.enableInvertIQ();                // active invert I and Q signals
}

void sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(true);                 // finish packet and send it
}

void readMessage(int packetSize, int node) {
  packet ="";
  packSize = String(packetSize,DEC);
  String origin = (String)LoRa.read();
  String destination=nodes[node].value;
  for (int i = 0; i < packetSize-1; i++) { packet += (char) LoRa.read(); }
  rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
  Serial.println(packet);
  if (destination == origin){
    nodes[node].value=packet;
  }
}

boolean runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

void sendMqttBroker() {
  // Enviar información de todos los nodos a la función sendMqttBroker
  // for (int i = 0; i < numNodes; i++) {
  //   byte id = nodes[i].id;
  //   String topic = "avSanPablo/sensor"+id+"/leq";
  //   client.publish(topic, nodes[i].value);
  // }
  for (int i=0;i<numNodes; i++){
    Serial.print("nodeId = "+nodes[i].id);
    Serial.println("  nodeValue = "+nodes[i].value);
  }
}

void onConnectionEstablished()
{
  Serial.println("Conexion establecida");

}


