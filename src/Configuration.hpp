#ifndef DOORBELL_CONFIGURATION_HPP
#define DOORBELL_CONFIGURATION_HPP

#include <SD.h>
#include <ArduinoJson.h>

const char *config_filename = "/config.txt";

class Configuration {

public:
    char RingSound[100];
    int Volume;

    Configuration() {
        load();
    };

    void load() {
        // Open file for reading
        if (SD.begin(CARDCS)) {
            File file = SD.open(config_filename);

            // Allocate the memory pool on the stack.
            // Don't forget to change the capacity to match your JSON document.
            // Use arduinojson.org/assistant to compute the capacity.
            StaticJsonBuffer<1024> jsonBuffer;

            // Parse the root object
            JsonObject &root = jsonBuffer.parseObject(file);

            if (!root.success()) {
                Serial.println(F("Failed to read file, using default configuration"));
                strlcpy(RingSound,                   // <- destination
                        "/Ringtones/track001.mp3",  // <- source
                        sizeof(RingSound));          // <- destination's capacity
                Volume = 1;
            } else {
                // Copy values from the JsonObject to the Config
                strlcpy(RingSound,                   // <- destination
                        root["RingSound"] | "/Ringtones/track001.mp3",  // <- source
                        sizeof(RingSound));          // <- destination's capacity
                Volume = root["Volume"];
            }
            // Close the file (File's destructor doesn't close the file)
            file.close();
            Serial.println("Config reading done.");
            SD.end();
            if (!root.success())
                save();
        } else {
            Serial.println("SD not present, using default config!");
            strlcpy(RingSound,                   // <- destination
                    "/track001.mp3",  // <- source
                    sizeof(RingSound));          // <- destination's capacity
            Volume = 90;
        }
    }

    void save() {
        if (SD.begin(CARDCS)) {
            // Delete existing file, otherwise the configuration is appended to the file
            SD.remove(config_filename);

            // Open file for writing
            File file = SD.open(config_filename, FILE_WRITE);
            if (!file) {
                Serial.println(F("Failed to create file"));
                return;
            }

            // Allocate the memory pool on the stack
            // Don't forget to change the capacity to match your JSON document.
            // Use https://arduinojson.org/assistant/ to compute the capacity.
            StaticJsonBuffer<1024> jsonBuffer;

            // Parse the root object
            JsonObject &root = jsonBuffer.createObject();

            // Set the values
            root["RingSound"] = RingSound;
            root["Volume"] = Volume;
            // Serialize JSON to file
            if (root.printTo(file) == 0) {
                Serial.println(F("Failed to write to file"));
            }

            // Close the file (File's destructor doesn't close the file)
            file.close();
            SD.end();
        }
    }

};

#endif //DOORBELL_CONFIGURATION_HPP
