from influxdb_client import InfluxDBClient
import matplotlib.pyplot as plt

# Configura la conexión a la base de datos de InfluxDB
url = "http://localhost:8086"
token = "C9O-oOelr8HnWrZi2-FVqH3O8rYRqCWdnNeAyRlMKQbRLK63FK-KNap75jI8QCYpJHeBXnFJ7QfoJwhxNLBU-w=="
org = "PI"
bucket = "nivel_ruido"
client = InfluxDBClient(url=url, token=token, org=org)

# Define tu consulta Flux
query = '''
    from(bucket: "nivel_ruido")
      |> range(start: -12h)
      |> filter(fn: (r) => r["_measurement"] == "nivel_ruido")
      |> filter(fn: (r) => r["_field"] == "ruido")
      |> filter(fn: (r) => r["tag1"] == "sensor1")
      |> aggregateWindow(every: 10s, fn: mean, createEmpty: false)
'''

# Ejecuta la consulta
result = client.query_api().query_data_frame(org=org, query=query)

# Obtén los datos del resultado
times = result['_time']
values = result['_value']

# Grafica los datos
plt.plot(times, values)
plt.xlabel('Tiempo')
plt.ylabel('Valor')
plt.title('Datos de nivel de ruido')
#plt.show()

# determinar cuando ocurren los valores pico que sobrepasan los 65 dBA
val_ref=48 #dB
indices_val_pico = [i for i, x in enumerate(values) if x > val_ref]

valores_pico=[]
tiempo_val_pico=[]
for i in indices_val_pico:
	valores_pico.append(values[i])
	tiempo_val_pico.append(times[i])

plt.plot(tiempo_val_pico, valores_pico, color='red')
plt.show()

