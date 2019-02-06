#ifndef DOORBELL_NETWORK_HPP
#define DOORBELL_NETWORK_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoOTA.h>

#include "Passwords.h"


namespace Network {

    AsyncMqttClient mqttClient;
    TimerHandle_t mqttReconnectTimer;
    TimerHandle_t wifiReconnectTimer;

    void setupOTA() {
        ArduinoOTA.setHostname("DoorBell");

        ArduinoOTA.setPassword(OTA_PASSWORD);

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

    void connectToWifi() {
        Serial.println("Connecting to Wi-Fi...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    void connectToMqtt() {
        Serial.println("Connecting to MQTT...");
        mqttClient.connect();
    }

    void onMqttConnect(bool sessionPresent) {
        Serial.println("Connected to MQTT.");
        Serial.print("Session present: ");
        Serial.println(sessionPresent);
    }

    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
        Serial.println("Disconnected from MQTT.");

        if (WiFi.isConnected()) {
            xTimerStart(mqttReconnectTimer, 0);
        }
    }


    void WiFiEvent(WiFiEvent_t event) {
        Serial.printf("[WiFi-event] event: %d\n", event);
        switch (event) {
            case SYSTEM_EVENT_STA_GOT_IP:
                Serial.println("WiFi connected");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                connectToMqtt();
                setupOTA();
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
                Serial.println("WiFi lost connection");
                xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
                xTimerStart(wifiReconnectTimer, 0);
                break;
            default:
                break;
        }
    }

    void setup(AsyncMqttClientInternals::OnMessageUserCallback &onMqttMessage) {

        mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *) 0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
        wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *) 0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

        WiFi.onEvent(WiFiEvent);

        mqttClient.setClientId("DoorBellMQTTClient");
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.setServer(HOSTNAME, MQTT_PORT);
        connectToWifi();
    }

    void loop() {
        ArduinoOTA.handle();
    }

    void send(const char *topic, const char *message) {
        mqttClient.publish(topic, 0, false, message);
    }

    void send(String &topic, String &message) {
        send(topic.c_str(), message.c_str());
    }

    void send(String &topic, char *message) {
        send(topic.c_str(), message);
    }

    void send(const char *topic, String &message) {
        send(topic, message.c_str());
    }

}

#endif //DOORBELL_NETWORK_HPP
