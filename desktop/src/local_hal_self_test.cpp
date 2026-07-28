#include "local_hal_self_test.h"

#include <Arduino.h>
#include <Adafruit_BMP5xx.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <RTClib.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "maclock_hal.h"
#include "local_maclock_hal.h"

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
constexpr uint8_t kBytes[] = {0x4D, 0x41, 0x43, 0x4B};

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "Local HAL self-test: " << message << "\n";
    return condition;
}

bool i2cPresent(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool testDevices()
{
    if (!require(Wire.begin(I2C_SDA, I2C_SCL), "Wire.begin failed") ||
        !require(i2cPresent(0x18), "ES8311 is missing") ||
        !require(i2cPresent(0x38), "FT6336 is missing") ||
        !require(i2cPresent(0x47), "BMP5xx is missing") ||
        !require(i2cPresent(0x68), "RTC is missing") ||
        !require(!i2cPresent(0x50), "unexpected BMP at 0x50"))
    {
        return false;
    }

    RTC_DS3231 rtc;
    if (!require(rtc.begin(&Wire), "DS3231 did not initialize"))
        return false;
    const DateTime before = rtc.now();
    rtc.adjust(DateTime(before.unixtime() + 60));
    const DateTime adjusted = rtc.now();
    if (!require(
            adjusted.unixtime() >= before.unixtime() + 59,
            "RTC session offset did not apply"))
    {
        return false;
    }
    local_maclock_hal().resetRtc();

    Adafruit_BMP5xx bmp;
    return require(
               bmp.begin(0x47, &Wire) && bmp.performReading(),
               "BMP5xx reading failed") &&
           require(
               std::fabs(bmp.temperature - 21.0f) < 0.1f,
               "BMP5xx temperature mismatch");
}

bool writePersistence()
{
    Preferences preferences;
    if (!require(
            preferences.begin("hal-test", false),
            "Preferences begin failed"))
    {
        return false;
    }
    const bool preferences_ok =
        preferences.putBool("bool", true) == sizeof(bool) &&
        preferences.putUChar("uchar", 42) == sizeof(uint8_t) &&
        preferences.putUShort("ushort", 4242) ==
            sizeof(uint16_t) &&
        preferences.putInt("int", -123456) == sizeof(int32_t) &&
        preferences.putDouble("double", 12.5) ==
            sizeof(double) &&
        preferences.putString("string", "Maclock") == 7 &&
        preferences.putBytes(
            "bytes", kBytes, sizeof(kBytes)) ==
            sizeof(kBytes);
    preferences.end();
    if (!require(preferences_ok, "Preferences write failed"))
        return false;

    if (!require(EEPROM.begin(64), "EEPROM begin failed"))
        return false;
    EEPROM.write(7, 0xA5);
    if (!require(EEPROM.commit(), "EEPROM commit failed"))
        return false;

    if (!require(LittleFS.begin(), "LittleFS mount failed") ||
        !require(
            LittleFS.exists("/startup.mp3"),
            "LittleFS base layer is unavailable"))
    {
        return false;
    }
    fs::File file =
        LittleFS.open("/__local_hal_smoke.bin", "w");
    if (!require(
            file &&
                file.write(kBytes, sizeof(kBytes)) ==
                    sizeof(kBytes),
            "LittleFS overlay write failed"))
    {
        return false;
    }
    file.close();
    const auto source_path =
        std::filesystem::path(
            maclock_hal().storage().dataDirectory()) /
        "__local_hal_smoke.bin";
    const auto startup_source =
        std::filesystem::path(
            maclock_hal().storage().dataDirectory()) /
        "startup.mp3";
    return require(
               !std::filesystem::exists(source_path),
               "LittleFS write modified the repository base") &&
           require(
               LittleFS.remove("/startup.mp3") &&
                   !LittleFS.exists("/startup.mp3") &&
                   std::filesystem::exists(startup_source),
               "LittleFS overlay deletion failed");
}

bool verifyPersistence()
{
    Preferences preferences;
    uint8_t bytes[sizeof(kBytes)]{};
    if (!require(
            preferences.begin("hal-test", true),
            "Preferences reopen failed"))
    {
        return false;
    }
    const bool preferences_ok =
        preferences.getBool("bool", false) &&
        preferences.getUChar("uchar", 0) == 42 &&
        preferences.getUShort("ushort", 0) == 4242 &&
        preferences.getInt("int", 0) == -123456 &&
        std::fabs(
            preferences.getDouble("double", 0) - 12.5) <
            0.001 &&
        std::strcmp(
            preferences.getString("string", "").c_str(),
            "Maclock") == 0 &&
        preferences.getBytes(
            "bytes", bytes, sizeof(bytes)) == sizeof(bytes) &&
        std::memcmp(bytes, kBytes, sizeof(bytes)) == 0;
    preferences.end();
    if (!require(preferences_ok, "Preferences verify failed"))
        return false;

    if (!require(
            EEPROM.begin(64) && EEPROM.read(7) == 0xA5,
            "EEPROM persistence failed"))
    {
        return false;
    }
    if (!require(LittleFS.begin(), "LittleFS remount failed"))
        return false;
    if (!require(
            !LittleFS.exists("/startup.mp3"),
            "LittleFS overlay deletion did not persist"))
    {
        return false;
    }
    fs::File file =
        LittleFS.open("/__local_hal_smoke.bin", "r");
    uint8_t overlay_bytes[sizeof(kBytes)]{};
    return require(
        file &&
            file.read(overlay_bytes, sizeof(overlay_bytes)) ==
                sizeof(overlay_bytes) &&
            std::memcmp(
                overlay_bytes, kBytes,
                sizeof(overlay_bytes)) == 0,
        "LittleFS overlay persistence failed");
}

bool testDisplayAndAudio()
{
    TFT_eSPI display;
    display.init();
    display.fillScreen(TFT_BLACK);
    const uint16_t pixels[] = {
        TFT_WHITE, TFT_RED, TFT_RED, TFT_WHITE};
    display.startWrite();
    display.setAddrWindow(10, 10, 2, 2);
    display.pushColors(pixels, 4);
    display.endWrite();

    constexpr size_t kSilentFrames = 44100 / 4;
    std::vector<int16_t> silence(kSilentFrames * 2);
    auto &audio = maclock_hal().audio();
    audio.setVolume(100);
    audio.setMuted(false);
    const auto audio_started =
        std::chrono::steady_clock::now();
    const bool written =
        audio.write(
            silence.data(),
            kSilentFrames, 2) ==
        kSilentFrames;
    audio.drain();
    const auto audio_elapsed =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::steady_clock::now() -
            audio_started)
            .count();
    audio.setMuted(true);
    return require(
        written &&
            audio_elapsed >= 50 &&
            audio_elapsed < 1000,
        "I2S/audio real-time drain failed");
}

bool testNetworkServer()
{
    if (!require(
            WiFi.scanNetworks() == 1 &&
                WiFi.SSID(0) == "Mac Host Network",
            "simulated Wi-Fi scan failed"))
    {
        return false;
    }
    WiFi.begin("Mac Host Network", "anything");
    if (!require(
            WiFi.status() == WL_CONNECTED &&
                WiFi.localIP().toString() == "127.0.0.1",
            "simulated Wi-Fi connection failed"))
    {
        return false;
    }

    WebServer server(80);
    server.on(
        "/hal-self-test", HTTP_GET,
        [&server]()
        {
            server.send(200, "text/plain", "Maclock");
        });
    server.on(
        "/hal-client-error", HTTP_GET,
        [&server]()
        {
            server.send(409, "text/plain", "Protected");
        });
    server.begin();

    std::atomic<bool> finished{false};
    std::atomic<bool> response_ok{false};
    const uint16_t port =
        maclock_hal().network().remapServerPort(80);
    std::thread client(
        [&]()
        {
            httplib::Client http("127.0.0.1", port);
            http.set_connection_timeout(2);
            const auto response = http.Get("/hal-self-test");
            const auto client_error =
                http.Get("/hal-client-error");
            response_ok =
                response && response->status == 200 &&
                response->body == "Maclock" &&
                client_error && client_error->status == 409 &&
                client_error->body == "Protected";
            finished = true;
        });
    const uint32_t started = millis();
    while (!finished && millis() - started < 3000)
    {
        server.handleClient();
        delay(1);
    }
    server.stop();
    client.join();
    return require(
        finished && response_ok,
        "localhost firmware server dispatch failed");
}
} // namespace

bool maclock_local_run_self_test(
    LocalHalSelfTestMode mode)
{
    const bool common =
        testDevices() &&
        testDisplayAndAudio() &&
        testNetworkServer();
    const bool persistence =
        mode == LocalHalSelfTestMode::Write
            ? writePersistence()
            : verifyPersistence();
    const bool passed = common && persistence;
    std::cout << "Local HAL self-test ("
              << (mode == LocalHalSelfTestMode::Write
                      ? "write"
                      : "verify")
              << ") " << (passed ? "passed" : "failed")
              << "\n";
    return passed;
}
