#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__has_include)
#if __has_include(<esp_err.h>)
#include <esp_err.h>
#endif
#endif

#ifndef ESP_OK
typedef int esp_err_t;
#endif

// Minimal stub for host builds without the EspUsbHost library.
// Provides only the symbols used by this project with no real behavior.

#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_ERR_INVALID_ARG
#define ESP_ERR_INVALID_ARG -2
#endif

typedef void *usb_device_handle_t;
typedef void *usb_host_client_handle_t;

struct usb_transfer_t;
typedef void (*usb_transfer_cb_t)(usb_transfer_t *transfer);

struct usb_transfer_t {
    uint8_t *data_buffer = nullptr;
    size_t num_bytes = 0;
    size_t actual_num_bytes = 0;
    uint8_t bEndpointAddress = 0;
    usb_device_handle_t device_handle = nullptr;
    usb_transfer_cb_t callback = nullptr;
    void *context = nullptr;
};

struct endpoint_data_t {
    uint8_t bInterfaceClass = 0;
    uint8_t bInterfaceSubClass = 0;
    uint8_t bInterfaceProtocol = 0;
};

struct usb_host_client_event_msg_t {
    int dummy = 0;
};

struct hid_keyboard_report_t {
    uint8_t modifier = 0;
    uint8_t reserved = 0;
    uint8_t keycode[6] = {};
};

struct hid_mouse_report_t {
    uint8_t buttons = 0;
    int8_t x = 0;
    int8_t y = 0;
    int8_t wheel = 0;
};

static inline esp_err_t usb_host_transfer_alloc(int num_bytes, int, usb_transfer_t **out_transfer)
{
    if (out_transfer == nullptr || num_bytes <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    usb_transfer_t *transfer = new usb_transfer_t();
    transfer->num_bytes = static_cast<size_t>(num_bytes);
    transfer->data_buffer = new uint8_t[transfer->num_bytes];
    std::memset(transfer->data_buffer, 0, transfer->num_bytes);
    *out_transfer = transfer;
    return ESP_OK;
}

static inline esp_err_t usb_host_transfer_submit_control(usb_host_client_handle_t, usb_transfer_t *transfer)
{
    if (transfer == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static inline void usb_host_transfer_free(usb_transfer_t *transfer)
{
    if (transfer == nullptr) {
        return;
    }
    delete[] transfer->data_buffer;
    delete transfer;
}

class EspUsbHost {
public:
    virtual ~EspUsbHost() = default;

    virtual void begin() { isReady = false; }
    virtual void task() {}

    virtual void onKeyboard(hid_keyboard_report_t, hid_keyboard_report_t) {}
    virtual void onMouseMove(hid_mouse_report_t) {}
    virtual void onMouseButtons(hid_mouse_report_t, uint8_t) {}
    virtual void onReceive(const usb_transfer_t *) {}
    virtual void onGone(const usb_host_client_event_msg_t *) {}

protected:
    bool isReady = false;
    usb_device_handle_t deviceHandle = nullptr;
    usb_host_client_handle_t clientHandle = nullptr;
    endpoint_data_t endpoint_data_list[16]{};
};
