#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid="iot";
const char* password="123456789";
const char* POST_URL="https://iotcloud22.in/4933/post_value.php";

WiFiClientSecure client;

#define DHT_PIN 4
#define DHT_TYPE DHT11
#define CURRENT_PIN 35
#define VIBRATION_PIN 27
#define SDA_PIN 21
#define SCL_PIN 22
#define SMOKE_PIN 34
#define VOC_AO 32
#define DOOR_PIN 14

DHT dht(DHT_PIN,DHT_TYPE);
Adafruit_BMP085 bmp;

const float SENSITIVITY=0.185;

float temperature=0.0;
float humidity=0.0;
float airPressure=0.0;
float altitude=0.0;
float zeroCurrentVoltage=2.41;
float current=0.0;

int vibrationState=0;
int smokeValue=0;
int vocValue=0;

int doorState=LOW;
int lastDoorState=LOW;
int doorOpenCount=0;

unsigned long lastUploadTime=0;
const unsigned long uploadInterval=5000;

float getAveragedVoltage(int pin,int samples)
{
  unsigned long sum=0;

  for(int i=0;i<samples;i++)
  {
    sum+=analogRead(pin);
    delayMicroseconds(100);
  }

  float avgADC=(float)sum/samples;
  float voltage=(avgADC*3.3)/4095.0;

  return voltage;
}

void calibrateCurrentSensor()
{
  Serial.println();
  Serial.println("CURRENT SENSOR CALIBRATION");
  Serial.println("Make sure NO LOAD is connected.");
  Serial.println("Calibration starting...");

  delay(2000);

  Serial.print("Calibrating Current Sensor... ");

  zeroCurrentVoltage=getAveragedVoltage(CURRENT_PIN,1000);

  Serial.println("DONE!");

  Serial.print("Zero Current Voltage: ");
  Serial.print(zeroCurrentVoltage,3);
  Serial.println(" V");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 MULTI-PARAMETER IoT MONITOR");

  dht.begin();
  Serial.println("DHT11 Initialized");

  pinMode(VIBRATION_PIN,INPUT);
  Serial.println("Vibration Sensor Initialized");

  pinMode(CURRENT_PIN,INPUT);
  Serial.println("Current Sensor Initialized");

  pinMode(SMOKE_PIN,INPUT);
  Serial.println("Smoke Sensor Initialized");

  pinMode(VOC_AO,INPUT);
  Serial.println("VOC Sensor Initialized");

  pinMode(DOOR_PIN,INPUT_PULLUP);
  Serial.println("MC-38 Door Sensor Initialized on GPIO 14");

  calibrateCurrentSensor();

  Wire.begin(SDA_PIN,SCL_PIN);

  Serial.println("I2C Initialized");

  if(!bmp.begin())
  {
    Serial.println("BMP180 NOT FOUND!");
    Serial.println("Check SDA -> GPIO 21");
    Serial.println("Check SCL -> GPIO 22");
    Serial.println("Check VCC and GND");
  }
  else
  {
    Serial.println("BMP180 FOUND!");
  }

  lastDoorState=digitalRead(DOOR_PIN);

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid,password);

  while(WiFi.status()!=WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();

  Serial.println("SYSTEM READY");
}

void loop()
{
  doorState=digitalRead(DOOR_PIN);

  if(doorState==HIGH&&lastDoorState==LOW)
  {
    doorOpenCount++;
  }

  lastDoorState=doorState;

  temperature=dht.readTemperature();
  humidity=dht.readHumidity();

  Serial.println();
  Serial.println("------------ SENSOR DATA ----------------");

  if(isnan(temperature)||isnan(humidity))
  {
    Serial.println("DHT11 ERROR!");
    temperature=0.0;
    humidity=0.0;
  }
  else
  {
    Serial.print("Temperature : ");
    Serial.print(temperature,2);
    Serial.println(" °C");

    Serial.print("Humidity    : ");
    Serial.print(humidity,2);
    Serial.println(" %");
  }

  airPressure=bmp.readPressure()/100.0;
  altitude=bmp.readAltitude();

  Serial.print("Air Pressure: ");
  Serial.print(airPressure,2);
  Serial.println(" hPa");

  Serial.print("Altitude    : ");
  Serial.print(altitude,2);
  Serial.println(" m");

  float currentVoltage=getAveragedVoltage(CURRENT_PIN,500);
  float voltageDiff=currentVoltage-zeroCurrentVoltage;

  current=voltageDiff/SENSITIVITY;

  if(abs(current)<0.05)
  {
    current=0.00;
  }

  Serial.print("Current     : ");
  Serial.print(current,2);
  Serial.println(" A");

  vibrationState=digitalRead(VIBRATION_PIN);

  if(vibrationState==HIGH)
  {
    Serial.println("Vibration   : DETECTED");
  }
  else
  {
    Serial.println("Vibration   : NORMAL");
  }

  smokeValue=analogRead(SMOKE_PIN);

  Serial.print("Smoke Sensor: ");
  Serial.print(smokeValue);
  Serial.print(" | Status : ");

  if(smokeValue<=1500)
  {
    Serial.println("NORMAL AIR");
  }
  else
  {
    Serial.println("SMOKE DETECTED");
  }

  vocValue=analogRead(VOC_AO);

  Serial.print("VOC Value   : ");
  Serial.print(vocValue);
  Serial.print(" | Status : ");

  if(vocValue<=1500)
  {
    Serial.println("NORMAL AIR");
  }
  else if(vocValue<=3000)
  {
    Serial.println("MODERATE");
  }
  else if(vocValue<=3400)
  {
    Serial.println("POLLUTED AIR");
  }
  else
  {
    Serial.println("HIGHLY POLLUTED AIR");
  }

  Serial.print("Door Status : ");

  if(doorState==HIGH)
  {
    Serial.println("OPEN");
  }
  else
  {
    Serial.println("CLOSED");
  }

  Serial.print("Door Opened : ");
  Serial.print(doorOpenCount);
  Serial.println(" times");

  if(doorOpenCount>10)
  {
    Serial.println("DOOR ALERT! Opened more than 10 times!");
  }

  Serial.println("----------------------------------------");

  if(millis()-lastUploadTime>=uploadInterval)
  {
    lastUploadTime=millis();

    if(WiFi.status()==WL_CONNECTED)
    {
      HTTPClient http;

      http.begin(client,POST_URL);

      http.addHeader("Content-Type","application/x-www-form-urlencoded");

      String httpRequestData=
        "value1="+String(temperature,2)+" & "+String(humidity,2)+
        "&value2="+String(vocValue)+
        "&value3="+String(airPressure,2)+
        "&value4="+String(current,2)+
        "&value5="+String(vibrationState)+
        "&value6="+String(smokeValue)+
        "&value7="+String(vocValue)+
        "&value8="+String(doorState)+
        "&value9="+String(doorOpenCount);

      Serial.println();
      Serial.println("------------ IoT CLOUD -----------------");

      Serial.print("Sending Data: ");
      Serial.println(httpRequestData);

      int httpResponseCode=http.POST(httpRequestData);

      if(httpResponseCode>0)
      {
        Serial.print("HTTP CODE: ");
        Serial.println(httpResponseCode);

        String response=http.getString();

        Serial.print("Server Response: ");
        Serial.println(response);
      }
      else
      {
        Serial.print("HTTP ERROR: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    }
    else
    {
      Serial.println("WiFi Disconnected!");
      Serial.println("Reconnecting...");

      WiFi.disconnect();
      WiFi.begin(ssid,password);
    }
  }

  delay(2000);
}