#ifndef DOORBELL_SOUND_HPP
#define DOORBELL_SOUND_HPP

#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include "globals.hpp"
#include "Network.hpp"


class Sound {
    Adafruit_VS1053_FilePlayer Ada_musicPlayer =
            Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

    Configuration &config;

    void setup() {
        while (!Ada_musicPlayer.begin()) {

        }

        if (!SD.begin(CARDCS)) {
            //Check the SD Card to be present. If not, we need to play a hardcoded file.
            Serial.println(F("SD failed, or not present"));
        }

        Ada_musicPlayer.setVolume(config.Volume, config.Volume);
        Ada_musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

        SD.end();
    }

public:

    Sound(Configuration &config) : config{config} {
        setup();
    }

    String list(File directory, int numTabs) {
        String result = "";
        while (true) {

            File entry = directory.openNextFile();
            if (!entry) {
                // no more files
                //Serial.println("**nomorefiles**");
                break;
            }
            for (uint8_t i = 0; i < numTabs; i++) {
                result += '\t';
            }
            result += entry.name();
            if (entry.isDirectory()) {
                result += "/";
                result += "\n";
                result += list(entry, numTabs + 1);
            } else {
                // files have sizes, directories do not
                result += "\t\t";
                result += entry.size();
                result += "\n";
            }
            entry.close();
        }
        return result;
    }

    String list(const char *dirPath, int numTabs = 0) {
        auto directory = SD.open(dirPath);
        return list(directory, numTabs);
    }

    String list() {
        auto directory = SD.open(F("/"));
        return list(directory, 0);
    }

    void updateVolume(uint8_t volume) {
        // Set volume for left, right channels. lower numbers == louder volume!
        Ada_musicPlayer.setVolume(volume, volume);
    }

    boolean play() {
        return play(config.RingSound);
    }

    boolean play(const char *filename) {
        return play(String{filename});
    }

    boolean play(const String &filename) {
        updateVolume(config.Volume);
        Serial.println("Ring Ring");
        Network::send("/DoorBell/Ring", "Ring");

        if (SD.begin(CARDCS)) {
            //Play the MP3 file. In case this does not work, fall back to just playing a sound.
            bool playSuccessful = Ada_musicPlayer.playFullFile(config.RingSound);
            SD.end();
            return playSuccessful;
        }
        return false;
    }

    void playRingtone() {
        if(!play(config.RingSound)) {
            Serial.println("No SD Card, playing a sine test sound.");
            Ada_musicPlayer.sineTest(0x44, 2000);
            Ada_musicPlayer.stopPlaying();
        }
    }
};

#endif //DOORBELL_SOUND_HPP
