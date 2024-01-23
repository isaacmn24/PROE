/** \file PROE_DETOB.ino
 * \author PROE
 * \link https://github.com/jcbrenes/PROE \endlink
 * \brief Código de detección de obstáculos
 * Usado en la placa Bluepill STM32
 *
 * Se encarga de la lectura de sensores y comunica cualquier evento mediante i2c al feather al que está conectado despues de notificarle mediante un pin adicional de interurpcion
 * Se usa el formato "x,y,z." donde "x" es el código del evento, "y" es el valor de lectura si aplica y "z" el angulo en caso del sensor Sharp.
 * Códigos de evento:
 *   1)Sensor Sharp
 *   2)IR frontal
 *   3)IR derecho
 *   4)IR izquierdo
 *   5)Temperatura
 *   6)Batería baja
 * Adicionalmente tiene tres salidas conectadas a leds para notificación y un switch extra
 */

#include <Servo.h>
#include <Wire_slave.h>   //para usar el STM32 como secundario en el puerto I2C                   
//https://github.com/rogerclarkmelbourne/Arduino_STM32/tree/master/STM32F1/libraries/WireSlave 

/************ Serial Setup ***************/
#define debug 1

#if debug == 1
#define serialPrint(x) Serial1.print(x)
#define serialPrintln(x) Serial1.println(x)
#else
#define serialPrint(x)
#define serialPrintln(x)
#endif

Servo myServo;

//Declaración de pines//
#define irLeft PB12 //Sensor ir izquierda
#define irFrontal PB13 //Sensor ir frontal
#define irRight PB14 //Sensor ir derecha
#define lowBattery PB15 //Pin de batería baja del boost
#define interruptor PA8 //Interruptor adicional, usar pullup de software
#define led1 PB5 //Led adicional rojo
#define led2 PA3 //Led adicional amarillo
#define led3 PA5 //Led adicional azul
#define serv PB9 //Servo
#define temp PB1 //Sensor de temperatura
#define int1 PA4 //Pin de interrupción para el feather
#define sharp PA0 //Sharp (ir de distancia)

// Configuración inicial
bool distInicialListo = false;
int inicioFeatherListo = 0;
int offsetDistanciaInicial = 160; // Se agrega un offset de 16cm para la medición del sharp

//Constantes de configuración//
const float beta=0.1;  //Constante para el filtro, ajustar para cambiar el comportamiento
const int servoDelay=5; //Tiempo entre cada paso del servo en milisegundos
const float distanciaMinimaSharp=140; //Distancia mínima en milimetros para detección de obstáculos
const int debounceTime=400; //Debounce para sensor IR frontal
const float maxTemp=40; //Temperatura de detección de fuego
const int movimientoMaximo=180; //Maxima rotacion en grados realizada por el servo que rota el sharp

//Variables globales//
int angulo=0;
int anguloAnterior=0;
int lectura=0;
float distancia=5000; // Distancia medida por el sharp...se inicializa en un número grande para que no envíe mensaje falso al inicio
float filtrado=0;
int cuentaBateria=0;
int sensorIRdetectado=0;
uint32_t millisAnterior=0;
int servoPos=0;
int incremento=1; //cantidad de grados que aumenta el servo en cada movimiento
bool ledON = false; 
int onTime= 300; //tiempo de encendido del LED en milisegundos
unsigned long millisLED=0;

int c=0; //Pulso de led para ver actividad del STM
unsigned long lastSend=0; //Almacena cuando se envio el ultimo obstaculo para evitar saturar la comunicación
const int sendDelay=100; //Tiempo minimo en ms entre cada envio de obstaculo (evita crash del stm)
char dato[50];
bool nuevoObstaculo = false;
unsigned long tiempoObstaculo=0; //Guarda en que momento se detecto el ultimo obstaculo
const int maxTiempoObstaculo=200; //Si no se atiende un obstaculo en 200ms se ignora

unsigned long lastPoll=0; //Almacena tiempo de ultima lectura de sensores IR
unsigned long pollingTime=100; //Tiempo minimo entre lectura de sonsores IR
int lastIR=0; //Ultimo sensor IR que se detecto, evita repetir mensajes


void setup() {
  myServo.attach(serv,500,1600); //Une el objeto myServo al pin del servo (serv, min, max) valores en us para mover el servo a su posición minima y máxima (calibración)

  //Configuración de pines//
  pinMode(irFrontal, INPUT);
  pinMode(irLeft, INPUT);
  pinMode(irRight, INPUT);
  pinMode(interruptor, INPUT_PULLUP);
  pinMode(lowBattery, INPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(temp, INPUT);
  pinMode(int1, OUTPUT);
  pinMode(sharp, INPUT);

  digitalWrite(int1,LOW); //Que el pin de int esté en bajo
  delay(100); //Algunos sensores capturan ruido al inicio, los delays es para evitar eso

  //Configuración de perifericos//
  Wire.begin(8);           //Inicia comunicación i2c como master
  Wire.onRequest(responderFeather);
  Serial1.begin(115200);  //Inicia la comunicación serial a 115200 baud
  Serial1.println("On");  //Enviar un "on" por el serial para confirmar que el setup se ejecutó correctamente

  servoPos = movimientoMaximo/2;
  myServo.write(servoPos); //Coloca el servo en el centro para confirmar alineación
  //myServo.writeMicroseconds(1500);
  revisarInterruptor();
  delay(100); //Algunos sensores capturan ruido al inicio, los delays es para evitar eso

  avizarDistancia(lecturaInicialSharp()); // Hace la lectura para la transformación de coordenadas
  serialPrintln("Lectura de distancia lista");
}
 
void loop() {
  if (distInicialListo){
    moverServo(); //Serial1.println("Servo");
    revisarSharp(); //Serial1.println("Sharp");
    pollingSensoresIR(); //Serial1.println("Poll");  //Leer sensores IR por polling
    revisarSensoresIR(); //Serial1.println("IR");
    revisarTemperatura(); //desactivado mientras no está soldado el sensor al PCB
    revisarBateria(); //Serial1.println("Bat");
    encenderLED(); //Serial1.println("LED"); //enciende un led si alguna función lo activó 
    actividad(); //Serial1.println("Actividad");//Parpadea led amarillo para confirmar actividad del stm
  }
}

/// \brief Realiza la lectura inicial de la distancia que está el robot.
/// \return La distancia filtrada después de 100 mediciones
int lecturaInicialSharp(){
    int contador = 0;
    while (contador < 100){
        lectura=analogRead(sharp);
        filtrado=filtrado-(beta*(filtrado-lectura)); //Filtro de media movil
        distancia=1274160*pow(filtrado,-1.174); //Regresión lineal de datos experimentales para obtener distancia en milimetros
        delay(50);
        contador += 1;
    }
    Serial1.println(distancia);
    return int(distancia + offsetDistanciaInicial);
}

/// \brief Crea el mensaje a ser enviado para la transformación de coordenadas e indica que está listo para transmitir
void avizarDistancia(int information){ //Prepara el paquete de datos con la información de obstaculo detectado    
  sprintf(dato, "%d.", information); //Genera un string para la transmisión
  nuevoObstaculo = true;
  digitalWrite(int1,HIGH); //Pulsar salida int1 para indicarle al feather que se detectó un obstaculo 
  delay(50);
  digitalWrite(int1,LOW);  
  delay(1000); //delay para que el STM no sature al feather con la misma interrupción
}

void moverServo(){ //Mueve el servo un valor determinado cada cierto tiempo
  if((millis() - millisAnterior) > servoDelay) {  //Se utiliza la función millis para evitar usar los delays
    millisAnterior = millis();
    servoPos += incremento;
    myServo.write(servoPos);
    angulo= movimientoMaximo/2 - servoPos; //se considera 0 grados justo al frente
    angulo=-angulo; //Posición de servo invertida en nuevo chasis
    if ((servoPos >= movimientoMaximo) || (servoPos <= 0)){ // se terminó el semiciclo, se invierte la dirección
      incremento = -incremento;
    }
  }
}

void encenderLED(){ //Se encarga de encender el LED por un tiempo determinado. Se utiliza la función millis para evitar usar los delays
  if(ledON){ //enciende el LED
    digitalWrite(led3,HIGH); 
    millisLED = millis();
    ledON = false; 
  }
  if((millis() - millisLED) > onTime) {  //Se apaga el LED luego de un rato
    digitalWrite(led3,LOW); 
  }
}

void revisarSharp(){ //Revisa el valor del sensor Sharp  
  lectura=analogRead(sharp);
  filtrado=filtrado-(beta*(filtrado-lectura)); //Filtro de media movil
  distancia=1274160*pow(filtrado,-1.174); //Regresión lineal de datos experimentales para obtener distancia en milimetros
  //Serial1.println(int(distancia));
  if ((distancia<distanciaMinimaSharp) && (angulo!=anguloAnterior)){ //revisa distancia umbral y que solo se haga un envío por cada ángulo
    crearMensaje(1,int(distancia),angulo); //Enviar obstaculo tipo 1 con distancia en cm y angulo
  }
  anguloAnterior=angulo;
}

void revisarTemperatura(){ //Lectura del sensor de temperatura
  float lectura = analogRead(temp);
  float temperatura = (0.08394*lectura)-54.162; //Formula extraida de la hoja de datos del sensor
  serialPrint(lectura);serialPrint(' ');serialPrintln(temperatura);
  if(temperatura>maxTemp){ //Valor máximo de temperatura aceptable
    int i = int(temperatura);
    crearMensaje(5,i,90); //Se pone angulo 90 para que no crea que es un obstáculo frontal
    //Serial1.println("T");
  }
}

void revisarBateria(){ //Revisar la batería cada 20 ciclos del loop, codigo 6
  if(!digitalRead(lowBattery)){ //Batería baja = pin bajo
    cuentaBateria++;
    if(cuentaBateria>=10000){ //Si la bateria esta baja enviar advertencia cada 10000 ciclos del loop principal
      crearMensaje(6,0,90);
      digitalWrite(led1,HIGH); //Encender led rojo si la batería está baja
      ledON=true;
      onTime=300;
      cuentaBateria=0;
    }
  }
}

void pollingSensoresIR(){ //Lectura de sensores IR por polling, prioridad al frontal
  if((millis() - lastPoll) > pollingTime){
    if(!digitalRead(irFrontal)){
      sensorIRdetectado=2;
    }
  
    else if(digitalRead(irRight)){
      sensorIRdetectado=3;
    }
  
    else if(digitalRead(irLeft)){
      sensorIRdetectado=4;
    }
  
    else{}

    lastPoll = millis();
  }
}

void revisarSensoresIR(){ //Revisa si la variable de sensores IR cambio de estado en un interupción, en ese caso envía el dato
//Esto es porque los serial y wire dentro de interrupciones dan problema

  switch (sensorIRdetectado) {
    case 0:    // no se ha detectado nada
      break;
    case 2:    // sensor IR frontal
      crearMensaje(2,0,0);
      sensorIRdetectado=0;
      //Serial1.print("F ");
      break;
    case 3:    // sensor IR inferior derecho
      crearMensaje(3,0,0);
      sensorIRdetectado=0;
      //Serial1.print("R ");
      break;
    case 4:    // sensor IR inferior izquierdo
      crearMensaje(4,0,0);
      sensorIRdetectado=0;
      //Serial1.print("L ");
      break;
  }
}

void actividad(){
  if(c>5000){//Parpadea led amarillo para señalar actividad del stm
    digitalWrite(led2,!digitalRead(led2));
    c=0;
  }
  else{
    c=c+1;
  }
}

void revisarInterruptor(){
  if(!digitalRead(interruptor)){
    digitalWrite(led1, HIGH);
    crearMensaje(-1,0,0);
  }
  else{
    digitalWrite(led1, LOW);
  }
}

void crearMensaje(int caso, int distancia, int angulo){ //Prepara el paquete de datos con la información de obstaculo detectado    
  sprintf(dato, "%d,%d,%d.", caso, distancia, angulo); //Genera un string para la transmisión
  nuevoObstaculo = true;
  tiempoObstaculo = millis();
  ledON=true;
  onTime=300;

  //Solo si el obstáculo está en frente activa el pin de interrupción para el feather (modo crítico)
  if (abs(angulo)<=40){
    digitalWrite(int1,HIGH); //Pulsar salida int1 para indicarle al feather que se detectó un obstaculo 
    digitalWrite(int1,LOW);  
    delay(1000); //delay para que el STM no sature al feather con la misma interrupción
  }
}

void responderFeather(){ 
  if (nuevoObstaculo){ //Evita enviar el mismo obstaculo dos veces
    Wire.write(dato);  //Envia el valor al master
    nuevoObstaculo = false;
    // El feather debe avisar que terminó el proceso de arranque, luego la operación es normal
    if (inicioFeatherListo == 2 && distInicialListo == false){
      distInicialListo = true;
    }else{
      inicioFeatherListo += 1;
      nuevoObstaculo = true;
    }
  }else{
    Wire.write(""); 
  }
}
