#include "SevSeg.h" // inkluderer sevseg biblitotek
#include<CircusESP32Lib.h>
// setter opp internett kobling
char ssid[] = "Speed"; 
char password[] = "TryCatchMyPassword";   // kobler opp mot serial
char server[]="www.circusofthings.com";
char key[]="4986";
char key_Vtank[]="5041";
char token[]= "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTc2In0.19FjRH_yjwZ3dE7QTS3p97U_Qh62a5HnunXh0_gVYu0";

CircusESP32Lib circusESP32(server,ssid,password);
SevSeg sevseg; // lager sevseg til 7 segment display

const long interval = 5000;  // setter opp intervaller på 1 time som i ett døgn
unsigned long previousMillis = 0; // stter oppp millis
int teller=0;
void setup(){
byte numDigits = 2; // 1 antall nummer på display
byte digitPins[] = {12,13}; // pin på digit 1 og 2
                     //a, b, c,  d,  e,  f,  g
byte segmentPins[] = {27, 26, 5, 33, 14, 32, 4}; // pinner
bool resistorsOnSegments = true; // motstand er koblet til display pins
bool updateWithDelaysIn = true;

byte hardwareConfig = COMMON_ANODE; // Felles anode
sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments); // starter Sevseg for variablene
sevseg.setBrightness(90); // setter lysstyrke på ledene
circusESP32.begin(); // Kobler opp mot Circus
Serial.begin(115200);
}
void loop()
{
  timer_teller();   // henter inn funksjon som teller timer på døgnet
  dag_og_natt_senking(); // henter in funkjson som styrer natt og dag senking på varmekablene og på varmtvannstanken.

  
  
}
// setter opp en fuksjon som teller hver time i et døgn og som starter på 00.00 og slutter på 23.00 akkurat som en digital klokke
void timer_teller(){
  if(millis() - previousMillis >= interval) {    
      previousMillis = millis();      
      teller +=1; 
  }
  if(teller == 23){ // døgnet slutter på 23:00
  teller =0;
}
}
// setter opp en funksjon som styrer dag og natt senking. når telleren er større enn 7 og mindre en 22 på dagtid er temperaturen på varmekablene satt til 25 grader. 
// ellers så vil temperaturen på badet være satt til 20 grader som vil skje fra 22 til 7 på natten. Dette vil så blir printet i cot og på 7 segment display.

void dag_og_natt_senking(){
if(teller >=7 && teller <= 22){
    int varmekabler = 25;    // dag temperatur varmekabler
    int Vtank = 70; // dag tempratur varmtvannstank
    circusESP32.write(key,varmekabler,token); // skriver varmekabler temperatur til cot 
    circusESP32.write(key_Vtank, Vtank, token); // skirver varmtvanntank temperatur til cot
    sevseg.setNumber(varmekabler); // Skriver nummer inn på 7 segment display
    //delay(1000);
    sevseg.refreshDisplay();  // refresher display
  }
  else{
    int varmekabler=20; // natt temperatur varmekabler
    int Vtank = 40; // natt tempratur varmtvannstank
    sevseg.setNumber(varmekabler); // skriver på 7 segment
    //delay(1000);
    sevseg.refreshDisplay(); //refresh
    circusESP32.write(key,varmekabler,token);// skriver varmekabler temperatur til cot
    circusESP32.write(key_Vtank, Vtank, token); // skriver varmtvanntank temperatur til cot
  }
}
