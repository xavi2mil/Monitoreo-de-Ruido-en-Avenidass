#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include <driver/i2s.h>
#include "sos-iir-filter.h"
#include <Wire.h>
#include <ESP32Time.h>
#include <ArduinoJson.h>  

// informacion y configuracion del nodo
const uint8_t nodeId = 1;
int numMeasurements = 10; // Número de mediciones que guarda antes de enviarlas. 
bool startMeasurements = false;
int period=1;           // Tiempo de expocision o periodo 
float vBatt=0;            // Voltaje de la batería
const uint8_t vbatPin = 35;
const uint8_t bottomPin = 2;
double leq_1s;          // Valor equivalente de un segundo 

ESP32Time rtc(-21600);  // rtc UTC-6
WiFiClient espClient;
PubSubClient client(espClient);

#define LEQ_PERIOD        1           // second(s)
#define WEIGHTING         A_weighting // Also avaliable: 'C_weighting' or 'None' (Z_weighting)
#define LEQ_UNITS         "LAeq"      // customize based on above weighting used
#define DB_UNITS          "dBA"       // customize based on above weighting used
#define USE_DISPLAY       1

// NOTE: Some microphones require at least DC-Blocker filter
#define MIC_EQUALIZER     INMP441    // See below for defined IIR filters or set to 'None' to disable
#define MIC_OFFSET_DB     3.0103      // Default offset (sine-wave RMS vs. dBFS). Modify this value for linear calibration

// Customize these values from microphone datasheet
#define MIC_SENSITIVITY   -26         // dBFS value expected at MIC_REF_DB (Sensitivity value from datasheet)
#define MIC_REF_DB        94.0        // Value at which point sensitivity is specified in datasheet (dB)
#define MIC_OVERLOAD_DB   116.0       // dB - Acoustic overload point
#define MIC_NOISE_DB      33         // dB - Noise floor
#define MIC_BITS          24          // valid number of bits in I2S data
#define MIC_CONVERT(s)    (s >> (SAMPLE_BITS - MIC_BITS))
#define MIC_TIMING_SHIFT  0           // Set to one to fix MSB timing for some microphones, i.e. SPH0645LM4H-x

// Calculate reference amplitude value at compile time
constexpr double MIC_REF_AMPL = pow(10, double(MIC_SENSITIVITY)/20) * ((1<<(MIC_BITS-1))-1);

//
// I2S pins - Can be routed to almost any (unused) ESP32 pin.
//            SD can be any pin, inlcuding input only pins (36-39).
//            SCK (i.e. BCLK) and WS (i.e. L/R CLK) must be output capable pins
//
// Below ones are just example for my board layout, put here the pins you will use
//
#define I2S_WS            12
#define I2S_SCK           0 
#define I2S_SD            4

// I2S peripheral to use (0 or 1)
#define I2S_PORT          I2S_NUM_0

//
// Setup your display library (and geometry) here
// 
#if (USE_DISPLAY > 0)
  // ThingPulse/esp8266-oled-ssd1306, you may need the latest source and PR#198 for 64x48
  #include <SSD1306Wire.h>
  //#define OLED_GEOMETRY     GEOMETRY_64_48
  //#define OLED_GEOMETRY GEOMETRY_128_32
  #define OLED_GEOMETRY GEOMETRY_128_64
  #define OLED_FLIP_V       1
  SSD1306Wire display(0x3c, SDA, SCL, OLED_GEOMETRY);
#endif

//
// IIR Filters
//

// DC-Blocker filter - removes DC component from I2S data
// See: https://www.dsprelated.com/freebooks/filters/DC_Blocker.html
// a1 = -0.9992 should heavily attenuate frequencies below 10Hz
SOS_IIR_Filter DC_BLOCKER = { 
  gain: 1.0,
  sos: {{-1.0, 0.0, +0.9992, 0}}
      };

// 
// Equalizer IIR filters to flatten microphone frequency response
// See respective .m file for filter design. Fs = 48Khz.
//
// Filters are represented as Second-Order Sections cascade with assumption
// that b0 and a0 are equal to 1.0 and 'gain' is applied at the last step 
// B and A coefficients were transformed with GNU Octave: 
// [sos, gain] = tf2sos(B, A)
// See: https://www.dsprelated.com/freebooks/filters/Series_Second_Order_Sections.html
// NOTE: SOS matrix 'a1' and 'a2' coefficients are negatives of tf2sos output
//

// TDK/InvenSense ICS-43434
// Datasheet: https://www.invensense.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.1.pdf
// B = [0.477326418836803, -0.486486982406126, -0.336455844522277, 0.234624646917202, 0.111023257388606];
// A = [1.0, -1.93073383849136326, 0.86519456089576796, 0.06442838283825100, 0.00111249298800616];
SOS_IIR_Filter ICS43434 = { 
  gain: 0.477326418836803,
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
   {+0.96986791463971267, 0.23515976355743193, -0.06681948004769928, -0.00111521990688128},
   {-1.98905931743624453, 0.98908924206960169, +1.99755331853906037, -0.99755481510122113}
  }
};

// TDK/InvenSense ICS-43432
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/ICS-43432-data-sheet-v1.3.pdf
// B = [-0.45733702338341309   1.12228667105574775  -0.77818278904413563, 0.00968926337978037, 0.10345668405223755]
// A = [1.0, -3.3420781082912949, 4.4033694320978771, -3.0167072679918010, 1.2265536567647031, -0.2962229189311990, 0.0251085747458112]
SOS_IIR_Filter ICS43432 = {
  gain: -0.457337023383413,
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-0.544047931916859, -0.248361759321800, +0.403298891662298, -0.207346186351843},
    {-1.909911869441421, +0.910830292683527, +1.790285722826743, -0.804085812369134},
    {+0.000000000000000, +0.000000000000000, +1.148493493802252, -0.150599527756651}
  }
};

// TDK/InvenSense INMP441
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/INMP441.pdf
// B ~= [1.00198, -1.99085, 0.98892]
// A ~= [1.0, -1.99518, 0.99518]
SOS_IIR_Filter INMP441 = {
  gain: 1.00197834654696, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091}
  }
};

// Infineon IM69D130 Shield2Go
// Datasheet: https://www.infineon.com/dgdl/Infineon-IM69D130-DS-v01_00-EN.pdf?fileId=5546d462602a9dc801607a0e46511a2e
// B ~= [1.001240684967527, -1.996936108836337, 0.995703101823006]
// A ~= [1.0, -1.997675693595542, 0.997677044195563]
// With additional DC blocking component
SOS_IIR_Filter IM69D130 = {
  gain: 1.00124068496753,
  sos: {
    {-1.0, 0.0, +0.9992, 0}, // DC blocker, a1 = -0.9992
    {-1.994461610298131, 0.994469278738208, +1.997675693595542, -0.997677044195563}
  }
};

// Knowles SPH0645LM4H-B, rev. B
// https://cdn-shop.adafruit.com/product-files/3421/i2S+Datasheet.PDF
// B ~= [1.001234, -1.991352, 0.990149]
// A ~= [1.0, -1.993853, 0.993863]
// With additional DC blocking component
SOS_IIR_Filter SPH0645LM4H_B_RB = {
  gain: 1.00123377961525, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-1.0, 0.0, +0.9992, 0}, // DC blocker, a1 = -0.9992
    {-1.988897663539382, +0.988928479008099, +1.993853376183491, -0.993862821429572}
  }
};

//
// Weighting filters
//

//
// A-weighting IIR Filter, Fs = 48KHz 
// (By Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
// B = [0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003]
// A = [1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968]
SOS_IIR_Filter A_weighting = {
  gain: 0.169994948147430, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
    {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
    {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989}
  }
};

//
// C-weighting IIR Filter, Fs = 48KHz 
// Designed by invfreqz curve-fitting, see respective .m file
// B = [-0.49164716933714026, 0.14844753846498662, 0.74117815661529129, -0.03281878334039314, -0.29709276192593875, -0.06442545322197900, -0.00364152725482682]
// A = [1.0, -1.0325358998928318, -0.9524000181023488, 0.8936404694728326   0.2256286147169398  -0.1499917107550188, 0.0156718181681081]
SOS_IIR_Filter C_weighting = {
  gain: -0.491647169337140,
  sos: { 
    {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
    {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
    {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.0356365756680430}
  }
};


//
// Sampling
//
#define SAMPLE_RATE       48000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS       32    // bits
#define SAMPLE_T          int32_t 
#define SAMPLES_SHORT     (SAMPLE_RATE / 8) // ~125ms
#define SAMPLES_LEQ       (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE     (SAMPLES_SHORT / 16)
#define DMA_BANKS         32

// Data we push to 'samples_queue'
struct sum_queue_t {
  // Sum of squares of mic samples, after Equalizer filter
  float sum_sqr_SPL;
  // Sum of squares of weighted mic samples
  float sum_sqr_weighted;
  // Debug only, FreeRTOS ticks we spent processing the I2S data
  uint32_t proc_ticks;
};
QueueHandle_t samples_queue;

// Static buffer for block of samples
float samples[SAMPLES_SHORT] __attribute__((aligned(4)));

//
// I2S Microphone sampling setup 
//
void mic_i2s_init() {
  // Setup I2S to sample mono channel for SAMPLE_RATE * SAMPLE_BITS
  // NOTE: Recent update to Arduino_esp32 (1.0.2 -> 1.0.3)
  //       seems to have swapped ONLY_LEFT and ONLY_RIGHT channels
  const i2s_config_t i2s_config = {
    mode: i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    sample_rate: SAMPLE_RATE,
    bits_per_sample: i2s_bits_per_sample_t(SAMPLE_BITS),
    channel_format: I2S_CHANNEL_FMT_ONLY_LEFT,
    communication_format: i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    intr_alloc_flags: ESP_INTR_FLAG_LEVEL1,
    dma_buf_count: DMA_BANKS,
    dma_buf_len: DMA_BANK_SIZE,
    use_apll: true,
    tx_desc_auto_clear: false,
    fixed_mclk: 0
  };
  // I2S pin mapping
  const i2s_pin_config_t pin_config = {
    bck_io_num:   I2S_SCK,  
    ws_io_num:    I2S_WS,    
    data_out_num: -1, // not used
    data_in_num:  I2S_SD   
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  #if (MIC_TIMING_SHIFT > 0) 
    // Undocumented (?!) manipulation of I2S peripheral registers
    // to fix MSB timing issues with some I2S microphones
    REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));   
    REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);  
  #endif
  
  i2s_set_pin(I2S_PORT, &pin_config);

  //FIXME: There is a known issue with esp-idf and sampling rates, see:
  //       https://github.com/espressif/esp-idf/issues/2634
  //       In the meantime, the below line seems to set sampling rate at ~47999.992Hz
  //       fifs_req=24576000, sdm0=149, sdm1=212, sdm2=5, odir=2 -> fifs_reached=24575996  
  //NOTE:  This seems to be fixed in ESP32 Arduino 1.0.4, esp-idf 3.2
  //       Should be safe to remove...
  //#include <soc/rtc.h>
  //rtc_clk_apll_enable(1, 149, 212, 5, 2);
}


// I2S Reader Task
//
// Rationale for separate task reading I2S is that IIR filter
// processing cam be scheduled to different core on the ESP32
// while main task can do something else, like update the 
// display in the example
//
// As this is intended to run as separate hihg-priority task, 
// we only do the minimum required work with the I2S data
// until it is 'compressed' into sum of squares 
//
// FreeRTOS priority and stack size (in 32-bit words) 
#define I2S_TASK_PRI   4
#define I2S_TASK_STACK 2048
//
void mic_i2s_reader_task(void* parameter) {
  mic_i2s_init();

  // Discard first block, microphone may have startup time (i.e. INMP441 up to 83ms)
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

  while (true) {
    // Block and wait for microphone values from I2S
    //
    // Data is moved from DMA buffers to our 'samples' buffer by the driver ISR
    // and when there is requested ammount of data, task is unblocked
    //
    // Note: i2s_read does not care it is writing in float[] buffer, it will write
    //       integer values to the given address, as received from the hardware peripheral. 
    i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read, portMAX_DELAY);

    TickType_t start_tick = xTaskGetTickCount();
    
    // Convert (including shifting) integer microphone values to floats, 
    // using the same buffer (assumed sample size is same as size of float), 
    // to save a bit of memory
    SAMPLE_T* int_samples = (SAMPLE_T*)&samples;
    for(int i=0; i<SAMPLES_SHORT; i++) samples[i] = MIC_CONVERT(int_samples[i]);

    sum_queue_t q;
    // Apply equalization and calculate Z-weighted sum of squares, 
    // writes filtered samples back to the same buffer.
    q.sum_sqr_SPL = MIC_EQUALIZER.filter(samples, samples, SAMPLES_SHORT);

    // Apply weighting and calucate weigthed sum of squares
    q.sum_sqr_weighted = WEIGHTING.filter(samples, samples, SAMPLES_SHORT);

    // Debug only. Ticks we spent filtering and summing block of I2S data
    q.proc_ticks = xTaskGetTickCount() - start_tick;

    // Send the sums to FreeRTOS queue where main task will pick them up
    // and further calcualte decibel values (division, logarithms, etc...)
    xQueueSend(samples_queue, &q, portMAX_DELAY);
  }
}

void mqtt_conect(void *parameter) {
  setup_wifi();
  client.setServer(MQTT_SERVER, PORT);
  client.setCallback(callback);
  JsonDocument doc;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xDelay = pdMS_TO_TICKS(5); // Esperar 5 ms
  while (true) {
    if (!client.connected()) {reconnect();}
    client.loop();
    vTaskDelayUntil(&xLastWakeTime, xDelay);
    // Dormir durante un breve período para evitar sobrecargar el procesador
  }
}


void setup(){
  // If needed, now you can actually lower the CPU frquency,
  // i.e. if you want to (slightly) reduce ESP32 power consumption 
  setCpuFrequencyMhz(80); // It should run as low as 80MHz
  
  Serial.begin(112500);
  delay(1000); // Safety
  pinMode(vbatPin, INPUT);
  pinMode(2, INPUT_PULLDOWN);
  #if (USE_DISPLAY > 0)
    display.init();
    #if (OLED_FLIP_V > 0)
      display.flipScreenVertically();
    #endif
    display.setFont(ArialMT_Plain_16);
  #endif
  // Create FreeRTOS queue
  samples_queue = xQueueCreate(8, sizeof(sum_queue_t));
  
  // Create the I2S reader FreeRTOS task
  // NOTE: Current version of ESP-IDF will pin the task 
  //       automatically to the first core it happens to run on
  //       (due to using the hardware FPU instructions).
  //       For manual control see: xTaskCreatePinnedToCore
  xTaskCreatePinnedToCore(
    mic_i2s_reader_task, 
    "Mic I2S Reader", 
    I2S_TASK_STACK, 
    NULL, 
    I2S_TASK_PRI, 
    NULL,
    1
  );
  xTaskCreatePinnedToCore(
    mqtt_conect,            // Función de la tarea //Maneja el bucle de mqtt
    "MQTT Task",                // Nombre de la tarea
    4096,                      // Tamaño del stack de la tarea
    NULL,                      // Parámetro de entrada de la tarea
    6,                         // Prioridad de la tarea (0 es la más baja)
    NULL,                      // Manejador de la tarea
    0                          // Núcleo en el que se ejecutará la tarea (0 o 1)
  );
  sum_queue_t q;
  uint32_t Leq_samples = 0;
  double Leq_sum_sqr = 0;
  double Leq_dB = 0;
  JsonDocument doc;
  double leq=0;
  double sum=0;
  int n = 0;
  int measurement=0;
  JsonArray values=doc["values"].to<JsonArray>();
  JsonArray time = doc["time"].to<JsonArray>();
  bool stateDisplay = true;
  unsigned long lastTime = millis();

  // Read sum of samaples, calculated by 'i2s_reader_task'
  while (xQueueReceive(samples_queue, &q, portMAX_DELAY)) {

    // Calculate dB values relative to MIC_REF_AMPL and adjust for microphone reference
    double short_RMS = sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT);
    double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(short_RMS / MIC_REF_AMPL);

    // In case of acoustic overload or below noise floor measurement, report infinty Leq value
    if (short_SPL_dB > MIC_OVERLOAD_DB) {
      Leq_sum_sqr = INFINITY;
    } else if (isnan(short_SPL_dB) || (short_SPL_dB < MIC_NOISE_DB)) {
      Leq_sum_sqr = -INFINITY;
    }

    // Accumulate Leq sum
    Leq_sum_sqr += q.sum_sqr_weighted;
    Leq_samples += SAMPLES_SHORT;

    // When we gather enough samples, calculate new Leq value
    if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD) {
      double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
      Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
      Leq_sum_sqr = 0;
      Leq_samples = 0;
      leq_1s=Leq_dB;
      Serial.println(leq_1s);
      // startTime<rtc.getEpoch()<stopTime
      
      if(n<period && startMeasurements){
        sum+=pow(10, (leq_1s/10));
        n++;
      }
      if(n>=period){
        n=0;
        leq=10*log10(sum/period);
        sum=0;
        measurement++;
        // char buffer[10];
        // dtostrf(leq, 5, 2, buffer);
        values.add(leq);
        time.add(rtc.getEpoch()+21600);// correccion de 21600 para utc-6
      }
      if(measurement>=numMeasurements){
        measurement=0;
        doc["nodeId"]=nodeId;
        doc.shrinkToFit();  // optional
        char jsonMeasurements[512];
        serializeJson(doc, jsonMeasurements);
        if (client.connected()){
          client.publish("sono/measurements", jsonMeasurements);
        }
        JsonArray values=doc["values"].to<JsonArray>(); // limpiar el array values
        JsonArray time = doc["time"].to<JsonArray>();   // limpiar el array de time
        Serial.println(jsonMeasurements);
        }
    }
    
    #if (USE_DISPLAY > 0)
      if (digitalRead(bottomPin) == HIGH){
        if(millis()-lastTime > 500){
          stateDisplay = !stateDisplay;
          lastTime = millis();
        }
      }
      if (!stateDisplay){
        display.displayOff();
      }
      else{
        display.displayOn();
        display.clear();

        // It is important to somehow notify when the deivce is out of its range
        // as the calculated values are very likely with big error
        if (Leq_dB > MIC_OVERLOAD_DB) {
          // Display 'Overload' if dB value is over the AOP
          display.drawString(0, 24, "Overload");
        } else if (isnan(Leq_dB) || (Leq_dB < MIC_NOISE_DB)) {
          // Display 'Low' if dB value is below noise floor
          display.drawString(0, 24, "Low");
        }
        
        // The 'short' Leq line
        double short_Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(q.sum_sqr_weighted) / SAMPLES_SHORT) / MIC_REF_AMPL);
        uint16_t len = min(max(0, int(((short_Leq_dB - MIC_NOISE_DB) / MIC_OVERLOAD_DB) * (display.getWidth()-1))), display.getWidth()-1);
        display.drawHorizontalLine(0, 0, len);
        display.drawHorizontalLine(0, 1, len);
        display.drawHorizontalLine(0, 2, len);
        
        // The Leq numeric decibels
        display.drawString(0, 4, String(Leq_dB, 1) + " " + DB_UNITS);
        display.drawString(0, 30, "Nodo "+String(nodeId));
        display.drawString(0, 48, rtc.getTime());
        
        display.display();
      }
    #endif // USE_DISPLAY
  }
}
void loop(){

}

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
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    // Intentar conectarse
    if (client.connect(client_id.c_str(), MQTT_USER_NAME, MQTT_PASSWORD)) {
        Serial.println("Conectado al servidor MQTT.");
        client.subscribe("sono/setTime");      // Recibe la fecha y hora actuales.
        client.subscribe("sono/setConfig");    // Recibe la configuración.
        client.subscribe("sono/info/request"); // Recibe una solicitud de envio de información.
        client.subscribe("sono/startMeasurements");
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

// Función que se ejecuta cuando llega un mensaje a un topic
// al que el cliente esta suscrito
void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  int nodoDestino=0;

  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }

  message[length] = '\0';
  Serial.println(message);

  if (strcmp(topic, "sono/setTime") == 0) { 
    rtc.setTime(atol(message) + 1);
  } else if (strcmp(topic, "sono/setConfig") == 0) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return; // Salir de la función si hay un error
    }
    period = doc["period"];
    numMeasurements = doc["numMeasurements"];
  } else if (strcmp(topic, "sono/info/request") == 0) {

    nodoDestino = atol(message);

    if (nodoDestino==nodeId||nodoDestino == 0){
      JsonDocument doc2;
      vBatt = (float)(analogRead(vbatPin)) / 4095 * 2 * 3.3 * 1.1;
      doc2["battery"] = vBatt;
      doc2["period"] = period;
      doc2["numMeasurements"] = numMeasurements;
      doc2["nodeId"] = nodeId;

      char jsonBuffer[256]; 
      serializeJson(doc2, jsonBuffer);

      client.publish("sono/info/response", jsonBuffer);
      Serial.println(jsonBuffer);
    }
    
  }else if (strcmp(topic, "sono/startMeasurements") == 0){
    if (strcmp(message, "true")==0){
      startMeasurements = true;
    }
    else if(strcmp(message, "false")==0){
      startMeasurements = false;
    }
  }
}
