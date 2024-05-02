#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <Wire.h>
#include "EspMQTTClient.h"
#include "config.h"
#include "SSD1306.h"


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
  "Gateway",     // Client name that uniquely identify your device
  MQTT_PORT              // The MQTT port, default to 1883. this line can be omitted
);
// EspMQTTClient client(
//   "W308-rep",
//   "W308-internet",
//   "172.17.76.13",  // MQTT Broker server ip
//   // "MQTTUsername",   // Can be omitted if not needed
//   // "MQTTPassword",   // Can be omitted if not needed
//   "Sonometro2",     // Client name that uniquely identify your device
//   1883              // The MQTT port, default to 1883. this line can be omitted
// );
SSD1306 display(0x3c, 21, 22);
String rssi = "RSSI --";
String packSize = "--";
String packet ;

byte nodesId[] = {0x01,0x2}; // Definir la direccion de los nodos. Maximo 4 nodos.

struct NodeInfo{
  byte id;
  String value;
};

struct NodeInfo nodes[sizeof(nodesId)];

int numNodes=sizeof(nodesId);

void printScreen(){
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  // display.drawString(0 , 15 , "Received "+ packSize + " bytes");
  display.drawString(0, 0, "Conexion WIFI/MQTT");
  if (client.isConnected()){
    display.fillRect(120, 2, 8, 8);
  }
  else{
    display.drawRect(120, 2, 8, 8);
  }
  //display.drawStringMaxWidth(0 , 26 , 128, packet);
  //display.drawString(0, 0, rssi);

  // display.drawString(0, 40, "wifi:" + conexionWifi);
  // display.drawString(0, 50, "MQTT:"+ conexionMQTT);
  display.display();
  //Serial.println(rssi);
  //Serial.println(packet);
}

void setup() {
  delay(50);
  digitalWrite(16, HIGH);
  Serial.begin(115200);                   
  while (!Serial);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS, RST, DI0);
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

  // LoRa.onReceive(onReceive);
  // LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
  for (int i=0;i<numNodes;i++){
    nodes[i].id=nodesId[i];
  }
  delay(1500);
}


void loop() {
  client.loop();
  printScreen();
  if(!client.isConnected()){
    delay(1000);
  }
  else if (runEvery(1000)){
    for (int i=0;i<numNodes;i++){
      sendByte(nodes[i].id);
      unsigned long lastTime=millis();
      // while((millis()-lastTime)<50);
      LoRa_rxMode();
      while(true){
        int packetSize = LoRa.parsePacket();
        if (packetSize) { 
          readMessage(packetSize);
          nodes[i].value = packet;
          break;
        }
        if((millis()-lastTime)>500){
          //Serial.println("nan");
          // Serial.println(lastTime);
          // Serial.println(millis());
          nodes[i].value="nan";
          break;
        }
      }
    
    }
    sendMqttBroker();

  }
  
  // int packetSize=0;

  // if (runEvery(1000)) { // repeat every 1000 millis
  //   for(byte i =0;numNodes;i++){
  //     String destination=nodes[i].id;
  //     String message= destination+"send data";
  //     sendMessage(message);
  //     unsigned long lastTime=millis();
  //     do {
  //       packetSize = LoRa.parsePacket();
  //       if (packetSize) { readMessage(packetSize, i);  }
  //       delay(10);
  //     }
  //     while((millis()-lastTime)<200||packetSize);// intenta leer el mensaje del nodo  por hasta 200 ms 
  //     if (!packetSize){
  //       nodes[i].value="Nan";
  //     }
  //   }
  //   sendMqttBroker();

  // }

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

void sendByte(byte nodeId){
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.write(nodeId);                  // add payload
  LoRa.endPacket(false);                 // finish packet and send it
}

void sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(false);                 // finish packet and send it
}

void readMessage(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);
//   String origin = (String)LoRa.read();
//   String destination=nodes[node].value;
  for (int i = 0; i < packetSize; i++) { packet += (char) LoRa.read(); }
  // rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
//   Serial.println(packet);
//   if (destination == origin){
//     nodes[node].value=packet;
//   }

  // Serial.println(packet);
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
    Serial.print("nodeId = "+ (String)nodes[i].id);
    Serial.println("  nodeValue = "+ nodes[i].value);
  }
}

void onConnectionEstablished(){
  Serial.println("Conexion establecida");

}


