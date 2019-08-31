#ifndef DOORBELL_MESSAGE_HPP
#define DOORBELL_MESSAGE_HPP

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include "Arduino.h"

#include "globals.hpp"
#include "Sound.hpp"
#include "Configuration.hpp"

namespace Message {

    void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        Serial.print("Got the Message:");
        Serial.println(payload);
        DynamicJsonBuffer jsonBuffer;

        JsonObject &control_message = jsonBuffer.parseObject(payload);
        String command = control_message["command"].as<String>();
        const char* messagePayload = control_message["payload"];
        
        Serial.print(F("Got the following command: "));
        Serial.println(command);
        Serial.print(F("Got the following payload: "));
        Serial.println(control_message["payload"].as<String>());

        if (command.equalsIgnoreCase("play")) {
            Serial.print("Playing: ");
            Serial.println((const char *) control_message["payload"]);
            String answer = "Playing the file: ";
            answer.concat((const char *) control_message["payload"]);
            Network::send(answerTopic, answer);

            Sound::play((const char *) control_message["payload"]);

        } else if (command.equalsIgnoreCase("selectRingFile")) {
            strlcpy(Configuration::RingSound,                   // <- destination
                    control_message["payload"],  // <- source
                    strlen(control_message["payload"]) + 1);
            Configuration::save();
            String answer = "The ringtone got changed to: ";
            answer.concat((const char *) control_message["payload"]);
            Network::send(answerTopic, answer);
        } else if (command.equalsIgnoreCase("setVolume")) {
            Serial.print("Setting Volume to: ");
            Serial.println((int) control_message["payload"]);
            Configuration::Volume = (int) control_message["payload"];
            Configuration::save();
        } else if (command.equalsIgnoreCase("listSD")) {
            Serial.println("Listing SD...");
            String directoryList = "SD Content:\n";
            if (SD.begin(CARDCS)) {
                directoryList.concat(Sound::list((const char *) control_message["payload"]));
                Serial.print("Length of directory list: ");
                Serial.println(directoryList.length());
                Network::send(answerTopic, directoryList);
                Serial.println(directoryList);
                SD.end();
            } else {
                Network::send(answerTopic, "SD Card could not be opened");
            }
        } else if (command.equalsIgnoreCase("listConfig")) {
            Serial.println("listing config");
            String configuration = "";
            configuration.concat("Ringtone: ");
            configuration.concat(Configuration::RingSound);
            configuration.concat("\n");
            configuration.concat("Volume: ");
            configuration.concat(Configuration::Volume);
            configuration.concat("\n");
            Network::send(answerTopic, configuration);
            Serial.println(configuration);
        } else {
            Serial.print("Got some other command:");
            Serial.println(command);
        }
        jsonBuffer.clear();
        
    };

}

#endif //DOORBELL_MESSAGE_HPP
