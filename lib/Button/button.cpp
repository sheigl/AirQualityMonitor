#include "button.h"

Button::Button(uint8_t pin)
{
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
}

void Button::OnPress(void(*callback)())
{
    _onPressCallback = callback;
}

void Button::OnLongPress(void(*callback)())
{
    _onLongPressCallback = callback;
}

long _lastNotifyTime;

void Button::Update()
{
    unsigned long now = millis();
    unsigned long timeDelta = now - _lastPressTime;

    /* if ((now - _lastNotifyTime) > 5000)
    {
        _lastNotifyTime = now;
        DEBUG_MSG("Last");
        DEBUG_MSG(_lastState);
        DEBUG_MSG("Current");
        DEBUG_MSG(_currentState);
    } */
    

    if (_lastState == BTN_NOT_PRESSED && _currentState == BTN_NOT_PRESSED) {
        if (isPressed() && timeDelta > DEBOUNCE) {
            _lastState = _currentState;
            _currentState = BTN_PRESSED;
            _lastPressTime = now;
            
            //DEBUG_MSG("press start");
        }
    }
    // BTN still pressed
    else if (_lastState == BTN_NOT_PRESSED && _currentState == BTN_PRESSED) {
        if (isPressed())
        {
            if (timeDelta > 2000 && !_didLongPress)
            {
                //DEBUG_MSG("long press");
                
                if (_onLongPressCallback != nullptr) {
                    (*_onLongPressCallback) ();
                }

                _didLongPress = true;
            }            
            
        }
        else {
            if (timeDelta > DEBOUNCE && !_didShortPress && !_didLongPress) {
                //DEBUG_MSG("short press");

                if (_onPressCallback != nullptr) {
                    (*_onPressCallback) ();
                }

                _didShortPress = true;
            }

            _lastState = _currentState;
            _currentState = BTN_NOT_PRESSED;
        }        
    }
    else if (_lastState == BTN_PRESSED && _currentState == BTN_NOT_PRESSED) {
        if (isPressed())
        {
            if (timeDelta > 250 && !_didDoublePress) {
                //DEBUG_MSG("double press");
                _didDoublePress = true;
            }
            
        }
        else {
            if (timeDelta > 500) {
                _lastState = _currentState;
                _currentState = BTN_NOT_PRESSED;
                _didDoublePress = false;
                _didLongPress = false;
                _didShortPress = false;

                //DEBUG_MSG("released");
            }
        }  
    }
}

bool Button::isPressed()
{
    return digitalRead(_pin) == BTN_PRESSED;
}