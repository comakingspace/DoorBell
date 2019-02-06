#ifndef DOORBELL_GLOBALS_HPP
#define DOORBELL_GLOBALS_HPP

#include "Passwords.h"

const char *answerTopic = "/DoorBell/Answers";

// Stuff for the VS1053 board
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

#endif //DOORBELL_GLOBALS_HPP
