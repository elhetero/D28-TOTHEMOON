/*** ESP32 ved inngangsdør***/

//Inkluderer bibliotek:
#include "SevSeg.h" //bibliotek for 7-segment display
#include<CircusESP32Lib.h> //bibliotek for Circus of Things kommunikasjon
#include <ESP32Servo.h> //bibliotek for servomotor

#define SECOND 1000 //definere 1 sekund i millisekund

//Variabler for åpning av dør
const int servo_pin = 22; //pinne for styring av servo knytt til åpning og lukking av dørlås
const int cam_signal_pin = 2; //pinne koblet til ESP32-CAM med signal om døra skal åpnes eller lukkes basert på ansiktsgjennkjenning
unsigned long previous_millis_door_open = 0; //siste tid døra var åpen
int servo_pos = 20; //posisjon til servo i grader (0 grader = lukket, 90 grader = åpen) 
const int open_time = 5*SECOND; //hvor lenge døra står 
int door_state = 0; //status på om dør er lukka eller åpen utifrå signal frå ESP32-ctrl

//Variabler for breakbeam oppsett
int sensor1_receiver = 18; //breakbeam sensor 1 reciver
int sensor2_receiver = 35;//breakbeam sensor 1 reciver
int personer_hybelhus = 0; //variabel for antall personer i hybelhuset
boolean state1 = true; //status verdi for breabeam sensor 1
boolean state2 = true; //status verdi for breabeam sensor 2
int i = 1; //varibel for kontroll av hvilken vei personer paserer breakbeam sensorene
int romnummer; //verdi som hviser hvilket romnummer en kan ringe på til. Verdi = 0 er ingen, og verdi = 7 er alle i hybelhuset

//Variabler for ringeklokke
int potensvelg = 34; //pinne til potensiometer for styring av hvilket rom en ønsker å ringe på til
int potmeter_verdi; //brukes til å velge hvilken beboer en vil ringe på hos
int ringeklokke_knapp = 0; //utgangsverdi for ringeklokke_knapp
int person_breakbeam_status; //status verdi som forteller noen forlater eller ankommer kollektivet
const int ringepin = 5; // pinne for ringeklokke
int ringeklokke_knapp_teller = 1; /*teller variabel for å ungå at CoT kontinuerleg skriver når ringeklokke knappen ikke blir trykt. 
                                   *Kontnerleg skriving fører til at andre deler av programmet virker tregt eller er uresponsivt*/

//Oppsett for CoT med interntt oppkobling, signalnøkkler og token
char ssid[] = "Speed"; 
char password[] = "TryCatchMyPassword";
char server[]="www.circusofthings.com"; 
char doorStateKey[]="18418";
char key_personer_hybelhus[]="17082";
char soverom1[] = "11604";
char soverom2[] ="13615";
char soverom3[] ="32364";
char soverom4[] ="5007";
char soverom5[] ="25891";
char soverom6[] ="11480";
char key_ringeklokke[]="27851";
char beboer_som_sjekker_inn[] = "30345";
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTc2In0.19FjRH_yjwZ3dE7QTS3p97U_Qh62a5HnunXh0_gVYu0";

CircusESP32Lib circusESP32(server,ssid,password); //setter opp CoT element med inngangsvariabler server, ssid og passord

SevSeg sevseg; //lager et sevseg objekt kalla sevseg til 7-segment displayet
Servo servo; //oppretter et servo objekt kalla servo

TaskHandle_t Task1; // oppretter task 1 
TaskHandle_t Task2; // oppretter task 2


void setup() {
  Serial.begin(115200);
  circusESP32.begin(); //starter CoT element
  pinMode(ringepin, INPUT); //pinnemodus for ringeklokke

  //Oppsett for 7-segment display
  byte numbers = 1; // 1 antall nummer av display
  byte digit_pins[] = {}; //a, b, c,  d,  e,  f,  g
  byte segment_pins[] = {27, 14, 25, 33, 4, 19, 17}; // pinner for 7-segment display
  bool resistors_On_Segments = true; // motstand er koblet til 7-segment display
  byte hardware_Config = COMMON_ANODE; //felles anode
  sevseg.begin(hardware_Config, numbers, digit_pins, segment_pins, resistors_On_Segments); // starter Sevseg for variablene
  sevseg.setBrightness(90); //setter lysstyrke på ledene
  
  pinMode(cam_signal_pin, INPUT); //pinnemodus for signal fra ESP32-CAM
  pinMode(sensor1_receiver, INPUT); //pinnemodus for breakbeam sensor 1 receiver
  pinMode(sensor2_receiver, INPUT); //pinnemodus for breakbeam sensor 2 receiver
  
  //Oppsett for servo
  pinMode(servo_pin, OUTPUT); 
  servo.attach(servo_pin,500,2400);
  servo.write(0);
  
  //Lager en task 1 som funkerer i Task1code(), med prioritet 6 og core 0
  xTaskCreatePinnedToCore(
                    Task1code,   //funskjonen som implementerer oppgaven
                    "Task1",     //navn på oppgaven
                    10000,       //stabel størrelse i ord
                    NULL,        //input parameter for oppgave 
                    6,           //prioritet av oppgaven
                    &Task1,      //Oppgave referanse
                    0);          //kjerne som koden skal kjøre på               
  delay(500); 

  //Lager en task 2 som funker i Task2code() function, med prioritet 5 og core 1
  xTaskCreatePinnedToCore(
                    Task1code,   //funskjonen som implementerer oppgaven
                    "Task2",     //navn på oppgaven
                    10000,       //stabel størrelse i ord
                    NULL,        //input parameter for oppgave 
                    5,           //prioritet av oppgaven
                    &Task2,      //Oppgave referanse
                    1);          //kjerne som koden skal kjøre på            
 delay(500); 
}

//Task1code: Kjører breakbeam funksjonen og ringeklokke funksjonen
void Task1code( void * pvParameters ){
  for(;;){
    breakbeam(); //kjører kode for breakbeam oppsett
    vTaskDelay(10); //legger inn litt delay for å ungå watchdog timer error
    ringeklokke(); //kjører kode for ringeklokke 
  } 
}

//Task2code: Kjører sjekk funksjon for om noen sjekker inn i hybelhuset vha. ESP32-CAM og hvilke beboere som er hjemme eller ikke
void Task2code( void * pvParameters ){
  for(;;){ 
    opne_dor(); //kjører kode for åpning av dør
    vTaskDelay(10); //legger inn litt delay for å ungå watchdog timer error
    beboer_status(); //kjører kode for status på beboere 
    }
  }
  
// Vi bruker et potmeter til å velge mellom hvilken hybelboer man ønsker å ringe på hos. Verdien 1 til 6 indikerer en spesifikk hybelbeboer.
// Man vrir potmeteret til den personen man ønsker å deretter trykker på ringekappen. Dette vil det ringe på esp-ctrl hos den beboeren lyser opp og en ringetone høres på konsollet 
// Ved å vri veriden på 7-segmentet til 7 vil alle beboerne bli ringt

void ringeklokke(){
  potmeter_verdi = analogRead(potensvelg); // leser av analogverdi fra potemeter
  ringeklokke_knapp = digitalRead(ringepin); // leser av om ringeklokke knappen er trykket eller ikke
  romnummer = map(potmeter_verdi, 0,4095,0,7);  // gir tallene til hybelbeboerene fra 0-7 et analogtverdi spenn hvor man kan velge dem med potmeter.
  sevseg.setNumber(romnummer); // skriver ut hybelbeboerne sine tall (0-7) ut på 7-segment display for å se hvilke person man ringer på hos
  sevseg.refreshDisplay(); // refresher display
  
  
  if(ringeklokke_knapp == HIGH && ringeklokke_knapp_teller == 1){   
    circusESP32.write(key_ringeklokke,romnummer,token); // når ringeklokken trykkes sendes den verdien potmeteret er innstilt på opp til CoT, hvor den kan leses av med esp-ctrl
    ringeklokke_knapp_teller = 0;
  }
  if(ringeklokke_knapp == LOW && ringeklokke_knapp_teller == 0){
    romnummer = 0; //hvis ringeklokken ikke er trykket inn så vil verdien i cot sendes til å bli 0, slik at ingen av esp-ctrl ringer.
    circusESP32.write(key_ringeklokke,romnummer, token); //sender verdien 0 til CoT
    ringeklokke_knapp_teller += 1;
    }
  }
  
void breakbeam(){
  // Breakbeam programmet under nytter if-setninger for å telle hvor mange personer som går inn og ut av hybelhuset ved hjelp av ir-transmitter og mottaker 
  // Det er sett opp to sensorer slik at når en person bryter først sensor 1 etterfulgt av  sensor 2 vil det bli registret som at en person ankommer kollketivet
  // Når en person bryter sensor 2 først etterfulgt av sensor 1 vil det bli registert som at en person forlater hybelhuset
   
  if (!digitalRead(sensor1_receiver) && i == 1 && state1){ //sensor 1 bryter først
     i+=1;
     state1 = false;  
  }
  
   if (!digitalRead(sensor2_receiver) && i == 2 && state2 ){ //sensor 2 blir brutt etterpå noe som betyr at en person ankommer hybelhuset
     personer_hybelhus++; // antall personer i hybelhuset øker
     person_breakbeam_status = 1; //person ankommer
     i = 1;
     state2 = false;
     Serial.println("En person ankommer hybelhuset");
     Serial.print("Antall personer i hybelhuset: ");
     Serial.println(personer_hybelhus);   
     circusESP32.write(key_personer_hybelhus, personer_hybelhus, token);  //printer antall personer til CoT   
  }
  
   if (!digitalRead(sensor2_receiver)&& i == 1 && state1){ //sensor 2 blir brutt først 
     i += 1;
     state2 = false; 
  }

  if (!digitalRead(sensor1_receiver) && i == 2 && state1){ //sensor 1 blir brutt etterpå noe som betyr at en person forlater hybelhuset
     personer_hybelhus--; // antall personer i hybelhuset minker
     person_breakbeam_status = 0; //person forlater
     i = 1;
     state1 = false;
     Serial.println("En person forlater hybelhuset");
     Serial.print("Antall i rommet: ");
     Serial.println(personer_hybelhus);
     circusESP32.write(key_personer_hybelhus, personer_hybelhus, token); //Skriver antall personer i hybelhuset til CoT 
  }  
  
  if (digitalRead(sensor1_receiver)){ //setter state til true på sensor 1 igjen etter brutt senor
     state1 = true;
     }

  if (digitalRead(sensor2_receiver)){ //setter state til true på sensor 2 igjen etter brutt senor
     state2 = true;
     } 
}

void opne_dor() {
  //Leser av signal frå ansiktsgjennkjenning i ESP32-CAM
  int cam_door_signal = digitalRead(cam_signal_pin);

  /*Fra ESP32-ctrl er det opprettet et avlesningssystem med to varibaler der det første siffer indikerer hvilket beboer som sender signalet, og det andre om døren skal åpnes eller lukkes*/
  double esp32_ctrl_id_and_door_state = circusESP32.read(doorStateKey,token); //leser av verdi fra alle esp32-ctrl og om det blir sendt et signal om at døren skal vere låst eller åpenast
  if ((esp32_ctrl_id_and_door_state == 11) or (esp32_ctrl_id_and_door_state == 21) or (esp32_ctrl_id_and_door_state == 31) or (esp32_ctrl_id_and_door_state == 41) or (esp32_ctrl_id_and_door_state == 51) or (esp32_ctrl_id_and_door_state = 61)) {
    door_state = 1; //ulåst dør
  }
  
  //Hvis ESP32-CAM eller døråpningsknapp ved ESP32-ctrl sender et høyt signal vrir servoen knytt til døra seg 90 grader, noe som åpner låsen 
  if (cam_door_signal or door_state == 1) {
    unsigned long current_millis_door_open = millis(); //starter millis tidtaking
   
    for (servo_pos = 0; servo_pos <= 90; servo_pos += 1) { 
      servo.write(servo_pos);  
      delay(15);          
      }
      
  //Etter 5 sekunder vrir servoen seg tilbake og døra er pånytt låst
  if ((current_millis_door_open - previous_millis_door_open) == open_time) {
    previous_millis_door_open = millis(); //setter forrige millis verdi for dør åpning til nåverende millis
    door_state = 0; //låst dør
    for (servo_pos = 90; servo_pos >= 0; servo_pos -= 1) { 
      servo.write(servo_pos);
      delay(15);    
    }          
   }   
  }
}

void beboer_status() {
  double esp32_ctrl_id_and_door_state = circusESP32.read(doorStateKey,token); //leser av verdi fra alle esp32-ctrl og om de sender signal om at døren skal vere låst eller ulåst
  double beboer_nummer = circusESP32.read(beboer_som_sjekker_inn, token); //leser av om en beboer sjekker inn fra CoT verdi gitt av ESP32-CAM
  int beboer_tilstede = 1; //tilstands variabel for å indikere at beboer er hjemme
  int beboer_fraverende = 0; //tilstands variabel for å indikere at bebober ikke er hjemme

  /*Signalet frå ESP-CAM i CoT viser hvilken beboer som har sjekket inn. Denne variablene blir her lagret og vist frem i en større oversikt av CoT signaler for hvert soverom om beboeren er tilstede eller ikke*/
  if (beboer_nummer == 1) {
    circusESP32.write(soverom1,beboer_tilstede, token);
  }

  if (beboer_nummer == 2) {
    circusESP32.write(soverom2, beboer_tilstede, token);
  }

  if (beboer_nummer == 3){
    circusESP32.write(soverom3, beboer_tilstede, token);
  }

  if (beboer_nummer == 4) {
    circusESP32.write(soverom4, beboer_tilstede, token);
  }

  if (beboer_nummer == 5) {
    circusESP32.write(soverom5, beboer_tilstede, token);
  }

  if (beboer_nummer == 6) {
    circusESP32.write(soverom6, beboer_tilstede, token);
  }

  
  /*Hvis knappen for å åpne døra er trykt og en beboer paserer breakbeam-oppsette som viser at beboeren forlater hybelhuset blir statusen på om beboeren er hjemme eller ikke sett frå 1 til 0 i CoT.
    Dette gjelder for hver av beboerne i hybelhuset*/
    
  if (esp32_ctrl_id_and_door_state == 11 && (person_breakbeam_status == 0)) {
    circusESP32.write(soverom1,beboer_fraverende, token);
  }
  if (esp32_ctrl_id_and_door_state == 21 && (person_breakbeam_status == 0)) {
    circusESP32.write(soverom2,beboer_fraverende, token);
  }
  if (esp32_ctrl_id_and_door_state == 31 && (person_breakbeam_status == 0)) {
    circusESP32.write(soverom3,beboer_fraverende, token);
  }
  if (esp32_ctrl_id_and_door_state == 41 && (person_breakbeam_status == 0)) {
    circusESP32.write(soverom4,beboer_fraverende, token);
  }
  if (esp32_ctrl_id_and_door_state == 51 && person_breakbeam_status == 0) {
    circusESP32.write(soverom5,beboer_fraverende, token);
  }
  if (esp32_ctrl_id_and_door_state == 61 && (person_breakbeam_status == 0)) {
    circusESP32.write(soverom6,beboer_fraverende, token);
  }
}

//Ikkje brukt i programmet, men nøvendig for at koden skal kunne kjøre
void loop() {}
