#ifndef BTN
#define BTN

#ifndef ARD
#define ARD
#include <Arduino.h>
#endif

#define BTN_PRESSED 0
#define BTN_NOT_PRESSED 1
#define DEBOUNCE 100

#ifndef DEBUG
    #define DEBUG Serial.println
#endif

#ifdef DEBUG
    #define DEBUG_MSG(msg) { DEBUG(msg); }
#else
    #define DEBUG_MSG(msg)
#endif

class Button
{
private:
    unsigned long _lastPressTime;
    int _lastState = BTN_NOT_PRESSED;
    int _currentState = BTN_NOT_PRESSED;
    bool _didShortPress;
    bool _didLongPress;
    bool _didDoublePress;
    uint8_t _pin;
    void(*_onPressCallback)() = nullptr;
    void(*_onLongPressCallback)() = nullptr;

    bool isPressed();
public:    
    Button(uint8_t pin);
    void OnPress(void(*callback)());
    void OnLongPress(void(*callback)());
    void Update();
};

#endif