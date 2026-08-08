#ifdef MACLOCK_COMBINED_SOURCE

namespace
{
static constexpr int16_t kExtWidth = 304;
static constexpr int16_t kExtHeight = 224;
static constexpr uint16_t kExtStride =
    ((kExtWidth + LV_DRAW_BUF_STRIDE_ALIGN - 1) /
     LV_DRAW_BUF_STRIDE_ALIGN) * LV_DRAW_BUF_STRIDE_ALIGN;
static constexpr size_t kExtBufferBytes =
    LV_CANVAS_BUF_SIZE(
        kExtWidth, kExtHeight, 8, LV_DRAW_BUF_STRIDE_ALIGN);
static constexpr uint8_t kToasterSpeedStateOffset = 16;

static bool is_extended_mode(ScreensaverMode mode)
{
    return mode >= ScreensaverMode::FlyingToasters &&
           mode <= ScreensaverMode::PhotoSlideshow;
}

static uint8_t *pixels(ClockView &view)
{
    return view.screensaver_canvas_buffer;
}

static void clear_canvas(ClockView &view, bool white = false)
{
    memset(
        pixels(view), white ? 0xFF : 0,
        kExtStride * kExtHeight);
}

static void put_pixel(
    ClockView &view, int16_t x, int16_t y, bool white = true);
static void line(
    ClockView &view, int x0, int y0, int x1, int y1,
    bool white = true);

static bool atlas_pixel_white(
    const lv_draw_buf_t *atlas, uint32_t x, uint32_t y)
{
    if (!atlas || x >= atlas->header.w || y >= atlas->header.h)
        return false;
    const uint8_t *pixel = static_cast<const uint8_t *>(
        lv_draw_buf_goto_xy(atlas, x, y));
    switch (static_cast<lv_color_format_t>(atlas->header.cf))
    {
    case LV_COLOR_FORMAT_L8:
        return pixel[0] >= 128;
    case LV_COLOR_FORMAT_RGB888:
    case LV_COLOR_FORMAT_XRGB8888:
        return static_cast<uint16_t>(pixel[0]) + pixel[1] + pixel[2] >= 384;
    case LV_COLOR_FORMAT_ARGB8888:
    case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
    {
        const lv_color32_t color = *reinterpret_cast<const lv_color32_t *>(pixel);
        return color.alpha >= 128 &&
               static_cast<uint16_t>(color.red) + color.green + color.blue >= 384;
    }
    case LV_COLOR_FORMAT_RGB565:
    {
        const lv_color16_t color = *reinterpret_cast<const lv_color16_t *>(pixel);
        return color.red + color.green + color.blue >= 48;
    }
    default:
        return false;
    }
}

static void draw_atlas_sprite(
    ClockView &view, const lv_draw_buf_t *atlas,
    uint8_t variant, uint16_t cell_width, uint16_t cell_height,
    int16_t x, int16_t y, uint16_t width, uint16_t height,
    bool mirror = false, bool white = true, uint8_t row = 0)
{
    if (!atlas)
        return;
    const uint32_t source_left = (variant % 10U) * cell_width;
    const uint32_t source_top = row * cell_height;
    for (uint16_t yy = 0; yy < height; ++yy)
        for (uint16_t xx = 0; xx < width; ++xx)
        {
            const uint16_t sample_x = static_cast<uint16_t>(
                (static_cast<uint32_t>(xx) * cell_width) / width);
            const uint16_t source_x = mirror
                ? static_cast<uint16_t>(cell_width - 1 - sample_x)
                : sample_x;
            const uint16_t source_y = static_cast<uint16_t>(
                (static_cast<uint32_t>(yy) * cell_height) / height);
            if (atlas_pixel_white(
                    atlas, source_left + source_x,
                    source_top + source_y))
                put_pixel(view, x + xx, y + yy, white);
        }
}

static void put_pixel(
    ClockView &view, int16_t x, int16_t y, bool white)
{
    if (x < 0 || y < 0 || x >= kExtWidth || y >= kExtHeight)
        return;
    pixels(view)[y * kExtStride + x] = white ? 0xFF : 0;
}

static void fill_rect(
    ClockView &view, int16_t x, int16_t y,
    int16_t width, int16_t height, bool white = true)
{
    for (int16_t yy = y; yy < y + height; ++yy)
        for (int16_t xx = x; xx < x + width; ++xx)
            put_pixel(view, xx, yy, white);
}

static void draw_bubble(
    ClockView &view, int16_t x, int16_t y, uint8_t radius)
{
    if (radius <= 1)
    {
        put_pixel(view, x, y);
        put_pixel(view, x + 1, y);
        put_pixel(view, x, y + 1);
        put_pixel(view, x + 1, y + 1);
        return;
    }

    const int16_t inner = radius - 1;
    line(view, x - inner, y - radius, x + inner, y - radius);
    line(view, x - inner, y + radius, x + inner, y + radius);
    line(view, x - radius, y - inner, x - radius, y + inner);
    line(view, x + radius, y - inner, x + radius, y + inner);
    put_pixel(view, x - inner, y - inner);
    put_pixel(view, x + inner, y - inner);
    put_pixel(view, x - inner, y + inner);
    put_pixel(view, x + inner, y + inner);

    if (radius >= 3)
        put_pixel(view, x - 1, y - 1);
}

static void line(
    ClockView &view, int x0, int y0, int x1, int y1,
    bool white)
{
    const int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;)
    {
        put_pixel(view, x0, y0, white);
        if (x0 == x1 && y0 == y1)
            break;
        const int doubled = error * 2;
        if (doubled >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

static const uint8_t *glyph(char character)
{
    static const uint8_t blank[5] = {};
    static const uint8_t digits[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E},
        {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46},
        {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10},
        {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30},
        {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36},
        {0x06, 0x49, 0x49, 0x29, 0x1E}};
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
        {0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},
        {0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43}};
    static const uint8_t colon[5] = {0, 0x36, 0x36, 0, 0};
    if (character >= '0' && character <= '9')
        return digits[character - '0'];
    if (character >= 'A' && character <= 'Z')
        return letters[character - 'A'];
    if (character == ':')
        return colon;
    return blank;
}

static void draw_text(
    ClockView &view, int16_t x, int16_t y,
    const char *text, uint8_t scale = 1, bool white = true)
{
    while (text && *text)
    {
        const uint8_t *columns = glyph(*text++);
        for (uint8_t column = 0; column < 5; ++column)
            for (uint8_t row = 0; row < 7; ++row)
                if (columns[column] & (1U << row))
                    fill_rect(
                        view, x + column * scale,
                        y + row * scale, scale, scale, white);
        x += 6 * scale;
    }
}

static void draw_time(
    ClockView &view, const DateTime &current,
    int16_t x, int16_t y, uint8_t scale)
{
    char value[6];
    snprintf(value, sizeof(value), "%02u:%02u",
             current.hour(), current.minute());
    draw_text(view, x, y, value, scale);
}

static void seed_particles(ClockView &view, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        view.screensaver_x[i] = random(0, kExtWidth);
        view.screensaver_y[i] = random(0, kExtHeight);
        view.screensaver_dx[i] = random(0, 2) ? 1 : -1;
        view.screensaver_dy[i] = random(0, 2) ? 1 : -1;
    }
}

static uint8_t random_fish_variant()
{
    return static_cast<uint8_t>(random(0, 9));
}

static void draw_toaster(
    ClockView &view, int16_t x, int16_t y,
    uint8_t variant, bool wings_up)
{
    const int16_t body_w = 18 + (variant % 3) * 3;
    const int16_t body_h = 12 + ((variant / 3) % 3) * 2;
    fill_rect(view, x, y, body_w, body_h);
    fill_rect(view, x + 3, y + 3, body_w - 6, body_h - 6, false);
    line(view, x + 4, y + 1, x + body_w - 5, y + 1, false);
    put_pixel(view, x + body_w - 2, y + body_h / 2, false);

    const int16_t root_x = x + 5 + (variant & 3);
    const int16_t root_y = y;
    const int16_t reach = 7 + (variant % 5) * 2;
    const int16_t rise = wings_up ? -(8 + (variant % 4) * 3)
                                  : 7 + (variant % 4) * 2;
    line(view, root_x, root_y, root_x - reach, root_y + rise);
    line(view, root_x + 3, root_y, root_x - reach / 2, root_y + rise);
    if (variant == 1 || variant == 6 || variant == 9)
        line(view, root_x - reach, root_y + rise,
             root_x - reach - 4, root_y + rise / 2);
    if (variant == 4)
    {
        fill_rect(view, root_x - 3, root_y - 4, 4, 4);
        line(view, root_x - 1, root_y - 3,
             root_x - reach, root_y + rise);
    }
    if (variant == 5)
        line(view, root_x - reach, root_y + rise,
             root_x - reach - 5, root_y - rise / 3);
    if (variant == 7)
        fill_rect(view, x + body_w, y + body_h / 2, 5, 3);
    if (variant == 8)
    {
        line(view, x, y + body_h, x + 5, y + body_h + 4);
        line(view, x + body_w, y + body_h,
             x + body_w - 5, y + body_h + 4);
    }
}

static int16_t fish_x(int16_t x, int16_t width, int16_t local, bool right)
{
    return right ? x + local : x + width - 1 - local;
}

static void fish_line(
    ClockView &view, int16_t x, int16_t y, int16_t width,
    bool right, int x0, int y0, int x1, int y1)
{
    line(view, fish_x(x, width, x0, right), y + y0,
         fish_x(x, width, x1, right), y + y1);
}

static void draw_fish(
    ClockView &view, int16_t x, int16_t y,
    uint8_t variant, bool right)
{
    const int16_t width = 26;
    const int16_t body_h = 7 + (variant % 4) * 2;
    const int16_t top = 7 - body_h / 2;
    const int16_t body_start = variant == 6 ? 8 : 5;
    const int16_t body_width = variant == 6 ? 11 : 15;
    for (int row = 0; row < body_h; ++row)
    {
        const int inset = (row == 0 || row == body_h - 1) ? 2 :
                          (row == 1 || row == body_h - 2) ? 1 : 0;
        fish_line(view, x, y, width, right,
                  body_start + inset, top + row,
                  body_start + body_width - 1 - inset, top + row);
    }
    fish_line(view, x, y, width, right, body_start, 7, 0, 2);
    fish_line(view, x, y, width, right, body_start, 7, 0, 12);
    fish_line(view, x, y, width, right, 0, 2, 0, 12);
    put_pixel(view, fish_x(x, width, body_start + body_width - 3, right),
              y + top + 2, false);

    switch (variant)
    {
    case 1: // puffer
        for (int spike = 5; spike < 20; spike += 4)
        {
            put_pixel(view, fish_x(x, width, spike, right), y + top - 2);
            put_pixel(view, fish_x(x, width, spike, right), y + top + body_h + 1);
        }
        break;
    case 2: // angelfish
        fish_line(view, x, y, width, right, 10, top, 14, top - 7);
        fish_line(view, x, y, width, right, 14, top - 7, 17, top);
        fish_line(view, x, y, width, right, 10, top + body_h,
                  14, top + body_h + 7);
        break;
    case 3: // fast fish
        fish_line(view, x, y, width, right, 8, top - 1, 18, top - 1);
        fish_line(view, x, y, width, right, 8, top + body_h,
                  18, top + body_h);
        break;
    case 4: // flowing fins
        fish_line(view, x, y, width, right, 9, top, 6, top - 5);
        fish_line(view, x, y, width, right, 10, top + body_h - 1,
                  5, top + body_h + 5);
        break;
    case 5: // striped
        for (int stripe = 9; stripe <= 15; stripe += 3)
            fish_line(view, x, y, width, right, stripe, top + 1,
                      stripe, top + body_h - 2);
        break;
    case 6: // tiny schooling fish
        fish_line(view, x, y, width, right, 8, 7, 4, 4);
        fish_line(view, x, y, width, right, 8, 7, 4, 10);
        break;
    case 7: // seahorse-like crest
        fish_line(view, x, y, width, right, 17, top, 19, top - 4);
        fish_line(view, x, y, width, right, 19, top - 4, 21, top);
        break;
    case 8: // friendly shark
        fish_line(view, x, y, width, right, 11, top, 14, top - 5);
        fish_line(view, x, y, width, right, 17, top + body_h - 1,
                  21, top + body_h + 3);
        break;
    case 9: // bottom dweller
        fish_line(view, x, y, width, right, 6, top + body_h,
                  21, top + body_h);
        fish_line(view, x, y, width, right, 9, top + body_h,
                  5, top + body_h + 3);
        break;
    default:
        fish_line(view, x, y, width, right, 10, top, 13, top - 3);
        break;
    }
}

static constexpr uint8_t kMazeColumns = 19;
static constexpr uint8_t kMazeRows = 14;
static constexpr uint16_t kMazeCells =
    kMazeColumns * kMazeRows;

static void generate_maze(ClockView &view)
{
    uint8_t *walls = view.screensaver_state;
    uint8_t *visited = walls + kMazeCells;
    uint16_t *stack = reinterpret_cast<uint16_t *>(
        view.screensaver_state + kMazeCells * 2);
    int16_t *parent = reinterpret_cast<int16_t *>(
        view.screensaver_state + kMazeCells * 2 +
        kMazeCells * sizeof(uint16_t));
    uint16_t *solution = reinterpret_cast<uint16_t *>(
        view.screensaver_state + kMazeCells * 2 +
        kMazeCells * sizeof(uint16_t) * 2);

    memset(walls, 0x0F, kMazeCells);
    memset(visited, 0, kMazeCells);
    uint16_t depth = 1;
    stack[0] = 0;
    visited[0] = 1;
    static constexpr int8_t dx[4] = {0, 1, 0, -1};
    static constexpr int8_t dy[4] = {-1, 0, 1, 0};
    static constexpr uint8_t opposite[4] = {2, 3, 0, 1};

    while (depth)
    {
        const uint16_t cell = stack[depth - 1];
        const int x = cell % kMazeColumns;
        const int y = cell / kMazeColumns;
        uint8_t choices[4];
        uint8_t count = 0;
        for (uint8_t direction = 0; direction < 4; ++direction)
        {
            const int nx = x + dx[direction];
            const int ny = y + dy[direction];
            if (nx >= 0 && nx < kMazeColumns &&
                ny >= 0 && ny < kMazeRows &&
                !visited[ny * kMazeColumns + nx])
                choices[count++] = direction;
        }
        if (!count)
        {
            --depth;
            continue;
        }
        const uint8_t direction = choices[random(0, count)];
        const uint16_t next =
            static_cast<uint16_t>((y + dy[direction]) *
                kMazeColumns + x + dx[direction]);
        walls[cell] &= static_cast<uint8_t>(~(1U << direction));
        walls[next] &= static_cast<uint8_t>(
            ~(1U << opposite[direction]));
        visited[next] = 1;
        stack[depth++] = next;
    }

    for (uint16_t i = 0; i < kMazeCells; ++i)
        parent[i] = -1;
    uint16_t head = 0;
    uint16_t tail = 1;
    stack[0] = 0;
    parent[0] = 0;
    while (head < tail && parent[kMazeCells - 1] < 0)
    {
        const uint16_t cell = stack[head++];
        const int x = cell % kMazeColumns;
        const int y = cell / kMazeColumns;
        for (uint8_t direction = 0; direction < 4; ++direction)
        {
            if (walls[cell] & (1U << direction))
                continue;
            const int nx = x + dx[direction];
            const int ny = y + dy[direction];
            const uint16_t next =
                static_cast<uint16_t>(ny * kMazeColumns + nx);
            if (parent[next] >= 0)
                continue;
            parent[next] = static_cast<int16_t>(cell);
            stack[tail++] = next;
        }
    }

    uint16_t length = 0;
    for (uint16_t cell = kMazeCells - 1;;
         cell = static_cast<uint16_t>(parent[cell]))
    {
        solution[length++] = cell;
        if (cell == 0)
            break;
    }
    for (uint16_t i = 0; i < length / 2; ++i)
    {
        const uint16_t swap = solution[i];
        solution[i] = solution[length - i - 1];
        solution[length - i - 1] = swap;
    }
    view.screensaver_x[63] = static_cast<int16_t>(length);
}

static bool load_next_photo(ClockView &view)
{
    File directory = LittleFS.open("/screensaver");
    if (!directory || !directory.isDirectory())
        return false;
    File selected;
    bool use_next = view.screensaver_photo_path[0] == '\0';
    for (File file = directory.openNextFile(); file;
         file = directory.openNextFile())
    {
        const String path = file.name();
        const char *path_text = path.c_str();
        const size_t path_length = strlen(path_text);
        const bool jpg = path_length >= 4 &&
            strcasecmp(path_text + path_length - 4, ".jpg") == 0;
        const bool jpeg = path_length >= 5 &&
            strcasecmp(path_text + path_length - 5, ".jpeg") == 0;
        if (!jpg && !jpeg)
            continue;
        if (use_next ||
            strcmp(path_text, view.screensaver_photo_path) > 0)
        {
            selected = file;
            break;
        }
    }
    directory.close();
    if (!selected && view.screensaver_photo_path[0])
    {
        view.screensaver_photo_path[0] = '\0';
        return load_next_photo(view);
    }
    if (!selected)
        return false;
    strlcpy(
        view.screensaver_photo_path,
        selected.name(), sizeof(view.screensaver_photo_path));
    selected.close();
    char source[104];
    snprintf(source, sizeof(source), "S:%s",
             view.screensaver_photo_path);
    lv_image_set_src(view.screensaver_photo, source);
    return true;
}

static void draw_photo(ClockView &view)
{
    if (!view.screensaver_photo_loaded)
    {
        clear_canvas(view);
        line(view, 96, 70, 208, 70);
        line(view, 96, 70, 96, 150);
        line(view, 96, 150, 208, 150);
        line(view, 208, 70, 208, 150);
        draw_text(view, 112, 101, "ADD JPG", 2);
        return;
    }
    lv_obj_set_style_opa(
        view.screensaver_photo,
        static_cast<lv_opa_t>(min<uint16_t>(255,
            view.screensaver_photo_transition * 17)), 0);
}
} // namespace

void ClockView::initExtendedScreensavers(lv_obj_t *parent)
{
    screensaver_toaster_atlas =
        load_png_once("S:/screensavers/toasters.png");
    screensaver_fish_atlas =
        load_png_once("S:/screensavers/fish.png");
    screensaver_error_atlas =
        load_png_once("S:/screensavers/errors.png");
    screensaver_canvas_buffer = static_cast<uint8_t *>(
        heap_caps_malloc(
            kExtBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!screensaver_canvas_buffer)
        screensaver_canvas_buffer = static_cast<uint8_t *>(
            lv_malloc(kExtBufferBytes));
    if (!screensaver_canvas_buffer)
        return;

    screensaver_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(
        screensaver_canvas, screensaver_canvas_buffer,
        kExtWidth, kExtHeight, LV_COLOR_FORMAT_L8);
    lv_obj_set_pos(screensaver_canvas, 0, 0);
    lv_obj_add_flag(screensaver_canvas, LV_OBJ_FLAG_HIDDEN);
    screensaver_photo = lv_image_create(parent);
    lv_obj_set_size(screensaver_photo, kExtWidth, kExtHeight);
    lv_obj_set_pos(screensaver_photo, 0, 0);
    lv_image_set_inner_align(
        screensaver_photo, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_add_flag(screensaver_photo, LV_OBJ_FLAG_HIDDEN);
    clear_canvas(*this);
}

bool ClockView::activateExtendedScreensaver(
    ScreensaverMode mode, bool reset)
{
    if (!screensaver_canvas)
        return false;
    const bool active = is_extended_mode(mode);
    const bool photo = mode == ScreensaverMode::PhotoSlideshow;
    if (active && !photo)
        lv_obj_clear_flag(screensaver_canvas, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(screensaver_canvas, LV_OBJ_FLAG_HIDDEN);
    if (active && photo)
        lv_obj_clear_flag(screensaver_photo, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(screensaver_photo, LV_OBJ_FLAG_HIDDEN);
    if (!active)
        return false;
    if (reset)
    {
        screensaver_extended_frame = 0;
        memset(screensaver_state, 0, sizeof(screensaver_state));
        seed_particles(*this, 64);
        if (mode == ScreensaverMode::FlyingToasters)
            for (uint8_t i = 0; i < 7; ++i)
            {
                screensaver_state[i] = random(0, 10);
                screensaver_state[
                    kToasterSpeedStateOffset + i] =
                    random(70, 131);
            }
        if (mode == ScreensaverMode::Aquarium)
            for (uint8_t i = 0; i < 9; ++i)
                screensaver_state[i] = random_fish_variant();
        if (mode == ScreensaverMode::ErrorParade)
            for (uint8_t i = 0; i < 5; ++i)
                screensaver_state[i] = random(0, 10);
        screensaver_photo_transition = 0;
        screensaver_photo_due_ms = millis() + 10000;
        if (mode == ScreensaverMode::Life)
            for (size_t i = 0; i < 38U * 28U; ++i)
                screensaver_state[i] = random(0, 3) == 0;
        if (mode == ScreensaverMode::Maze)
            generate_maze(*this);
        if (mode == ScreensaverMode::PhotoSlideshow)
            screensaver_photo_loaded = load_next_photo(*this);
    }
    return true;
}

bool ClockView::updateExtendedScreensaver(
    const ClockRenderSnapshot &snapshot)
{
    if (!is_extended_mode(screensaver_active_mode) ||
        !screensaver_canvas)
        return false;
    const unsigned long now_ms = millis();
    const unsigned long frame_interval =
        screensaver_frame_interval(screensaver_active_mode);
    if (screensaver_last_move_ms &&
        now_ms - screensaver_last_move_ms < frame_interval)
        return true;
    screensaver_last_move_ms = now_ms;
    const uint32_t frame = screensaver_extended_frame++;
    clear_canvas(*this);

    switch (screensaver_active_mode)
    {
    case ScreensaverMode::FlyingToasters:
        for (uint8_t i = 0; i < 7; ++i)
        {
            const uint8_t speed = screensaver_state[
                kToasterSpeedStateOffset + i];
            const uint64_t distance_before =
                static_cast<uint64_t>(frame) * speed;
            const uint64_t distance_after =
                static_cast<uint64_t>(frame + 1) * speed;
            const int16_t horizontal_step =
                static_cast<int16_t>(
                    distance_after * 2 / 100 -
                    distance_before * 2 / 100);
            const int16_t vertical_step =
                static_cast<int16_t>(
                    distance_after / 100 -
                    distance_before / 100);
            screensaver_x[i] += horizontal_step;
            screensaver_y[i] +=
                (i & 1) ? vertical_step : -vertical_step;
            if (screensaver_x[i] > kExtWidth + 56)
            {
                screensaver_x[i] = -56;
                screensaver_state[i] = random(0, 10);
            }
            if (screensaver_y[i] < -20) screensaver_y[i] = kExtHeight;
            if (screensaver_y[i] > kExtHeight) screensaver_y[i] = -20;
            const int x = screensaver_x[i], y = screensaver_y[i];
            if (screensaver_toaster_atlas)
                draw_atlas_sprite(
                    *this, screensaver_toaster_atlas,
                    screensaver_state[i],
                    64, 48,
                    x - 8, y - 10, 56, 42,
                    true, true,
                    static_cast<uint8_t>(
                        ((distance_before / 100) / 5) & 1U));
            else
                draw_toaster(
                    *this, x, y, screensaver_state[i],
                    (((distance_before / 100) / 5) + i) & 1U);
        }
        break;

    case ScreensaverMode::Marquee:
    {
        char text[32];
        snprintf(text, sizeof(text), "MACLOCK %02u:%02u",
                 snapshot.current.hour(), snapshot.current.minute());
        const int width = static_cast<int>(strlen(text)) * 18;
        const int x = kExtWidth - static_cast<int>((frame * 2) %
                                                   (kExtWidth + width));
        draw_text(*this, x, 96, text, 3);
        line(*this, 0, 82, kExtWidth - 1, 82);
        line(*this, 0, 142, kExtWidth - 1, 142);
        break;
    }

    case ScreensaverMode::DigitalRainClock:
        for (uint8_t i = 0; i < 38; ++i)
        {
            screensaver_y[i] += 2 + (i % 4);
            if (screensaver_y[i] > kExtHeight + 20)
                screensaver_y[i] = -random(10, 100);
            for (uint8_t tail = 0; tail < 5; ++tail)
                put_pixel(*this, i * 8 + 2,
                          screensaver_y[i] - tail * 4);
        }
        draw_time(*this, snapshot.current, 43, 83, 6);
        break;

    case ScreensaverMode::Mystify:
        for (uint8_t i = 0; i < 4; ++i)
        {
            screensaver_x[i] += screensaver_dx[i] * 2;
            screensaver_y[i] += screensaver_dy[i] * 2;
            if (screensaver_x[i] <= 2 || screensaver_x[i] >= 301)
                screensaver_dx[i] = -screensaver_dx[i];
            if (screensaver_y[i] <= 2 || screensaver_y[i] >= 221)
                screensaver_dy[i] = -screensaver_dy[i];
            line(*this, screensaver_x[i], screensaver_y[i],
                 screensaver_x[(i + 1) & 3], screensaver_y[(i + 1) & 3]);
        }
        break;

    case ScreensaverMode::Aquarium:
        for (uint8_t i = 0; i < 9; ++i)
        {
            screensaver_x[i] += (i & 1) ? 1 : -1;
            if (screensaver_x[i] < -48)
            {
                screensaver_x[i] = kExtWidth + 48;
                screensaver_state[i] = random_fish_variant();
            }
            if (screensaver_x[i] > kExtWidth + 48)
            {
                screensaver_x[i] = -48;
                screensaver_state[i] = random_fish_variant();
            }
            const int x = screensaver_x[i], y = 15 + i * 22;
            if (screensaver_fish_atlas)
                draw_atlas_sprite(
                    *this, screensaver_fish_atlas,
                    screensaver_state[i], 64, 48,
                    x - 8, y - 8, 48, 36, i & 1U);
            else
                draw_fish(
                    *this, x, y,
                    screensaver_state[i] >= 6
                        ? screensaver_state[i] + 1
                        : screensaver_state[i],
                    i & 1U);
        }
        for (uint8_t i = 0; i < 18; ++i)
        {
            const uint8_t radius = 1 + (i % 3);
            const uint16_t travel = kExtHeight + radius * 2 + 12;
            const uint16_t rise =
                (frame * (1 + (i % 3)) + i * 29) % travel;
            const int16_t y = kExtHeight + radius - rise;
            const int8_t drift = static_cast<int8_t>(
                ((frame / (5 + i % 4) + i * 3) % 7)) - 3;
            const int16_t x =
                12 + ((i * 53 + (i % 4) * 17) % (kExtWidth - 24)) +
                drift;
            draw_bubble(*this, x, y, radius);
        }
        break;

    case ScreensaverMode::Life:
        if ((frame & 3) == 0)
        {
            uint8_t *current = screensaver_state;
            uint8_t *next = screensaver_state + 38 * 28;
            for (int y = 0; y < 28; ++y)
                for (int x = 0; x < 38; ++x)
                {
                    uint8_t neighbors = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if ((dx || dy) &&
                                current[((y + dy + 28) % 28) * 38 +
                                        ((x + dx + 38) % 38)])
                                ++neighbors;
                    next[y * 38 + x] =
                        neighbors == 3 ||
                        (neighbors == 2 && current[y * 38 + x]);
                }
            memcpy(current, next, 38 * 28);
        }
        for (int y = 0; y < 28; ++y)
            for (int x = 0; x < 38; ++x)
                if (screensaver_state[y * 38 + x])
                    fill_rect(*this, x * 8 + 1, y * 8 + 1, 6, 6);
        break;

    case ScreensaverMode::Maze:
    {
        const uint8_t *walls = screensaver_state;
        const uint16_t *solution = reinterpret_cast<const uint16_t *>(
            screensaver_state + kMazeCells * 2 +
            kMazeCells * sizeof(uint16_t) * 2);
        for (uint16_t cell = 0; cell < kMazeCells; ++cell)
        {
            const int x = (cell % kMazeColumns) * 16;
            const int y = (cell / kMazeColumns) * 16;
            if (walls[cell] & 1U) line(*this, x, y, x + 15, y);
            if (walls[cell] & 2U) line(*this, x + 15, y, x + 15, y + 15);
            if (walls[cell] & 4U) line(*this, x, y + 15, x + 15, y + 15);
            if (walls[cell] & 8U) line(*this, x, y, x, y + 15);
        }
        const uint16_t length =
            static_cast<uint16_t>(screensaver_x[63]);
        const uint16_t progress = min<uint16_t>(
            length, static_cast<uint16_t>(frame / 2 + 1));
        for (uint16_t i = 0; i < progress; ++i)
        {
            const uint16_t cell = solution[i];
            fill_rect(*this, (cell % kMazeColumns) * 16 + 6,
                      (cell / kMazeColumns) * 16 + 6, 4, 4);
        }
        if (progress == length && frame > length * 2 + 90)
        {
            generate_maze(*this);
            screensaver_extended_frame = 0;
        }
        break;
    }

    case ScreensaverMode::ErrorParade:
        for (uint8_t i = 0; i < 5; ++i)
        {
            constexpr int kErrorWidth = 84;
            constexpr int kErrorTravel = kExtWidth + kErrorWidth;
            const int x = static_cast<int>(
                (screensaver_x[i] + frame * (i + 1)) % kErrorTravel) -
                kErrorWidth;
            const int y = 12 + ((screensaver_y[i] + frame) % 145);
            if (screensaver_error_atlas)
            {
                const uint8_t variant = static_cast<uint8_t>(
                    (screensaver_state[i] +
                     (frame * (i + 1)) / 220) % 10);
                fill_rect(*this, x, y, 84, 54);
                line(*this, x + 1, y + 1, x + 82, y + 1, false);
                line(*this, x + 1, y + 1, x + 1, y + 52, false);
                line(*this, x + 82, y + 1, x + 82, y + 52, false);
                line(*this, x + 1, y + 52, x + 82, y + 52, false);
                line(*this, x + 2, y + 12, x + 81, y + 12, false);
                line(*this, x + 5, y + 3, x + 12, y + 3, false);
                line(*this, x + 5, y + 10, x + 12, y + 10, false);
                line(*this, x + 5, y + 3, x + 5, y + 10, false);
                line(*this, x + 12, y + 3, x + 12, y + 10, false);
                for (int stripe_y = 3; stripe_y <= 9; stripe_y += 2)
                {
                    line(*this, x + 16, y + stripe_y,
                         x + 25, y + stripe_y, false);
                    line(*this, x + 58, y + stripe_y,
                         x + 79, y + stripe_y, false);
                }
                draw_text(*this, x + 28, y + 3, "ERROR", 1, false);
                draw_atlas_sprite(
                    *this, screensaver_error_atlas,
                    variant, 40, 40, x + 3, y + 13, 40, 40,
                    false, false);
                line(*this, x + 53, y + 36, x + 77, y + 36, false);
                line(*this, x + 53, y + 48, x + 77, y + 48, false);
                line(*this, x + 53, y + 36, x + 53, y + 48, false);
                line(*this, x + 77, y + 36, x + 77, y + 48, false);
                draw_text(*this, x + 59, y + 39, "OK", 1, false);
            }
            else
            {
                fill_rect(*this, x, y, 84, 54);
                fill_rect(*this, x + 2, y + 2, 80, 50, false);
                line(*this, x + 2, y + 12, x + 81, y + 12);
                fill_rect(*this, x + 8, y + 20, 10, 10);
                draw_text(*this, x + 25, y + 22, "ERROR", 1);
                fill_rect(*this, x + 49, y + 39, 25, 9);
                fill_rect(*this, x + 50, y + 40, 23, 7, false);
                draw_text(*this, x + 55, y + 40, "OK", 1);
            }
        }
        break;

    case ScreensaverMode::RainyWindow:
        for (uint8_t i = 0; i < 48; ++i)
        {
            screensaver_y[i] += 2 + (i % 5);
            if (screensaver_y[i] >= kExtHeight)
            {
                screensaver_y[i] = -random(1, 80);
                screensaver_x[i] = random(0, kExtWidth);
            }
            line(*this, screensaver_x[i], screensaver_y[i],
                 screensaver_x[i] - 2, screensaver_y[i] + 7);
        }
        draw_time(*this, snapshot.current, 67, 91, 5);
        break;

    case ScreensaverMode::Fireworks:
    {
        static constexpr uint8_t kBurstFrames = 72;
        for (uint8_t burst = 0; burst < 3; ++burst)
        {
            const uint32_t shifted = frame + burst * 24;
            const uint8_t phase = shifted % kBurstFrames;
            const uint32_t sequence = shifted / kBurstFrames;
            const int center_x = 38 +
                static_cast<int>((sequence * 83 + burst * 67) % 228);
            const int center_y = 28 +
                static_cast<int>((sequence * 47 + burst * 31) % 104);

            if (phase < 14)
            {
                const int rocket_y = kExtHeight - 1 -
                    (kExtHeight - center_y) * phase / 14;
                line(*this, center_x, rocket_y,
                     center_x, min(kExtHeight - 1, rocket_y + 7));
                continue;
            }

            const uint8_t age = phase - 14;
            if (age >= 48)
                continue;
            const int radius = 2 + age;
            const uint8_t particle_step = age < 28 ? 1 : 2;
            for (uint8_t i = 0; i < 32; i += particle_step)
            {
                const float angle = i * 6.2831853f / 32.0f;
                const int gravity = age * age / 180;
                const int x = center_x + cosf(angle) * radius;
                const int y = center_y + sinf(angle) * radius + gravity;
                put_pixel(*this, x, y);
                if (age > 4 && (i & 1) == 0)
                    put_pixel(*this,
                              center_x + cosf(angle) * (radius - 3),
                              center_y + sinf(angle) * (radius - 3) +
                                  (age - 3) * (age - 3) / 180);
            }
        }
        break;
    }

    case ScreensaverMode::PhotoSlideshow:
        if (millis() >= screensaver_photo_due_ms)
        {
            screensaver_photo_loaded = load_next_photo(*this);
            screensaver_photo_transition = 0;
            screensaver_photo_due_ms = millis() + 10000;
        }
        if (screensaver_photo_transition < 16 && (frame & 1) == 0)
            ++screensaver_photo_transition;
        if (!screensaver_photo_loaded)
        {
            lv_obj_clear_flag(screensaver_canvas, LV_OBJ_FLAG_HIDDEN);
            draw_photo(*this);
        }
        else
            draw_photo(*this);
        break;

    default:
        return false;
    }

    if (screensaver_active_mode != ScreensaverMode::PhotoSlideshow ||
        !screensaver_photo_loaded)
        lv_obj_invalidate(screensaver_canvas);
    return true;
}

#endif
