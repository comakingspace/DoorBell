#ifndef DOORBELL_MESSAGE_HPP
#define DOORBELL_MESSAGE_HPP

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include "globals.hpp"
#include "instances.hpp"

namespace Message {

    void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        Serial.print("Got the Message:");
        Serial.println(payload);
        DynamicJsonBuffer jsonBuffer;

        JsonObject &control_message = jsonBuffer.parseObject(payload);
        Serial.print("Got the following command: ");
        Serial.println(control_message["command"].as<String>());
        Serial.print("Got the following payload: ");
        Serial.println(control_message["payload"].as<String>());

        if (control_message["command"].as<String>().equalsIgnoreCase("play")) {
            Serial.print("Playing: ");
            Serial.println((const char *) control_message["payload"]);
            String answer = "Playing the file: ";
            answer.concat((const char *) control_message["payload"]);
            Network::send(answerTopic, answer);

            sound.play((const char *) control_message["payload"]);

        } else if (control_message["command"].as<String>().equalsIgnoreCase("selectRingFile")) {
            strlcpy(config.RingSound,                   // <- destination
                    control_message["payload"],  // <- source
                    strlen(control_message["payload"]) + 1);
            config.save();
            String answer = "The ringtone got changes to: ";
            answer.concat((const char *) control_message["payload"]);
            Network::send(answerTopic, answer);
        } else if (control_message["command"].as<String>().equalsIgnoreCase("setVolume")) {
            Serial.print("Setting Volume to: ");
            Serial.println((int) control_message["payload"]);
            config.Volume = (int) control_message["payload"];
            config.save();
        } else if (control_message["command"].as<String>().equalsIgnoreCase("listSD")) {
            Serial.println("Listing SD...");
            String directoryList = "SD Content:\n";
            if (SD.begin(CARDCS)) {
                directoryList.concat(sound.list((const char *) control_message["payload"]));
                Serial.print("Length of directory list: ");
                Serial.println(directoryList.length());
                Network::send(answerTopic, directoryList);
                Serial.println(directoryList);
                SD.end();
            } else {
                Network::send(answerTopic, "SD Card could not be opened");
            }
        } else if (control_message["command"].as<String>().equalsIgnoreCase("listConfig")) {
            Serial.println("listing config");
            String configuration = "";
            configuration.concat("Ringtone: ");
            configuration.concat(config.RingSound);
            configuration.concat("\n");
            configuration.concat("Volume: ");
            configuration.concat(config.Volume);
            configuration.concat("\n");
            Network::send(answerTopic, configuration);
            Serial.println(configuration);
        } else {
            Serial.print("Got some other command:");
            Serial.println(control_message["command"].as<String>());
        }

    };

}

#endif //DOORBELL_MESSAGE_HPP
