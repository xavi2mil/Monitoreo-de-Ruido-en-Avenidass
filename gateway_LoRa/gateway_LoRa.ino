#include <ArduinoJson.h>
#include <Wire.h>
#include <LoRa.h>
#include <ESP32Time.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h> 
#include "config.h"

#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)
#define BAND    915E6

WiFiClient espClient;
PubSubClient client(espClient);
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

// Inicializa la conexión wifi
void setup_wifi() {
  // Conexión WiFi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  // Loop hasta que nos reconectemos exitosamente
  while (!client.connected()) {
    Serial.println("Intentando conexión MQTT...");
    // Intentar conectarse
    if (client.connect("ESP32Client")) {
        Serial.println("Conectado al servidor MQTT.");
        // Suscribirse a un tema
        client.subscribe("time/response");
        client.subscribe("time/setTimeNode");
        client.subscribe("config/response");
        client.subscribe("nodeInfo/request");
        client.subscribe("nodeValues/request");
        client.subscribe("config/setConfigNode");
        client.publish("time/request", "time");
        client.publish("config/request", "config");
    } 
    else {
      // Si no nos podemos conectar, esperamos un momento antes de intentar de nuevo
      Serial.print("Error de conexión MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Intentando de nuevo en 5 segundos...");
      delay(5000);
    }
  }
}

// Funcion que se ejecuta cuando llega un mensaje a un topic
// al que el cliente esta suscrito
void callback(char* topic, byte* payload, unsigned int length) {
  String message="";
    for (int i = 0; i < length; i++) {
    message+=(char)payload[i];
    }
    Serial.println(message);
    if(String(topic)=="time/response"){
        isSetTime=true;
        rtc.setTime((unsigned long)message.toInt()+1);// Establecer el tiempo del gateway
        // mandar un mensaje a todos los nodos para que configuren su rtc
        // {"command":"setTime","time":rtc.getEpochTime, "nodeId":0}
        JsonDocument doc;
        doc["command"] = "setTime";
        doc["time"] = rtc.getEpoch();
        doc["nodeId"] = 0;          // 0 es la direccion para todos los nodos
        String output;
        doc.shrinkToFit();          // optional
        serializeJson(doc, output);
        LoRa_sendMessage(output);   // mandar a todos nos nodos la hora actual
        //delay(10);
        LoRa_rxMode();
    }
    else if(String(topic)=="config/response"){
        isSetConfig=true;
        // en el mensaje viene el json con la  configuracion del nodo que LoRa a todos los nodos
        LoRa_sendMessage(message);
        //delay(10);
        LoRa_rxMode();
    }
    else if(String(topic)=="time/setTimeNode"){ // En caso de que no se haya establecido el tiempo en los nodos con "time/response"
        JsonDocument doc;
        doc["command"] = "setTime";
        doc["time"] = rtc.getEpoch();
        doc["nodeId"] = message.toInt();          // En el mensaje se esepcifica el numero del nodo al que se le quiere ajustar la hora
        String output;
        doc.shrinkToFit();          // optional
        serializeJson(doc, output);
        LoRa_sendMessage(output);   // mandar a todos nos nodos la hora actual
        //delay(10);
        LoRa_rxMode();
    }
    else if(String(topic)=="config/setConfigNode"){ //En caso que no se haya establecida la configuracion en todos los nodos
      LoRa_sendMessage(message);
      //delay(10);
      LoRa_rxMode();
    }
    else if(String(topic)=="nodeValues/request"){
      LoRa_sendMessage(message);
      //delay(10);
      mess = message;
      LoRa_rxMode();
      // lastTime=millis();
    }
}

void setup(){
    Serial.begin(115200);
    rtc.setTime(1609459200);
    setup_wifi();
    client.setServer(MQTT_SERVER, PORT);
    client.setCallback(callback);
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
void loop(){
  if (!client.connected()) {reconnect();}
  client.loop();
  if(mess!=""){
    // if(millis()-lastTime>1000){
    //   LoRa_sendMessage(mess);
    //   LoRa_rxMode();
    //   lastTime=millis();
    // }
    lastTime = millis();
    int nTry=0;
    while(nTry<3){
      if (millis()-lastTime>700){
        LoRa_sendMessage(mess);
        LoRa_rxMode();
        lastTime=millis();
        nTry++;
      }
      int packetSize=LoRa.parsePacket();
      if (packetSize){
        String packet="";
        String output;
        JsonDocument doc;
        for (int i = 0; i < packetSize; i++) { packet += (char) LoRa.read(); }
        Serial.println(packet);
        DeserializationError error = deserializeJson(doc, packet);
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          // return;
          LoRa_sendMessage(mess);
          LoRa_rxMode();
        }
        else{
          char buff[packetSize+1];
          packet.toCharArray(buff,packetSize+1);
          // Lo que se recibe de un nodo es su configuracion
          // o sus valores guardados. Debe de ser un String de
          // un json. Se debe de asegurar que los que se recibio es un json.
          // de no ser así no se debe de hacer nada.
          client.publish("node/response", buff);
          mess="";
          break;
        }
      }
      delay(5);
    }
  }
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