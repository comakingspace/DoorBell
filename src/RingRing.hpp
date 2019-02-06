#ifndef DOORBELL_RINGRING_HPP
#define DOORBELL_RINGRING_HPP

#include <Arduino.h>
#include "globals.hpp"
#include "Sound.hpp"


namespace RingRing {
//    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    void checkBell() {
        //core functionality is to play music, so let´s do this first!
        if (digitalRead(Ring_Pin) != LOW) {
            return;
        }
        sound.playRingtone();
    }

    void checkBell_Task(void *pvParameters) {
        Serial.println("Starting Check Bell Task");
        while (true) {
            checkBell();
            delay(100);
        }
    }

    void setup() {
//        attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
        pinMode(Ring_Pin, INPUT_PULLUP);
        xTaskCreatePinnedToCore(checkBell_Task, "checkBell", 10000, NULL, 0, NULL, 0);
    }

}

#endif //DOORBELL_RINGRING_HPP
