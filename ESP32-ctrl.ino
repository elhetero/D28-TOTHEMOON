/***ESP32 CTRL***/

//Inkluderering av bibliotekter:
#include "Arduino.h" //inkluderer Arduino bibliotek
#include <CircusESP32Lib.h> //inkluderer CircusESP32 bibliotek
#include <ESP32Servo.h> //inkluderer ESP32Servo bibliotek
#include "SevSeg.h" //inkulderer "sevseg" bibliotek for styring av 7-segment display
#include <melody_player.h> //inkulderer bibliotek for avspilling av musikk
#include <melody_factory.h> //inkluderer bibliotek for laging av musikk
#include <esp_task_wdt.h> //inkluderer task bibliotek for watchdog timer
#define MINUTE 60000 //definerer 1 minutt i millisekund

//CoT oppkoblingsvaribler for internett oppsett, CoT bruker og nøkkelverider for ulike signal
char ssid[] = "Speed"; 
char password[] = "TryCatchMyPassword"; 
char server[] = "www.circusofthings.com";
char bathroom_state_key[] = "22147";
char livingroom_state_key[] = "30032"; 
char kitchen_state_key[] = "11257"; 
char vifteKey[] = "1811"; 
char tempKey[] = "29584";   
char servoKey[] = "19052";  
char termostatKey[] = "32065";   
char RingeknappKey[] = "27851"; 
char doorStateKey[] = "18418";
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1Mjc1In0.UXcypxoNDC-05QVKc2CSNaQFXqwZbpw9C4CEhTX9aH0";

//Oppretter to håndterings oppgaver for flerkjerne kjøring kalla Task1 og Taks2:
TaskHandle_t Task1;
TaskHandle_t Task2;

//Generelle variabler for frekvens og oppløysning knytt til bruk av ledc:
int freq = 5000; //frekvens for signal
int resolution = 8; //oppløysing av signal

//Varialer for takvifte:
const int8_t EN1 = 23; // enable utgang 1
const int8_t IN1 = 12; // input ingang 1
const int vifteKnapp = 22; //knapp for av og på av motor
int vifteVerdi = 0; // varabel for styring av vifte hastighet
int vifteStyring = 0;  // leser HIGH eller LOW på vifteknapp
int buttonVifte = 0;  // variabel for avlesning av knapp verdi knyttet til vifte
int lastbuttonVifte = 0; //siste status for vifteknapp

//Variabler for temperaturnivå i grader celcius:
float tempLevel1 = 20.0; //temperaturnivå 1 
float tempLevel2 = 23.0; //temperaturnivå 2 
float tempLevel3 = 26.0; //temperaturnivå 3 

//Variabler for viftehastigheit:
const int level1 = 200;
const int level2 = 230;
const int level3 = 255;

//Variabler knytt til temperaturmåling:
const int temperaturPin = 32;
int analogMax = 4095; //maks analog verdi
float voltage = 3.3; //maks spenning til temperaturmåler [V]
float R10k = 10000.0; //motstandsverdi til spennigsdeler [Ω]
float betaNTC = 3950.0; //beta verdi for NTC thermistor
float kritiskTemperaturKelvin = 273.15 + 25.0; //tempratur for 25 grader i kelvin
float motstandThermistor25C = 10000.0; //motstand for thermistor ved 25 grader celsius
float temp; //temperatur i grader celcius
float ThermistorMotstand;//thermistor motstand
float spenningTempPin; //spenning over pinnen knytt til thermistor

//Variabler for luftevindu (servo):
Servo servo; //initialiserer et servo objekt kalla servo
unsigned long previousMillisLuftevindu = 0; //siste millis verdi for luftevinduet
const long lufteVinduTid = 5*MINUTE; //tid før vinduet overtar for vifte
int lufteVinduTidTeller = 0; //variabel for å holde luftevindu åpent
const int luftevinduServoPin = 19; //pinne for servo til styring av luftevindu
int trykkTellerVifte; //angir vifte modus og teller antall trykk av vifteknapp
int graderServo = 0; //angir posisjon til servo i grader
int vinduVerdi = 0; //forteller om vinduet er åpent eller igjen

//Panelovn termostat:
const int panelovn_pin = 18; //bruker Ledlys på for å simulere om panelovn er av/på
int TempPanelovn; //temperatur variabel for panelovn

//Variabler for Ringeklokke:
const int buzzer_pin = 16; //ringeklokke pinne
const int channel_buzzer = 2; //kanal for ringeklokke
const int led_ringeklokke = 17; //ledlys knytt til ringeklokke
MelodyPlayer player(buzzer_pin, HIGH); //konfigurer MelodyPlayer med buzzer ringeklokke pinne og ventende tilstand satt til høy

//Variabler for åpning av ytterdør i hybelhuset:
const int door_open_button_pin = 39; //pinne knytt til knapp for åpning og låsing av ytterdør
int door_state = 0; //status som indikerer om ytterdøra er åpen eller låst
int door_open_button; //knapp for åpning av ytterdør

//Variabler for lyspære styring:
const int channel_LedLys = 0; //kanal for lyspære
const int LedLys = 15;  //pinne for lyspære på soverommet
const int Lysbryter = 4; //pinne for avlesning av lysbryter
int Lyssensor = 34;  //pinne for å lese det analoge signalet til lyssensoren
int Lyssensorread = 0; //variabel for avlesning av analogt signal
int ButtonLysbryter = 0;  //variabel for avlesning av lysbryter verdi
int trykktellerLysbryter = 0; //teller for antall trykk på lysbryter
int LastButtonLysbryter = 0; //variabel for tilstand til siste siste lysbryter trykk 
const float morgen = 3000;  //lyssensor grense for lyspære styrke om morgenen
const float dag = 3500;    //lyssensor grense for lyspære styrke på dagtid
const float kveld = 2400;  //lyssensor grense for lyspære styrke om kvelden

//Variabler for booking av kjøkken, stue og bad:
int booking_button = 0; //status på booking-knapp. Utgangspunkt: AV
int booking_potmeter_value; //verdi på potmeter knyttet til valg av booking fasilitet
int kitchen_state = 0; //antall personer på kjøkkenet. Utaganspunkt: 0
int livingroom_state = 0; //antall personer i stua. Utaganspunkt: 0
int bathroom_state = 0; //antall personer på bade. Utaganspunkt: 0
int kitchen_state_total; //totalt antall personer på kjøkkenet 
int livingroom_state_total; //totalt antall personer i stua
int bathroom_state_total; //totalt antall personer på badet
int opptatt_ledlys_pin = 2; //pinne til Ledlys for å vise at et rom er fullbooket
unsigned long kitchen_book_start_millis_1; //starttid for 1. person som booker kjøkkenet
unsigned long kitchen_book_start_millis_2; //starttid for 2. person som booker kjøkkenet
unsigned long kitchen_book_start_millis_3; //starttid for 3. person som booker kjøkkenet
unsigned long livingroom_book_start_millis_1; //starttid for 1. person som booker stua
unsigned long livingroom_book_start_millis_2; //starttid for 2. person som booker stua
unsigned long livingroom_book_start_millis_3; //starttid for 3. person som booker stua
unsigned long bathroom_book_start_millis; //starttid for personen som booker badet 
const int booking_button_pin = 5; //pinne for booking-knapp
const int booking_potmeter_pin = 35; //pinne for potmeter knyttet til valg av booking fasilitet
const int booking_time_kitchen = 45*MINUTE; //booking tid for kjøkken
const int booking_time_livingroom = 45*MINUTE; //booking tid for stue
const int booking_time_bathroom = 15*MINUTE; //booking tid for bad

//7-segment display konfigurassjon
byte number_of_digits = 1; // 1 antall av display
byte digit_pins[] = {};//a, b, c,  d,  e,  f,  g
byte segment_pins[] = {27, 14, 21, 33, 13, 26, 25}; // pinner til 7-segment
bool resistors_on_segments = true;
byte hardware_config = COMMON_ANODE; // Felles anode

CircusESP32Lib circusESP32(server,ssid,password); //oppretter et CircusEPS32 objekt av typen CircusESP32Lib med inngangsvariabler av server, ssid og password
SevSeg sevseg; //oppretter et sevseg objekt


void setup() {
  Serial.begin(115200); //starte serialmonitor
  circusESP32.begin(); //starter CoT elementet

  //Oppsett av knapp for åpning av ytterdør
  pinMode(door_open_button_pin,INPUT);

  //Oppsett av luftevindu
  servo.attach(luftevinduServoPin); //kobler servo objekt til servo pinne 

  //Oppsett av vifte
  pinMode(EN1, OUTPUT); // enable ingang 1 for vifte
  pinMode(IN1, OUTPUT); // input ingang 1 for vifte
  pinMode(vifteKnapp, INPUT); //knapp knytt til styring av vifte 

  //Oppsett av panelovn (nytter et ledlys for å simulerer om panelovn er av/på)
  pinMode(panelovn_pin, OUTPUT); 

  //Oppsett for ringeklokke ledlys
  pinMode(led_ringeklokke,OUTPUT);
  
  //Oppsett for temperaturmåler
  pinMode(temperaturPin, INPUT);
  
  //Oppsett for booking-knapp og opptatt ledlys
  pinMode(booking_button_pin,INPUT);
  pinMode(opptatt_ledlys_pin,OUTPUT);
  
  //Oppsett av 7-segments display
  sevseg.begin(hardware_config, number_of_digits, digit_pins, segment_pins, resistors_on_segments); // starter serial for variablene
  sevseg.setBrightness(90); //setter lystrykren på led-segmentene

  //Oppsett av lyspære kontroll
  pinMode(LedLys, OUTPUT); 
  pinMode(Lysbryter, INPUT); 
  
  //Oppsett av analog sytring av lyspæra vha. ledc
  ledcSetup(channel_LedLys, freq, resolution); 
  ledcAttachPin(LedLys,channel_LedLys);


  //Oppsett for flerkjerne operasjoner:
  xTaskCreatePinnedToCore(
      Task1code, //funskjonen som implementerer oppgaven
      "Task1", //navn på oppgaven
      10000,  //stabel størrelse i ord
      NULL,  //input parameter for oppgave 
      5,  //prioritet av oppgaven
      &Task1,  //Oppgave referanse
      1); //kjerne som koden skal kjøre på
  delay(500);
  
  xTaskCreatePinnedToCore(
      Task2code, //funskjonen som implementerer oppgaven 
      "Task2", //navn på oppgaven
      10000,  //stabel størrelse i ord
      NULL,  //prioritet av oppgaven
      6,  //prioritet av oppgaven
      &Task2, //oppgave referanse
      0); //kjerne som koden skal kjøre på
  delay(500);
  
}

//Kode som kjører på prosessorkjerne 0
void Task1code( void * pvParameter) {
  for(;;) {
    esp_task_wdt_reset(); //resetter watchdog timer
    booking(); //kjører kode for booking
    esp_task_wdt_reset(); //resetter watchdog timer
    lysbryter(); //kjører kode for lysbryter
    esp_task_wdt_reset(); //resetter watchdog timer
    vifteknapp(); //kjører kode for viteknapp
    //opne_ytterdor(); //kjører kode for døråpnings knapp
    esp_task_wdt_reset(); //resetter watchdog timer
  }
}

//Kode som kjører på prosessorkjerne 1
void Task2code( void * pvParameter) {
  //esp_task_wdt_add(nullptr);
  for(;;) { 
    esp_task_wdt_reset(); //resetter watchdog timer
    ringeklokke(); //kjører kode for ringeklokke
    esp_task_wdt_reset(); //resetter watchdog timer
    Temperatur_Termostat(); //kjører kode for temperatur måler
    esp_task_wdt_reset(); //resetter watchdog timer
    vifte(); //kjører kode for vifte
  }
}


void booking() {
  //Starter millis tid knytt til tidtaking av booking
  unsigned long current_millis = millis();
 
  //Leser av booking-knapp tilstand og verdi fra potmeter for valg av rom
  booking_button = digitalRead(booking_button_pin);
  booking_potmeter_value = analogRead(booking_potmeter_pin);
  booking_potmeter_value = map(booking_potmeter_value,0,4095,0,2);

  //Oppdaterer 7-segments display
  sevseg.refreshDisplay();
  
  //Definerer potmeter verdier fra 0 til 2 som kjøkken, stove og bad
  if (booking_potmeter_value == 0) {
     sevseg.setNumber(kitchen_state_total);  
  }
  if (booking_potmeter_value == 1) {
     sevseg.setNumber(livingroom_state_total);  
  }
  if (booking_potmeter_value == 2) {
     sevseg.setNumber(bathroom_state_total);    
  }
 
  //Ved trykk på booking-knappen øker statusen for antall personer på kjøkkenet med 1
  if (booking_button == LOW && booking_potmeter_value == 0 && kitchen_state <= 4){
     kitchen_state += 1;
     
     //Leser av verdi fra CoT for potensiell endring fra ESP32-crtl hos andre beboere
     double kitchen_state_cot = circusESP32.read(kitchen_state_key,token);
     
     /*For hver personer som booker blir det opprettet en egen millis verdi for når de booket. Viss det er mer enn 3 personer som har booket 
      *samtidig får andre som prøver å booke beskjed i seriemonitor om at det er fullbooket, og et rødt ledlys på konsollet lyser opp*/
     if (kitchen_state == 1) {
        kitchen_book_start_millis_1 = millis();    
     }  digitalWrite(opptatt_ledlys_pin, LOW); 
     if (kitchen_state == 2) {
        kitchen_book_start_millis_2 = millis(); 
        digitalWrite(opptatt_ledlys_pin, LOW);     
     } 
     if (kitchen_state == 3) {
        kitchen_book_start_millis_3 = millis(); 
        digitalWrite(opptatt_ledlys_pin, LOW);    
     }   
     if (kitchen_state == 4) {
        kitchen_state -=1;
        Serial.println("Fullbooket!");   
        digitalWrite(opptatt_ledlys_pin, HIGH);    
     } 
     //Setter at nåverende totalverdien av personer på kjøkkenet er summen av kitchen_state og kitchen_state_cot
     kitchen_state_total = kitchen_state + kitchen_state_cot;

     //Viss totalen av personer i stua er større enn 3 kommer det en varsel i seriemonitor, og et rødt ledlys lyser opp
     if (kitchen_state_total > 3)
     {
      kitchen_state_total = 3;
      Serial.println("Fullbooket!");
      digitalWrite(opptatt_ledlys_pin, HIGH);
     }
     
     //Skriver status verdi for antall personer på kjøkkenet til CoT
     circusESP32.write(kitchen_state_key,kitchen_state_total,token);
  }

  //Ved trykk av booking-knappen øker statusen for antall personer i stua med 1
  if (booking_button == LOW && booking_potmeter_value == 1 && livingroom_state <= 4) {
     livingroom_state += 1;

     //Leser av verdi fra CoT for potensiell endring fra ESP32-crtl hos andre beboere
     double livingroom_state_cot = circusESP32.read(livingroom_state_key,token);
     
     /*For hver personer som booker blir det opprettet en egen millis verdi for når de booket. Viss det er mer enn 3 personer som har booket 
      *samtidig får andre som prøver å booke beskjed i seriemonitor om at det er fullbooket, og et rødt ledlys på konsollet lyser opp. */
     if (livingroom_state == 1) {
        livingroom_book_start_millis_1 = millis();
        digitalWrite(opptatt_ledlys_pin, LOW);   
     }  
     if (livingroom_state == 2) {
        livingroom_book_start_millis_2 = millis(); 
        digitalWrite(opptatt_ledlys_pin, LOW);  
     } 
     if (livingroom_state == 3) {
        livingroom_book_start_millis_3 = millis();
        digitalWrite(opptatt_ledlys_pin, LOW);     
     }
     if (livingroom_state == 4) {
        livingroom_state -=1;
        Serial.println("Fullbooket!");
        digitalWrite(opptatt_ledlys_pin, HIGH);  
     } 
     //Setter at nåverende totalverdien av personer i stua er summen av livingroom_state og livingroom_state_cot
     livingroom_state_total = livingroom_state + livingroom_state_cot;

     //Viss totalen av personer i stua er større enn 3 kommer det en varsel i seriemonitor, og et rødt ledlys lyser opp
     if (livingroom_state_total > 3)
     {
      livingroom_state_total = 3;
      Serial.println("Fullbooket!");
      digitalWrite(opptatt_ledlys_pin, HIGH);  
     }
         
     //Skriver statusverdi for antall personer i stua til CoT
     circusESP32.write(livingroom_state_key, livingroom_state, token);   
  }

  //Ved trykk av booking-knappen øker statusen for antall personer på badet med 1
  if (booking_button == LOW && booking_potmeter_value == 2 && bathroom_state <= 2) {
     bathroom_state += 1;
     
     //Leser av verdi fra CoT for potensiell endring fra ESP32-crtl
     double bathroom_state_cot = circusESP32.read(bathroom_state_key,token);
     
     /*Viss det allerede er 1 person på badet får andre som prøver å 
      *booke opplysning om at det er opptatt via seriemonitor. */
     if (bathroom_state == 2) {
        bathroom_state -=1;
        Serial.println("Opptatt");
        digitalWrite(opptatt_ledlys_pin, HIGH);
     }
         
     //Setter at nåverende totalverdien av personer på badet er summen av bathroom_state og bathroom_state_cot
     bathroom_state_total = bathroom_state + bathroom_state_cot;

     //Viss totalen av personer på badet er større enn 1 kommer en varsel i seriemonitor, og et rødt ledlys lyser opp
     if (bathroom_state_total > 1)
     {
      bathroom_state_total = 1;
      bathroom_state -=1;
      Serial.println("Opptatt");
      digitalWrite(opptatt_ledlys_pin, HIGH);
     }
     else {
        //Setter starttid for booking av badet
        bathroom_book_start_millis = millis();
        digitalWrite(opptatt_ledlys_pin, LOW);
     }
     
     
     //Skriver statusverdi for badet til CoT
     circusESP32.write(bathroom_state_key, bathroom_state_total, token);   
  }
  
  /* Oppretter 3 switch cases for om enten 1,2 eller 3 personer booker kjøkkenet samtidig. Siden programmet hele tiden er nødt til å vite når en person sin booketid er utløpt
   * må case nr. 2 ha kontroll på tiden for både person nr. 1 og nr.2 , og den 3. casen må ha kontroll på tida for både 1. 2. og 3. person. Når den nåverende millis verdien trekt i fra millis 
   * veriden for når noen booka kjøkkenet er lik booking_time_kitchen, på 45 minutt, minker status på personer på kjøkkenet med 1, og videre blir den nåverende gjeldende veriden av personer på kjøkknet skrivet til CoT. */
  
  switch (kitchen_state) {
    case 1:
      if ((current_millis - kitchen_book_start_millis_1) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token);
        break;
      }
    case 2: 
      if ((current_millis - kitchen_book_start_millis_1) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token); 
        break; 
      }
      if ((current_millis - kitchen_book_start_millis_2) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token);
        break;
      }
    case 3:
      if ((current_millis - kitchen_book_start_millis_1) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token);
        break;
      }
      
      if ((current_millis - kitchen_book_start_millis_2) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token); 
        break; 
      }
        
      if ((current_millis - kitchen_book_start_millis_3) == booking_time_kitchen) {
        kitchen_state_total -= 1;
        circusESP32.write(kitchen_state_key, kitchen_state_total, token);
        break;
      }  
  }
  
  /*Oppretter 3 switch cases for om enten 1,2 eller 3 personer booker stua samtidig. Siden programmet hele tiden er nødt til å vite når en person sin booketid er utløpt
   * må case nr. 2 ha kontroll på tiden for både person nr. 1 og nr.2 , og den 3. casen må ha kontroll på tida for både 1. 2. og 3. person. Når den nåverende millis verdien trekt i fra millis 
   * veriden for når noen booka stua er lik booking_time_living room, på 45 minutt minker, status på personer i stua med 1, og videre blir den nåverende gjeldende veriden av personer i stua skrivet til CoT. */

   switch (livingroom_state) {
    case 1:
      if ((current_millis - livingroom_book_start_millis_1) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token);
        break;
      }
    case 2: 
      if ((current_millis - livingroom_book_start_millis_1) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token); 
        break; 
      }
      if ((current_millis - livingroom_book_start_millis_2) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token);
        break;
      }
    case 3:
      if ((current_millis - livingroom_book_start_millis_1) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token);
        break;
      }
      
      if ((current_millis - livingroom_book_start_millis_2) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token); 
        break; 
      }
        
      if ((current_millis - livingroom_book_start_millis_3) == booking_time_livingroom) {
        livingroom_state_total -= 1;
        circusESP32.write(livingroom_state_key, livingroom_state_total, token);
        break;
      }
     
  }
   
  /* Når den nåverende millis verdien trekt i frå millis veriden for når noen booka badet er lik booking_time_bathroom på 15 minutt minker status på personer i stua med 1. 
  Videre blir den nåverende gjeldende veriden av personer på badet skrivet i CoT */
  if ((current_millis - bathroom_book_start_millis) == booking_time_bathroom && bathroom_state == 1) {
      bathroom_state_total -=1;
      circusESP32.write(bathroom_state_key, bathroom_state_total, token);    
      } 
}

void lysbryter() {
  Lyssensorread = analogRead(Lyssensor); //leser av analogt signal fra fotoresistor
  ButtonLysbryter = digitalRead(Lysbryter); //leser av verdi fra lysbryter
  knapptellerLysbryter(); //Henter inn funksjon for å telle antall lysbryter trykk
  
  //Automatisk styring av soveromsbelsyning basert på lysstyrken i rommet
  if( trykktellerLysbryter % 2 == 0){  //en toggel bryter for å skru av eller på lyspæra
    if( morgen < Lyssensorread < dag){  //setter lysstyrken til ca. 60% av maks om morgen
    ledcWrite(channel_LedLys,150);
    }
    if(Lyssensorread > dag){ //setter lysstyrken til 100% på dagen
    ledcWrite(channel_LedLys,255);
    }
    if(Lyssensorread < kveld){ //setter lysstyrken til ca. 30% av maks på kveld
    ledcWrite(channel_LedLys,80);
    }
  }
    else{
    ledcWrite(channel_LedLys, 0); //når en slår av lysbryteren slår lyspæra seg av
    }
}

void knapptellerLysbryter(){
  
//sammenligner veriden av lysbryteren med den forrige tilstanden til lysbryteren
  if (ButtonLysbryter != LastButtonLysbryter) {
    //hvis tilstanden har endret seg begynner telleren
    if (ButtonLysbryter == HIGH) {
      //hvis forrige tilstand er HIGH går lysbryteren fra av til på
      trykktellerLysbryter += 1; //nummertet av trykk blir inkrementert med 1
      Serial.println("På");
      Serial.print("Antall bryter trykk: ");
      Serial.println(trykktellerLysbryter);
    } else {
      //hvis ButtonLysbryter er LOW går lysbryteren fra på til av
      Serial.println("Av");
    }
    delay(50);
  }
  //lagerer nåverende knappetilstans som sistetilstand for neste gang loopen kjøres
  LastButtonLysbryter = ButtonLysbryter;
}

void ringeklokke() {
  //leser av ringeklokkeverdi fra CoT mellom 0 og 7 som angir hvilket rom som blir oppringt. Verdier fra 1-6 tilsvarer de 6 soverommene, og 7 er felles for å ringe alle soverom. 
  double RingeklokkeVerdi = circusESP32.read(RingeknappKey, token); // sjekker om verdien er riktig for rom 1-6 sin esp32-ctrl
  digitalWrite(led_ringeklokke,LOW); //ringeklokke ledlys av
  
  if (RingeklokkeVerdi == 2 or RingeklokkeVerdi == 7 ) {  // Siden denne ESP32-ctrl høyrer til soverom 2 er utslagsgivene verdi for ringing her satt til 2 
    digitalWrite(led_ringeklokke,HIGH); //ringeklokke ledlys er på
    String notes[] = {"A4,2","SILENCE,1","A4,1","A4","A4","G4","A4","A4,2","SILENCE,1","A4,1","A4", "A4", "G4", "A4","A4,2","SILENCE,1", "C5,2" ,"A4,2", "G4,2", "F4,2", "D4", "D4", "E4", "F4", "D4", "A4,2"};
    Melody melody = MelodyFactory.load("Epic Sax Guy", 170, notes, 26); //laster inn melodi på 26 noter med tempo på 170
    player.play(melody); //spiller av melodi
  }
}

//Knapp for endring av vifte moduser (av/på/automatisk styring)
void vifteknapp() {
  
  //leser av verdi fra pinne knytt til vifte knapp
  buttonVifte = digitalRead(vifteKnapp);

  /*Viss siste vifteknapp verdi ikke er lik forrige vifteknapp verdi og knappeveriden er høg øker trykkTellerVifte med 1. 
   *Viss trykkTellerVifte er større enn 2 settes den til 0. TrykkTellerVifte angir hvilket modus vifta er i basert på om veriden er 0,1 eller 2. */   
  if(buttonVifte != lastbuttonVifte){
    if(buttonVifte == HIGH) {
      trykkTellerVifte += 1;
    }
    delay(50);
  }
  lastbuttonVifte = buttonVifte;
  
  if (trykkTellerVifte > 2) {
    trykkTellerVifte = 0;
  }
}
 

void vifte() {

//Switchcase for vifte modus 
  switch (trykkTellerVifte) {
    
    //Vifta er slått av
    case 0:
      Serial.println("Av");
      digitalWrite(IN1, LOW);
      analogWrite(EN1, 0);
      Serial.println("Verdi 0");
      circusESP32.write(vifteKey, vifteVerdi, token); //viser vifte hastighetsnivå i CoT  
      break;
      
    //Vifta er slått og går på maks hastigheit
    case 1:
      Serial.println("På");
      digitalWrite(IN1, HIGH);
      analogWrite(EN1, level3);   
      vifteVerdi = 3;
      circusESP32.write(vifteKey, vifteVerdi, token); //viser vifte hastighetsnivå i CoT
      break; 
      
    //Vifta går automatisk av og på ifht. temperatur 
    case 2:
      Serial.println("Automatisk styring");
      if(tempLevel1 <= temp && temp < tempLevel2) {
          vifteVerdi = 1;
          digitalWrite(IN1, HIGH);
          analogWrite(EN1, level1);
        }
        if(tempLevel2 <= temp && temp < tempLevel3) {
          vifteVerdi = 2;
          digitalWrite(IN1, HIGH);
          analogWrite(EN1, level2); 
        }
        if(temp >= tempLevel3) {
          vifteVerdi = 3;
          digitalWrite(IN1, HIGH);
          analogWrite(EN1, level3); 
        }
        if(temp < tempLevel1) {
          vifteVerdi = 0;
          digitalWrite(IN1, LOW);
          analogWrite(EN1, 0);     
        }
        circusESP32.write(vifteKey, vifteVerdi, token); //viser vifte hastighetsnivå i CoT  
        break;
    }
  
  /* Automatisk styring av luftevinduet. Viss temperaturen er over tempLevel1 og vifta har stått på i mer enn 5 minutt skrues vifta av og luftevinduet åpner seg til temperaturen er under templevel1*/
  unsigned long nowMillis = millis();
  Serial.println(lufteVinduTidTeller);
  if(temp >= tempLevel1 && nowMillis - previousMillisLuftevindu >= lufteVinduTid && trykkTellerVifte == 2) {       
    previousMillisLuftevindu = millis(); 
    lufteVinduTidTeller += 1; 

    //Setter vifteverdien til null og skriver dette til CoT siden lufteVinduTidTeller nå er 1, noe som indikerer at luftevinduet skal åpne seg
    vifteVerdi = 0;
    circusESP32.write(vifteKey, vifteVerdi, token);
    
  }
  //Hvis lufteVinduTidTeller er 1 og modusen for vifa er 2:
  if (lufteVinduTidTeller == 1 && trykkTellerVifte == 2){
    
      //luftevinduet åpner seg med at servoen vrir seg 80 grader
      for (graderServo = 0; graderServo <= 80; graderServo += 1) {
        servo.write(graderServo);              
        delay(10);    
      }
      vinduVerdi = 1; //luftevindu er åpent 
      digitalWrite(IN1,LOW); //vifteverdien blir satt til null
      circusESP32.write(servoKey, vinduVerdi, token);  //viser status på luftevindu i CoT
   } 
  //Hvis temperaturen blir mindre enn tempLevel1 lukker luftevinduet seg igjen
  if(temp < tempLevel1){  
        
    //luftevindet lukker seg
    for (graderServo = 80; graderServo >= 0; graderServo -= 1) { 
      servo.write(graderServo);              
      delay(10);
      }
    vinduVerdi = 0; //vindu lukket
    circusESP32.write(servoKey, vinduVerdi, token); //viser status på luftevindu i CoT
   }
}

void Temperatur_Termostat() {
  spenningTempPin = analogRead(temperaturPin)*voltage/analogMax; //leser av analog verdi ved pinne og regner ut spenningen
  ThermistorMotstand = R10k * spenningTempPin /(voltage-spenningTempPin); //regner ut thermistor motstanden
  temp = 1/(1/kritiskTemperaturKelvin + log(ThermistorMotstand/motstandThermistor25C)/betaNTC);//regner ut tempraturen og gjer den om til celcius 
  temp = temp -  273.15;
  circusESP32.write(tempKey,temp,token); //sender tempraturen oppgitt i grader celsius til CoT
  
  double tempStyring = circusESP32.read(termostatKey,token); //leser av tempraturen fra celsius opp til CoT

    //Hvis temperaturen angitt fra CoT trekt fra den nåverende temperaturen i rommet er større enn 0 skal panelovnen skru seg på.
    if((tempStyring - temp) > 0) {  
      digitalWrite(panelovn_pin, HIGH);   
    } 
    //panelovnen er elles slått av
    else {
      digitalWrite(panelovn_pin, LOW);
    }
}

void opne_ytterdor() {
  /*Oppretter her et avlesningssystem med to varibaler der det første siffer indikerer hvilket beboer som sender signalet, og det andre om døren skal åpnes eller lukkes (0/1). 
   * Siden dette ESP32-ctrl konsollet tilhører beboer nr. 2 blir door_state for låst dør her 20 og åpen dør 21*/
  int esp32_ctrl_id_and_door_state;
  
  door_open_button = digitalRead(door_open_button_pin); //leser av verdi fra knapp for åpning av ytterdør
  esp32_ctrl_id_and_door_state = 20; //ytterdør låst
  
  if (door_open_button == LOW) {
    esp32_ctrl_id_and_door_state = 21; //ytterdør ulåst
    circusESP32.write(doorStateKey,door_state,token); //sender ytterdør status til CoT
  }
  if (door_open_button == HIGH && door_state == 21) {
    esp32_ctrl_id_and_door_state = 20; //ytterdøra låst
    circusESP32.write(doorStateKey,door_state,token); //sender ytterdør status til CoT      
  }
}

//Ikke brukt i programmet, men nøvendig for at koden skal kunne kjøre
void loop() { 
}
