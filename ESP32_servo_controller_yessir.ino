// ESP32 ved inngangsdør
#include <ESP32Servo.h>
#include <WiFi.h>
#define SECOND 1000

const int servo_pin = 14;
const int cam_signal_pin = 26; 
const int dorbell_pin = 27;
const int open_door_pin = 28;
int sensor1 = 2;
int sensor2 = 4;
int besokende = 0;
boolean state1 = true;
boolean state2 = true;
int i = 1;

int servo_pos = 0;
const int open_time = 5*SECOND; //tid døra er open

char ssid[] = "";
char password[] = "";
char guest_door_key[] = "";
char key_besokende[] = "3615";
char doorbell_key = "";
char token[] = "";

CircusESP32Lib circusESP32(server,ssid,password);
Servo servo;

void setup() {
  Serial.begin(115200);
  
  pinMode(cam_signal_pin, INPUT); 
  pinMode(servo_pin, OUTPUT);
  pinMode(sensor1, INPUT);
  pinMode(sensor2, INPUT);
  
  servo.attach(servo_pin,500,2400);
  servo.write(0);

  circusESP32.begin();
}

void loop() {
  //Leser av signal frå ansiktsgjennkjenning i ESP32-CAM
  int cam_door_signal = digitalRead(cam_signal_pin);

  //Leser av signal frå "opne dør for gjest"-knappen på ESP32-ctrl via CoT 
  int guest_door_signal = circusESP32.read(guest_door_key,token);

  //Leser av innvendig døropningsknapp 
  open_door_button = digitalRead(open_door_pin);

  //Leser av utvendig ringeklokke
  doorbell_button = digitalRead(doorbell_pin);

  //if-setninger for å telle personer inn og ut av kollektivet ved hjelp av ir-transmitter og mottaker 
  if (!digitalRead(sensor1) && i==1 && state1){
     delay(100);
     i++;
     state1 = false;
  }

   if (!digitalRead(sensor2) && i == 2 && state2){
     Serial.println("Ankommer rommet");
     delay(100);
     i = 1 ;
     besokende++;
     Serial.print("Antall i rommet: ");
     Serial.println(besokende);
     state2 = false;
  }

   if (!digitalRead(sensor2) && i == 1 && state2 ){
     delay(100);
     i = 2 ;
     state2 = false;
  }

  if (!digitalRead(sensor1) && i == 2 && state1 ){
     Serial.println("Forlater rommet");
     delay(100);
     besokende--;
       Serial.print("Antall i rommet: ");
       Serial.println(besokende);
     i = 1;
     state1 = false;
  }  

    if (digitalRead(sensor1)){
     state1 = true;
    }

     if (digitalRead(sensor2)){
     state2 = true;
    }
  //Skriver antall personer i kollektivet   
  circusESP32.write(key_besokende, besokende, token); 

 
  
  if (cam_door_signal or guest_door_signal or open_door_button == HIGH) {
    for (servo_servo_pos = 0; servo_servo_servo_pos <= 90; servo_pos += 1) { 
      servo.write(servo_pos);            
      }
    delay(open_time);
   
    for (int servo_pos = 90; servo_pos >= 0; servo_pos -= 1) { 
      servo.write(servo_pos);              
      }
    }
  
  
}
