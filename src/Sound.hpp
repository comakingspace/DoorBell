#ifndef DOORBELL_SOUND_HPP
#define DOORBELL_SOUND_HPP

#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include "globals.hpp"
#include "Network.hpp"
#include "Configuration.hpp"

struct debugAudioPlayer {
    bool begin() { return true; }
    void setVolume(uint8_t left, uint8_t right) {}
    bool playFullFile(char *str) { return true; }
    void sineTest(uint8_t n, uint16_t ms) {}
    void stopPlaying() {}
    void useInterrupt(uint8_t type) {}
};

namespace Sound {
#ifndef DEBUG_DOOR_BELL
    Adafruit_VS1053_FilePlayer audioPlayer =
            Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);
#else
    debugAudioPlayer audioPlayer;
#endif

    void setup() {
        while (!audioPlayer.begin()) {

        }

        if (SD.begin(CARDCS)) {
            //Check the SD Card to be present. If not, we need to play a hardcoded file.
            audioPlayer.setVolume(Configuration::Volume, Configuration::Volume);
            audioPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
            SD.end();
        } else {
            Serial.println(F("SD failed, or not present"));
        }
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
        audioPlayer.setVolume(volume, volume);
    }

    boolean play(const char* filename) {
        updateVolume(Configuration::Volume);
        

        if (SD.begin(CARDCS)) {
            //Play the MP3 file. In case this does not work, fall back to just playing a sound.
            bool playSuccessful = audioPlayer.playFullFile(filename);
            SD.end();
            return playSuccessful;
        }
        return false;
    }

    // boolean play() {
    //     return play(Configuration::RingSound);
    //     Serial.println("Ring Ring");
    //     Network::send("/DoorBell/Ring", "Ring Ring");
    // }

    // boolean play(const char *filename) {
    //     return play(filename);
    // }


    void playRingtone() {
        Serial.println("Ring Ring");
        Network::send("/DoorBell/Ring", "Ring Ring");
        if(!play(Configuration::RingSound)) {
            Serial.println("No SD Card, playing a sine test sound.");
            audioPlayer.sineTest(0x44, 2000);
            audioPlayer.stopPlaying();
        }
    }
};

#endif //DOORBELL_SOUND_HPP
