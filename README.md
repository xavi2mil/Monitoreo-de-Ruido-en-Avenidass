# Manual de Usuario - [Red de Sensores para el Monitoreo Acústico]

## Introducción
El presente sistema consiste en una red de sensores diseñados para monitorear el ruido acústico en tiempo real. La red está conformada por varios sonómetros basados en el microcontrolador LoRa32, un micrófono digital INMP441, y una batería recargable, todo alojado en una carcasa impresa en 3D.

![Imagen del sonómetro](imagenes/sonometro.jpg "Sonometro/Nodo")

La red utiliza una topología de estrella en la que los sonómetros se comunican directamente con un gateway central, que también está basado en un LoRa32. Este gateway actúa como puente entre los sensores y una computadora central, permitiendo la transferencia de datos de ruido en tiempo real. La conexión entre el gateway y la computadora puede realizarse mediante un enlace USB o el protocolo MQTT.

![Imagen de la topología de la red](imagenes/topologia.jpg "topología")

Los sonómetros miden el nivel equivalente de ruido (**Leq**) cada segundo. Sin embargo, para optimizar la transmisión mediante LoRa, los valores de **Leq** se almacenan localmente junto con su marca de tiempo. Esto permite enviar los datos en intervalos más grandes, reduciendo la frecuencia de transmisión y facilitando la recolección de datos.

Los parámetros configurables desde la interfaz gráfica de Node-RED son:
- El período de tiempo para calcular el **Leq**.
- El número de mediciones almacenadas antes de enviarlas mediante LoRa.
- El número total de nodos a los que se solicitarán valores. La numeración de los nodos debe ser secuencial, de 1 a *n*.

Además, los datos recolectados se almacenan en una base de datos **InfluxDB** para su análisis posterior.

---

## Tabla de Contenidos
1. [Descripción del Hardware](#descripción-del-hardware)
   1.1. [Componentes del sonómetro](#componentes-del-sonómetro)
   1.2. [Gateway](#gateway)
   1.3. [Esquema de conexión](#esquema-de-conexión)
2. [Programación de los Dispositivos](#programación-de-los-dispositivos)
3. [Uso de los Dispositivos](#uso-de-los-dispositivos)
   3.1. [Configuración inicial](#configuración-inicial)
   3.2. [Mediante Conexión USB](#mediante-conexión-usb)
   3.3. [Mediante Conexión MQTT](#mediante-conexión-mqtt)
4. [Configuración de Node-RED](#configuración-de-node-red)
   4.1. [Configuración de nodos InfluxDB](#configuración-de-nodos-influxdb)
   4.2. [Configuración de nodos Serial](#configuración-de-nodos-serial)
   4.3. [Configuración de nodos MQTT](#configuración-de-nodos-mqtt)
5. [Registro y Visualización de los Niveles de Ruido](#registro-y-visualización-de-los-niveles-de-ruido)
6. [Solución de Problemas](#solución-de-problemas)

---

## Descripción del Hardware
### Componentes del sonómetro
+ Tarjeta TTGO-LoRa32-V2.1 T3_V1.6 (LoRa32)
+ Micrófono INMP441
+ Batería recargable de 1200 o 2000 mAh

### Gateway
+ Tarjeta TTGO-LoRa32-V2.1 T3_V1.6 (LoRa32)

### Esquema de Conexión
**Diagrama de conexión entre el LoRa32 y el INMP441:**
![Imagen de las conexiones entre los componentes del sonómetro](imagenes/diagrama_conexiones.jpg "conexiones")
**Especificaciones de pines utilizados**.

| LoRa32 | INMP441 |
|--------------|--------------|
| 12      | WS     |
| 0       | CLK    |
| 4       | SD     |
| VDD     | VDD    |
| GND     | GND    |

---

## Programación de los Dispositivos
### Requisitos previos
- **Sistema Operativo:** Windows, macOS o Linux.
- **Programas instalados:** 
  - Git
  - Arduino IDE 2 con la versión 2.0.17 del soporte de placas ESP32 y las siguientes librerías:
    - ArduinoJson.h
    - LoRa.h
    - ESP32Time.h
    - SSD1306Wire.h
    - PubSubClient.h

### Pasos para la programación
1. **Clonar el repositorio**
   Ejecuta el siguiente comando dentro de la carpeta de Arduino:
   ```bash
   git clone https://github.com/xavi2mil/Monitoreo-de-Ruido-en-Avenidass.git
   ```

2. **Cargar el sketch del sonómetro**
   - El sketch se llama "*node_LoRa*" y es importante modificar la variable *nodeId* dentro del sketch.
   - Se debe asignar un número único a cada sonómetro (1, 2, 3, ...):
     ```c
     #define nodeId 1
     ```

3. **Cargar el sketch del gateway**
   - Hay dos versiones:
     1. **MQTT:** Modifica el archivo `config.h` dentro de la carpeta `gateway_LoRa` y carga el sketch "*gateway_LoRa*".
     2. **USB:** Carga el sketch "*gateway_LoRa_2*" directamente.

---

## Uso de los Dispositivos
### Configuración inicial
Los sonómetros son controlados mediante comandos enviados a través del gateway en formato JSON. Los comandos principales son:

| Comando | Descripción | Formato JSON |
|---------|-------------|--------------|
| **setTime** | Establece el tiempo actual. | `{"command":"setTime", "nodeId":0, "time":1736849056}` |
| **setConfig** | Configura parámetros como el período de cálculo y el número de mediciones. | `{"command":"setConfig", "nodeId":0, "period":1, "numMeasurements":10}` |
| **getConfig** | Solicita la configuración actual de un nodo. | `{"command":"getConfig", "nodeId":1}` |
| **getValues** | Solicita los valores de ruido almacenados. | `{"command":"getValues", "nodeId":1}` |

Estos comandos pueden enviarse mediante conexión USB o MQTT.

### Mediante Conexión USB
1. Conecta el gateway a la computadora mediante un cable USB.
2. Usa el monitor serial del Arduino IDE configurado a 115200 Baud.
3. Envía los comandos JSON directamente desde el monitor.

### Mediante Conexión MQTT
<!-- Añadir detalles sobre cómo enviar comandos y recibir datos mediante MQTT -->

---

## Configuración de Node-RED
### Configuración de nodos InfluxDB
1. Instala el paquete `node-red-contrib-influxdb` en Node-RED.
2. Crea un nodo InfluxDB Out y configura:
   - **Servidor:** Dirección y puerto de tu instancia InfluxDB.
   - **Base de datos:** Nombre de la base de datos.
   - **Medición:** Nombre de la tabla donde se almacenarán los datos.

### Configuración de nodos Serial
1. Agrega un nodo Serial In en tu flujo.
2. Configura:
   - **Puerto serial:** Selecciona el puerto al que está conectado el gateway.
   - **Baud Rate:** 115200.
3. Conecta el nodo Serial In a un nodo JSON para decodificar los mensajes.

### Configuración de nodos MQTT
<!-- Añadir detalles sobre cómo configurar nodos para comunicación MQTT -->

---

## Registro y Visualización de los Niveles de Ruido
1. Importa el flujo desde el archivo JSON proporcionado en `flows_nodered`.
2. Configura los nodos de InfluxDB y Serial según las instrucciones anteriores.
3. Usa la interfaz gráfica para:
   - Configurar los parámetros de los sonómetros.
   - Visualizar los datos en tiempo real.
   - Registrar valores en la base de datos.

---

## Solución de Problemas
- **Problema:** El gateway no se comunica con los sonómetros.
  - **Solución:** Verifica que los *nodeId* sean únicos y que estén dentro del rango configurado.
- **Problema:** No se reciben datos en InfluxDB.
  - **Solución:** Asegúrate de que el nodo InfluxDB esté configurado correctamente y la base de datos esté activa.

---