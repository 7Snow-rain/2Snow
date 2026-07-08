/**
 * speaker.h — 蜂鸣器控制类
 * 
 * 功能：
 *   - speakStart(): 开机音效
 *   - speakNotify(): 来消息时响铃提醒
 *   - beep(): 短促的响声
 *   - speak(freq, duration): 指定频率和时长
 *   - mute()/unmute(): 静音/取消静音
 */

#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>

class Speaker {
public:
    Speaker(uint8_t pin);

    void begin();

    void speak(uint16_t freq, uint16_t duration);
    void beep(uint16_t duration = 100);

    void speakStart();
    void speakNotify();

    void mute();
    void unmute();
    bool isMuted() const { return _muted; }

private:
    uint8_t _pin;
    bool _muted;
};

#endif
