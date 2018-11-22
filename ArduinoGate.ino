#include <Wire.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Adafruit_VL53L0X.h"
#define ID "SG2"
/****** setup WiFi ******/
char ssid[] = "wlan_saltuaria";
char pass[] = "antlabpolitecnicomilano";
int status = WL_IDLE_STATUS;     
WiFiClient ClientWiFi;
PubSubClient client(ClientWiFi);

/****** setup MQTT ******/
const char* MQTT_SERVER = "10.79.1.176";
const char* TOPIC = "smartgate/sg2/mls/multiple_tof";
const char* TEST_TOPIC = "smartgate/sg2/mls/multiple_tof/test";

/****** setup temporal parameter ******/
const int MILLISECOND_PER_PACKET = 1000;
const int MIN_MOVEMENT = 500;
const int DURATA = 1000;

/****** PIN MKR1000 ******/
const int PINtoRESET = 1;
const int ENTRY_LED = 2;
const int EXIT_LED = 3;
const int SENSOR_0 = 4;
const int SENSOR_1 = 5;



/****** setup JSON array ******/
char JSONmessagebuffer[10000];
StaticJsonBuffer<10000>  JSONbuffer;
JsonObject& root = JSONbuffer.createObject();
JsonArray& tof0 = root.createNestedArray("TOF0");
JsonArray& tof1 = root.createNestedArray("TOF1");

/****** sampling variables ******/
int sample = 0;
int sequenceNumber = 0;
int secondi = 1;
long int counter = 0;


/****** logic of the counter ******/
int new_ts_0 = 0;
int new_ts_1 = 0;
bool flag_0 = false;
bool flag_1 = false;
int timer_entrata = 0;
int timer_uscita = 0;


/****** ToF object ******/
Adafruit_VL53L0X sens0 = Adafruit_VL53L0X();
Adafruit_VL53L0X sens1 = Adafruit_VL53L0X();

void setup()
{
  delay(2000);
  Serial.println("Run this shit");
  /****** RESET ******/
  pinMode(PINtoRESET, INPUT); 
  digitalWrite(PINtoRESET, LOW);
  
  /****** LED ******/
  pinMode(ENTRY_LED, OUTPUT);
  pinMode(EXIT_LED, OUTPUT);


  
  /****** TOF ******/
  digitalWrite(SENSOR_0, LOW);
  digitalWrite(SENSOR_1, LOW);

  
  //start initialization multiple sensor 
  pinMode(SENSOR_0, OUTPUT);
  pinMode(SENSOR_1, OUTPUT);

  digitalWrite(SENSOR_0, LOW);
  digitalWrite(SENSOR_1, LOW);
 
  delay(10);

  digitalWrite(SENSOR_0, HIGH);
  digitalWrite(SENSOR_1, HIGH);
  //end initialization multiple sensor

  
  //Test SENSOR 0
  digitalWrite(SENSOR_0, HIGH);
  digitalWrite(SENSOR_1, LOW);
  if (!sens0.begin(0x30)) {
    Serial.println(F("Failed to boot VL53L0X - 0 "));
    while(1);
  }

  //Test SENSOR 1
  digitalWrite(SENSOR_0, HIGH);
  digitalWrite(SENSOR_1, HIGH);
  if (!sens1.begin(0x31)) {
    Serial.println(F("Failed to boot VL53L0X - 1 "));
    while(1);
  }
    

  /****** WIFI ******/
  while ( status != WL_CONNECTED) 
  {status = WiFi.begin(ssid, pass);}

  /****** MQTT ******/
  client.setServer(MQTT_SERVER, 1883);  
  if(!client.connected())
  {reconnect();}
  client.loop();

  counter=millis();
  Serial.println("Finish setup");
}

void loop()
{
  //Read Length Side 0
  VL53L0X_RangingMeasurementData_t measure0;
  sens0.rangingTest(&measure0, false);
  long distance_side_0 = measure0.RangeMilliMeter;

  //Read Length Side 1
  VL53L0X_RangingMeasurementData_t measure1;
  sens1.rangingTest(&measure1, false);
  long distance_side_1 = measure1.RangeMilliMeter;

  //Check error - reset side 0 
  if (distance_side_0 < -1 or (distance_side_0 > 1500  and distance_side_0 < 7999))
  {
    
    client.publish(TEST_TOPIC,"Reset - side 0");
    /*** Debug reset message 
    String pubString = String(distance_side_0);
    char message_buff [] = "";
    pubString.toCharArray(message_buff, pubString.length()+1); 
    client.publish(TEST_TOPIC,message_buff);
    ***/
    delay(10);
    pinMode(PINtoRESET, OUTPUT);   
  }

  //Check error - reset side 1
  if (distance_side_1 < -1 or (distance_side_1 > 1500  and distance_side_1 < 7999))
  {
    client.publish(TEST_TOPIC,"Reset - side 1");
    /*** Debug reset message 
    String pubString_1 = String(distance_side_1);
    char message_buff_1 [] = "";
    pubString_1.toCharArray(message_buff_1, pubString_1.length()+1); 
    client.publish(TEST_TOPIC,message_buff_1);
    ***/
    delay(10);
    pinMode(PINtoRESET, OUTPUT);
  }

  //Correct length error
  if (distance_side_0 > 1000)
  {
    distance_side_0 = 8190;
  }
  
  if (distance_side_1 > 1000)
  {
    distance_side_1 = 8190;
  }
  //Plot multiple graph
  Serial.print(distance_side_0);
  Serial.print(",");
  Serial.println(distance_side_1);

  //save lengths in json arrays
  tof0.add(distance_side_0);
  tof1.add(distance_side_1);
  
  /****** Real Time analysis ******/
  if (distance_side_0 < 8000 and flag_0 == false)
  {
    new_ts_0 = millis();
    flag_0 = true;
  }
  if (distance_side_1 < 8000 and flag_1 == false)
  {
    new_ts_1 = millis();
    flag_1 = true;
  }
  if (millis()-MIN_MOVEMENT > new_ts_0 and distance_side_0 > 8000)
  {
    flag_0 = false;
  }
  if (millis()-MIN_MOVEMENT > new_ts_1 and distance_side_1 > 8000)
  {
    flag_1 = false;
  }  
  if (flag_0 == true and flag_1 == true)
  {
    if (new_ts_0 > new_ts_1)
      {
        flag_0 = false;
        flag_1 = false;

        client.publish(TEST_TOPIC,"2,Entry");
        digitalWrite(ENTRY_LED, HIGH);
        timer_entrata = millis();
      }
    else if (new_ts_0 < new_ts_1)
      {
        flag_0 = false;
        flag_1 = false;

        client.publish(TEST_TOPIC,"2,Exit");
        digitalWrite(EXIT_LED, HIGH);
        timer_uscita = millis();
      }
  }
  if (millis()-DURATA > timer_entrata)
  {
    digitalWrite(ENTRY_LED, LOW);
  }
  
  if (millis()-DURATA > timer_uscita)
  {
    digitalWrite(EXIT_LED, LOW);
  }
  /****** End Real Time analysis ******/
    
  sample = sample+1;

  /****** sending packet ******/
  if((millis()-counter)>secondi*MILLISECOND_PER_PACKET)
    {
      //check WiFi status
      while(WiFi.status() != WL_CONNECTED)
      {WiFi.begin(ssid, pass);}
      
      //check MQTT status 
      if(!client.connected())
      {reconnect();}
      client.loop();    

      root["SN"]=String(sequenceNumber);
      root["ID"]=ID;
      //sampling result
      /*
      Serial.print("\n#SN ");
      Serial.print(sequenceNumber);    
      Serial.print(" Campioni= ");
      Serial.print(sample);
      Serial.print(" istante= ");
      Serial.println(millis()-counter);
      */
           
      sample=0;
      secondi=secondi+1;
      sequenceNumber = sequenceNumber+1;  
      
      //publish JSON message via MQTT
      root.printTo(JSONmessagebuffer);//, 
      client.publish(TOPIC,JSONmessagebuffer);

      //reset JSON buffer
      JSONbuffer.clear();
      JsonObject& root = JSONbuffer.createObject();
      JsonArray& tof0 = root.createNestedArray("TOF0");  
      JsonArray& tof1 = root.createNestedArray("TOF1");  
    }
}

void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    if (client.connect("")) 
    {
       //connected
    } 
    else 
    {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
