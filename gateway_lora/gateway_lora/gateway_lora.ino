#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include "SSD1306.h" 
#include "images.h"
#include "EspMQTTClient.h"

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     23   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    915E6

EspMQTTClient client(
  "W308-rep",
  "W308-internet",
  "172.17.76.13",  // MQTT Broker server ip
  // "MQTTUsername",   // Can be omitted if not needed
  // "MQTTPassword",   // Can be omitted if not needed
  "Sonometro2",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);


SSD1306 display(0x3c, 21, 22);
String rssi = "RSSI --";
String packSize = "--";
String packet ;
String conexion_wifi;
String conexion_MQTT;

void loraData(){
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0 , 15 , "Received "+ packSize + " bytes");
  display.drawStringMaxWidth(0 , 26 , 128, packet);
  display.drawString(0, 0, rssi); 
  display.drawString(0, 40, "wifi:" + conexion_wifi);
  display.drawString(0, 50, "MQTT:"+ conexion_MQTT);
  display.display();
  Serial.println(rssi);
  Serial.println(packet);
}

void cbk(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);
  for (int i = 0; i < packetSize; i++) { packet += (char) LoRa.read(); }
  rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
  loraData();
}

void setup() {
  // pinMode(16,OUTPUT);
  // digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、
  
  Serial.begin(115200);
  while (!Serial);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  
  Serial.println();
  Serial.println("LoRa Receiver Callback");
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);  
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //LoRa.onReceive(cbk);
  LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
  delay(1500);
}

void loop() {
  client.loop();
  if (client.isMqttConnected()&client.isWifiConnected()){
    conexion_wifi="conectado";
    conexion_MQTT="conectado";
    int packetSize = LoRa.parsePacket();
    if (packetSize) { 
      cbk(packetSize);  
      client.publish("ruido_avenida/nivel_ruido/nodo_1", packet);
    }

    delay(10);
  }
  else{
    Serial.println("Conexion no establecida...");
    conexion_wifi="desconectado";
    conexion_MQTT="desconectado";
    loraData();
  }
}
void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  // client.subscribe("temperatura/temp", [](const String & payload) {
  //   Serial.println(payload);
  // });

  // // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  // // client.subscribe("mytopic/wildcardtest/#", [](const String & topic, const String & payload) {
  // //   Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  // // });

  // // Publish a message to "mytopic/test"
  // client.publish("temperatura/temp", "This is a message"); // You can activate the retain flag by setting the third parameter to true

  // // Execute delayed instructions
  // client.executeDelayed(5 * 1000, []() {
  //   client.publish("mytopic/wildcardtest/test123", "This is a message sent 5 seconds later");
  // });
}
