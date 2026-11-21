#include <pebble.h>
#include <stdint.h>
#include <stdlib.h>
#include "glyphs.h"

#define SCREEN_W PBL_DISPLAY_WIDTH
#define SCREEN_H PBL_DISPLAY_HEIGHT

// Layout per platform. All Y values are box tops; the generated glyph tables
// (glyphs.h) hold the crop offsets inside each box. TOP_MARGIN is the distance
// of the weekday / AM-PM texts from the screen side when both are shown.
#if defined(PBL_PLATFORM_BASALT)
  #define HAS_HR_SENSOR 0
  #define CORNER_R   12
  #define TOP_Y      3
  #define TOP_MARGIN 16
  #define TIME_Y     16
  #define DATE_Y     57
  #define BOARD_Y    83
  #define BATT_Y     144
#elif defined(PBL_PLATFORM_CHALK)
  // Pebble Time Round (180x180 round, no heart-rate sensor)
  #define HAS_HR_SENSOR 0
  #define CORNER_R   0
  #define TOP_Y      14
  #define TOP_MARGIN 50
  #define TIME_Y     30
  #define DATE_Y     72
  #define BOARD_Y    92
  #define BATT_Y     147
#elif defined(PBL_PLATFORM_GABBRO)
  // Pebble Round 2 (260x260 round, no heart-rate sensor)
  #define HAS_HR_SENSOR 0
  #define CORNER_R   0
  #define TOP_Y      20
  #define TOP_MARGIN 72
  #define TIME_Y     46
  #define DATE_Y     108
  #define BOARD_Y    136
  #define BATT_Y     214
#else
  // Pebble Time 2 (emery, 200x228, heart-rate sensor)
  #define HAS_HR_SENSOR 1
  #define CORNER_R   16
  #define TOP_Y      6
  #define TOP_MARGIN 24
  #define TIME_Y     24
  #define DATE_Y     80
  #define BOARD_Y    110
  #define BATT_Y     192
#endif

#define BOARD_X ((SCREEN_W - BOARD_W) / 2)

#define SETTINGS_KEY 1
#define COLOR_STATE_KEY 2
#define SETTINGS_VERSION 1
#define SECOND_BOARD_TIMEOUT_MS 20000
#define HR_SHAKE_DURATION_MS 45000
#define HR_CONFIRMATION_TOLERANCE_BPM 5
#define HR_SHAKE_RESULT_HOLD_SEC 60

// Background color selection, same values as Blokk (without "time of day").
typedef enum {
  BG_OPTION_FIX = 0,
  BG_OPTION_MINUTE = 1,
  BG_OPTION_HOUR = 2,
  BG_OPTION_SHAKE = 3,
  BG_OPTION_DAY = 4
} BgOption;

typedef enum {
  PAINT_RAINBOW = 0,  // random color for every letter
  PAINT_ONE = 1,      // one random color for the whole text
  PAINT_FIXED = 2     // one fixed color
} PaintMode;

typedef enum {
  SLOT_NONE = 0,
  SLOT_HR = 1,
  SLOT_STEPS = 2,
  SLOT_ACTIVE = 3,
  SLOT_DISTANCE = 4,
  SLOT_CALORIES = 5,
  SLOT_SLEEP = 6,
  SLOT_VIBE = 7  // bell "on", or crossed bell "off" while Quiet Time is active
} SlotType;

// Element ids for the random color streams.
enum { EL_BG = 1, EL_BOARD, EL_WDAY, EL_AMPM, EL_TIME, EL_DATE, EL_BATT, EL_VALUE };

typedef struct {
  int version;
  int bg_option;
  int bg_color;      // GColor8 argb
  int text_mode;     // PaintMode for weekday, AM/PM, time, date and battery %
  int text_color;
  int board_mode;    // 0 = random, 1 = fixed
  int board_color;
  int value_mode;    // 0 = random, 1 = fixed
  int value_color;
  int slots[6];      // 0..2 main board (left, middle, right), 3..5 second board
  int second_board;  // 0/1: wrist flick shows the second board
  int hr_shake;      // 0/1: wrist flick starts an on-demand heart-rate measurement
  int tap_vibe;      // 0/1
  int vibe_in_quiet; // 0/1
  int hour_mode;     // 0 = watch, 1 = 24h, 2 = 12h
  int show_ampm;     // 0/1
  int dist_unit;     // 0 = watch setting, 1 = km, 2 = miles
} Settings;

typedef struct {
  uint32_t seed;
  int last_day;
  int last_hour;
} ColorState;

static Settings s_settings = {
  .version = SETTINGS_VERSION,
  .bg_option = BG_OPTION_MINUTE,
  .bg_color = GColorPictonBlueARGB8,
  .text_mode = PAINT_RAINBOW,
  .text_color = GColorIcterineARGB8,
  .board_mode = 0,
  .board_color = GColorRajahARGB8,
  .value_mode = 0,
  .value_color = GColorDukeBlueARGB8,
#if HAS_HR_SENSOR
  .slots = { SLOT_HR, SLOT_STEPS, SLOT_VIBE, SLOT_DISTANCE, SLOT_CALORIES, SLOT_SLEEP },
#else
  // No heart-rate sensor: active minutes take the heart-rate slot.
  .slots = { SLOT_ACTIVE, SLOT_STEPS, SLOT_VIBE, SLOT_DISTANCE, SLOT_CALORIES, SLOT_SLEEP },
#endif
  .second_board = 1,
  .hr_shake = 0,
  .tap_vibe = 1,
  .vibe_in_quiet = 0,
  .hour_mode = 0,
  .show_ampm = 1,
  .dist_unit = 0
};

static ColorState s_color_state = { .seed = 1, .last_day = -1, .last_hour = -1 };

// Random color pool: the Pebble palette entries enabled in the Fitbit Kiddo
// color table (no black, white or the darkest shades). Index = 0b00rrggbb.
static const uint8_t RANDOM_POOL[] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 19, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
  54, 55, 56, 57, 58, 59, 60, 61, 62
};
#define POOL_SIZE ((int)sizeof(RANDOM_POOL))

// Fixed icon colors from the Fitbit source (palette indices).
static const uint8_t ICON_COLOR[ICON_COUNT] = {
  48,  // ICON_HR: Red (Light Gray without a reading)
  11,  // ICON_STEPS: Vivid Cerulean
  29,  // ICON_ACTIVE: Screamin' Green
  39,  // ICON_DISTANCE: Lavender Indigo
  52,  // ICON_CALORIES: Orange
  60,  // ICON_SLEEP: Yellow (the Fitbit floors color)
  14,  // ICON_BELL: Medium Spring Green
  48   // ICON_BELLOFF: Red
};

// Battery icon colors by charge decade, and while charging (Fitbit drawBatt()).
static const uint8_t BATT_COLORS[11] = { 32, 48, 52, 56, 60, 44, 29, 28, 12, 8, 23 };

static const char *MONTH_NAMES[12] = {
  "january", "february", "march", "april", "may", "june",
  "july", "august", "september", "october", "november", "december"
};
static const char *WDAY_NAMES[7] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

static Window *s_window;
static Layer *s_canvas;
static GBitmap *s_sheet;
static GColor *s_palette;
static int s_fill_index = -1;

static struct tm s_now;
static bool s_second_board_shown = false;
static AppTimer *s_second_board_timer = NULL;
static bool s_health_subscribed = false;

static AppTimer *s_hr_measurement = NULL;
static AppTimer *s_hr_blink_timer = NULL;
static bool s_hr_dim = false;
#if HAS_HR_SENSOR
static HealthValue s_hr_raw_at_start = 0;
static HealthValue s_hr_candidate = 0;
static time_t s_hr_measured_at = 0;
#endif

static int s_battery = 100;
static bool s_battery_charging = false;
static int s_steps = -1;
static int s_hr = -1;
static int s_distance_m = -1;
static int s_calories = -1;
static int s_active_min = -1;
static int s_sleep_sec = -1;

// ---- Colors ------------------------------------------------------------------

static GColor color_index(int index) {
  return (GColor){ .argb = (uint8_t)(0xC0 | (index & 0x3F)) };
}

static GColor color_argb(int argb) {
  return (GColor){ .argb = (uint8_t)(argb | 0xC0) };
}

static uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

// Deterministic random color for (seed, element, index), skipping up to three
// colors (background, board, previous letter). The same seed gives the same
// colors on every redraw, so the face only changes when the seed changes.
static GColor random_color(int element, int index, GColor a, GColor b, GColor c) {
  uint32_t h = mix32(s_color_state.seed ^ ((uint32_t)element * 0x9E3779B9u) ^ ((uint32_t)(index + 1) * 0x85EBCA6Bu));
  int start = (int)(h % POOL_SIZE);
  for (int i = 0; i < POOL_SIZE; i++) {
    GColor col = color_index(RANDOM_POOL[(start + i) % POOL_SIZE]);
    if (!gcolor_equal(col, a) && !gcolor_equal(col, b) && !gcolor_equal(col, c)) return col;
  }
  return GColorWhite;
}

static GColor bg_color(void) {
  if (s_settings.bg_option == BG_OPTION_FIX) return color_argb(s_settings.bg_color);
  return random_color(EL_BG, 0, GColorClear, GColorClear, GColorClear);
}

static GColor board_color(void) {
  if (s_settings.board_mode == 1) return color_argb(s_settings.board_color);
  GColor bg = bg_color();
  return random_color(EL_BOARD, 0, bg, bg, bg);
}

typedef struct {
  uint8_t mode;
  uint8_t element;
  GColor fixed;
  GColor avoid_a;
  GColor avoid_b;
} Paint;

static GColor paint_color(const Paint *p, int i, GColor prev) {
  switch (p->mode) {
    case PAINT_FIXED: return p->fixed;
    case PAINT_ONE:   return random_color(p->element, 0, p->avoid_a, p->avoid_b, p->avoid_a);
    default:          return random_color(p->element, i, p->avoid_a, p->avoid_b, prev);
  }
}

static Paint text_paint(int element) {
  GColor bg = bg_color();
  return (Paint){
    .mode = (uint8_t)s_settings.text_mode, .element = (uint8_t)element,
    .fixed = color_argb(s_settings.text_color), .avoid_a = bg, .avoid_b = bg
  };
}

// New random colors: every minute, hour, day or on wrist flick, following the
// background option. With a fixed background the letters change every minute.
static void new_seed(void) {
  s_color_state.seed = mix32(((uint32_t)rand() << 16) ^ (uint32_t)rand() ^ (uint32_t)time(NULL));
}

static void update_color_seed(bool from_shake) {
  switch (s_settings.bg_option) {
    case BG_OPTION_SHAKE:
      if (from_shake) {
        new_seed();
        persist_write_data(COLOR_STATE_KEY, &s_color_state, sizeof(s_color_state));
      }
      break;
    case BG_OPTION_DAY:
      if (s_now.tm_mday != s_color_state.last_day) {
        s_color_state.last_day = s_now.tm_mday;
        new_seed();
        persist_write_data(COLOR_STATE_KEY, &s_color_state, sizeof(s_color_state));
      }
      break;
    case BG_OPTION_HOUR:
      if (s_now.tm_hour != s_color_state.last_hour) {
        s_color_state.last_hour = s_now.tm_hour;
        new_seed();
        persist_write_data(COLOR_STATE_KEY, &s_color_state, sizeof(s_color_state));
      }
      break;
    default:  // every minute, and fixed background
      if (!from_shake) new_seed();
      break;
  }
}

// ---- Glyph drawing -----------------------------------------------------------

static void draw_glyph(GContext *ctx, const KGlyph *g, int x, int y, GColor fill) {
  if (!s_sheet) return;
  if (s_fill_index >= 0) s_palette[s_fill_index] = fill;
  gbitmap_set_bounds(s_sheet, GRect(g->x, g->y, g->w, g->h));
  graphics_draw_bitmap_in_rect(ctx, s_sheet, GRect(x + g->dx, y + g->dy, g->w, g->h));
}

static const KGlyph *find_glyph(char c, bool big) {
  const char *set = big ? BIG_CHARS : SMALL_CHARS;
  for (int i = 0; set[i]; i++) {
    if (set[i] == c) return big ? &GL_BIG[i] : &GL_SMALL[i];
  }
  return NULL;
}

static int text_width(const char *s, bool big) {
  int w16 = 0;
  for (; *s; s++) {
    if (*s == ' ') {
      w16 += big ? 0 : SMALL_SPACE16;
      continue;
    }
    const KGlyph *g = find_glyph(*s, big);
    if (g) w16 += g->adv16;
  }
  w16 -= big ? BIG_SP16 : SMALL_SP16;
  return w16 > 0 ? (w16 + 8) >> 4 : 0;
}

typedef enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 } Align;

static int __attribute__((noinline)) draw_text(GContext *ctx, const char *s, bool big, int x, int y,
                                               Align align, const Paint *paint) {
  int width = text_width(s, big);
  if (align == ALIGN_CENTER) x -= width / 2;
  else if (align == ALIGN_RIGHT) x -= width;
  int pen16 = x << 4;
  GColor prev = GColorClear;
  for (int i = 0; s[i]; i++) {
    if (s[i] == ' ') {
      pen16 += big ? 0 : SMALL_SPACE16;
      continue;
    }
    const KGlyph *g = find_glyph(s[i], big);
    if (!g) continue;
    GColor col = paint_color(paint, i, prev);
    prev = col;
    draw_glyph(ctx, g, (pen16 + 8) >> 4, y, col);
    pen16 += g->adv16;
  }
  return width;
}

// ---- Heart rate (shared on-demand measurement logic with Blokk) --------------

static bool hr_slot_used(void) {
#if HAS_HR_SENSOR
  for (int i = 0; i < 6; i++) {
    if (s_settings.slots[i] == SLOT_HR) return true;
  }
#endif
  return false;
}

static void restore_system_hr(void) {
#if defined(PBL_HEALTH) && HAS_HR_SENSOR
  if (!health_service_set_heart_rate_sample_period(0)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Could not clear HR sampling request");
  }
#endif
}

static void stop_hr_blink(void) {
  if (s_hr_blink_timer) {
    app_timer_cancel(s_hr_blink_timer);
    s_hr_blink_timer = NULL;
  }
  s_hr_dim = false;
}

#if HAS_HR_SENSOR
static void blink_hr_icon(void *context) {
  s_hr_blink_timer = NULL;
  if (!s_hr_measurement) return;
  s_hr_dim = !s_hr_dim;
  layer_mark_dirty(s_canvas);
  s_hr_blink_timer = app_timer_register(500, blink_hr_icon, NULL);
  if (!s_hr_blink_timer) s_hr_dim = false;
}
#endif

static void cancel_hr_measurement(void) {
  if (!s_hr_measurement) return;
  app_timer_cancel(s_hr_measurement);
  s_hr_measurement = NULL;
  stop_hr_blink();
  restore_system_hr();
}

static void refresh_heart_rate(void) {
#if defined(PBL_HEALTH) && HAS_HR_SENSOR
  time_t now = time(NULL);
  if (s_hr_measurement || (s_hr_measured_at && (now - s_hr_measured_at) < HR_SHAKE_RESULT_HOLD_SEC)) {
    return;
  }
  const bool hr_available =
    (health_service_metric_accessible(HealthMetricHeartRateBPM, now, now) & HealthServiceAccessibilityMaskAvailable) != 0;
  HealthValue filtered = hr_available ? health_service_peek_current_value(HealthMetricHeartRateBPM) : 0;
  s_hr = (filtered > 0) ? (int)filtered : -1;
#else
  s_hr = -1;
#endif
}

#if HAS_HR_SENSOR
static void hr_measurement_ended(void *context) {
  s_hr_measurement = NULL;
  stop_hr_blink();
  restore_system_hr();
  refresh_heart_rate();
  APP_LOG(APP_LOG_LEVEL_INFO, "Shake HR measurement timed out");
  layer_mark_dirty(s_canvas);
}
#endif

static bool start_hr_measurement(void) {
#if defined(PBL_HEALTH) && HAS_HR_SENSOR
  if (!s_settings.hr_shake || !hr_slot_used() || s_hr_measurement || !s_health_subscribed) return false;
  s_hr_raw_at_start = health_service_peek_current_value(HealthMetricHeartRateRawBPM);
  s_hr_candidate = 0;
  if (!health_service_set_heart_rate_sample_period(1)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Shake HR sampling request declined");
    return false;
  }
  s_hr_measurement = app_timer_register(HR_SHAKE_DURATION_MS, hr_measurement_ended, NULL);
  if (!s_hr_measurement) {
    restore_system_hr();
    return false;
  }
  blink_hr_icon(NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "Shake HR measurement started");
  return true;
#else
  return false;
#endif
}

// ---- Health ------------------------------------------------------------------

static void refresh_health(void) {
#if defined(PBL_HEALTH)
  const time_t start = time_start_of_today();
  const time_t now = time(NULL);
  if (health_service_metric_accessible(HealthMetricStepCount, start, now) & HealthServiceAccessibilityMaskAvailable) {
    s_steps = (int)health_service_sum_today(HealthMetricStepCount);
  }
  if (health_service_metric_accessible(HealthMetricWalkedDistanceMeters, start, now) & HealthServiceAccessibilityMaskAvailable) {
    s_distance_m = (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
  }
  if (health_service_metric_accessible(HealthMetricActiveSeconds, start, now) & HealthServiceAccessibilityMaskAvailable) {
    s_active_min = (int)(health_service_sum_today(HealthMetricActiveSeconds) / 60);
  }
  if (health_service_metric_accessible(HealthMetricActiveKCalories, start, now) & HealthServiceAccessibilityMaskAvailable) {
    s_calories = (int)(health_service_sum_today(HealthMetricActiveKCalories) +
                       health_service_sum_today(HealthMetricRestingKCalories));
  }
  if (health_service_metric_accessible(HealthMetricSleepSeconds, start, now) & HealthServiceAccessibilityMaskAvailable) {
    s_sleep_sec = (int)health_service_sum_today(HealthMetricSleepSeconds);
  }
  refresh_heart_rate();
#endif
}

static bool use_miles(void) {
  if (s_settings.dist_unit == 1) return false;
  if (s_settings.dist_unit == 2) return true;
#if defined(PBL_HEALTH)
  return health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters) == MeasurementSystemImperial;
#else
  return false;
#endif
}

static int clamp_value(int v, int max) {
  if (v < 0) return 0;
  return v > max ? max : v;
}

// Icon and value text of a board slot. Returns false for an empty slot.
static bool slot_content(int type, int *icon, GColor *icon_color, char *val, size_t val_sz) {
  *icon_color = color_index(ICON_COLOR[0]);
  switch (type) {
    case SLOT_HR:
#if HAS_HR_SENSOR
      *icon = ICON_HR;
      *icon_color = (s_hr_measurement && s_hr_dim) ? GColorDarkGray
                  : (s_hr > 0 ? color_index(ICON_COLOR[ICON_HR]) : GColorLightGray);
      if (s_hr > 0) snprintf(val, val_sz, "%d", s_hr);
      else snprintf(val, val_sz, "...");
      return true;
#else
      return false;
#endif
    case SLOT_STEPS:
      *icon = ICON_STEPS;
      snprintf(val, val_sz, "%d", clamp_value(s_steps, 99999));
      break;
    case SLOT_ACTIVE:
      *icon = ICON_ACTIVE;
      snprintf(val, val_sz, "%d", clamp_value(s_active_min, 9999));
      break;
    case SLOT_DISTANCE: {
      *icon = ICON_DISTANCE;
      int m = s_distance_m < 0 ? 0 : s_distance_m;
      // Fitbit: two decimals below 10, one decimal above.
      int hundredths = use_miles() ? (int)(((int64_t)m * 1000 + 8046) / 16093) : (m + 5) / 10;
      if (hundredths >= 1000) {
        int tenths = (hundredths + 5) / 10;
        if (tenths >= 10000) snprintf(val, val_sz, "%d", tenths / 10);
        else snprintf(val, val_sz, "%d.%d", tenths / 10, tenths % 10);
      } else {
        snprintf(val, val_sz, "%d.%02d", hundredths / 100, hundredths % 100);
      }
      break;
    }
    case SLOT_CALORIES:
      *icon = ICON_CALORIES;
      snprintf(val, val_sz, "%d", clamp_value(s_calories, 99999));
      break;
    case SLOT_SLEEP: {
      *icon = ICON_SLEEP;
      int tenths = s_sleep_sec < 0 ? 0 : (s_sleep_sec + 180) / 360;
      snprintf(val, val_sz, "%d.%d", tenths / 10, tenths % 10);
      break;
    }
    case SLOT_VIBE: {
      bool quiet = quiet_time_is_active();
      *icon = quiet ? ICON_BELLOFF : ICON_BELL;
      snprintf(val, val_sz, "%s", quiet ? "off" : "on");
      break;
    }
    default:
      return false;
  }
  *icon_color = color_index(ICON_COLOR[*icon]);
  return true;
}

// ---- Drawing -----------------------------------------------------------------

static bool is_24h_mode(void) {
  if (s_settings.hour_mode == 1) return true;
  if (s_settings.hour_mode == 2) return false;
  return clock_is_24h_style();
}

static char s_text[24];

static void __attribute__((noinline)) draw_header(GContext *ctx) {
  int hour = s_now.tm_hour;
  const bool is_24 = is_24h_mode();
  const bool show_ampm = !is_24 && s_settings.show_ampm;
  if (!is_24) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  int wday = (s_now.tm_wday >= 0 && s_now.tm_wday <= 6) ? s_now.tm_wday : 0;
  Paint p = text_paint(EL_WDAY);
  if (show_ampm) {
    // Weekday left and AM/PM right (pulled in on round displays).
    draw_text(ctx, WDAY_NAMES[wday], false, TOP_MARGIN, TOP_Y, ALIGN_LEFT, &p);
    Paint pa = text_paint(EL_AMPM);
    draw_text(ctx, s_now.tm_hour < 12 ? "am" : "pm", false, SCREEN_W - TOP_MARGIN, TOP_Y, ALIGN_RIGHT, &pa);
  } else {
    draw_text(ctx, WDAY_NAMES[wday], false, SCREEN_W / 2, TOP_Y, ALIGN_CENTER, &p);
  }

  // Fitbit Kiddo: hours are never zero padded.
  snprintf(s_text, sizeof(s_text), "%d:%02d", hour, s_now.tm_min);
  p = text_paint(EL_TIME);
  draw_text(ctx, s_text, true, SCREEN_W / 2, TIME_Y, ALIGN_CENTER, &p);

  int mon = (s_now.tm_mon >= 0 && s_now.tm_mon <= 11) ? s_now.tm_mon : 0;
  snprintf(s_text, sizeof(s_text), "%s %d", MONTH_NAMES[mon], s_now.tm_mday);
  p = text_paint(EL_DATE);
  draw_text(ctx, s_text, false, SCREEN_W / 2, DATE_Y, ALIGN_CENTER, &p);
}

static void __attribute__((noinline)) draw_board(GContext *ctx) {
  const GColor board = board_color();
  draw_glyph(ctx, &GL_MISC[MISC_BOARD], BOARD_X, BOARD_Y, board);

  // Nails (Fitbit positions in the 300x100 board), light gray.
  static const int16_t nail_xy[4][2] = { { 7, 8 }, { 272, 5 }, { 7, 68 }, { 272, 65 } };
  for (int i = 0; i < 4; i++) {
    draw_glyph(ctx, &GL_MISC[MISC_NAIL], BOARD_X + nail_xy[i][0] * BOARD_W / 300,
               BOARD_Y + nail_xy[i][1] * BOARD_H / 100, GColorLightGray);
  }

  static const int16_t col_x[3] = { 60, 150, 240 };
  const int base = s_second_board_shown ? 3 : 0;
  for (int i = 0; i < 3; i++) {
    int icon = 0;
    GColor icon_color;
    if (!slot_content(s_settings.slots[base + i], &icon, &icon_color, s_text, sizeof(s_text))) continue;
    const int cx = BOARD_X + col_x[i] * BOARD_W / 300;
    // Side slots: icon above the value; middle slot: value above the icon.
    const int icon_y = BOARD_Y + (i == 1 ? 48 : 18) * BOARD_H / 100;
    const int val_y = BOARD_Y + (i == 1 ? 20 : 55) * BOARD_H / 100;
    draw_glyph(ctx, &GL_ICON[icon], cx - ICON_BOX / 2, icon_y, icon_color);
    Paint p = {
      .mode = s_settings.value_mode == 1 ? PAINT_FIXED : PAINT_ONE,
      .element = (uint8_t)(EL_VALUE + base + i),
      .fixed = color_argb(s_settings.value_color),
      .avoid_a = board,
      .avoid_b = icon_color
    };
    draw_text(ctx, s_text, false, cx, val_y, ALIGN_CENTER, &p);
  }
}

static void __attribute__((noinline)) draw_battery(GContext *ctx) {
  int level = s_battery / 10;
  if (level > 9) level = 9;
  if (level < 0) level = 0;
  const GColor icon = color_index(BATT_COLORS[s_battery_charging ? 10 : level]);
  draw_glyph(ctx, &GL_MISC[MISC_BATT], SCREEN_W / 2 - BATT_W, BATT_Y, icon);
  snprintf(s_text, sizeof(s_text), "%d", s_battery);
  Paint p = text_paint(EL_BATT);
  draw_text(ctx, s_text, false, SCREEN_W / 2 + 2, BATT_Y + (3 * SMALL_BOX + 12) / 25, ALIGN_LEFT, &p);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = GRect(0, 0, SCREEN_W, SCREEN_H);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, bg_color());
  graphics_fill_rect(ctx, bounds, CORNER_R, CORNER_R > 0 ? GCornersAll : GCornerNone);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  draw_header(ctx);
  draw_board(ctx);
  draw_battery(ctx);
}

// ---- Events ------------------------------------------------------------------

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  update_color_seed(false);
  refresh_health();
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  s_battery = state.charge_percent;
  s_battery_charging = state.is_charging || state.is_plugged;
  layer_mark_dirty(s_canvas);
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
#if HAS_HR_SENSOR
  if (event == HealthEventHeartRateUpdate && s_hr_measurement) {
    const HealthValue raw = health_service_peek_current_value(HealthMetricHeartRateRawBPM);
    if (raw <= 0 || raw == s_hr_raw_at_start) {
      s_hr_candidate = 0;
    } else if (s_hr_candidate > 0 && abs(raw - s_hr_candidate) <= HR_CONFIRMATION_TOLERANCE_BPM) {
      s_hr = (int)raw;
      s_hr_measured_at = time(NULL);
      cancel_hr_measurement();
      APP_LOG(APP_LOG_LEVEL_INFO, "Shake HR measurement received a fresh result: %d", s_hr);
    } else {
      s_hr_candidate = raw;
    }
  }
#endif
  refresh_health();
  layer_mark_dirty(s_canvas);
}
#endif

static void hide_second_board(void) {
  if (s_second_board_timer) {
    app_timer_cancel(s_second_board_timer);
    s_second_board_timer = NULL;
  }
  s_second_board_shown = false;
}

static void second_board_timeout(void *context) {
  s_second_board_timer = NULL;
  if (s_second_board_shown) {
    s_second_board_shown = false;
    layer_mark_dirty(s_canvas);
  }
}

static void vibe_tap(void) {
  if (!s_settings.tap_vibe) return;
  if (quiet_time_is_active() && !s_settings.vibe_in_quiet) return;
  static const uint32_t segments[] = { 60 };
  VibePattern pat = { .durations = segments, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  bool did_action = false;

  if (s_settings.bg_option == BG_OPTION_SHAKE) {
    update_color_seed(true);
    did_action = true;
  }

  if (s_settings.second_board) {
    const bool show = !s_second_board_shown;
    hide_second_board();
    if (show) {
      s_second_board_shown = true;
      s_second_board_timer = app_timer_register(SECOND_BOARD_TIMEOUT_MS, second_board_timeout, NULL);
    }
    did_action = true;
  }

  // The measurement keeps running while the second board is shown; the fresh
  // value is held for a minute, so it is there when the main board returns.
  if (start_hr_measurement()) did_action = true;

  if (did_action) {
    vibe_tap();
    layer_mark_dirty(s_canvas);
  }
}

static bool read_int(DictionaryIterator *iter, uint32_t key, int *dest) {
  Tuple *t = dict_find(iter, key);
  if (!t) return false;
  int v = (int)t->value->int32;
  if (*dest == v) return false;
  *dest = v;
  return true;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  bool changed = false;
  const int old_bg_option = s_settings.bg_option;

  changed |= read_int(iter, MESSAGE_KEY_BG_OPTION, &s_settings.bg_option);
  changed |= read_int(iter, MESSAGE_KEY_BG_COLOR, &s_settings.bg_color);
  changed |= read_int(iter, MESSAGE_KEY_TEXT_MODE, &s_settings.text_mode);
  changed |= read_int(iter, MESSAGE_KEY_TEXT_COLOR, &s_settings.text_color);
  changed |= read_int(iter, MESSAGE_KEY_BOARD_MODE, &s_settings.board_mode);
  changed |= read_int(iter, MESSAGE_KEY_BOARD_COLOR, &s_settings.board_color);
  changed |= read_int(iter, MESSAGE_KEY_VALUE_MODE, &s_settings.value_mode);
  changed |= read_int(iter, MESSAGE_KEY_VALUE_COLOR, &s_settings.value_color);
  changed |= read_int(iter, MESSAGE_KEY_SLOT1, &s_settings.slots[0]);
  changed |= read_int(iter, MESSAGE_KEY_SLOT2, &s_settings.slots[1]);
  changed |= read_int(iter, MESSAGE_KEY_SLOT3, &s_settings.slots[2]);
  changed |= read_int(iter, MESSAGE_KEY_SLOT4, &s_settings.slots[3]);
  changed |= read_int(iter, MESSAGE_KEY_SLOT5, &s_settings.slots[4]);
  changed |= read_int(iter, MESSAGE_KEY_SLOT6, &s_settings.slots[5]);
  changed |= read_int(iter, MESSAGE_KEY_SECOND_BOARD, &s_settings.second_board);
  changed |= read_int(iter, MESSAGE_KEY_HR_SHAKE, &s_settings.hr_shake);
  changed |= read_int(iter, MESSAGE_KEY_TAP_VIBE, &s_settings.tap_vibe);
  changed |= read_int(iter, MESSAGE_KEY_VIBE_IN_QUIET, &s_settings.vibe_in_quiet);
  changed |= read_int(iter, MESSAGE_KEY_HOUR_MODE, &s_settings.hour_mode);
  changed |= read_int(iter, MESSAGE_KEY_SHOW_AMPM, &s_settings.show_ampm);
  changed |= read_int(iter, MESSAGE_KEY_DIST_UNIT, &s_settings.dist_unit);

  if (!s_settings.second_board && s_second_board_shown) hide_second_board();
  if (!s_settings.hr_shake || !hr_slot_used()) {
    cancel_hr_measurement();
    refresh_heart_rate();
  }
  if (s_settings.bg_option != old_bg_option) {
    // Start the new mode with fresh colors.
    s_color_state.last_day = s_now.tm_mday;
    s_color_state.last_hour = s_now.tm_hour;
    new_seed();
    persist_write_data(COLOR_STATE_KEY, &s_color_state, sizeof(s_color_state));
  }
  if (changed) persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));

  Tuple *saved = dict_find(iter, MESSAGE_KEY_SAVED);
  if (saved && saved->value->int32) light_enable_interaction();

  refresh_health();
  layer_mark_dirty(s_canvas);
}

// Quiet Time has no change event: besides the minute tick, refresh the bell
// as soon as the watchface regains focus (e.g. after a notification).
static void app_focus_handler(bool in_focus) {
  if (in_focus && s_canvas) layer_mark_dirty(s_canvas);
}

static void load_sheet(void) {
  s_sheet = gbitmap_create_with_resource(KIDDO_SHEET);
  if (!s_sheet) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Glyph sheet could not be loaded");
    return;
  }
  s_palette = gbitmap_get_palette(s_sheet);
  const GBitmapFormat format = gbitmap_get_format(s_sheet);
  int count = 0;
  switch (format) {
    case GBitmapFormat1BitPalette: count = 2; break;
    case GBitmapFormat2BitPalette: count = 4; break;
    case GBitmapFormat4BitPalette: count = 16; break;
    default: break;
  }
  // The fill is the white palette entry; it is recolored before every glyph.
  for (int i = 0; s_palette && i < count; i++) {
    if (s_palette[i].argb == GColorWhiteARGB8) s_fill_index = i;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Glyph sheet format %d, fill index %d", (int)format, s_fill_index);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  load_sheet();
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  gbitmap_destroy(s_sheet);
  s_sheet = NULL;
  s_palette = NULL;
}

static void init(void) {
  time_t now = time(NULL);
  srand((unsigned int)now);
  struct tm *t = localtime(&now);
  if (t) s_now = *t;

  Settings loaded;
  if (persist_exists(SETTINGS_KEY) &&
      persist_read_data(SETTINGS_KEY, &loaded, sizeof(loaded)) == (int)sizeof(loaded) &&
      loaded.version == SETTINGS_VERSION) {
    s_settings = loaded;
  }
  ColorState cs;
  if (persist_exists(COLOR_STATE_KEY) &&
      persist_read_data(COLOR_STATE_KEY, &cs, sizeof(cs)) == (int)sizeof(cs)) {
    s_color_state = cs;
  }
  if (s_settings.bg_option == BG_OPTION_MINUTE || s_settings.bg_option == BG_OPTION_FIX ||
      !persist_exists(COLOR_STATE_KEY)) {
    new_seed();
  }
  update_color_seed(false);

  BatteryChargeState battery = battery_state_service_peek();
  s_battery = battery.charge_percent;
  s_battery_charging = battery.is_charging || battery.is_plugged;
  refresh_health();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  accel_tap_service_subscribe(tap_handler);
  app_focus_service_subscribe(app_focus_handler);
#if defined(PBL_HEALTH)
  s_health_subscribed = health_service_events_subscribe(health_handler, NULL);
#endif

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(384, 64);
}

static void deinit(void) {
  cancel_hr_measurement();
  hide_second_board();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  accel_tap_service_unsubscribe();
  app_focus_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
