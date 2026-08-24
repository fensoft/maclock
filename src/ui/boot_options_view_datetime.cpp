#ifdef MACLOCK_COMBINED_SOURCE
static uint8_t datetime_days_in_month(
    uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31};
    if (month == 2)
    {
        const bool leap =
            (year % 4 == 0 && year % 100 != 0) ||
            year % 400 == 0;
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

static void datetime_set_enabled(bool enabled)
{
    lv_obj_t *objects[] = {
        boot_options_view.datetime_fields,
        boot_options_view.datetime_minus,
        boot_options_view.datetime_plus};
    for (lv_obj_t *object : objects)
    {
        if (!object)
            continue;
        if (enabled)
            lv_obj_remove_state(object, LV_STATE_DISABLED);
        else
            lv_obj_add_state(object, LV_STATE_DISABLED);
    }
}

void BootOptionsView::refreshDateTime()
{
    if (!boot_options_view.datetime_fields)
        return;

    if (!rtc_service.available())
    {
        for (uint8_t i = 0;
             i < BOOT_DATETIME_FIELD_COUNT; ++i)
        {
            strlcpy(
                boot_options_view.datetime_text[i], "--",
                sizeof(boot_options_view.datetime_text[i]));
        }
        datetime_set_enabled(false);
    }
    else
    {
        datetime_set_enabled(true);
        const DateTime current = rtc_now();
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_HOUR],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Hour"), current.hour());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_MINUTE],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Minute"), current.minute());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_SECOND],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Second"), current.second());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_DAY],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Day"), current.day());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_MONTH],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Month"), current.month());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_YEAR],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%04u", tr("Year"), current.year());
    }

    boot_options_view.datetime_field_order[0] =
        BOOT_DATETIME_HOUR;
    boot_options_view.datetime_field_order[1] =
        BOOT_DATETIME_MINUTE;
    boot_options_view.datetime_field_order[2] =
        BOOT_DATETIME_SECOND;
    switch (g_date_format)
    {
    case UI_DATE_FORMAT_MDY:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_DAY;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_YEAR;
        break;
    case UI_DATE_FORMAT_YMD:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_YEAR;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_DAY;
        break;
    default:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_DAY;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_YEAR;
        break;
    }

    for (uint8_t i = 0; i < 3; ++i)
    {
        boot_options_view.datetime_map[i] =
            boot_options_view.datetime_text[
                boot_options_view.datetime_field_order[i]];
        boot_options_view.datetime_map[i + 4] =
            boot_options_view.datetime_text[
                boot_options_view.datetime_field_order[i + 3]];
    }
    boot_options_view.datetime_map[3] = "\n";
    boot_options_view.datetime_map[7] = "";
    lv_buttonmatrix_set_map(
        boot_options_view.datetime_fields,
        boot_options_view.datetime_map);

    uint32_t selected_button = 0;
    for (uint8_t i = 0;
         i < BOOT_DATETIME_FIELD_COUNT; ++i)
    {
        if (boot_options_view.datetime_field_order[i] ==
            boot_options_view.datetime_selected)
        {
            selected_button = i;
            break;
        }
    }
    set_checked_button(
        boot_options_view.datetime_fields, selected_button);
}

static void boot_datetime_field_event(lv_event_t *event)
{
    lv_obj_t *matrix =
        (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(matrix);
    if (selected >= BOOT_DATETIME_FIELD_COUNT)
        return;
    boot_options_view.datetime_selected =
        boot_options_view.datetime_field_order[selected];
    set_checked_button(matrix, selected);
}

static void adjust_boot_datetime(int delta)
{
    if (!rtc_service.available())
        return;

    const DateTime current = rtc_now();
    int year = current.year();
    int month = current.month();
    int day = current.day();
    int hour = current.hour();
    int minute = current.minute();
    int second = current.second();

    switch (boot_options_view.datetime_selected)
    {
    case BOOT_DATETIME_HOUR:
        hour = constrain(hour + delta, 0, 23);
        break;
    case BOOT_DATETIME_MINUTE:
        minute = constrain(minute + delta, 0, 59);
        break;
    case BOOT_DATETIME_SECOND:
        second = constrain(second + delta, 0, 59);
        break;
    case BOOT_DATETIME_DAY:
        day = constrain(
            day + delta, 1,
            (int)datetime_days_in_month(year, month));
        break;
    case BOOT_DATETIME_MONTH:
        month = constrain(month + delta, 1, 12);
        day = min(
            day,
            (int)datetime_days_in_month(year, month));
        break;
    case BOOT_DATETIME_YEAR:
        year = constrain(year + delta, 2000, 2099);
        day = min(
            day,
            (int)datetime_days_in_month(year, month));
        break;
    default:
        return;
    }

    app_events.adjustRtc(
        DateTime(year, month, day, hour, minute, second));
    boot_options_view.refreshDateTime();
}

static void boot_datetime_minus_event(lv_event_t *event)
{
    (void)event;
    adjust_boot_datetime(-1);
}

static void boot_datetime_plus_event(lv_event_t *event)
{
    (void)event;
    adjust_boot_datetime(1);
}
#endif
