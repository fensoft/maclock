#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>

#include "wifi_mode.h"

enum class UpdateStage : uint8_t
{
    Idle,
    Checking,
    UpToDate,
    Available,
    DownloadingAssets,
    InstallingAssets,
    DownloadingFirmware,
    UploadingFirmware,
    ReadyToReboot,
    Error,
    Unsupported
};

struct UpdateSnapshot
{
    UpdateStage stage = UpdateStage::Idle;
    bool supported = true;
    bool busy = false;
    bool update_available = false;
    bool prompt_pending = false;
    bool reboot_required = false;
    uint8_t progress = 0;
    uint16_t changed_assets = 0;
    char current_version[32] = {};
    char asset_version[32] = {};
    char latest_version[32] = {};
    char release_url[192] = {};
    char release_notes[384] = {};
    char message[160] = {};
};

class UpdateService
{
public:
    struct State;

    void begin(Preferences &preferences);
    void tick(
        const WifiModeSnapshot &wifi,
        bool allow_device_prompt,
        bool allow_network_check);
    bool needsNetworkCheck(
        const WifiModeSnapshot &wifi) const;
    bool networkOperationActive() const;
    UpdateSnapshot snapshot() const;

    bool requestCheck();
    bool requestInstall();
    void dismiss(bool ignore_version);
    bool consumePrompt();

    bool beginManualFirmware(const char *filename);
    bool writeManualFirmware(
        const uint8_t *data, size_t length);
    bool finishManualFirmware();
    void abortManualFirmware();
    bool reboot();

private:
    State *state_ = nullptr;
};
