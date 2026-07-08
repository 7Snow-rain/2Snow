#include "Speaker.h"

#define SPEAKER_CHANNEL 0
#define SPEAKER_RESOLUTION 8

Speaker::Speaker(uint8_t pin)
: _pin(pin), _muted(false) {}

void Speaker::begin() {
    ledcSetup(SPEAKER_CHANNEL, 1000, SPEAKER_RESOLUTION);
    ledcAttachPin(_pin, SPEAKER_CHANNEL);
    ledcWrite(SPEAKER_CHANNEL, 0);
}

void Speaker::speak(uint16_t freq, uint16_t duration) {
    if (_muted) return;

    ledcSetup(SPEAKER_CHANNEL, freq, SPEAKER_RESOLUTION);
    ledcWrite(SPEAKER_CHANNEL, 128);   // 50% 占空比
    delay(duration);
    ledcWrite(SPEAKER_CHANNEL, 0);
}

void Speaker::beep(uint16_t duration) {
    speak(2000, duration);
}

void Speaker::speakStart() {
    speak(1200, 80);
    delay(40);
    speak(1600, 80);
    delay(40);
    speak(2000, 120);
}

void Speaker::speakNotify() {
    speak(1800, 60);
    speak(0, 40);
    speak(1800, 60);
}

void Speaker::mute() {
    _muted = true;
}

void Speaker::unmute() {
    _muted = false;
}
