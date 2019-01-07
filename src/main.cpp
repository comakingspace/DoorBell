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

//#define MQTT_PAHO 1
#define MQTT_PUBSUB 1


struct Config{
  char RingSound[100];
  int Volume;
};

const char* config_filename = "/config.txt";
Config config; // <- global configuration object

WiFiClient client;

#ifdef MQTT_PAHO
  IPStack ipstack(client);
  #define MQTT_MAX_PACKET_SIZE 5000
  MQTT::Client<IPStack, Countdown, MQTT_MAX_PACKET_SIZE, 1> MQTTclient = MQTT::Client<IPStack, Countdown, maxMqttSize, 1>(ipstack);
#endif
#ifdef MQTT_PUBSUB
  #include <PubSubClient.h>
  PubSubClient pubsub_client(client);
#endif

char* answerTopic = "/DoorBell/Answers";

//Stuff for the VS1053 board
// These are the pins used
#define VS1053_RESET 32 // VS1053 reset pin (not used!)
#define Ring_Pin 21

// Feather ESP32
#if defined(ESP32)
  #define VS1053_CS 5    // VS1053 chip select pin (output)
  #define VS1053_DCS 16  // VS1053 Data/command select pin (output)
  #define CARDCS 17      // Card chip select pin
  #define VS1053_DREQ 4 // VS1053 Data request, ideally an Interrupt pin
#endif
Adafruit_VS1053_FilePlayer Ada_musicPlayer =
    Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

boolean fallback = false;

//Defining method headers.
String printDirectory(File dir, int numTabs);
void message_play(MQTT::MessageData &md);
void start_OTA();
void saveConfiguration(const char *filename, const Config &config);
void loadConfiguration(const char *filename, Config &config);

//void paho_checkMQTT(void * pvParameters);
void checkBell();
void checkBell_Task(void * pvParameters);

void generic_MQTT_message_control(char* payload);
void generic_MQTTSend(String msg, char* topic);
#ifdef MQTT_PAHO
  void paho_MQTT_message_control(MQTT::MessageData &md);
  void paho_MQTT_connect();
  void paho_checkMQTT();
  void paho_checkMQTT_Task(void * pvParameters);
  void paho_MQTTSend(String msg, char* topic);
#endif
#ifdef MQTT_PUBSUB
  void pubsub_callback(char* topic, byte* payload, unsigned int length);
  void pubsub_connect();
  void pubsub_MQTT_update();
  void pubsub_MQTTSend(String msg, char* topic);
#endif

void setup(){
  // General DoorBell setup: Input Pin and VS1053
    Serial.begin(9600);
    Serial.println("\n\n CoMakingSpace Door Bell");
    while (!Ada_musicPlayer.begin() && !Serial.available()){ 
      // initialise the music player
      Serial.println(F("Couldn't find VS1053, do you have the right pins defined? We will keep trying."));
    }
    if (!SD.begin(CARDCS)){ 
      //Check the SD Card to be present. If not, we need to play a hardcoded file.
      Serial.println(F("SD failed, or not present"));
      fallback = true;
    }
    SD.end();
    loadConfiguration(config_filename, config);
    // Set volume for left, right channels. lower numbers == louder volume!
    Ada_musicPlayer.setVolume(config.Volume, config.Volume);
    //I do not think we will need the following line with the new library, but we need to test that, so I leave it in for now
    Ada_musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT); // DREQ int
    pinMode(Ring_Pin, INPUT_PULLUP);
    xTaskCreatePinnedToCore(checkBell_Task, "checkBell", 10000, NULL, 0, NULL,0);
    //attachInterrupt(Ring_Pin,checkBell,FALLING);
  // WiFi Setup
    Serial.begin(9600);
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.setHostname("DoorBell");
    int count = 0;
    while (WiFi.waitForConnectResult() != WL_CONNECTED && count <=3 )
    {
      Serial.println("Connection Failed! Trying again...");
      delay(1000);
      count++;
    }
    if (WiFi.isConnected()){
    Serial.print("Wifi Connected. Ip Adress: ");
    Serial.println(WiFi.localIP());
  // OTA Setup
    Serial.println("Starting OTA...");
    start_OTA();
  // MQTT Setup
    Serial.println("OTA started. Starting MQTT now...");
    
    #ifdef MQTT_PAHO
      paho_MQTT_connect();
    #endif
    #ifdef MQTT_PUBSUB
      pubsub_client.setServer(hostname, port);
      pubsub_client.setCallback(pubsub_callback);
    #endif
    }
    else{
      Serial.println("Wifi not connected. We will try it again later.");
    }
  // Initialize Tasks
  //runner.init();
  //runner.addTask(bellTask);
  //runner.addTask(MQTTTask);
  //bellTask.enable();
  //MQTTTask.enable();
  
  
  //xTaskCreatePinnedToCore(paho_checkMQTT_Task, "paho_checkMQTT", 10000, NULL, 0, NULL, 1);
}

void loop(){
  //vTaskDelay(10);
  ArduinoOTA.handle();
  if (!WiFi.isConnected()){
    WiFi.begin();
  
  }
  #ifdef MQTT_PAHO
    paho_checkMQTT();
  #endif
  #ifdef MQTT_PUBSUB
    pubsub_MQTT_update();
  #endif
  delay(100);
  //runner.execute();
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
  if(SD.begin(CARDCS))
  {
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
          "/Ringtones/track001.mp3",  // <- source
          sizeof(config.RingSound));          // <- destination's capacity
  config.Volume = 1;
  }
  else
  {
  // Copy values from the JsonObject to the Config
  strlcpy(config.RingSound,                   // <- destination
          root["RingSound"] | "/Ringtones/track001.mp3",  // <- source
          sizeof(config.RingSound));          // <- destination's capacity
  config.Volume = root["Volume"];
  }
  // Close the file (File's destructor doesn't close the file)
  file.close();
  Serial.println("Config reading done.");
  SD.end();
  if (!root.success())
    saveConfiguration(filename, config);
  }
  else
  {
    Serial.println("SD not present, using default config!");
    strlcpy(config.RingSound,                   // <- destination
          "/track001.mp3",  // <- source
          sizeof(config.RingSound));          // <- destination's capacity
    config.Volume = 90;
  }

}
void saveConfiguration(const char *filename, const Config &config) {
  if(SD.begin(CARDCS))
  {
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
  SD.end();
  }
}

void checkBell(){
  //core functionality is to play music, so let´s do this first!
    if (digitalRead(21) < 1)
    {
      Ada_musicPlayer.setVolume(config.Volume, config.Volume);
      Serial.println("Ring Ring");
      if(SD.begin(CARDCS))
      {
        //Play the MP3 file. In case this does not work, fall back to just playing a sound.
        bool playSuccessfull = Ada_musicPlayer.playFullFile(config.RingSound); 
        if (!playSuccessfull)
        {
          Serial.println("Playing the file was not successful. Playing a sine tone now.");
          Ada_musicPlayer.sineTest(0x44, 2000);
          Ada_musicPlayer.stopPlaying();
        }
        SD.end();
      }
      else
      {
        Serial.println("No SD Card, playing a sine test sound.");
        Ada_musicPlayer.sineTest(0x44, 2000);
        Ada_musicPlayer.stopPlaying();
        //Ada_musicPlayer.playData(sampleMp3, sizeof(sampleMp3));
      }
      generic_MQTTSend("Ring Ring","/DoorBell/Ring");
      
      Serial.println("Message sent");
    }
}

void checkBell_Task(void * pvParameters){
  Serial.println("Starting Check Bell Task");
  while(true){
    checkBell();
    delay(100);
  }
}

void generic_MQTT_message_control(char* payload){
    Serial.print("Got the Message:");
    Serial.println(payload);
    StaticJsonBuffer<MQTT_MAX_PACKET_SIZE> jsonBuffer;
    JsonObject &control_message = jsonBuffer.parseObject(payload);
    Serial.print("Got the following command: ");
    Serial.println(control_message["command"].asString());
    Serial.print("Got the following payload: ");
    Serial.println(control_message["payload"].asString());
    if (control_message["command"].as<String>().equalsIgnoreCase("play")){
      Serial.print("Playing: ");
      Serial.println((const char*)control_message["payload"]);
      String answer = "Playing the file: ";
      answer.concat((const char*)control_message["payload"]);
      generic_MQTTSend(answer,answerTopic);
      if(SD.begin(CARDCS)){
        Ada_musicPlayer.setVolume(config.Volume,config.Volume);
        Ada_musicPlayer.playFullFile((const char*)control_message["payload"]);
        SD.end();
      }
    }
    else if (control_message["command"].as<String>().equalsIgnoreCase("selectRingFile")){
      strlcpy(config.RingSound,                   // <- destination
          control_message["payload"],  // <- source
          strlen(control_message["payload"]) + 1); 
      saveConfiguration(config_filename,config);
      String answer = "The ringtone got changes to: ";
      answer.concat((const char*)control_message["payload"]);
      generic_MQTTSend(answer,answerTopic);
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
      String directoryList = "SD Content:\n";
      if (SD.begin(CARDCS)){
        directoryList.concat(printDirectory(SD.open((const char*)control_message["payload"]),0));
        Serial.print("Length of directory list: ");
        Serial.println(directoryList.length());
        //char buf[directoryList.length() +1];
        //directoryList.toCharArray(buf, directoryList.length() +1);
        //int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
        generic_MQTTSend(directoryList,answerTopic);
        Serial.println(directoryList);
        SD.end();
      }
      else{
        generic_MQTTSend("SD Card could not be opened",answerTopic);
      }
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
      //char buf[configuration.length() +1];
      //configuration.toCharArray(buf, configuration.length()+1);
      //int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
      generic_MQTTSend(configuration,answerTopic);
      Serial.println(configuration);
    }
    else{
      Serial.print("Got some other command:");
      Serial.println(control_message["command"].as<String>());
    }
  }

void generic_MQTTSend(String msg, char* topic){
      #ifdef MQTT_PAHO
        paho_MQTTSend(msg, topic);
      #endif
      #ifdef MQTT_PUBSUB
        pubsub_MQTTSend(msg, topic);
      #endif
  }

#ifdef MQTT_PUBSUB
  void pubsub_callback(char* topic, byte* payload, unsigned int length){
    char p[length + 1];
    for(int i = 0; i < length; i++){
      p[i] = (char)payload[i];
    }
    p[length] = '\0'; 
    generic_MQTT_message_control(p);
  }

  void pubsub_connect(){
    // Loop until we're reconnected
    while (!pubsub_client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "DoorBellMQTTClient";
      // Attempt to connect
      if (pubsub_client.connect(clientId.c_str())) {
        Serial.println("connected");
        // Once connected, subscribe
        pubsub_client.subscribe("/DoorBell/Control");
      } else {
        Serial.print("failed, rc=");
        Serial.print(pubsub_client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    } 
  }

  void pubsub_MQTT_update(){
    if (!pubsub_client.connected()) {
      pubsub_connect();
    }
    pubsub_client.loop();
  }

  void pubsub_MQTTSend(String msg, char* topic){
    pubsub_client.publish(topic, msg.c_str());
  }
#endif

#ifdef MQTT_PAHO
  void paho_checkMQTT(){
    //Serial.println("Checking MQTT");
    if (!MQTTclient.isConnected())
    {//ipstack.disconnect();
      Serial.println("MQTT disconnected");
      paho_MQTT_connect();
    }
    //Ensure the MQTT Messages get handled properly.
    /*if (MQTTclient.yield(500) != 0)
    {
      paho_MQTT_connect();
    }*/
    MQTTclient.yield(500);
  }

  void paho_checkMQTT_Task(void * pvParameters){
    Serial.println("Starting MQTT Task");
    while(true){
    paho_checkMQTT();
    }
  }

  void paho_MQTT_message_control(MQTT::MessageData &md){
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
      Serial.print("Playing: ");
      Serial.println((const char*)control_message["payload"]);
      String answer = "Playing the file: ";
      answer.concat((const char*)control_message["payload"]);
      paho_MQTTSend(answer,answerTopic);
      if(SD.begin(CARDCS)){
        Ada_musicPlayer.setVolume(config.Volume,config.Volume);
        Ada_musicPlayer.playFullFile((const char*)control_message["payload"]);
        SD.end();
      }
    }
    else if (control_message["command"].as<String>().equalsIgnoreCase("selectRingFile")){
      strlcpy(config.RingSound,                   // <- destination
          control_message["payload"],  // <- source
          strlen(control_message["payload"]) + 1); 
      saveConfiguration(config_filename,config);
      String answer = "The ringtone got changes to: ";
      answer.concat((const char*)control_message["payload"]);
      paho_MQTTSend(answer,answerTopic);
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
      String directoryList = "SD Content:\n";
      if (SD.begin(CARDCS)){
        directoryList.concat(printDirectory(SD.open((const char*)control_message["payload"]),0));
        Serial.print("Length of directory list: ");
        Serial.println(directoryList.length());
        //char buf[directoryList.length() +1];
        //directoryList.toCharArray(buf, directoryList.length() +1);
        //int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
        paho_MQTTSend(directoryList,answerTopic);
        Serial.println(directoryList);
        SD.end();
      }
      else{
        paho_MQTTSend("SD Card could not be opened",answerTopic);
      }
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
      //char buf[configuration.length() +1];
      //configuration.toCharArray(buf, configuration.length()+1);
      //int rc = MQTTclient.publish("/DoorBell/Answers", (void *)buf, strlen(buf) + 1);
      paho_MQTTSend(configuration,answerTopic);
      Serial.println(configuration);
    }
    else{
      Serial.print("Got some other command:");
      Serial.println(control_message["command"].as<String>());
    }
  }
  void paho_MQTT_connect(){

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
      data.keepAliveInterval = 200;
      rc = MQTTclient.connect(data);
      if (rc != 0)
      {
        Serial.print("rc from MQTT connect is ");
        Serial.println(rc);
      }
      Serial.println("MQTT connected");
      rc = MQTTclient.subscribe((const char *)"/DoorBell/Control", MQTT::QOS0, paho_MQTT_message_control);
      if (rc != 0)
      {
        Serial.print("rc from MQTT subscribe is ");
        Serial.println(rc);
      }
      Serial.println("MQTT subscribed");
    }
  }

  void paho_MQTTSend(String msg, char* topic){
      char buf[msg.length() +1];
      msg.toCharArray(buf, msg.length() +1);
      int rc = MQTTclient.publish(topic, (void *)buf, strlen(buf) + 1);
  }
#endif