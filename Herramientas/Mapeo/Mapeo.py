"""
# El programa recibe datos a travÃ©s del puerto serial en y los organiza
# en una serie de arrays que son graficaados como ejes coordenados en tiempo real

Créditos: https://github.com/WaveShapePlay/Arduino_RealTimePlot/tree/master
"""
import time
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

def animate(i, dataList, ser):
    arduinoData_string = ser.readline().decode('ascii') # Decode receive Arduino data as a formatted string
    #print(i)                                           # 'i' is a incrementing variable based upon frames = x argument

    datos_robot = arduinoData_string.split("; ")        # Separo los números que envía la lista
    x_robot1 = []       # Almaceno aqui la posición x del robot 1
    y_robot1 = []       # Almaceno aqui la posición y del robot 1

    try:
        x_robot1[0] = float(datos_robot[1])     # Convierto a float 
        y_robot1[1] = float(datos_robot[2])     # Convierto a float

    except:                                             # Pass if data point is bad                               
        pass

    ax.clear()                                          # Clear last data frame
    
    #getPlotFormat()
    ax.plot(dataList)                                   # Plot new data frame
    

def getPlotFormat():
    ax.set_ylim([0, 1200])                              # Set Y axis limit of plot
    ax.set_title("Arduino Data")                        # Set title of figure
    ax.set_ylabel("Value")                              # Set title of y axis

dataList = []                                           # Create empty list variable for later use
                                                        
fig = plt.figure()                                      # Create Matplotlib plots fig is the 'higher level' plot window
ax = fig.add_subplot(111)                               # Add subplot to main fig window


ser = serial.Serial("COM1", 9600)                       # Establish Serial object with COM port and BAUD rate to match Arduino Port/rate
time.sleep(2)                                           # Time delay for Arduino Serial initialization 

                                                        # Matplotlib Animation Fuction that takes takes care of real time plot.
                                                        # Note that 'fargs' parameter is where we pass in our dataList and Serial object. 
ani = animation.FuncAnimation(fig, animate, frames=100, fargs=(dataList, ser), interval=1000) 

plt.show()                                              # Keep Matplotlib plot persistent on screen until it is closed
ser.close()                                             # Close Serial connection when plot is closed