#pragma once

#include <ESP32Encoder.h>

#include "TouchSensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

struct InputSnapshot
{
    bool floppy = false;
    bool alarm = false;
    bool clock = false;
    bool touch = false;
};

class InputService
{
public:
    InputService();

    void begin();
    bool startTask();
    void suspendTask();
    void resumeTask();

    InputSnapshot read();
    int encoderPosition() const;
    void setEncoderPosition(int value);
    bool discreteTouchPressed() const;
    bool readEmulatorMouseButton();
    ESP32Encoder &emulatorEncoder();

private:
    static void taskEntry(void *context);
    void runTask();

    TouchSensor touch_;
    ESP32Encoder encoder_;
    InputSnapshot state_;
    portMUX_TYPE state_mux_ = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t task_handle_ = nullptr;
};

void bind_emulator_input(InputService &input);
ESP32Encoder &emulator_encoder();
bool emulator_mouse_button_read();
