#include "control_panel_internal.h"

namespace
{
String safe_loading_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 48; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')
            name += c;
    }
    return name;
}

String safe_loading_asset_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 64; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
            name += c;
    }
    const size_t length = name.length();
    return length >= 4 &&
            strcasecmp(name.c_str() + length - 4, ".png") == 0
        ? name : String();
}

bool requested_loading_name(String &name)
{
    if (!active_control_panel->state().server.hasArg("screen") &&
        !active_control_panel->state().server.hasArg("name"))
        return false;
    name = safe_loading_name(active_control_panel->state().server.hasArg("screen")
        ? active_control_panel->state().server.arg("screen")
        : active_control_panel->state().server.arg("name"));
    return name.length() > 0;
}

void send_loading_file(File &file, const char *mime)
{
    WebServer &server = active_control_panel->state().server;
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(file.size());
    server.send(200, mime, "");
    uint8_t buffer[1024];
    while (file.position() < file.size())
    {
        const size_t count = file.read(buffer, sizeof(buffer));
        if (!count) break;
        server.sendContent(reinterpret_cast<const char *>(buffer), count);
    }
}

void send_loading_list()
{
    LittleFS.mkdir("/loading");
    JsonDocument document;
    JsonArray screens = document["screens"].to<JsonArray>();
    File directory = LittleFS.open("/loading");
    if (directory && directory.isDirectory())
    {
        for (File file = directory.openNextFile(); file;
             file = directory.openNextFile())
        {
            if (!file.isDirectory()) continue;
            const String path = file.name();
            const int slash = path.lastIndexOf("/");
            const String name = safe_loading_name(path.substring(slash + 1));
            File project = name.length() ? LittleFS.open(
                (String("/loading/") + name + "/loading.json").c_str(), "r") : File();
            if (project)
            {
                screens.add(name);
                project.close();
            }
        }
        directory.close();
    }
    send_json(document);
}

void send_loading_project()
{
    String name;
    const String path = requested_loading_name(name)
        ? String("/loading/") + name + "/loading.json" : String();
    File file = path.length() ? LittleFS.open(path.c_str(), "r") : File();
    if (!file)
    {
        send_result(false, "Loading screen not found", 404);
        return;
    }
    send_loading_file(file, "application/json");
    file.close();
}

void send_loading_assets()
{
    String name;
    if (!requested_loading_name(name))
    {
        send_result(false, "Loading screen not found", 404);
        return;
    }
    JsonDocument document;
    JsonArray assets = document["assets"].to<JsonArray>();
    File directory = LittleFS.open((String("/loading/") + name).c_str());
    if (!directory || !directory.isDirectory())
    {
        send_result(false, "Loading screen not found", 404);
        return;
    }
    for (File file = directory.openNextFile(); file;
         file = directory.openNextFile())
    {
        if (file.isDirectory()) continue;
        const String path = file.name();
        const int slash = path.lastIndexOf("/");
        const String asset = safe_loading_asset_name(
            slash >= 0 ? path.substring(slash + 1) : path);
        if (asset.length()) assets.add(asset);
    }
    directory.close();
    send_json(document);
}

void save_loading_project()
{
    WebServer &server = active_control_panel->state().server;
    String name;
    const String json = server.arg("json");
    if (!requested_loading_name(name) || !json.length() || json.length() > 32768)
    {
        send_result(false, "Invalid loading screen project", 400);
        return;
    }
    JsonDocument document;
    if (deserializeJson(document, json) ||
        document["format"] != "maclock-loading-screen" ||
        document["width"] != 304 || document["height"] != 224)
    {
        send_result(false, "Invalid loading screen JSON", 400);
        return;
    }
    LittleFS.mkdir("/loading");
    LittleFS.mkdir((String("/loading/") + name).c_str());
    File file = LittleFS.open(
        (String("/loading/") + name + "/loading.json").c_str(), "w");
    if (!file || file.write(reinterpret_cast<const uint8_t *>(json.c_str()),
                            json.length()) != json.length())
    {
        if (file) file.close();
        send_result(false, "Could not save loading screen", 500);
        return;
    }
    file.close();
    send_result(true, "Loading screen saved");
}

void receive_loading_asset_upload()
{
    auto &state = active_control_panel->state();
    WebServer &server = state.server;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        state.loading_upload_started = true;
        state.loading_upload_error = "";
        state.loading_upload_size = 0;
        String screen;
        const String asset = safe_loading_asset_name(upload.filename);
        if (!requested_loading_name(screen) || !asset.length())
            state.loading_upload_error = "Only PNG loading-screen assets are accepted";
        else
        {
            LittleFS.mkdir("/loading");
            LittleFS.mkdir((String("/loading/") + screen).c_str());
            state.loading_upload_path = String("/loading/") + screen + "/" + asset;
            state.loading_upload = LittleFS.open(state.loading_upload_path.c_str(), "w");
            if (!state.loading_upload)
                state.loading_upload_error = "Could not create loading-screen asset";
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!state.loading_upload_error.length() &&
            state.loading_upload.write(upload.buf, upload.currentSize) != upload.currentSize)
            state.loading_upload_error = "Could not write loading-screen asset";
        state.loading_upload_size += upload.currentSize;
        if (state.loading_upload_size > 1024U * 1024U)
            state.loading_upload_error = "Loading-screen assets are limited to 1 MB";
    }
    else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED)
    {
        if (state.loading_upload) state.loading_upload.close();
        if (upload.status == UPLOAD_FILE_ABORTED)
            state.loading_upload_error = "Loading-screen upload was cancelled";
        if (state.loading_upload_error.length())
            LittleFS.remove(state.loading_upload_path.c_str());
    }
}

void finish_loading_asset_upload()
{
    auto &state = active_control_panel->state();
    if (!state.loading_upload_started || state.loading_upload_error.length())
    {
        send_result(false, state.loading_upload_error.length()
            ? state.loading_upload_error.c_str() : "No loading-screen asset was uploaded", 400);
        return;
    }
    state.loading_upload_started = false;
    send_result(true, "Loading-screen asset saved");
}

void send_loading_asset()
{
    WebServer &server = active_control_panel->state().server;
    String name;
    const String asset = safe_loading_asset_name(server.arg("name"));
    if (!requested_loading_name(name) || !asset.length())
    {
        send_result(false, "Loading-screen asset not found", 404);
        return;
    }
    File file = LittleFS.open((String("/loading/") + name + "/" + asset).c_str(), "r");
    if (!file)
    {
        send_result(false, "Loading-screen asset not found", 404);
        return;
    }
    send_loading_file(file, "image/png");
    file.close();
}
} // namespace

void register_control_panel_loading_routes(WebServer &server)
{
    server.on("/api/loading/list", HTTP_GET, send_loading_list);
    server.on("/api/loading/project", HTTP_GET, send_loading_project);
    server.on("/api/loading/assets", HTTP_GET, send_loading_assets);
    server.on("/api/loading/project", HTTP_POST, save_loading_project);
    server.on("/api/loading/asset", HTTP_GET, send_loading_asset);
    server.on("/api/loading/asset/upload", HTTP_POST,
              finish_loading_asset_upload, receive_loading_asset_upload);
}
