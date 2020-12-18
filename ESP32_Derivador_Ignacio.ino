#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RBDdimmer.h>
#include <HardwareSerial.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
//3 seconds WDT
#define WDT_TIMEOUT 8

//Parametrización de la logica
#define Bomba_Pin 21
float Tmax = 68.5;
float Tbomba = 42.0;
float histeresis = 1;
unsigned long lastMsg1 = 0;
unsigned long lastMsg2 = 0;


//Parametrización comunicaciones
const char* ssid = "JAZZTEL_nrSK";
const char* password = "pwxftdqbnsey";
const char* mqtt_server = "192.168.1.210";
const char* MQTT_user = "inavarro";
const char* MQTT_password = "RBPi.";
IPAddress local_IP(192, 168, 1, 237);
IPAddress gateway(192, 168, 1, 1);
/*const char* ssid = "CASTILLA";
const char* password = "28Nmqwyyff74";
const char* mqtt_server = "192.168.0.10";
const char* MQTT_user = "Ruben114";
const char* MQTT_password = "nmqwyyff";
IPAddress local_IP(192, 168, 0, 37);
IPAddress gateway(192, 168, 1, 1);*/
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//Parametrización medidor potencia
#define RXD 32
#define TXD 26
SoftwareSerial pzem(RXD,TXD);
ModbusMaster node;
uint8_t result;
uint16_t data[6];
double Pot_Medidor = 0;

//Parametrización del Dimmer
#define USE_SERIAL  Serial
#define zerocross  22
#define outputPin  19
int Pot_Dimmer = 0;
dimmerLamp termo_dimmer(outputPin, zerocross);

//Parametrización de la sonda de temperatura
// Pin donde se conecta la sonda de temperatura
#define Sonda_Pin 23
// Instancia a comunicación onewire
OneWire bus_OnWire(Sonda_Pin);
// Instancia al sensor de temperatura 
DallasTemperature Sonda_Temp(&bus_OnWire);
float tempC;

void setup_wifi() {
  delay(10);
  USE_SERIAL.println();
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    USE_SERIAL.println("Fallo config...");
  }
  USE_SERIAL.print("Connectando a ");
  USE_SERIAL.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USE_SERIAL.print(".");
  }

  randomSeed(micros());
  USE_SERIAL.println("");
  USE_SERIAL.println("WiFi connectado.");
  USE_SERIAL.println("IP: ");
  USE_SERIAL.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = topic;
  USE_SERIAL.print("LLegada de mensaje [");
  USE_SERIAL.print(topicStr);
  USE_SERIAL.print("] ");
  for (int i = 0; i < length; i++) {
    USE_SERIAL.print((char)payload[i]);
  }
  USE_SERIAL.println();
  payload[length] = '\0';
  Pot_Dimmer = atoi((char *)payload);
  if (topicStr == "/TERMO/POTENCIA/IN")
  {
    //que queremos hacer
    USE_SERIAL.print("Mensaje de ");
    USE_SERIAL.print(topicStr);
    USE_SERIAL.println(Pot_Dimmer);
  }
}

void reconnect() {
  while (!client.connected()) {
    USE_SERIAL.print("Conectando a mosquitto...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_user, MQTT_password)) {
      USE_SERIAL.println("conectado a mosquitto");
      client.subscribe("/TERMO/POTENCIA/IN");
    } else {
      setup_wifi();
      USE_SERIAL.print("fallo de conexion, codigo error: ");
      USE_SERIAL.print(client.state());
      USE_SERIAL.println(" nuevo intento en 5 segundos");
      delay(5000);
    }
  }
}

void Set_Dimmer(unsigned int Consigna, double Pot_Metter){
  if(abs(Consigna-Pot_Metter) > 15){
    if(Consigna > Pot_Metter){                                         //Incrementar potencia
      if((Consigna - Pot_Metter) >= 500){
        if ((termo_dimmer.getPower() + 10) > 95){
          termo_dimmer.setPower(95);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() + 10);
        }
        
      }
      else if((Consigna - Pot_Metter) >= 200 && (Consigna - Pot_Metter) < 500){
        if ((termo_dimmer.getPower() + 7) > 95){
          termo_dimmer.setPower(95);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() + 7);
        }
      }
      else if((Consigna - Pot_Metter) >= 100 && (Consigna -Pot_Metter) < 200){
        if ((termo_dimmer.getPower() + 4) > 95){
          termo_dimmer.setPower(95);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() + 4);
        }
      }
      else if((Consigna - Pot_Metter) >= 50 && (Consigna - Pot_Metter) < 100){
        if ((termo_dimmer.getPower() + 2) > 95){
          termo_dimmer.setPower(95);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() + 2);
        }
      }
      else if((Consigna - Pot_Metter) < 50){
        if ((termo_dimmer.getPower() + 1) > 95){
          termo_dimmer.setPower(95);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() + 1);
        }
      }      
    }
    else if(Consigna < Pot_Metter){                                    //Disminuir potencia
      if((Pot_Metter - Consigna) >= 500){
        if ((termo_dimmer.getPower() - 10) > 3){
          termo_dimmer.setPower(3);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() - 10);
        }
      }
      else if((Pot_Metter - Consigna) >= 200 && (Pot_Metter - Consigna) < 500){
        if ((termo_dimmer.getPower() - 7) > 3){
          termo_dimmer.setPower(3);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() - 7);
        }
      }
      else if((Pot_Metter - Consigna) >= 100 && (Pot_Metter - Consigna) < 200){
        if ((termo_dimmer.getPower() - 4) > 3){
          termo_dimmer.setPower(3);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() - 4);
        }
      }
      else if((Pot_Metter - Consigna) >= 50 && (Pot_Metter - Consigna) < 100){
        if ((termo_dimmer.getPower() - 2) > 3){
          termo_dimmer.setPower(3);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() - 2);
        }
      }
      else if((Pot_Metter - Consigna) < 50){
        if ((termo_dimmer.getPower() - 1) > 3){
          termo_dimmer.setPower(3);
        }
        else{
          termo_dimmer.setPower(termo_dimmer.getPower() - 1);
        }
      }
    }
  }
}

void setup(){
  esp_task_wdt_init(WDT_TIMEOUT, true); //habilita el wdt
  esp_task_wdt_add(NULL);
  pinMode(Bomba_Pin, OUTPUT);
  digitalWrite(Bomba_Pin, HIGH);
  USE_SERIAL.begin(115200);
  termo_dimmer.begin(NORMAL_MODE, ON);
  Sonda_Temp.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pzem.begin(9600);
  USE_SERIAL.println("Inicio puerto serie Medidor");
  node.begin(1, pzem);
  USE_SERIAL.println("Inicio Medidor");
  USE_SERIAL.println();
  USE_SERIAL.println("*** Iniciando ***");
  Sonda_Temp.requestTemperatures();
}

void loop() {
  esp_task_wdt_reset();

  unsigned long now = millis();
  if ((now - lastMsg1) > 5000) {
    lastMsg1 = now;
    //hacemos algo cada 5 segundos
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    result = node.readInputRegisters(0x0000, 10);
    if (result == node.ku8MBSuccess) {
      Pot_Medidor = (node.getResponseBuffer(0x03)/10.0f);
    }
    tempC = Sonda_Temp.getTempCByIndex(0);
    Sonda_Temp.requestTemperatures();
  }
  if ((now - lastMsg2) > 5000) {
    lastMsg2 = now;
    //hacemos algo cada 5 segundos
    if(tempC > (Tmax + histeresis)){
      digitalWrite(Bomba_Pin, HIGH);
      termo_dimmer.setPower(0);
    }
    else if((tempC <= (Tmax - histeresis)) && (tempC >= (Tbomba + histeresis))){
      digitalWrite(Bomba_Pin, HIGH);
      Set_Dimmer(Pot_Dimmer, Pot_Medidor);
    }
    else if(tempC < (Tbomba - histeresis)){
      digitalWrite(Bomba_Pin, LOW);
      Set_Dimmer(Pot_Dimmer, Pot_Medidor);      
    }
    snprintf (msg, MSG_BUFFER_SIZE,"%3.2f", Pot_Medidor);
    client.publish("/TERMO/POTENCIA/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%2.2f", tempC);
    client.publish("/TERMO/TEMP/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%i", !digitalRead(Bomba_Pin));
    client.publish("/TERMO/BOMBA/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%i", Pot_Dimmer);
    client.publish("/TERMO/CONSIGNA/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%i", termo_dimmer.getPower());
    client.publish("/TERMO/INDICE_POTENCIA/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%2.2f", Tbomba);
    client.publish("/TERMO/CONSIGNA_TBOMBA/OUT", msg);
    snprintf (msg, MSG_BUFFER_SIZE,"%2.2f", Tmax);
    client.publish("/TERMO/CONSIGNA_TMAX/OUT", msg);
    USE_SERIAL.println(Pot_Medidor);
    USE_SERIAL.println(tempC);
    USE_SERIAL.println(!digitalRead(Bomba_Pin));
    USE_SERIAL.println(Pot_Dimmer);
    USE_SERIAL.println(termo_dimmer.getPower());
  }
}
