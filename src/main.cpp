#include <Arduino.h>
#include <BH1750.h>
#include <DRV8834.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define wifi_ssid "ASUS_F8_2G"
#define wifi_password "zaq12wsx"
#define mqtt_broker "192.168.0.227"

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 400

// Microstepping mode. If you hardwired it to save pins, set to the same value here.
#define MICROSTEPS 32

#define DIR 14
#define STEP 33
volatile short maxRotates = 0;
volatile short currentRotate = 0;
volatile float lightIntensity = 0;
volatile float lightIntensityMean = 0;
volatile uint16_t sample = 0;
volatile float lowIntensityMode = 0.3;
volatile float mediumIntensityMode = 0.6;
volatile float highIntensityMode = 0.9;
volatile bool upperHeightSet = false;
volatile bool calibrated = false;
volatile bool ready = false;

String calibrationState;
String RollershutterState;
String AutomaticModeState = "OFF";
volatile bool receivedMessage = false;

#define M0 26
#define M1 27
DRV8834 stepper(MOTOR_STEPS, DIR, STEP, M0, M1);

WiFiClient espClient;
PubSubClient client(espClient);

BH1750 lightMeter(0x23);

void setupWiFi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("-");
  }

  Serial.println("");
  Serial.println("Wifi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) 
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "cmnd/rollershutter") 
  {
    Serial.print("Changing output to ");
    if(messageTemp == "UP")
    {
      Serial.println("UP");

    }
    else if(messageTemp == "DOWN")
    {
      Serial.println("DOWN");
      
    }
    else if(messageTemp == "STOP")
    {
      Serial.println("STOP");
      
    }
    RollershutterState = messageTemp;
    AutomaticModeState = "OFF";
    client.publish("state/automaticMode", "OFF");
  }

  if (String(topic) == "cmnd/automaticMode")
  {
    RollershutterState = "STOP";
    Serial.print("Changing output to ");
    if(messageTemp == "ON")
    {
      Serial.println(messageTemp);
    }
    else if(messageTemp == "OFF")
    {
      Serial.println(messageTemp);
    }
    AutomaticModeState = messageTemp;
  }
  if(String(topic) == "cmnd/calibration")
  {
    if(messageTemp == "UP")
    {
      Serial.println("UP");

    }
    else if(messageTemp == "DOWN")
    {
      Serial.println("DOWN");
      
    }
    else if(messageTemp == "STOP")
    {
      Serial.println("STOP");
      
    }
    calibrationState = messageTemp;
    AutomaticModeState = "OFF";
    client.publish("state/automaticMode", "OFF");
  }
  if(String(topic) == "cmnd/rollershutter/upperHeight")
  {
    if(messageTemp == "set")
    {
      upperHeightSet = true;
      calibrationState = "STOP";
    }
    else if(messageTemp == "done")
    {
      upperHeightSet = false;
    }
  }
  if(String(topic) == "cmnd/rollershutter/bottomHeight")
  {
    if(messageTemp == "set" && upperHeightSet == true)
    {
      ready = true;
    }
    else if(messageTemp == "set" && upperHeightSet == false)
    {
      client.publish("state/rollershutter/bottomHeight", "done");
    }
  }
}

void reconnect()
{
  while(!client.connected())
  {
    Serial.print("\nConnecting to broker... ");
    if(client.connect(mqtt_broker))
    {
      Serial.println("\nConnected to broker");
      client.subscribe("cmnd/rollershutter");
      client.subscribe("cmnd/automaticMode");
      client.subscribe("cmnd/rollershutter/upperHeight");
      client.subscribe("cmnd/rollershutter/bottomHeight");
      client.subscribe("cmnd/calibration");
    }
    else
    {
      Serial.println("\nFailed to connect broker...");
      delay(1000);
    }
  }
}

void AutomaticMode(float intensity)
{
  uint8_t currentRotateTmp = currentRotate;
  uint8_t height;

  if(intensity >= 1000)
  {
    height = maxRotates * highIntensityMode;
  }
  else if(intensity < 1000 && intensity >= 400)
  {
    height = maxRotates * mediumIntensityMode;
  }
  else if(intensity < 400 && intensity >= 0)
  {
    height = maxRotates * lowIntensityMode;
  }

  if(currentRotateTmp < height)
  {
    for(uint8_t i = 0; i < height - currentRotateTmp; i++)
    {
      stepper.rotate(-60);
      currentRotate++;
    }
  }
  else if(currentRotateTmp > height)
  {
    for(uint8_t i = 0; i < currentRotateTmp - height; i++)
    {
      stepper.rotate(60);
      currentRotate--;
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupWiFi();
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);
  stepper.begin(90, MICROSTEPS);
  Wire.begin();
  lightMeter.begin(lightMeter.CONTINUOUS_HIGH_RES_MODE_2);
  
}

void loop() {
  // put your main code here, to run repeatedly:
  if(!client.connected())
  {
    reconnect();
  }
  client.loop();
  if(RollershutterState == "UP" && currentRotate > 0 && AutomaticModeState == "OFF" && calibrated == true)
  {
    stepper.rotate(60);
    currentRotate--;
  }
  if(RollershutterState == "DOWN" && currentRotate < maxRotates && AutomaticModeState == "OFF" && calibrated == true)
  {
    stepper.rotate(-60);
    currentRotate++;
  }
  if(currentRotate == maxRotates || currentRotate == 0)
  {
    RollershutterState = "STOP";
  }
  if(calibrationState == "UP" && upperHeightSet == false || calibrationState == "DOWN" && upperHeightSet == false)
  {
    if(calibrationState == "UP")
      stepper.rotate(60);
    else if(calibrationState == "DOWN")
      stepper.rotate(-60);
    currentRotate = 0;
    calibrated = false;
    upperHeightSet = false;
  }
  if(calibrationState == "DOWN" && upperHeightSet == true || calibrationState == "UP" && upperHeightSet == true)
  {
    if(calibrationState == "DOWN")
    {
      stepper.rotate(-60);
      currentRotate++;
    }
    else if(calibrationState == "UP")
    {
      stepper.rotate(60);
      currentRotate--;
    }
    calibrated = false;
  }
  if(ready == true)
  {
    client.publish("state/rollershutter/bottomHeight", "done");
    client.publish("state/rollershutter/upperHeight", "done");
    maxRotates = currentRotate;
    calibrated = true;
    upperHeightSet = false;
    ready = false;
  }
  if(AutomaticModeState == "ON")
  {
    if(lightMeter.measurementReady() == true)
    {
      lightIntensity += lightMeter.readLightLevel();
      sample++;
    }
    if(sample == 250)
    {
      lightIntensityMean = lightIntensity/(float)sample;
      Serial.println(lightIntensityMean);
      lightIntensity = 0;
      sample = 0;
      AutomaticMode(lightIntensityMean);
    }
  }
}

