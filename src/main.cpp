//General Includes
#include "Passwords.h"
#include <ArduinoOTA.h>
// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
//#include "SampleMp3.h"
//Stuff for MQTT
#define WARN Serial.println
#define MQTTCLIENT_QOS2 0
//#define DEFAULT_STACK_SIZE -1
//#define MQTT_DEBUG
#include <WiFi.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>


struct Config{
  char RingSound[30];
  int Volume;
};



const char* config_filename = "/config.txt";
Config config; // <- global configuration object

WiFiClient client;
IPStack ipstack(client);
#define maxMqttSize 5000
MQTT::Client<IPStack, Countdown, maxMqttSize, 1> MQTTclient = MQTT::Client<IPStack, Countdown, maxMqttSize, 1>(ipstack);
//Stuff for the VS1053 board
// These are the pins used
#define VS1053_RESET 32 // VS1053 reset pin (not used!)
#define Ring_Pin 21

// Feather ESP32
#if defined(ESP32)
#define VS1053_CS 5    // VS1053 chip select pin (output)
#define VS1053_DCS 16  // VS1053 Data/command select pin (output)
#define CARDCS 17      // Card chip select pin
#define VS1053_DREQ 13 // VS1053 Data request, ideally an Interrupt pin
#endif
Adafruit_VS1053_FilePlayer Ada_musicPlayer =
    Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

boolean fallback = false;

//Defining method headers.
String printDirectory(File dir, int numTabs);
void message_play(MQTT::MessageData &md);
void MQTT_connect();
void start_OTA();
void saveConfiguration(const char *filename, const Config &config);
void loadConfiguration(const char *filename, Config &config);


void setup(){
  Serial.begin(9600);
  Serial.println("Connecting to WiFi...");
  pinMode(Ring_Pin, INPUT_PULLUP);
  // ### WiFi Setup ####

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setHostname("DoorBell");
  int count = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED && count <=5 )
  {
    Serial.println("Connection Failed! Trying again...");
    delay(2000);
  }
  if (WiFi.isConnected()){
  Serial.print("Wifi Connected. Ip Adress: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting OTA...");
  start_OTA();
  Serial.println("OTA started. Starting MQTT now...");
  MQTT_connect();
  }
  else{
    Serial.println("Wifi not connected. We will try it again later.");
  }

  Serial.println("\n\n CoMakingSpace Door Bell");
  while (!Ada_musicPlayer.begin())
  { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined? We will keep trying."));
  }
  if (!SD.begin(CARDCS))
  { //Check the SD Card to be present. If not, we need to play a hardcoded file.
    Serial.println(F("SD failed, or not present"));
    fallback = true;
  }
  loadConfiguration(config_filename, config);
  // Set volume for left, right channels. lower numbers == louder volume!
  Ada_musicPlayer.setVolume(config.Volume, config.Volume);
  //I do not think we will need the following line with the new library, but we need to test that, so I leave it in for now
  Ada_musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT); // DREQ int
  //Just a couple of reference lines.
  //printDirectory(SD.open("/"), 0);
  // Play a file in the background, REQUIRES interrupts!
  //Serial.println(F("Playing full track 001"));
  //Ada_musicPlayer.playFullFile("/track001.mp3");

  //Serial.println(F("Playing track 002"));
  //Ada_musicPlayer.startPlayingFile("/track002.mp3");
}

void loop(){
  
  //core functionality is to play music, so let´s do this first!
  if (digitalRead(21) < 1)
  {
    Serial.println("Ring Ring");
    if (!fallback)
    {
      //Play the MP3 file. In case this does not work, fall back to just playing a sound.
      if (!Ada_musicPlayer.playFullFile(config.RingSound));
        Ada_musicPlayer.sineTest(0x44, 500);
    }
    else
    {
      Ada_musicPlayer.sineTest(0x44, 500);
      //Ada_musicPlayer.playData(sampleMp3, sizeof(sampleMp3));
    }
    char buf[100];
    strcpy(buf, "Ring Ring");
    /*MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void *)buf;
    message.payloadlen = strlen(buf) + 1;*/
    int rc = MQTTclient.publish("/DoorBell/Ring", (void *)buf, strlen(buf) + 1);
    Serial.println("Message sent");
  }
  ArduinoOTA.handle();
  if (!WiFi.isConnected()){
    WiFi.begin();
  }
  if (!MQTTclient.isConnected())
    MQTT_connect();
  //Ensure the MQTT Messages get handled properly.
  MQTTclient.yield(1000);
}
/// File listing helper
String printDirectory(File dir, int numTabs){
  String result = "";
  while (true)
  {

    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      result += '\t';
    }
    result += entry.name();
    if (entry.isDirectory())
    {
      result += "/";
      result += "\n";
      result += printDirectory(entry, numTabs + 1);
    }
    else
    {
      // files have sizes, directories do not
      result += "\t\t";
      result += entry.size();
      result += "\n";
    }
    entry.close();
  }
  return result;
}
//handling arriving mqtt messages
void MQTT_message_control(MQTT::MessageData &md){
  MQTT::Message &message = md.message;
  Serial.print("Got the Message:");
  Serial.println((char *)message.payload);
  StaticJsonBuffer<maxMqttSize> jsonBuffer;
  JsonObject &control_message = jsonBuffer.parseObject((char *)message.payload);
  Serial.print("Got the following command: ");
  Serial.println(control_message["command"].asString());
  Serial.print("Got the following payload: ");
  Serial.println(control_message["payload"].asString());
  if (control_message["command"].as<String>().equalsIgnoreCase("play")){
    Serial.println("playing a file");
    Ada_musicPlayer.playFullFile((const char*)control_message["payload"]);
  }
  else if (control_message["command"].as<String>().equalsIgnoreCase("selectRingFile")){
    //config.RingSound = (char*)control_message["payload"].as<char*>();
      strlcpy(config.RingSound,                   // <- destination
          control_message["payload"],  // <- source
          strlen(control_message["payload"])); 
    saveConfiguration(config_filename,config);
  }
  else if (control_message["command"].as<String>().equalsIgnoreCase("setVolume")){
    //config.RingSound = (char*)control_message["payload"].as<char*>();
    Serial.print("Setting Volume to: ");
    Serial.println((int)control_message["payload"]);
    config.Volume = (int)control_message["payload"];
    Ada_musicPlayer.setVolume(config.Volume, config.Volume);
    saveConfiguration(config_filename,config);
  }
  else if (control_message["command"].as<String>().equalsIgnoreCase("listSD")){
    Serial.println("Listing SD...");
    String directoryList = printDirectory(SD.open("/"),0);
    Serial.print("Length of directory list: ");
    Serial.println(directoryList.length());
    char buf[directoryList.length() +1];
    directoryList.toCharArray(buf, directoryList.length() +1);
    int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
    Serial.println(directoryList);
  }
  else if (control_message["command"].as<String>().equalsIgnoreCase("listConfig")){
    Serial.println("listing config");
    String configuration = "";
    configuration.concat("Ringtone: ");
    configuration.concat(config.RingSound);
    configuration.concat("\n");
    configuration.concat("Volume: ");
    configuration.concat(config.Volume);
    configuration.concat("\n");
    char buf[configuration.length() +1];
    configuration.toCharArray(buf, configuration.length()+1);
    int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
    Serial.println(configuration);
  }
  else{
    Serial.print("Got some other command:");
    Serial.println(control_message["command"].as<String>());
  }



}
void MQTT_connect(){

  Serial.print("MQTT Connecting to ");
  Serial.print(hostname);
  Serial.print(":");
  Serial.println(port);

  int rc = ipstack.connect(hostname, port);
  if (rc != 1)
  {
    Serial.print("rc from TCP connect is ");
    Serial.println(rc);
    Serial.println("TCP connection not established. Not trying to connect using MQTT.");
  }
  else
  {
    //Only continue with MQTT Connection if the TCP connection was successful!
    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3.1;
    data.clientID.cstring = (char *)"DoorBellMQTTClient";
    rc = MQTTclient.connect(data);
    if (rc != 0)
    {
      Serial.print("rc from MQTT connect is ");
      Serial.println(rc);
    }
    Serial.println("MQTT connected");

    rc = MQTTclient.subscribe((const char *)"/DoorBell/Control", MQTT::QOS0, MQTT_message_control);
    if (rc != 0)
    {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
    }
    Serial.println("MQTT subscribed");
  }
}
void start_OTA(){
  ArduinoOTA.setHostname("DoorBell");

  // No authentication by default
  ArduinoOTA.setPassword(OTA_password);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File file = SD.open(filename);

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonBuffer<512> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  if (!root.success())
  {
    Serial.println(F("Failed to read file, using default configuration"));
      strlcpy(config.RingSound,                   // <- destination
          "/track001.mp3",  // <- source
          sizeof(config.RingSound));          // <- destination's capacity
  config.Volume = 90;

  }
  else
  {
  // Copy values from the JsonObject to the Config
  strlcpy(config.RingSound,                   // <- destination
          root["RingSound"] | "/track001.mp3",  // <- source
          sizeof(config.RingSound));          // <- destination's capacity
  config.Volume = root["Volume"];
  }
  // Close the file (File's destructor doesn't close the file)
  file.close();
  Serial.println("Config reading done.");
  if (!root.success())
    saveConfiguration(filename, config);
}
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate the memory pool on the stack
  // Don't forget to change the capacity to match your JSON document.
  // Use https://arduinojson.org/assistant/ to compute the capacity.
  StaticJsonBuffer<512> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.createObject();

  // Set the values
  root["RingSound"] = config.RingSound;
  root["Volume"] = config.Volume;
  // Serialize JSON to file
  if (root.printTo(file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file (File's destructor doesn't close the file)
  file.close();
}