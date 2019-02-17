//General Includes
#include "Arduino.h"
#include "globals.hpp"
#include "Configuration.hpp"
#include "Sound.hpp"
#include "Network.hpp"
#include "Message.hpp"
#include "RingRing.hpp"

void setup() {
    // General DoorBell setup: Input Pin and VS1053
    Serial.begin(115200);
    Serial.println("\n\n CoMakingSpace Door Bell");

    Configuration::setup();
    Sound::setup();
    AsyncMqttClientInternals::OnMessageUserCallback messageHandler = Message::onMessage;
    Network::setup(messageHandler);
    RingRing::setup();
}

void loop() {
//    Serial.println("Testmainloop");
    Network::loop();
}
