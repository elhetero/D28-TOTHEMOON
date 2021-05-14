import Functions as myModule
from datetime import datetime 
import time

myDate = datetime.today().day
previousHour = -1
token = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTcxIn0.DMqTkcQdjBSU7UJtbpJ3WwbHiyM5uUe3P7vkQO7v6IU"
Dag = 0
Hr = -1
#Definerer de forskjellige signalene og gir de hver sin key
temp = myModule.Signal("4159", token, 13)
winds = myModule.Signal("3237",token, 13)
windd = myModule.Signal("6136",token, 13)
humid = myModule.Signal("1983",token, 13)
clouda = myModule.Signal("17773", token, 13)
percip = myModule.Signal("21214", token, 13)
temperature = myModule.Signal("18910", token, 13)

df = myModule.dataVer()
temperature.write(df.iloc[0,3])
clouda.write(df.iloc[0,4])
stromPris = myModule.updatePrice()
stromDag = []
stromUke = []

while True:
    now = datetime.now()
    

    #Funksjoner som tar en plass én gang om dagen: dagens dato blir hentet
    #Dagteller går opp, stromprisen blir oppdatert for dagens dato, gårsdagens strømverdier blir lagret, deretter nullstilt
    if myDate != datetime.today().day:
        Dag+=1
        myDate = datetime.today().day
        if Dag == 8:
            Dag = 0
            stromUke =[]
        
        stromPris = myModule.updatePrice()
        stromUke.append(myModule.estRegn(stromDag))
        stromDag = []


    
   #Funksjoner som tar plass én gang i timen
   #Stromprisen for timen blir sendt. Vær data blir oppdatert.
   # Temperaturen for timen blir sendt, skydekket for timen blir sendt
   #strom differansen for timen blir hentet ut og lagret, med hensyn på timeprisen
    if int(now.strftime("%M")) == 0 and int(now.strftime("%H"))!=Hr :
        Hr = int(now.strftime("%H"))
        previousHour = myModule.sendPrice(previousHour, stromPris, token)
        df = myModule.dataVer()
        temperature.write(df.iloc[0,3])
        clouda.write(df.iloc[0,4])
        stromDag.append(myModule.getHourlyDifference(stromPris[int(now.strftime("%H"))]))
   
        
        
    #Funksjoner som tar plass fortløpende, oppdaterer verdier i cot hvis det blir spurt om

    t1 = temp.check(3,df)
    t2 = winds.check(8,df)
    t3 = windd.check(7,df)
    t4 = humid.check(6,df)
    t6 = percip.check(5,df)


    
