/*
  LoRa Simple Gateway/Node Exemple

  This code uses InvertIQ function to create a simple Gateway/Node logic.

  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()

  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()

  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.

  This code receives messages and sends a message every second.

  InvertIQ function basically invert the LoRa I and Q signals.

  See the Semtech datasheet, http://www.semtech.com/images/datasheet/sx1276.pdf
  for more on InvertIQ register 0x33.

  created 05 August 2018
  by Luiz H. Cassettari
*/

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

String rssi = "RSSI --";
String packSize = "--";
String packet ;
byte nodesId[] = {0x00,0x01, 0x02}; // Definir la direccion de los nodos. Maximo 4 nodos.

struct NodeInfo{
  byte id;
  String value;
};

struct NodeInfo nodes[sizeof(nodesId)];

byte numNodes=sizeof(nodesId);

void setup() {
  Serial.begin(115200);                   // initialize serial
  while (!Serial);
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

  //LoRa.onReceive(onReceive);
  //LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
  for (byte i=0;i<numNodes;i++){
    nodes[i].id=nodesId[i];
  }
}

void loop() {
  

  if (runEvery(1000)) { // repeat every 1000 millis
    for(byte i =0;numNodes;i++){
      byte destination=nodes[i].id;
      String message= "send data";
      sendMessage(message, destination);
      unsigned long lastTime=millis();
      do {
        int packetSize = LoRa.parsePacket();
        if (packetSize) { readMessage(packetSize);  }
        delay(10);
      }
      while((millis()-lastTime())<200||packetsize);// intenta leer el mensaje del nodo  por hasta 200 ms 
      if (!packetSize){
        nodes[i].value="Nan";
      }
    }

    byte destination=idNodes[0];
    String message = "HeLoRa World! ";
    message += "I'm a Gateway! ";
    message += millis();
    LoRa_sendMessage(message, destination); // manda un mensaje a los nodos
    Serial.println("Send Message!");
    //LoRa_rxMode();
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) { cbk(packetSize);  }
  delay(10);
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

void sendMessage(String message, byte id) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  Lora.write(id);
  LoRa.print(message);                  // add payload
  LoRa.endPacket(true);                 // finish packet and send it
}

void readMessage(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);

  for (int i = 0; i < packetSize; i++) { packet += (char) LoRa.read(); }
  rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
  Serial.println(packet);
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
  for (int i = 0; i < numNodes; i++) {
    // pub in place/nodeId/value
    // Aquí se enviaría la información de cada nodo a la función sendMqttBroker
    // Ejemplo:
    // sendMqttBroker(nodes[i].id, nodes[i].data);
  }
}

