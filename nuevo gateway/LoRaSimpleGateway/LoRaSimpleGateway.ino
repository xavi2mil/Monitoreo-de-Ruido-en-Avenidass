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

SSD1306 display(0x3c, 21, 22);
String rssi = "RSSI --";
String packSize = "--";
String packet ;

const int numNodes=4;
String nodesId[] = {"1", "2", "3", "4"}; // Definir la direccion, de un caracter, de los nodos. Máximo 4 nodos.

struct NodeInfo{
  String id;
  String value;
};

struct NodeInfo nodes[numNodes];


void setup() {
  delay(50);
  digitalWrite(16, HIGH);
  Serial.begin(115200);                   
  while (!Serial);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); 
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);
  }

  LoRa.setSignalBandwidth(125E3);
  LoRa.setSpreadingFactor(7);
  LoRa.setCodingRate4(7);
  LoRa.enableCrc();

  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Gateway");
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
      sendMessage(nodes[i].id);
      unsigned long lastTime=millis();
      LoRa_rxMode();
      while(true){
        int packetSize = LoRa.parsePacket();
        if (packetSize) { 
          readMessage(packetSize);
          nodes[i].value = packet;
          break;
        }
        if((millis()-lastTime)>(900/numNodes)){
          nodes[i].value="NaN";
          break;
        }
      }
    
    }
    sendMqttBroker();
  }
  
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
  LoRa.endPacket(false);                 // finish packet and send it
}

void readMessage(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);
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
// Enviar información de todos los nodos al broker MQTT
void sendMqttBroker() {
  // 
  for (int i = 0; i < numNodes; i++) {
    String id = nodes[i].id;
    String topic = "avSanPablo/sensor"+id+"/leq";
    client.publish(topic, nodes[i].value);
  }
  for (int i=0;i<numNodes; i++){
    Serial.print("nodeId = "+ (String)nodes[i].id);
    Serial.println("  nodeValue = "+ nodes[i].value);
  }
}

void onConnectionEstablished(){
  Serial.println("Conexion establecida");

}
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
  display.drawString(0, 15, "Nodos");
  display.drawString(40, 15, "Valor");
  for(int i=0;i<numNodes;i++){
    display.drawString(0, 18+8*(i+1), nodes[i].id);
    display.drawString(40, 18+8*(i+1),nodes[i].value );
  }
  // display.drawString(int16_t x, int16_t y, String text);
  //display.drawStringMaxWidth(0 , 26 , 128, packet);
  //display.drawString(0, 0, rssi);

  // display.drawString(0, 40, "wifi:" + conexionWifi);
  // display.drawString(0, 50, "MQTT:"+ conexionMQTT);
  display.display();
  //Serial.println(rssi);
  //Serial.println(packet);
}


