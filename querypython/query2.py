import matplotlib.pyplot as plt
import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
import pytz
from datetime import datetime
import yaml

def cargar_config(archivo):
    with open(archivo, "r", encoding="utf-8") as file:
        config = yaml.safe_load(file)
    return config

archivo_config = "querypython/acces.yaml"
config = cargar_config(archivo_config)

# Acceder a los valores por su estructura de clave
token = config["token"]
org = config["org"]
url = config["url"]


# Configura la conexión a la base de datos de InfluxDB
# url = "192.168.0.11:8086"
# token = "vibQs9pWxQM81iQlXLVC4fWxKwnnw3RrD9AtCITKk_lgFXMgdaEmt9MCygLV1feoSB4edFMDCRAdoZXEIlaVDg=="
# org = "PI"
# bucket = "nivel_ruido"
client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api(write_options=SYNCHRONOUS)
query_api = client.query_api()
# Define tu consulta Flux
query = '''
    from(bucket: "nivel_ruido")
      |> range(start: -48h)
      |> filter(fn: (r) => r["_measurement"] == "nivel_ruido")
      |> filter(fn: (r) => r["_field"] == "ruido")
      |> filter(fn: (r) => r["tag1"] == "sensor1")
      |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
'''

# Ejecuta la consulta
#result = client.query_api().query_data_frame(org=org, query=query)
zona_horaria_local = pytz.timezone('America/Mexico_City')
times=[]
values=[]
tables = query_api.query(query=query, org="PI")
for table in tables:
    for record in table.records:
        
        values.append(record.get_value())
        times.append(record.get_time())
        times[-1]=times[-1].astimezone(zona_horaria_local)
        
# zona_horaria_local = pytz.timezone('America/Mexico_City')
# print(times[-1])
# fecha_local = times[-1].astimezone(zona_horaria_local)
# print(fecha_local)
#Grafica los datos


# #determinar cuando ocurren los valores pico que sobrepasan los 65 dBA
# val_ref=48 #dB
# indices_val_pico = [i for i, x in enumerate(values) if x > val_ref]

# valores_pico=[]
# tiempo_val_pico=[]
# for i in indices_val_pico:
# 	valores_pico.append(values[i])
# 	tiempo_val_pico.append(times[i])

# plt.plot(tiempo_val_pico, valores_pico, color='red')
# plt.show()

def detect_threshold_exceedances(values, times, threshold, time_min):
    exceedance_intervals = []
    start_time = None

    for i in range(len(values)):
        if values[i] > threshold:
            if start_time is None:
                start_time = times[i]  # Comienza un nuevo intervalo de exceso de umbral
        else:
            if start_time is not None and ((times[i]-start_time).total_seconds()>time_min):
                end_time = times[i - 1]  # Finaliza el intervalo de exceso de umbral
                exceedance_intervals.append((start_time, end_time))
                start_time = None  # Reiniciar para el próximo intervalo
            else:
                start_time=None

    # Manejar el caso final si el último valor excedió el umbral
    if start_time is not None:
        exceedance_intervals.append((start_time, times[-1]))

    return exceedance_intervals

# Ejemplo de uso

threshold = 50
time_min = 1000 #seg

exceedance_intervals = detect_threshold_exceedances(values, times, threshold, time_min)

# Imprimir los intervalos de tiempo de exceso de umbral
print("Intervalos de exceso de umbral (valor > {}):".format(threshold))
for interval in exceedance_intervals:
    plt.plot(interval, [threshold, threshold], color='red')
for interval in exceedance_intervals:
    print("Desde {} hasta {}".format(interval[0], interval[1]))

plt.plot(times, values)
plt.xlabel('Tiempo')
plt.ylabel('Valor')
plt.title('Datos de nivel de ruido')
plt.show()