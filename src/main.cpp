#include <Arduino.h>

#include "esp32_maclock_hal.h"
#include "maclock_app.h"

static Esp32MaclockHal hal;
static MaclockApp app(hal);

void setup()
{
    app.begin();
}

void loop()
{
    app.tick();
}
