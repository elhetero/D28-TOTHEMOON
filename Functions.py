from metno_locationforecast import Place, Forecast
from datetime import datetime
import pandas as pd
import requests as req
import json 
import time
import pandas as pd
import time
#Her ligger funksjonene som blir referert til i raspScript. 
# sendPrice sender strømprisen, blir kalt på én gang i timen
# updatePrice oppdaterer strømprisen, blir kalt én gang om dagen
# initierer signalene som skal kontinuerlig oppdateres, og tildeler egendefinerte verdier til hver variabler
# read-funksjon leser av verdien i cot, send-funksjon sender verdien til cot, check-funksjonen sjekker om verdien avlest skal sendes
# dataVer henter ut informasjon på været og bearbeider dataen og gir ut et dataset med verdiene vi trenger
def sendPrice(previousHour, stromPris, token):
    key1 = "9999"
    t = time.localtime()
    current_hour = int(time.strftime("%H", t))
    #Sjekker at den nåværende time ikke er den samme som forrige gang den ble oppdatert
    #Slik at den ikke oppdaterer gjenntatte ganger
    if current_hour != previousHour:
        data = {'Key':key1, 'Token':token, 'Value':stromPris[current_hour]*100}
        headers = {'Content-Type': 'application/json'}
        response=req.put('https://circusofthings.com/WriteValue', data = json.dumps(data), headers = headers)
        previousHour = current_hour
    return previousHour

def updatePrice():
    #Sjekker dagens dato og henter ut data for dagen
    date = datetime.today().strftime('%Y-%m-%d')
    url =  "https://norway-power.ffail.win/?zone=NO1&date=" + date
    response = req.request(method = "GET", url=url)

    dataPrices = response.text
    parsed = json.loads(dataPrices)
    stromPris=[]
    #returnerer en array med strømprisene
    for key in parsed:
        stromPris.append(parsed[key]["NOK_per_kWh"])
    return stromPris
#Funksjoner for å lese, sende og sjekke signaler og signalverdier, til valgte signaler
class Signal:
    def __init__(self, key, token, preValue):
        self.key = key
        self.token = token
        self.preValue = preValue
            
    def read(self):
        response = req.get('https://circusofthings.com/ReadValue', params = {'Key':self.key, 'Token':self.token})
        response = json.loads(response.content)
        return response.get('Value')

    def write(self, value):
        self.value = value
        data = {'Key':self.key, 'Token':self.token, 'Value':self.value}
        headers = {'Content-Type': 'application/json'}
        response=req.put('https://circusofthings.com/WriteValue', data = json.dumps(data), headers = headers)
        return response.status_code
        
    def check(self, data, df):
        
        if isinstance(self.read(), int) and self.read() != self.preValue :
            try:
                status = self.write(df.iloc[int(self.read()), data])
                self.preValue = df.iloc[int(self.read()), data]
           
            except:
                status=0
            return status 
        else:
            return 0 
#Henter ut værdata for de valgte variablene fra metno_locationforecast, bruker loops for å iterere og hente ut verdier
#Lager så et datasett med verdiene vi trenger
def dataVer():
   
    trd = Place("Trondheim", 63.45, 10.42, 10)
 
    trd_forecast = Forecast(trd, "ResponseReadingExample/0.1 lmtonnes@ntnu.no")
 
    trd_forecast.update()
    variables = ['air_pressure_at_sea_level','air_temperature','cloud_area_fraction','precipitation_amount','relative_humidity','wind_from_direction','wind_speed',]
    with open("my_forecast.csv", "w") as f:
        f.write("start_time;end_time;")
        for variable in variables:
            f.write(variable + ";")
        f.write("\r")
        
        for interval in trd_forecast.data.intervals:
            f.write(str(interval.start_time)+ ";" + str(interval.end_time) + ";")
            for variable in variables:
                try:
                    f.write(str(interval.variables[variable].value) + ";")
                
                except:
                    f.write("NaN" + ";")
            f.write("\r")

    data = pd.read_csv("my_forecast.csv")

    split_data = data[data.columns[0]].str.split(";")
    data1 = split_data.to_list()

    variables[0:0] = ["start_time", "end_time"]
    variables.append("null")


    new_df = pd.DataFrame(data1, columns=variables)
    new_df = new_df[new_df.columns[:-1]]
    return new_df
#Leser og henter ut verdier fra cot og returnerer differansen mellom forbruk og produksjon av strøm
def getHourlyDifference(pris):
    forbrukKey = "29769"
    prodKey = "25557"
    token = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MDE4In0.FRdl8o7BcpQBew7boRm52Y7Qu55irdzJopKOZ36_E8w"
    responseForbruk = req.get('https://circusofthings.com/ReadValue', params = {'Key':forbrukKey, 'Token':token})
    responseProd = req.get('https://circusofthings.com/ReadValue', params = {'Key':prodKey, 'Token':token})
    responseForbruk = json.loads(responseForbruk.content)
    responseProd = json.loads(responseProd.content)
    estStrom = (responseForbruk.get('Value')-responseProd.get('Value'))*pris
    return estStrom
#Tar inn alt av strøm som må betales for i løpet av en dag og legger dette sammen. returnerer mengden
def estRegn(stromDag):
    x=0
    for elements in stromDag:
        x+=elements
    return x
#Estimerer stromregningen for uka. Tar inn all strømmen som har blit brukt til nå i løpet av uka og ganger dette opp
#For å få et ukes estimat, og sender verdien til COT
def estimat(stromUke):
    x=0
    for elements in stromUke:
        x+=elements
    data = {'Key':"24035", 'Token':"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MDE4In0.FRdl8o7BcpQBew7boRm52Y7Qu55irdzJopKOZ36_E8w", 'Value':x*(7/len(stromUke))}
    headers = {'Content-Type': 'application/json'}
    req.put('https://circusofthings.com/WriteValue', data = json.dumps(data), headers = headers)


    