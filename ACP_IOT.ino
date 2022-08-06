#ifdef ESP8266
 #include <ESP8266WiFi.h>  // Pins for board ESP8266 Wemos-NodeMCU
 #else
 #include <WiFi.h>  
#endif
 
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include<Servo.h>

//MQTT_VERSION = 5
#define MQTT_VERSION 5

//---- WiFi settings
const char* ssid = "WIFI_NAME";
const char* password = "WIFI_PASSWORD";

//---- MQTT Broker settings
const char* mqtt_server = "MY_MQTT_CLOUD_SERVER"; // replace with your broker url
const char* mqtt_username = "NAME";
const char* mqtt_password = "PASS";
const int mqtt_port =8883;

String SPACE_ID = "6230e4050551177b1192d7cd";



unsigned long lastMillis = 0;
//servo variables
Servo servo;

const int trigPin = D5;
const int echoPin = D6;
long duration;
int distanceCm;

bool barrierIsOpened = false;

bool carJustParked = false;
bool carJustLeft = false;

WiFiClientSecure espClient;   // for no secure connection use WiFiClient instead of WiFiClientSecure 
//WiFiClient espClient;
PubSubClient client(espClient);


//==========================================
void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
}


//=====================================
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";   // Create a random client ID
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");

      String requestTopic = "request/openbarrier/" + SPACE_ID;
      client.subscribe(requestTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");   // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//================================================ setup
//================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);
  setup_wifi();
//  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
servo.attach(4);
 servo.write(0);
  
 pinMode(trigPin, OUTPUT);
 pinMode(echoPin, INPUT);

  #ifdef ESP8266
    espClient.setInsecure();
  #else   // for the ESP32
    //espClient.setCACert(root_ca);      // enable this line and the the "certificate" code for secure connection
  #endif
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}


//================================================ loop
//================================================
void loop() {
  sendJSON();  
  ultraSonicReader();
  
  if(carJustLeft && distanceCm > 20) {
    //close barrier
    delay(10000);
    servo.write(0);
    barrierIsOpened = false;
    carJustLeft = false;
    delay(1000);
  }

  while(distanceCm <= 20) { //trigger when driver leaves to close the barrier.
    ultraSonicReader();

  if(carJustParked) {
    //close barrier
    delay(10000);
    servo.write(0);
    barrierIsOpened = false;
    carJustParked = false;
    delay(1000);
  }


    delay(100);
    sendJSON();  
    if(!carJustParked && !carJustLeft && distanceCm > 20) {
      delay(2000);
      servo.write(0);
      barrierIsOpened = false;
      delay(1000);
    }
  }
}

void ultraSonicReader () {
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * 0.034 / 2;
  Serial.println(distanceCm);
  delay(500);
}

void sendJSON() {  
  if (!client.connected()) reconnect();
  client.loop();
  String JSON = "{\"_id\": \"" + SPACE_ID + "\", \"barrierIsOpened\": "+ (barrierIsOpened? "true" : "false") +", \"vacant\": " + (distanceCm > 20 ? "true" : "false") + " }"; 
 
  if(millis() - lastMillis > 5000) {
    lastMillis = millis();
    String topicSend = "parking/space/" + SPACE_ID;
    client.publish(topicSend.c_str(), JSON.c_str());
  }
}

//=======================================  
// This void is called every time we have a message from the broker

void callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for (int i = 0; i < length; i++) incommingMessage+=(char)payload[i];
  
  Serial.println("Message arrived ["+String(topic)+"]"+incommingMessage);

  if(incommingMessage == "openForParking") {
    servo.write(180);
    barrierIsOpened = true;
    carJustParked = true;
    carJustLeft = false;
    String responseTopic = "response/openbarrier/"+SPACE_ID;
    client.publish(responseTopic.c_str(), "{\"msg\": \"opend successfuly\"}");
  } else if (incommingMessage == "openForLeaving") {
    servo.write(180);
    barrierIsOpened = true;
    carJustLeft = true;
    carJustParked = false;
    String responseTopic = "response/openbarrier/"+SPACE_ID;
    client.publish(responseTopic.c_str(), "{\"msg\": \"opend successfuly\"}");
  }
}
