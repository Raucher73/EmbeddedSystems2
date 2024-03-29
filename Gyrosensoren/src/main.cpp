#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

// Akkustand
#define led_pin D5

// Verbindung WLAN
#define led_WLAN D7

// Wifi
#define WIFI_SSID "Access-Point LED_Spielmatte"
#define WIFI_PASS "LED_Spielmatte"
unsigned int udpPort = 8888;
IPAddress remoteUDPIP(255, 255, 255, 255);

// Gyrosensor
#define BNO08X_RESET -1
int id = 1;
Adafruit_BNO08x  bno08x(BNO08X_RESET);
sh2_SensorValue_t sensorValue;
float x_coordination, y_coordination;

// UDP
WiFiUDP UDP;
 
void setup() {
  // Setup serial port
  Serial.begin(9600);
  Serial.println();
  Wire.begin();

  pinMode(led_pin, OUTPUT);
  pinMode(led_WLAN, OUTPUT);

  // Begin WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
 
  // Connecting to WiFi...
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);

  // Loop continuously while WiFi is not connected
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }

  // Connected to WiFi
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(led_WLAN, HIGH);
  // Begin listening to UDP port
  UDP.begin(udpPort);
  Serial.print("Listening on UDP port ");
  Serial.println(udpPort);

  //Starting Gyroskop
  Serial.println("Adafruit BNO08x test!");

  // Try to initialize!
  if (!bno08x.begin_I2C()) {
    Serial.println("Failed to find BNO08x chip");
    while (1) { delay(10); }
  }
  Serial.println("BNO08x Found!");
  delay(100);
 
}

void sendUDPMessage(String strMessage)
{
  //Array for messages, only in chars sending
  char message[strMessage.length()+1];
  
  Serial.println("sending length: " + String(strMessage.length()+1));
  strMessage.toCharArray(message, strMessage.length()+1);

  //Send UDP Message
  UDP.beginPacket(remoteUDPIP, udpPort);
  UDP.write(message);
  UDP.endPacket();
  delay(200);
}

void setReports(void) {
  Serial.println("Setting desired reports");
  if (! bno08x.enableReport(SH2_GAME_ROTATION_VECTOR)) {
    Serial.println("Could not enable game vector");
  }
}

void loop() {
  // Batteriestand abfragen
  float akkustand = analogRead(A0);
   akkustand = (akkustand / 1024) * 5;
  Serial.println(akkustand);

  if (akkustand <= 3.5){
    digitalWrite(led_pin, HIGH);
  }else{
    digitalWrite(led_pin, LOW);
  }

  //Check if Gyroskop reset
    if (bno08x.wasReset()) {
    Serial.print("Sensor was Reset");
    setReports();
  }
  
  if (! bno08x.getSensorEvent(&sensorValue)) {
    return;
  }
  
  //Read datas from Gyroskop
  x_coordination = sensorValue.un.gameRotationVector.i;
  y_coordination = sensorValue.un.gameRotationVector.j;

  if (x_coordination > 0.05){
    x_coordination = 1;
  }else{
    if (x_coordination < -0.05)
    {
      x_coordination = -1;
    }else{
      x_coordination = 0;
    }
  }

  if (y_coordination > 0.05){
    y_coordination = 1;
  }else{
    if (y_coordination < -0.05)
    {
      y_coordination = -1;
    }else{
      y_coordination = 0;
    }
  }
  
  sendUDPMessage("g"+String(id)+","+String(x_coordination)+","+String(y_coordination)+";");

  delay(10);

  }

