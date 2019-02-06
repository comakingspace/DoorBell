//General Includes
#include "globals.hpp"
#include "instances.hpp"
#include "Network.hpp"
#include "Message.hpp"
#include "RingRing.hpp"

void setup() {
    // General DoorBell setup: Input Pin and VS1053
    Serial.begin(9600);
    Serial.println("\n\n CoMakingSpace Door Bell");

    AsyncMqttClientInternals::OnMessageUserCallback messageHandler = Message::onMessage;
    Network::setup(messageHandler);
    RingRing::setup();
}

void loop() {
    Network::loop();
}
