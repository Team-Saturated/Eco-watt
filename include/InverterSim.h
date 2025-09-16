#pragma once

#include <Arduino.h>

class InverterSim {
public:
    InverterSim(int pwmPin);
    void begin();
    void setVoltage(float voltage);
    float getVoltage();

private:
    int _pwmPin;
    float _voltage;
};
