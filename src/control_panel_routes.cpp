#include "control_panel_internal.h"

#define g_events (active_control_panel->state().events)
#define g_server (active_control_panel->state().server)
#define g_routes_ready (active_control_panel->state().routes_ready)
#define g_sound_library (active_control_panel->state().sound_library)
#define g_configuration_archive (active_control_panel->state().configuration_archive)

void register_control_panel_routes()
{
    if (g_routes_ready) return;
    g_server.on("/", HTTP_GET, send_control_page);
    register_control_panel_settings_routes(g_server);
    register_control_panel_clockface_routes(g_server);
    register_control_panel_screensaver_routes(g_server);
    register_control_panel_minivmac_routes(g_server);
    register_control_panel_update_routes(g_server);
    g_server.on("/api/configuration/export", HTTP_GET, []() { g_configuration_archive.sendExport(g_server); });
    g_server.on("/api/configuration/import", HTTP_POST, []() { g_configuration_archive.finishUpload(g_server); }, []() { g_configuration_archive.receiveUpload(g_server); });
    g_server.on("/api/sound/upload", HTTP_POST, []() { g_sound_library.finishUpload(g_server); }, []() { g_sound_library.receiveUpload(g_server); });
    g_server.on("/api/sound/import", HTTP_POST, []() { if (!g_events) { control_panel_send_result(false, "Control service is unavailable", 503); return; } g_sound_library.importFromUrl(g_server, *g_events); });
    g_server.on("/api/sound/myinstants/search", HTTP_POST, []() { if (!g_events) { control_panel_send_result(false, "Control service is unavailable", 503); return; } g_sound_library.searchMyInstants(g_server, *g_events); });
    g_server.on("/api/sound/delete", HTTP_POST, []() { if (!g_events) { control_panel_send_result(false, "Control service is unavailable", 503); return; } g_sound_library.remove(g_server, *g_events); });
    g_server.onNotFound([]() { control_panel_send_result(false, "Control-panel route not found", 404); });
    g_routes_ready = true;
}
