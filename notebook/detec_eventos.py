import pandas as pd

# df1=pd.read_csv('dataframe_sono1.csv')
# df2=pd.read_csv('dataframe_sono2.csv')
# df3=pd.read_csv('dataframe_sono3.csv')
# df4=pd.read_csv('dataframe_sono4.csv')
dfs=[]
dfs.append(pd.read_csv('dataframe_sono1.csv'))
dfs.append(pd.read_csv('dataframe_sono2.csv'))
dfs.append(pd.read_csv('dataframe_sono3.csv'))
dfs.append(pd.read_csv('dataframe_sono4.csv'))

# Identificacion de los eventos ruidosos
dfs[2]['value'] = dfs[2]['value']+8
# Parámetros para detección de eventos
threshold = 65  # Umbral de ejemplo
min_duration = 3  # Duración mínima del evento en segundo

# Identificación de eventos
events_dfs=[]
for df in dfs:
    df['time'] = pd.to_datetime(df['time'])
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