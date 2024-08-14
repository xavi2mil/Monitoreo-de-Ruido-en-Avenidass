# Librerias para hacer la consulta de los datos a la base de datos y poder hacer el analisis estadistico.
from influxdb_client import InfluxDBClient, Point, WritePrecision
import pandas as pd
import matplotlib.pyplot as plt
from influxdb_client.client.write_api import SYNCHRONOUS
# Parámetros de conexión al servidor central 
url = "http://192.168.0.100:8086"
token = "0Gogft785BaN9fzPYk3OdVcO8Qlrt3Y39dA3Ug2IwiJk2TDadgIwmc13AFEMoeakBqkmv08zdr7di072VuMICQ=="
org = "PI"
bucket = "nivel_ruido"
# Consulta de datos a la base de datos del servidor

# Crear el cliente
client = InfluxDBClient(url=url, token=token, org=org)
query_api = client.query_api()
sonometros = [1, 2, 3, 4]
# Definir la consulta en Flux con un rango específico
results=[]
for sonometro in sonometros:
    query = f'''
        from(bucket: "nivel_ruido")
            |> range(start: 2024-05-28T13:00:00Z, stop: 2024-05-29T12:00:00Z)
            |> filter(fn: (r) => r["_measurement"] == "leq")
            |> filter(fn: (r) => r["nodo"] == "{sonometro}")
            |> aggregateWindow(every: 1s, fn: last, createEmpty: false)
            |> yield(name: "last")
    '''

    # Ejecutar la consulta y obtener resultados
    results.append(query_api.query(org=org, query=query))

# Convertir los resultados a un DataFrame. Un dataframe por cada sonometro
dfs=[]
for result in results:
    data = []
    for table in result:
        for record in table.records:
            data.append((record.get_time(), record.get_value()))

    df = pd.DataFrame(data, columns=['time', 'value'])
    df['time'] = pd.to_datetime(df['time'])
    df['time'] = df['time'] - pd.Timedelta(hours=6)
    dfs.append(df)
# Convertir los data frames a csv
dfs[0].to_csv('dataframe_sono1.csv', index=False)
dfs[1].to_csv('dataframe_sono2.csv', index=False)
dfs[2].to_csv('dataframe_sono3.csv', index=False)
dfs[3].to_csv('dataframe_sono4.csv', index=False)

# Identificacion de los eventos ruidosos

# Parámetros para detección de eventos
threshold = 65  # Umbral de ejemplo
min_duration = 3  # Duración mínima del evento en segundo

# Identificación de eventos
events_dfs=[]
for df in dfs:
    events = []
    event_start = None
    event_values = []
    max_value = float('-inf')
    for index, row in df.iterrows():
        if row['value'] > threshold:
            if event_start is None:
                event_start = row['time']
                max_value = row['value']
                event_values = [row['value']]
            else:
                max_value = max(max_value, row['value'])
                event_values.append(row['value'])
        else:
            if event_start is not None:
                event_end = row['time']
                duration = (event_end - event_start).total_seconds()
                if duration >= min_duration:
                    avg_value = sum(event_values)/len(event_values)
                    events.append((event_start, event_end, max_value, avg_value, len(event_values)))
                event_start = None
                max_value = float('-inf')
                event_values = []
    # Convertir eventos a DataFrame
    events_df = pd.DataFrame(events, columns=['start_time', 'end_time', 'max_value', 'avg_value', 'length'])
    events_df['start_time'] = pd.to_datetime(events_df['start_time'])
    events_df['end_time'] = pd.to_datetime(events_df['end_time'])
    #events_dfs.append(events_df)
    events_df['duration'] = (events_df['end_time'] - events_df['start_time']).dt.total_seconds()
    # Calcular las diferencias de tiempo entre los eventos
    events_df['time_diff'] = events_df['start_time'].diff()
    events_df['time_diff'] = (events_df['time_diff']).dt.total_seconds()
    # Eliminar el primer valor NaT resultante de la diferencia
    events_df = events_df.dropna()
    # Filtrar el dataframe. Eliminar los eventos que tengan perdida de datos.
    events_df=events_df[events_df['length']==events_df['duration']]
    events_dfs.append(events_df)

# Convertir los dataframes a csv
events_dfs[0].to_csv('dataframe_eventos_sono1.csv', index=False)
events_dfs[1].to_csv('dataframe_eventos_sono2.csv', index=False)
events_dfs[2].to_csv('dataframe_eventos_sono3.csv', index=False)
events_dfs[3].to_csv('dataframe_eventos_sono4.csv', index=False)