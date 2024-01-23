#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const long utcOffsetInSeconds = -18000;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

/****** WiFi Connection Details *******/
const char* ssid = "1889 College";
const char* password = "Nafziger1889";

/******* MQTT Broker Connection Details *******/
const char* mqtt_server = "192.168.5.197";
const char* mqtt_username = "arduinoesp";
const char* mqtt_password = "arduinoesp";
const int mqtt_port = 8883;

/******* Device ID and Naming *******/
const char* deviceID = "ESP8266- 4";
const char* publishName = "espDATA/Bed";
const char* subscribeName = "espDATA/BedR";

/**** Secure WiFi Connectivity Initialisation *****/
WiFiClientSecure espClient;

/**** MQTT Client Initialisation Using WiFi Connection *****/
PubSubClient client(espClient);


/* Declare Servos */
Servo servoLeftPower;
Servo servoLeftTemp;
Servo servoLeftTime;
Servo servoRightPower;
Servo servoRightTemp;
Servo servoRightTime;
int SLP_pos = 0;
int SLTe_pos = 0;
int SLTi_pos = 0;
int SRP_pos = 0;
int SRTe_pos = 0;
int SRTi_pos = 0;

/* LIVE Settins*/
bool LeftPower_State = false;
int LeftTime_State = 8;
int LeftTemp_State = 2;
bool RightPower_State = false;
int RightTime_State = 8;
int RightTemp_State = 2;

bool LeftPower_New = false;
int LeftTime_New = 8;
int LeftTemp_New = 2;
bool RightPower_New = false;
int RightTime_New = 8;
int RightTemp_New = 2;

bool updatedLeft = false;
bool updatedRight = false;

int LeftDone = 0;
int RightDone = 0;

int LeftTimeCounter = 2;
int RightTimeCounter = 2;

int LeftTimeSave = 0;
int RightTimeSave = 0;

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];


/************* Connect to WiFi ***********/
void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
}

/************* Connect to MQTT Broker ***********/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    
    Serial.print("Attempting MQTT connection...");
    String clientId = deviceID;   // Create a random client ID
    clientId += " -";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(subscribeName);   // subscribe the topics here
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");   // Wait 5 seconds before retrying
      delay(5000);
      
    }
  }
}

// {"power":"on"/"off","temp":"1-10","time":"1,2,4,6,8"}
/***** Call back Method for Receiving MQTT messages and Switching LED ****/
void callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for (int i = 0; i < length; i++) incommingMessage+=(char)payload[i];

  Serial.println("Message arrived  ["+String(topic)+"]: "+incommingMessage);

  //--- check the incomming message   
  Serial.print("New Message: ");
  Serial.println(incommingMessage);
  
  DynamicJsonDocument incoming(1024);
  deserializeJson(incoming, incommingMessage);
  JsonObject obj = incoming.as<JsonObject>();

  DynamicJsonDocument status(1024);
  DynamicJsonDocument response(1024);
  int errorCode = 200;
  String power = obj["power"];
  String side = obj["side"];
  int temp = obj["temp"];
  int time = obj["time"];
  if(side == "left" || side == "right"){
    if(power == "on"){
      status["power"] = true;
      if(temp){
        if(temp > 0 && temp < 11){
          status["temp"] = temp;
          Serial.print("Temp: ");
          Serial.println(temp);
        }else{
          errorCode = 461;
        }
      }else{
        errorCode = 460;
      }
      if(time){
        if(time > 0 && time < 15){
          if(time % 2 == 1){
            errorCode = 472;
          }else{
            status["time"] = time;
            Serial.print("time: ");
            Serial.println(time);
          }
        }else{
          errorCode = 471;
        }
      }else{
        errorCode = 470;
      }
    }else if(power == "off"){
      status["power"] = false;
    }else{
      errorCode = 410;
    }
  }else{
    errorCode = 430;
  }
  response["response"] = errorCode;
  if(errorCode != 200){
    response["recived"] = incommingMessage;
  }else{
    response["status"] = status;
    bool pw = false;
    if(power == "on"){
      pw = true;
      if(side == "left"){
        LeftPower_New = pw;
        LeftTime_New = time;
        LeftTemp_New = temp;
        updatedLeft = true;
      }
      if(side == "right"){
        RightPower_New = pw;
        RightTime_New = time;
        RightTemp_New = temp;
        updatedRight = true;
      }
    }else{
      pw = false;
      if(side == "left"){
        LeftPower_New = pw;
        updatedLeft = true;
      }
      if(side == "right"){
        RightPower_New = pw;
        updatedRight = true;
      }
    }
  }
  response["time"] = timeClient.getEpochTime();
  char mqtt_message[128];
  serializeJson(response, mqtt_message);
  publishMessage(publishName, mqtt_message);
  

}

/**** Method for Publishing MQTT Messages **********/
void publishMessage(const char* topic, String payload){
  if (client.publish(topic, payload.c_str(), true)){
    Serial.println("Message publised ["+String(topic)+"]: "+payload);
  }
}

/**** Application Initialisation Function******/
void setup() {
  Serial.begin(9600);
  delay(200);
  Serial.println("Starting WiFi Start-up Proccess");

  timeClient.begin();

  setup_wifi();

  #ifdef ESP8266
    espClient.setInsecure();
  #else
    // enable this line and the the "certificate" code for secure connection
  #endif

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Setup Servos
  servoLeftPower.attach(D1);
  servoLeftTemp.attach(D2);
  servoLeftTime.attach(D3);
  servoRightPower.attach(D5);
  servoRightTemp.attach(D6);
  servoRightTime.attach(D7);

  servoLeftPower.write(0);
  servoLeftTemp.write(0);
  servoLeftTime.write(0);
  servoRightPower.write(0);
  servoRightTemp.write(0);
  servoRightTime.write(0);

  Serial.print("Time: ");
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.print(timeClient.getSeconds());
  Serial.print(" - ");
  Serial.println(timeClient.getEpochTime());
}

void powerFunction(
  Servo &ServoPW,Servo &ServoTemp,Servo &ServoTime,
  bool Power_State,int Time_State,int Temp_State,
  bool Power_New,int Time_New,int Temp_New,
  bool updated,int Done,int TimeCounter,int TimeSave, int choose
  ){
  
  // Serial.print("Power_State1: ");
  // Serial.println(Power_State);
  // Serial.print("Power_New1: ");
  // Serial.println(Power_New);

  if(updated){
    if(Power_State != Power_New){
      servoClick(ServoPW);
      Power_State = Power_New;
      Done++;
      Serial.println("Here - - -");
    
    }else if(Done == 0){
      Done++;
    }
    // Serial.print("Power_State2: ");
    // Serial.println(Power_State);
    // Serial.print("Power_New2: ");
    // Serial.println(Power_New);
    if(Temp_State != Temp_New){
      servoClick(ServoTemp);
      Temp_State++;
      if(Temp_State == 11){
        Temp_State = 1;
      }
      if(Temp_State == Temp_New){
        Done++;
      }
    }else if(Done == 1){
      Done++;
    }


    if(Time_State != Time_New){
      if(Time_New != 8){
        if(Time_New < 8){
          if(Time_New == 1){
            servoClick(ServoTime);
            Done++;
            Time_State = Time_New;
          }else{
            servoClick(ServoTime);
            if(Time_New == TimeCounter){
              Done++;
              Time_State = Time_New;
            }else{
              TimeCounter++;
              TimeCounter++;
            }
          }
        }else if(Time_New > 8){
          TimeSave = timeClient.getEpochTime();
          Done++;
          // In this instence, Time_State will NOT update for the next 8 hours. 
        }
      }else if(Time_New == 8 && Done == 2){
        Done++;
        Time_State = Time_New;
      }
    }



    if(Done == 3){
      updated = false;
      Done = 0;
    }
  }
  if(Time_New > 8 && (TimeSave + 28800) < timeClient.getEpochTime()){
    updated = true;
    Time_New = Time_New - 8;
  }

  if(choose == 0){
    LeftPower_State = Power_State;
    LeftTime_State = Time_State;
    LeftTemp_State = Temp_State;
    LeftPower_New = Power_New;
    LeftTime_New = Time_New;
    LeftTemp_New = Temp_New;
    updatedLeft = updated;
    LeftDone = Done;
    LeftTimeCounter = TimeCounter;
    LeftTimeSave = TimeSave;
    // Serial.println("Done with LEFT");
  }else{
    RightPower_State = Power_State;
    RightTime_State = Time_State;
    RightTemp_State = Temp_State;
    RightPower_New = Power_New;
    RightTime_New = Time_New;
    RightTemp_New = Temp_New;
    updatedRight = updated;
    RightDone = Done;
    RightTimeCounter = TimeCounter;
    RightTimeSave = TimeSave;
    // Serial.println("Done with RIGHT");
  }
  
}

void servoClick(Servo &theservo){
  // Serial.println("SERVO: ");
  // Serial.println();
  theservo.write(45);
  delay(150);
  theservo.write(0);
}

/******** Main Function *************/
void loop() {
  timeClient.update();
  if (!client.connected()) {
    reconnect(); // check if client is connected
  }
  client.loop();

  powerFunction(
    servoLeftPower,servoLeftTemp,servoLeftTime,
    LeftPower_State,LeftTime_State,LeftTemp_State,
    LeftPower_New,LeftTime_New,LeftTemp_New,
    updatedLeft,LeftDone,LeftTimeCounter,LeftTimeSave, 0
  );
  delay(150);

  powerFunction(
    servoRightPower,servoRightTemp,servoRightTime,
    RightPower_State,RightTime_State,RightTemp_State,
    RightPower_New,RightTime_New,RightTemp_New,
    updatedRight,RightDone,RightTimeCounter,RightTimeSave, 1
  );
  delay(150);

  // delay(50);
}
