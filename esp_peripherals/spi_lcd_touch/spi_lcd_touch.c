#include "spi_lcd_touch.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "lvgl.h"

static const char *TAG = "SPI_LCD_TOUCH";

#ifdef M5STACK_CORE_BASIC
#define CONFIG_LCD_CONTROLLER_ILI9488 0
#define CONFIG_LCD_CONTROLLER_ILI9341 0
#define CONFIG_LCD_CONTROLLER_ILI9342 1
#endif

#if CONFIG_LCD_CONTROLLER_ILI9341
#include "esp_lcd_ili9341.h"
#elif CONFIG_LCD_CONTROLLER_ILI9342
#include "esp_lcd_ili9342.h"
#elif CONFIG_LCD_CONTROLLER_ILI9488
#include "esp_lcd_ili9488.h"
#endif

#if CONFIG_LCD_TOUCH_ENABLED
#include "esp_lcd_touch_xpt2046.h"
#endif

#define LCD_HOST SPI2_HOST

#if CONFIG_LCD_CONTROLLER_ILI9341
#define TOUCH_HOST LCD_HOST

#define LCD_H_RES 240
#define LCD_V_RES 320

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define BITS_PER_PIXEL 16

#elif CONFIG_LCD_CONTROLLER_ILI9342
#define TOUCH_HOST LCD_HOST

#define LCD_H_RES 320
#define LCD_V_RES 240

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define BITS_PER_PIXEL 16

#elif CONFIG_LCD_CONTROLLER_ILI9488
#define TOUCH_HOST SPI3_HOST

#define LCD_H_RES 320
#define LCD_V_RES 480

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define BITS_PER_PIXEL 18
#endif

#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

#ifdef M5STACK_CORE_BASIC
#define LCD_CS_PIN 14
#define LCD_RST_PIN 33
#define LCD_DC_PIN 27
#define LCD_BL_PIN 32

// no touch capabilities on the Core Basic :(
#define CONFIG_LCD_TOUCH_ENABLED 0
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define LCD_CS_PIN 4
#define LCD_RST_PIN 13
#define LCD_DC_PIN 5
#define LCD_BL_PIN 2

#define TOUCH_CS_PIN 15
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define LCD_CS_PIN 10
#define LCD_RST_PIN 7
#define LCD_DC_PIN 6
#define LCD_BL_PIN 5

#define TOUCH_MOSI_PIN 39
#define TOUCH_CLK_PIN 38
#define TOUCH_MISO_PIN 40
#define TOUCH_CS_PIN 8
#endif

// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define LVGL_DRAW_BUF_LINES 20 // number of display lines in each draw buffer
// static const size_t LV_BUFFER_SIZE = LCD_H_RES * LVGL_DRAW_BUF_LINES;
#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ
#define LVGL_TASK_STACK_SIZE (8 * 1024)
#define LVGL_TASK_PRIORITY 2

// LVGL library is not thread-safe, this example will call LVGL APIs from
// different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *display = NULL;
static bool display_initialized = false;

#if CONFIG_LCD_CONTROLLER_ILI9341
static const bool mirror_x = true;
#else
static const bool mirror_x = false;
#endif

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_port_update_callback(lv_display_t *disp) {
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    switch (rotation) {
    case LV_DISPLAY_ROTATION_0:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, mirror_x, false);
        break;
    case LV_DISPLAY_ROTATION_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, mirror_x, true);
        break;
    case LV_DISPLAY_ROTATION_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, !mirror_x, true);
        break;
    case LV_DISPLAY_ROTATION_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, !mirror_x, false);
        break;
    }
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
    lvgl_port_update_callback(disp);

    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
#if CONFIG_LCD_CONTROLLER_ILI9341 || CONFIG_LCD_CONTROLLER_ILI9342
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) *
                                       (offsety2 + 1 - offsety1));
#endif
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1,
                              offsety2 + 1, px_map);
}

static void increase_lvgl_tick(void *arg) {
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void init_lvgl() {
    lv_init();

    // create a lvgl display
    display = lv_display_create(LCD_H_RES, LCD_V_RES);

    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least
    // 1/10 screen sized
    size_t draw_buffer_sz;
    draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);

    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf1);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the
    // display
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick, .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(
        TAG,
        "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };

    /* Register done callback */
    ESP_ERROR_CHECK(
        esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));
}

static void lvgl_port_task(void *arg) {
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, LVGL_TASK_MIN_DELAY_MS);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, LVGL_TASK_MAX_DELAY_MS);
        usleep(1000 * time_till_next_ms);
    }
}

void init_display() {
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = BITS_PER_PIXEL,
    };

#if CONFIG_LCD_CONTROLLER_ILI9341
    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
#elif CONFIG_LCD_CONTROLLER_ILI9342
    ESP_LOGI(TAG, "Install ILI9342 panel driver");
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ili9342(io_handle, &panel_config, &panel_handle));
#elif CONFIG_LCD_CONTROLLER_ILI9488
    ESP_LOGI(TAG, "Install ILI9488 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config,
                                              320 * 25, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
#if CONFIG_LCD_CONTROLLER_ILI9342
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));

    // user can flush pre-defined pattern to the screen before we turn on the
    // screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

#if CONFIG_LCD_TOUCH_ENABLED
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_handle_t touch_pad = lv_indev_get_user_data(indev);
    esp_lcd_touch_read_data(touch_pad);
    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(
        touch_pad, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        ESP_LOGW(TAG, "Touch pressed: x=%d, y=%d, count=%d", touchpad_x[0],
                 touchpad_y[0], touchpad_cnt);
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

#ifdef CONFIG_LCD_CONTROLLER_ILI9488
void init_touch_spi() {
    spi_bus_config_t touch_buscfg = {
        .sclk_io_num = TOUCH_CLK_PIN,
        .mosi_io_num = TOUCH_MOSI_PIN,
        .miso_io_num = TOUCH_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MISO |
                 SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM};
    ESP_ERROR_CHECK(
        spi_bus_initialize(TOUCH_HOST, &touch_buscfg, SPI_DMA_CH_AUTO));
}
#endif

#define CORRECTION_OFFSET 25

static void process_coords(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                           uint16_t *strength, uint8_t *point_num,
                           uint8_t max_point_num) {
    // dynamic offset corrections
    uint32_t y_coord = *y;
    uint32_t y_max_dist = tp->config.y_max / 2;
    if (y_coord > y_max_dist) {
        uint32_t y_dist_from_center = y_coord - y_max_dist;
        int32_t y_offset =
            (y_dist_from_center * CORRECTION_OFFSET) / y_max_dist;
        y_coord += y_offset;
    } else {
        uint32_t y_dist_from_center = y_max_dist - y_coord;
        int32_t y_offset =
            (y_dist_from_center * CORRECTION_OFFSET) / y_max_dist;
        if (y_coord >= y_offset) {
            y_coord -= y_offset;
        } else {
            y_coord = 0;
        }
    }

    uint32_t x_coord = *x;
    uint32_t x_max_dist = tp->config.x_max / 2;
    if (x_coord > x_max_dist) {
        uint32_t x_dist_from_center = x_coord - x_max_dist;
        int32_t x_offset =
            (x_dist_from_center * CORRECTION_OFFSET) / x_max_dist;
        x_coord += x_offset;
    } else {
        uint32_t x_dist_from_center = x_max_dist - x_coord;
        int32_t x_offset =
            (x_dist_from_center * CORRECTION_OFFSET) / x_max_dist;
        if (x_coord >= x_offset) {
            x_coord -= x_offset;
        } else {
            x_coord = 0;
        }
    }

    // clamp clamp
    if (x_coord > tp->config.x_max)
        x_coord = tp->config.x_max;
    if (y_coord > tp->config.y_max)
        y_coord = tp->config.y_max;

    *x = x_coord;
    *y = y_coord;
}

void init_touch() {
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config =
        ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_CS_PIN);
    // Attach the TOUCH to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = CONFIG_LCD_MIRROR_Y,
            },
        .process_coordinates = process_coords,
    };
    esp_lcd_touch_handle_t tp = NULL;

    ESP_LOGI(TAG, "Initialize touch controller XPT2046");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &tp));

    static lv_indev_t *indev;
    indev = lv_indev_create(); // Input device driver (Touch)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_user_data(indev, tp);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
}
#endif

void lcd_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                    .pin_bit_mask = 1ULL << LCD_BL_PIN};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Install panel IO");
    init_display();

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(LCD_BL_PIN, LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    init_lvgl();

#if CONFIG_LCD_ORIENTATION_LANDSCAPE_90
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
#elif CONFIG_LCD_ORIENTATION_LANDSCAPE_270
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);
#endif

#if CONFIG_LCD_TOUCH_ENABLED

#if CONFIG_LCD_CONTROLLER_ILI9488
    // for the ILI9488 it needs to be a separate bus...
    ESP_LOGI(TAG, "Initialize touch SPI bus");
    init_touch_spi();
#endif

    ESP_LOGI(TAG, "Initialize touch driver");
    init_touch();
#endif

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL,
                LVGL_TASK_PRIORITY, NULL);

    display_initialized = true;

    vTaskDelete(NULL);
}

_lock_t *spi_lcd_get_lvgl_lock(void) {
    if (!display_initialized) {
        return NULL;
    }

    return &lvgl_api_lock;
}

lv_display_t *spi_lcd_get_display(void) {
    if (!display_initialized) {
        return NULL;
    }

    return display;
}

bool is_display_initialized(void) { return display_initialized; }
