#include <Arduino.h>

#include "maclock_app.h"

static MaclockApp app;

void setup()
{
    app.begin();
}

void loop()
{
    app.tick();
}
