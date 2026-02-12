/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Virtual LED strip driver for QEMU mode.
 *
 * The ESP-IDF led_strip component uses a vtable pattern:
 *   - led_strip_api.c defines led_strip_set_pixel(), led_strip_refresh(), etc.
 *     as thin dispatchers through function pointers on the led_strip_t struct.
 *   - led_strip_spi_dev.c defines led_strip_new_spi_device() which allocates
 *     a struct, sets up SPI hardware, and populates the vtable.
 *
 * We ONLY override led_strip_new_spi_device() (and led_strip_new_rmt_device()).
 * Our implementation returns a handle with our own vtable functions that store
 * pixel data in a RAM buffer and serialize it over UART2 on refresh().
 * The led_strip_api.c dispatch layer stays intact and calls through our
 * function pointers — no duplicate symbol conflicts.
 */

#include "sdkconfig.h"

#ifdef CONFIG_BADGE_QEMU_MODE

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "led_strip.h"
#include "led_strip_interface.h"
#include "qemu_viz_transport.h"

#define TAG "LED_STRIP_QEMU"

/* Maximum LEDs we support (largest badge variant is TRON with 77) */
#define LED_STRIP_LEN_MAX 128

/* Our custom strip object — extends led_strip_t with pixel buffer */
typedef struct {
    led_strip_t base;                     /* vtable — MUST be first member */
    uint8_t pixels[LED_STRIP_LEN_MAX][3]; /* RGB per pixel */
    uint16_t num_leds;
    bool transport_initialized;
} led_strip_qemu_obj_t;

/* Single static instance — only one LED strip on the badge */
static led_strip_qemu_obj_t s_strip;

/* ------------------------------------------------------------------ */
/* Vtable implementations (called via led_strip_api.c dispatch)        */
/* ------------------------------------------------------------------ */

static esp_err_t qemu_set_pixel(led_strip_t *strip, uint32_t index,
                                 uint32_t red, uint32_t green, uint32_t blue)
{
    led_strip_qemu_obj_t *qemu_strip = __containerof(strip, led_strip_qemu_obj_t, base);
    if (index >= qemu_strip->num_leds) {
        return ESP_ERR_INVALID_ARG;
    }
    qemu_strip->pixels[index][0] = (uint8_t)red;
    qemu_strip->pixels[index][1] = (uint8_t)green;
    qemu_strip->pixels[index][2] = (uint8_t)blue;
    return ESP_OK;
}

static esp_err_t qemu_set_pixel_rgbw(led_strip_t *strip, uint32_t index,
                                      uint32_t red, uint32_t green, uint32_t blue,
                                      uint32_t white)
{
    /* Ignore white channel — WS2812 doesn't have it */
    return qemu_set_pixel(strip, index, red, green, blue);
}

static esp_err_t qemu_refresh(led_strip_t *strip)
{
    led_strip_qemu_obj_t *qemu_strip = __containerof(strip, led_strip_qemu_obj_t, base);

    if (!qemu_strip->transport_initialized) {
        return ESP_OK;
    }

    /*
     * LED Frame Protocol (Firmware → UI):
     * Byte 0:     0xAA           (frame start marker)
     * Byte 1:     0x01           (message type: LED_FRAME)
     * Byte 2-3:   uint16_t       (number of LEDs, little-endian)
     * Byte 4+:    [R, G, B] × N  (3 bytes per LED, in strip order)
     * Byte last:  0x55           (frame end marker)
     */
    uint16_t num_leds = qemu_strip->num_leds;
    uint8_t header[4];
    header[0] = QEMU_VIZ_FRAME_START;
    header[1] = QEMU_VIZ_MSG_LED_FRAME;
    header[2] = (uint8_t)(num_leds & 0xFF);
    header[3] = (uint8_t)((num_leds >> 8) & 0xFF);

    uint8_t footer = QEMU_VIZ_FRAME_END;

    /* Lock TX to ensure header+pixels+footer are sent atomically */
    qemu_viz_tx_lock();
    qemu_viz_send(header, 4);
    qemu_viz_send((const uint8_t *)qemu_strip->pixels, num_leds * 3);
    qemu_viz_send(&footer, 1);
    qemu_viz_tx_unlock();

    return ESP_OK;
}

static esp_err_t qemu_clear(led_strip_t *strip)
{
    led_strip_qemu_obj_t *qemu_strip = __containerof(strip, led_strip_qemu_obj_t, base);
    memset(qemu_strip->pixels, 0, sizeof(qemu_strip->pixels));
    return ESP_OK;
}

static esp_err_t qemu_del(led_strip_t *strip)
{
    led_strip_qemu_obj_t *qemu_strip = __containerof(strip, led_strip_qemu_obj_t, base);
    ESP_LOGI(TAG, "QEMU: Deleting virtual LED strip");
    memset(qemu_strip, 0, sizeof(*qemu_strip));
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Public API overrides — replace the real SPI/RMT device constructors */
/* ------------------------------------------------------------------ */

esp_err_t led_strip_new_spi_device(const led_strip_config_t *led_config,
                                    const led_strip_spi_config_t *spi_config,
                                    led_strip_handle_t *ret_strip)
{
    ESP_LOGI(TAG, "QEMU: Creating virtual LED strip (SPI stub), %d LEDs",
             (int)led_config->max_leds);

    memset(&s_strip, 0, sizeof(s_strip));
    s_strip.num_leds = led_config->max_leds;
    if (s_strip.num_leds > LED_STRIP_LEN_MAX) {
        s_strip.num_leds = LED_STRIP_LEN_MAX;
    }

    /* Populate vtable — led_strip_api.c will dispatch through these */
    s_strip.base.set_pixel      = qemu_set_pixel;
    s_strip.base.set_pixel_rgbw = qemu_set_pixel_rgbw;
    s_strip.base.refresh        = qemu_refresh;
    s_strip.base.clear          = qemu_clear;
    s_strip.base.del            = qemu_del;

    /* Initialize the shared viz transport (UART2) */
    esp_err_t err = qemu_viz_transport_init();
    if (err == ESP_OK) {
        s_strip.transport_initialized = true;
    } else {
        ESP_LOGW(TAG, "Viz transport init failed (%s), LED frames won't be sent",
                 esp_err_to_name(err));
    }

    *ret_strip = &s_strip.base;
    return ESP_OK;
}

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *led_config,
                                    const led_strip_rmt_config_t *rmt_config,
                                    led_strip_handle_t *ret_strip)
{
    /* Redirect to our SPI stub — same behavior in QEMU */
    ESP_LOGI(TAG, "QEMU: Creating virtual LED strip (RMT stub), %d LEDs",
             (int)led_config->max_leds);
    led_strip_spi_config_t dummy = {0};
    return led_strip_new_spi_device(led_config, &dummy, ret_strip);
}

#endif /* CONFIG_BADGE_QEMU_MODE */
