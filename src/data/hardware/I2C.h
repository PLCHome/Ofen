#ifndef I2C_H_
#define I2C_H_

#include <Arduino.h>
#include <Wire.h>
#include "Hardware.h"

class I2C  : public Hardware
{
private:
    int sda;
    int sdl;
    TwoWire *I2Cone;

public:
    I2C();
    void init() override;
    bool isActive() const override;
    int getSDA() const;
    int getSDL() const;
    TwoWire *getTwoWire() const;
};

#endif /* I2C_H_ */