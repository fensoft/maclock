#include "input_service.h"

#include <Arduino.h>

namespace
{
InputService *g_emulator_input = nullptr;
}

InputService::InputService() : touch_(GPIO_TOUCH)
{
}

void InputService::begin()
{
    touch_.begin();
    pinMode(GPIO_FLOPPY, INPUT);
    pinMode(GPIO_ALARM, INPUT);
    pinMode(GPIO_CLOCK, INPUT);
    pinMode(GPIO_ENCODER1, INPUT_PULLUP);
    pinMode(GPIO_ENCODER2, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder_.attachHalfQuad(GPIO_ENCODER1, GPIO_ENCODER2);
    bind_emulator_input(*this);
}

bool InputService::startTask()
{
    if (task_handle_)
        return true;
    return xTaskCreatePinnedToCore(
               taskEntry,
               "input_task",
               2048,
               this,
               1,
               &task_handle_,
               1) == pdPASS;
}

void InputService::suspendTask()
{
    if (task_handle_)
        vTaskSuspend(task_handle_);
}

void InputService::resumeTask()
{
    if (task_handle_)
        vTaskResume(task_handle_);
}

InputSnapshot InputService::read()
{
    portENTER_CRITICAL(&state_mux_);
    const InputSnapshot snapshot = state_;
    state_.alarm = false;
    state_.clock = false;
    state_.touch = false;
    portEXIT_CRITICAL(&state_mux_);
    return snapshot;
}

int InputService::encoderPosition() const
{
    return static_cast<int>(
        const_cast<ESP32Encoder &>(encoder_).getCount());
}

void InputService::setEncoderPosition(int value)
{
    encoder_.setCount(value);
}

bool InputService::discreteTouchPressed() const
{
    return const_cast<TouchSensor &>(touch_).touched();
}

bool InputService::readEmulatorMouseButton()
{
    return touch_.update();
}

ESP32Encoder &InputService::emulatorEncoder()
{
    return encoder_;
}

void InputService::taskEntry(void *context)
{
    static_cast<InputService *>(context)->runTask();
}

void InputService::runTask()
{
    bool previous_alarm = false;
    bool previous_clock = false;
    bool previous_touch = false;
    for (;;)
    {
        const bool floppy = digitalRead(GPIO_FLOPPY) == LOW;
        const bool alarm = digitalRead(GPIO_ALARM) == LOW;
        const bool clock = digitalRead(GPIO_CLOCK) == LOW;
        const bool touched = touch_.update();

        portENTER_CRITICAL(&state_mux_);
        state_.floppy = floppy;
        if (alarm && !previous_alarm)
            state_.alarm = true;
        if (clock && !previous_clock)
            state_.clock = true;
        if (touched && !previous_touch)
            state_.touch = true;
        portEXIT_CRITICAL(&state_mux_);

        previous_alarm = alarm;
        previous_clock = clock;
        previous_touch = touched;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void bind_emulator_input(InputService &input)
{
    g_emulator_input = &input;
}

ESP32Encoder &emulator_encoder()
{
    return g_emulator_input->emulatorEncoder();
}

bool emulator_mouse_button_read()
{
    return g_emulator_input &&
           g_emulator_input->readEmulatorMouseButton();
}
