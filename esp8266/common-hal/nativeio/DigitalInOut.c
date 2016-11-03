/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "shared-bindings/nativeio/DigitalInOut.h"

digitalinout_result_t common_hal_nativeio_digitalinout_construct(
        nativeio_digitalinout_obj_t* self, const mcu_pin_obj_t* pin) {
    self->pin = pin;
    PIN_FUNC_SELECT(self->pin->peripheral, self->pin->gpio_function);
    return DIGITALINOUT_OK;
}

void common_hal_nativeio_digitalinout_deinit(nativeio_digitalinout_obj_t* self) {
    uint32_t pin_mask = 1 << self->pin->gpio_number;
    gpio_output_set(0x0, 0x0, 0x0, pin_mask);
}

void common_hal_nativeio_digitalinout_switch_to_input(
        nativeio_digitalinout_obj_t* self, enum digitalinout_pull_t pull) {
    self->output = false;

    if (self->pin->gpio_number == 16) {
        WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | 1);
        WRITE_PERI_REG(RTC_GPIO_CONF, READ_PERI_REG(RTC_GPIO_CONF) & ~1);
        WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & ~1)); // input
    } else {
        PIN_PULLUP_DIS(self->pin->peripheral);
        gpio_output_set(0, 0, 0, 1 << self->pin->gpio_number);
    }
    common_hal_nativeio_digitalinout_set_pull(self, pull);
}

void common_hal_nativeio_digitalinout_switch_to_output(
        nativeio_digitalinout_obj_t* self, bool value,
        enum digitalinout_drive_mode_t drive_mode) {
    self->output = true;
    self->open_drain = drive_mode == DRIVE_MODE_OPEN_DRAIN;
    if (self->pin->gpio_number == 16) {
        WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | 1);
        WRITE_PERI_REG(RTC_GPIO_CONF, READ_PERI_REG(RTC_GPIO_CONF) & ~1);
        WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & ~1) | 1); // output
    } else if (!self->open_drain) {
        gpio_output_set(0, 0, 1 << self->pin->gpio_function, 0);
        PIN_PULLUP_DIS(self->pin->peripheral);
    }
    common_hal_nativeio_digitalinout_set_value(self, value);
}

enum digitalinout_direction_t common_hal_nativeio_digitalinout_get_direction(
        nativeio_digitalinout_obj_t* self) {
    return self->output? DIRECTION_OUT : DIRECTION_IN;
}

void common_hal_nativeio_digitalinout_set_value(
        nativeio_digitalinout_obj_t* self, bool value) {
    if (value) {
        if (self->open_drain) {
            // Disable output.
            gpio_output_set(0, 0, 0, 1 << self->pin->gpio_number);
            PIN_PULLUP_EN(self->pin->peripheral);
        } else {
            // Set high
            gpio_output_set(1 << self->pin->gpio_number, 0, 0, 0);
        }
    } else {
        if (self->open_drain) {
            // Enable the output
            PIN_PULLUP_DIS(self->pin->peripheral);
            gpio_output_set(0, 0, 1 << self->pin->gpio_number, 0);
        }
        // Set low
        gpio_output_set(0, 1 << self->pin->gpio_number, 0, 0);
    }
}

// Register addresses taken from: https://github.com/esp8266/esp8266-wiki/wiki/gpio-registers
volatile uint32_t* PIN_DIR = (uint32_t *) 0x6000030C;
volatile uint32_t* PIN_OUT = (uint32_t *) 0x60000300;
bool common_hal_nativeio_digitalinout_get_value(
        nativeio_digitalinout_obj_t* self) {
    if (!self->output) {
        if (self->pin->gpio_number == 16) {
            return READ_PERI_REG(RTC_GPIO_IN_DATA) & 1;
        }
        return GPIO_INPUT_GET(self->pin->gpio_number);
    } else {
        uint32_t pin_mask = 1 << self->pin->gpio_number;
        if (self->open_drain && ((*PIN_DIR) & pin_mask) == 0) {
            return true;
        } else {
            return ((*PIN_OUT) & pin_mask) != 0;
        }
    }
}

void common_hal_nativeio_digitalinout_set_drive_mode(
        nativeio_digitalinout_obj_t* self,
        enum digitalinout_drive_mode_t drive_mode) {
    bool value = common_hal_nativeio_digitalinout_get_value(self);
    self->open_drain = drive_mode == DRIVE_MODE_OPEN_DRAIN;
    // True is implemented differently between modes so reset the value to make
    // sure its correct for the new mode.
    if (value) {
        common_hal_nativeio_digitalinout_set_value(self, value);
    }
}

enum digitalinout_drive_mode_t common_hal_nativeio_digitalinout_get_drive_mode(
        nativeio_digitalinout_obj_t* self) {
    if (self->open_drain) {
        return DRIVE_MODE_OPEN_DRAIN;
    } else {
        return DRIVE_MODE_PUSH_PULL;
    }
}

void common_hal_nativeio_digitalinout_set_pull(
        nativeio_digitalinout_obj_t* self, enum digitalinout_pull_t pull) {
    if (pull == PULL_DOWN) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,
            "ESP8266 does not support pull down."));
        return;
    }
    if (self->pin->gpio_number == 16) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,
            "Pin does not support pull."));
        return;
    }
    if (pull == PULL_NONE) {
        PIN_PULLUP_DIS(self->pin->peripheral);
    } else {
        PIN_PULLUP_EN(self->pin->peripheral);
    }
}

enum digitalinout_pull_t common_hal_nativeio_digitalinout_get_pull(
        nativeio_digitalinout_obj_t* self) {
    if (self->pin->gpio_number < 16 &&
        (READ_PERI_REG(self->pin->peripheral) & PERIPHS_IO_MUX_PULLUP) != 0) {
        return PULL_UP;
    }
    return PULL_NONE;
}
