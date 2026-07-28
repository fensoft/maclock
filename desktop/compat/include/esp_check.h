#pragma once

#define ESP_RETURN_ON_FALSE(condition, error, tag, format, ...) \
    do { if (!(condition)) return error; } while (0)
#define ESP_RETURN_ON_ERROR(expression, tag, format, ...) \
    do { const int result = (expression); if (result != 0) return result; } while (0)
