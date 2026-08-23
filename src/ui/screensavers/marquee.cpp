#ifdef MACLOCK_COMBINED_SOURCE
static bool update_marquee(ClockView &view, const ClockRenderSnapshot &snapshot, uint32_t frame) { char text[32]; snprintf(text, sizeof(text), "MACLOCK %02u:%02u", snapshot.current.hour(), snapshot.current.minute()); const int width = static_cast<int>(strlen(text)) * 18; draw_text(view, kExtWidth - static_cast<int>((frame * 2) % (kExtWidth + width)), 96, text, 3); line(view, 0, 82, kExtWidth - 1, 82); line(view, 0, 142, kExtWidth - 1, 142); return true; }
#endif
