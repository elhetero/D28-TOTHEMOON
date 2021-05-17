#include<CircusESP32Lib.h>
// setter opp internett kobling
char ssid[] = "Speed"; 
char password[] = "TryCatchMyPassword";
char server[]="www.circusofthings.com";
char key_strompris[]="9999";
char key_forbrukpertime[]="29769";
char key_stromregning[]="4787";
char key_panelovn[]="32065";   // henter inn signaler og skriver signaler til cot
char key_utetemp[]="18910";
char key_tv[]="30032";
char key_bad[]="4986";
char key_ukeforbruk[]="6349";
char key_lopendeforbruk[]="8567";
char key_ukedag[]="19892";
char key_skydekke[]="17773";
char key_ukeproduksjon[]="27645";
char key_lopendeproduksjon[]="29634";
char key_produksjonpertime[]="25557";
char key_Vtank[]="5041";
char token[]= "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MDE4In0.FRdl8o7BcpQBew7boRm52Y7Qu55irdzJopKOZ36_E8w";
CircusESP32Lib circusESP32(server,ssid,password);

// solcelle produksjon variabler:
const long interval = 3600000;// intervaller på 1 time 
unsigned long previousMillis =0;
int teller =0;
float sumproduksjon =0;
float prevproduksjon =0;
float forrigeukeproduksjon=0;  
float ukesproduksjon=0;
float produksjonpertime =0;
float maks = 5;// maksimal kwh prousert per time ved perfekte forhold

// strømforbruk variabler per time
float dusj = 1.65; // Hver beborer dusjer en gang per dag på 10min: 6.6 kwh per dusj. 6 stk = 6x6.6 =39.8 kwh per dag = 1.65kwh per time
float baddag = 0; // varmekabel bad innstilt på 25 grader på dagen. forbruk dag = 1.8 kwh som er 14 timer på om dagen = 1.8/14 = 0.13kwh per time
float badnatt = 0; //varmekabel bad er fast innstilt på 20 grader om natten. forbruk natt = 1.0 kwh som er 10 timer om natten = 1.0 /10 = 0.1 kwh per time
float komfyr = 0.092; // brukes en gang av hver beborer hver dag: 6x0.365 kwh. 2.2kwh/24timer= 0.092kwh per time
float panelovn21 = 0.0; // ved panelovn på under 25 grader er forbruk 5 kwh per dag. 6 rom x 5 = 30kwh per dag. 30/24 = 1.25 kwh per time
float panelovn26 = 0.0; // ved panelovn over 30 grader er forbruk 7 kwh. 6 rom x 7 = 42 kwh. 42/24 = 1.75 kwh per time
float vaskemaskin = 0.05; //brukes 1 gang i uken av hver beboer 6 ganger i uken. 1.4 kwh per vask. snitt per døgn er på 1.2kwh. 1.2/24=0.05 kwh per time
float torketrommel = 0.042; // gåretter en vask altså 6 ganger i uken. 1.2 kwh per tørk. snitt per døgn 1kwh. 1/24 = 0.042
float TV = 0.015; // tv tid i uka på 25 timer, kun hvis tv/ stue er booket. 0.36 kwh per døgn = 0.36/24 = 0.015 kwh per time
float oppvaskmaskin = 0.12; // alle spiser alle måltider hver dag, da går denne 3 ganger i døgnet. 2.8 kwh per døgn. 2.8/24 = 0.12 kwh per time
float belysningsoverom = 0.54; // fast verdi for belysning per time på soverommene som er 0.09 kwh per time. 6x0.09 = 0.54 kwh per time totalt i kollektiv
float Vtank40 = 0.083; // varmtvannstanken er 40 grader så blir forbruket per døgn ekslusivt dusjing er 2kwh. per time = 0.083;
float Vtank70 =0.15;  // varmtvannstanken er 70 grader så blir forbruket per døgn ekslusivt dusjing er 3.5 kwh. per time = 0.15;
float panelovn =0;
float tv = 0;
float tvs =0;
float forpertime = 0;
float utetempratur = 0;
float stromreg = 0;
float bad =0;
float sumforbruk =0;
float prevforbruk =0;
int dag=0;
float forrigeukeforbruk=0;
float ukesforbruk=0;
float varmetank=0;
const long interval2 = 3600000;
unsigned long prevmill =0;


void setup() {
circusESP32.begin(); // Kobler opp mot Circus
Serial.begin(115200);
}

void loop() {
 
produksjonsteller(); //forbruket og solcelle produksjonen blir kalkulert og oppdatert hver time og skrevet opp til cot. Oppdaterer også ukes produksjon og forbruk og strømregning per uke.
solcelle();  // regner ut produsert strøm fra solcellepanel
if(millis() - prevmill >= interval2){ // starter millis intervall slik at funksjonene kjører kun en gang i timen
  prevmill = millis();
soveeovn(); // forbruk fra panelovn soveromm, forbruker øker ved høyere tempratur. forbruke øker og synker med utetempraturen
tvstue(); // forbruk hvis det er folk i tv stuen, forbruker øker ved at det er folk i stuen
stromregning(); // totalt ukentlig strømregning
badforbruk(); // forbruk fra bad, forbruket øker med automatisk dagtempratur og synker automatisk med natttempratur. forbruket øker og synker med utetempraturen
varmevtank(); // forbruk fra varmtvannstank som ikke har med dusjing, altså: vaskemaskin, vask osv,..
forbrukpertime(); // totalt ukentlig forbruk
}
} 
// starter en solcelle funksjon som regner produksjon av strøm. Solcellepanelet produserer mes midt på dagen mellom 12:00 og 18:00 og mindre fra 08:00 til 12:00 og 18:00 til 22:00. dette grunnet solens posisjon.
// vi får en verdi fra 0 til 100.0 fra hvorvidt det er skydekke, hvor 0 er klar himmel og 99.99 er tett skyet. Dermed øker produksjonen ved lav verdi, og produksjonen minker med høy verdi.
// hvis utetempraturen er under 10 grader produserer den mest, og er den mellom 10 og 25 produserer den litt mindre, og er den over 25 produserer den minst.
// vi har en maksimalt produksjon verdi ved perfekte forhold som vi innfører ulike taps prosenter for å finne produksjon akkurat nå.
void solcelle(){
  float skydekke = circusESP32.read(key_skydekke,token); // hyenter inn verdier fra cot
  float temp = circusESP32.read(key_utetemp, token);
  int index = skydekke/10;   // setter en index verdi av gitt fra skydekke verdien
  int skydekkeTap[11] = {0, 10, 10, 20, 20, 30, 30, 40, 40, 50, 50};  // lager en array hvor vi har 10 ulike taps verdier i produksjonen. Hvis det er klar sol legges til en taps verdi som er 0,
                                                                  // og ved tett skydekke legges til taps verdi 50 og produksjonen minker betraktelig
  int tap = 0;
if(teller>=23 or teller<=7){  // På natten mellom 23:00 og 07:00 er produksjonen fra solcellepanelet 0 
  produksjonpertime = 0;
}
else{
   if( teller>=8 && teller<12 or teller>=18 && teller<=22){  // mellom 8-12 og 18-22 tapes produksjonen med 10%
    tap+=10;
  }
   if (skydekke>=50) { // når skydekke er over 50 tapes produksjoen med 10%
    tap+=10;
    }
   if (temp>10 && temp<25) { // når tempraturen er mellom 10 og 25 tapes produksjonen med 10%
    tap+=10;
    } 
    else if (temp>=25){ // hvis tempraturen er over 25 tapes produksjonen med 20%
      tap+=20;
      }
      tap+=skydekkeTap[index];  // legger til taps prosenten ut i fra skydekke, og regner ut den totale tap prosenten som skal trekkes fra den maksimale mulige produksjonen
      produksjonpertime = maks*(100-tap)/100; // regner ut produksjon per time, ut i fra maksimal mulig produksjon per time som er 5kwh og trekker i fra det utregnede tapet.
}
    }
    // funksjon som regner ut forbruk og produksjon hver time og uke
void produksjonsteller(){
   if(millis() - previousMillis >= interval) {    // starter millis og teller som teller hver time i døgnet, vi simulerer et døgn som går fra 00:00 til 23:00
      previousMillis = millis();  
      teller +=1;        
      sumproduksjon = produksjonpertime + prevproduksjon; // regner totalt løpende produksjon i dette året som bare vil øke for hver time
      sumforbruk = forpertime + prevforbruk;      // regner totalt løpende forbruk i dette året som også vil øke per time
      circusESP32.write(key_lopendeproduksjon,sumproduksjon,token); // printer løpendeproduksjon til cot
      circusESP32.write(key_lopendeforbruk,sumforbruk,token); // printer løpendeforbruk til cot
      circusESP32.write(key_produksjonpertime,produksjonpertime,token); // printer forrige times produksjon i cot
      circusESP32.write(key_forbrukpertime,forpertime,token); // printer forrige times forbruk i cot
      }
      prevproduksjon = sumproduksjon; // setter forrige times produksjon = den totale produksjonen, for å kunne addere neste times produksjon på det totale forbruket
      prevforbruk = sumforbruk; // setter forrige times forbruk = det totale forbruket, for å kunne addere neste times forbruk på det totale forbruket
if(teller == 23){ // når telleren er kommet til 23.00 så resetter den telleren til 00:00 og neste dag starter
   teller =0;
   dag +=1; // starter ny dag 
  circusESP32.write(key_ukedag, dag, token); // printer dag i cot
}
if(dag==7){ 
  ukesproduksjon = sumproduksjon - forrigeukeproduksjon;    // Når dag 7 kommer vil ukasproduksjon regnes ut ifra totale forbruks summen substrahert med forrigeukes produkjson
  ukesforbruk = sumforbruk - forrigeukeforbruk;    // Når dag 7 kommer vil ukasforbruk regnes ut ifra totale forbruks summen substrahert med forrigeukes forbruk
  circusESP32.write(key_ukeproduksjon,ukesproduksjon, token); // printer til cot oppdateres hver uke
  circusESP32.write(key_ukeforbruk,ukesforbruk, token);
  forrigeukeproduksjon = sumproduksjon; // forrigeukes produksjon = summen til det totale produksjonen for å videre kunne regne ut nytt ukesforbruk
  forrigeukeforbruk = sumforbruk; // forrigeukes forbruk = summen til det totale forbruket for å videre kunne regne ut nytt ukesforbruk
  dag=0; // setter at uka starter på nytt etter dag 7
}
}


// funksjon for uketlig forbruk
void forbrukpertime(){
  forpertime = dusj + bad + komfyr + panelovn + vaskemaskin + torketrommel + tvs + oppvaskmaskin + belysningsoverom + varmetank;   // regner ut for bruk per time
}
// funksjon for forbruk ut ifra tempratur på panelovn på soverommet
void soveeovn(){
  float tempovn = circusESP32.read(key_panelovn,token); // henter inn tempratur fra soveromet panelovn
  utetemp();    // henter inn funksjon for utetempratur
  
  if(tempovn < 22 ){    // forbruket øker ved økt tempratur ved panelovnen og minker ved lavre temperatur . Har satt en grense på over og under 22 grader
  panelovn = panelovn21;
}
  if(tempovn >22){
  panelovn = panelovn26;
}
if(tempovn = 0){
  panelovn = 0;
}
}
// funksjon for å skjekke om det er folk i tv stuen og tv'en er på
void tvstue(){
  float tv = circusESP32.read(key_tv,token); // henter inn om det er folk i tv stua
  if( tv !=0){
    tvs = TV;  // økt forbruk hvis det er folk som har booket plass i stuen og da er tv'en er på helt til det er 0 personer som har booket seg inn i stua
  }
  else{
   tvs = 0; // hvis ingen har booket stue er tv'en av
  }
}
// funksjon for å regne ut ukentlig strømregning
void stromregning(){
  float strompris = circusESP32.read(key_strompris,token); // henter inn strømpriser
  stromreg = (ukesforbruk-ukesproduksjon) * (strompris/100); // regner ut ukentlig strømregning hvor vi trekker fra den ukentlige produserte strømmen.
  circusESP32.write(key_stromregning, stromreg, token); // skriver om estimert ukentlig strømpris, oppdateres ukentlig
}
void utetemp(){
  float utetemp = circusESP32.read(key_utetemp,token); // henter inn utetempratur
  if(utetemp < 0){             // hvis utetempratur er under null øker forbruket per ovn
    panelovn21 = 2.25;
    panelovn26 = 2.7;
    baddag = 0.22;
    badnatt = 0.16;
  }
  if(utetemp>20){       // hvis utetempraturn er over 20 synker ukentlige forbruket forbruket 
    panelovn21 = 0.475;
    panelovn26 = 0.74;
    baddag = 0.05;
    badnatt = 0.04;
  }
  else{                    // ellers er gjelder standard forbruk
    panelovn21 = 1.25;
    panelovn26 = 1.75;
    baddag = 0.13;
    badnatt = 0.1;
  }
}
void badforbruk(){
  float badtemp = circusESP32.read(key_bad, token);  // leser av tempraturen på badet
  utetemp(); // henter inn utetemp
  if(badtemp == 20){  // forbruket øker om dagen og synker om natten, det er fast automatisk natt og dag senking på badet
    bad = badnatt;
  }
  if(badtemp == 25){
    bad = baddag;
  }
}
void varmevtank(){
  float Vtank = circusESP32.read(key_Vtank, token); // henter inn verdien på varmtvannstanken som styres med dag og natt senking ved 40 og 70 grader.
  if( Vtank == 40){   // forbruket minker ved natt senking på 40 grader
    varmetank=Vtank40; 
  }
  if(Vtank == 70){
    varmetank=Vtank70; // forbruket øker ved dag økning på 70 grader, fast automatisk dag/natt senkning
  }
}
