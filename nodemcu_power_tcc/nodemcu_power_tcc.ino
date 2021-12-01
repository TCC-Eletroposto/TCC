#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <coredecls.h>                  // settimeofday_cb()
#include <PolledTimeout.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <TZ.h>

#define MYTZ TZ_America_Sao_Paulo

Adafruit_INA219 ina219;
const char* ssid = "time capsule";
const char* password = "jmsm2218";
const int sendTime = 10;
bool sentToday = false;

String serverName = "http://192.168.15.32:5000/teste";

float energy_mJ = 0;
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);
static time_t now;
static esp8266::polledTimeout::periodicMs showTimeNow(20000);

void showTime() {   // This function is used to print stuff to the serial port, it's not mandatory
  now = time(nullptr);      // Updates the 'now' variable to the current time value

  // human readable
  Serial.print("ctime:     ");
  Serial.print(ctime(&now));
  // Here is one example showing how you can get the current month
  Serial.print("current month: ");
  Serial.println(localtime(&now)->tm_mon);
  // Here is another example showing how you can get the current year
  Serial.print("current year: ");
  Serial.println(localtime(&now)->tm_year);
  // Look in the printTM method to see other data that is available
  Serial.println();
}

void time_is_set_scheduled() {    // This function is set as the callback when time data is retrieved
  // In this case we will print the new time to serial port, so the user can see it change (from 1970)
  showTime();
}

void setup(void) 
{
  Serial.begin(9600); 
  while (!Serial) {
      // will pause Zero, Leonardo, etc until serial console opens
      delay(1);
  }
  
  Wire.begin(D1, D2);

  uint32_t currentFrequency;
    
  Serial.println("Hello!");
  
  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();

  Serial.println("Measuring voltage and current with INA219 ...");

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  settimeofday_cb(time_is_set_scheduled);
  configTime(MYTZ, "pool.ntp.org");
}

void loop(void) 
{
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  energy_mJ = energy_mJ + power_mW;
  Serial.println( energy_mJ);
  //vai rodar no periodo explitado acima
  if (showTimeNow) {
    now = time(nullptr);
    int hour_now = localtime(&now)->tm_hour;
    if(hour_now==sendTime && sentToday == false){
      Serial.println("Entrou");
      //Check WiFi connection status
      if(WiFi.status()== WL_CONNECTED){
        WiFiClient client;
        HTTPClient http;
      
        // Your Domain name with URL path or IP address with path
       http.begin(client, serverName);

        // Specify content-type header
        http.addHeader("Content-Type", "application/json");
        char timestamp[80];
        sprintf(timestamp, "%d-%02d-%02dT%02d:%02d:%02d", localtime(&now)->tm_year+1900, localtime(&now)->tm_mon+1, localtime(&now)->tm_mday, localtime(&now)->tm_hour, localtime(&now)->tm_min, localtime(&now)->tm_sec);
        
        char body[256];
        sprintf(body, "{\"timestamp\":\"%s\",\"energy\":\"%s\"}", timestamp,  String(energy_mJ,3));
        int httpResponseCode = http.POST(body);
       
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        energy_mJ = 0;
        sentToday = true;
          
        // Free resources
        http.end();
      }
      else {
        Serial.println("WiFi Disconnected");
      }
    }
    if(hour_now==0 && sentToday == true){
      sentToday = false;
    }
    
  }

  delay(1000);
}
