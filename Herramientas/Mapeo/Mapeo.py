import time
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re

# Inicializa listas para almacenar las coordenadas de cada robot
robot_coordinates = {}

# Colores para los robots
colors = ['b', 'g', 'r']

# Define una función para procesar los datos en tiempo real
def animate(i, ser):
    arduinoData_string = ser.readline().decode('utf-8').strip()  # Lee una línea del puerto serie
    match = re.search(r'(\d+:\d+:\d+\.\d+) -> (\d+); (-?\d+); (-?\d+)', arduinoData_string)
    
    if match:
        robot_id = int(match.group(2))
        x = int(match.group(3))
        y = int(match.group(4))

        if robot_id not in robot_coordinates:
            robot_coordinates[robot_id] = {'x': [], 'y': []}

        robot_coordinates[robot_id]['x'].append(x)
        robot_coordinates[robot_id]['y'].append(y)

        ax.clear()
        for i, robot_id in enumerate(robot_coordinates.keys()):
            ax.plot(robot_coordinates[robot_id]['x'], robot_coordinates[robot_id]['y'], label=f'Robot {robot_id}', color=colors[i])

        ax.set_xlabel('Coordenada X')
        ax.set_ylabel('Coordenada Y')
        ax.set_title('Rastro de Robots')
        ax.legend(loc='upper left')
        ax.grid(True)

# Configura el puerto serie (ajusta el puerto y la velocidad según tus necesidades)
ser = serial.Serial('COM3', 9600)  # Reemplaza 'COM3' con el puerto serie correcto
time.sleep(2)  # Tiempo de espera para la inicialización del puerto serie de Arduino

# Crea una figura inicial
fig = plt.figure(figsize=(10, 6))
ax = fig.add_subplot(111)

# Crea la animación para actualizar en tiempo real
ani = animation.FuncAnimation(fig, animate, fargs=(ser,), interval=1000)

plt.show()  # Mantiene el gráfico visible hasta que se cierre
ser.close()  # Cierra la conexión del puerto serie al final
