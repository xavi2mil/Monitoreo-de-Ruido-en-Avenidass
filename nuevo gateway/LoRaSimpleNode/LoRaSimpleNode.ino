#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <Wire.h>

#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)
#define BAND    915E6

// String rssi = "RSSI --";
// String packSize = "--";
String id = "1";

String packet ;

struct NodeInfo{
  String id;
  String value;
};
struct NodeInfo node;


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

int count=0;

void loop() {

  int packetSize = LoRa.parsePacket();
  if (packetSize) { 
    readMessage(packetSize);
    Serial.println(packet);
    // unsigned long lastTime=millis();
    // while((millis()-lastTime)<70);
    if (packet==(String)node.id){
      LoRa_sendMessage((String)count);
      LoRa_rxMode();
      count++;
    }
    
  }
  
  
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

