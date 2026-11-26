#include "rg_system.h"
#include "rg_input.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef ESP_PLATFORM
#include <driver/gpio.h>
#include <driver/adc.h>
#include <driver/spi_master.h>
#define ADC_ATTEN_DB_11 3

#define TOUCH_CS_PIN   41
#define TOUCH_IRQ_PIN  42

static spi_device_handle_t touch_spi = NULL;

static void touch_init(void)
{
    gpio_set_direction(TOUCH_IRQ_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TOUCH_IRQ_PIN, GPIO_PULLUP_ONLY);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = TOUCH_CS_PIN,
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &touch_spi);
}

static uint16_t xpt2046_read_axis(uint8_t cmd)
{
    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0, 0, 0};
    spi_transaction_t t = {.length = 24, .tx_buffer = tx, .rx_buffer = rx};
    spi_device_polling_transmit(touch_spi, &t);
    return ((rx[1] << 8) | rx[2]) >> 3;
}

static uint32_t touch_get_scroll_keys(void)
{
    static int16_t last_y = -1;
    static bool was_touching = false;
    static bool did_drag = false;

    if (rg_system_get_app() && strcmp(rg_system_get_app()->name, "launcher") != 0)
        return 0;

    if (!touch_spi) touch_init();

    if (gpio_get_level(TOUCH_IRQ_PIN) == 1) {
        uint32_t tap_key = (was_touching && !did_drag) ? RG_KEY_A : 0;
        was_touching = false;
        did_drag = false;
        last_y = -1;
        return tap_key;
    }

    int16_t raw_y = xpt2046_read_axis(0x90);
    if (raw_y < 100 || raw_y > 4000) return 0;

    if (!was_touching) {
        was_touching = true;
        last_y = raw_y;
        return 0;
    }

    int16_t diff = raw_y - last_y;
    if (diff > 220) {
        last_y = raw_y;
        did_drag = true;
        return RG_KEY_UP;
    } else if (diff < -220) {
        last_y = raw_y;
        did_drag = true;
        return RG_KEY_DOWN;
    }
    return 0;
}
#else
#include <SDL2/SDL.h>
#endif

#if RG_BATTERY_DRIVER == 1
#include <esp_adc_cal.h>
static esp_adc_cal_characteristics_t adc_chars;
#endif

#ifdef RG_GAMEPAD_ADC_MAP
static rg_keymap_adc_t keymap_adc[] = RG_GAMEPAD_ADC_MAP;
#endif
#ifdef RG_GAMEPAD_GPIO_MAP
static rg_keymap_gpio_t keymap_gpio[] = RG_GAMEPAD_GPIO_MAP;
#endif
#ifdef RG_GAMEPAD_I2C_MAP
static rg_keymap_i2c_t keymap_i2c[] = RG_GAMEPAD_I2C_MAP;
#endif
#ifdef RG_GAMEPAD_KBD_MAP
static rg_keymap_kbd_t keymap_kbd[] = RG_GAMEPAD_KBD_MAP;
#endif
#ifdef RG_GAMEPAD_SERIAL_MAP
static rg_keymap_serial_t keymap_serial[] = RG_GAMEPAD_SERIAL_MAP;
#endif
#ifdef RG_GAMEPAD_VIRT_MAP
static rg_keymap_virt_t keymap_virt[] = RG_GAMEPAD_VIRT_MAP;
#endif
static bool input_task_running = false;
static uint32_t gamepad_state = -1;
static uint32_t gamepad_mapped = 0;
static rg_battery_t battery_state = {0};

#define UPDATE_GLOBAL_MAP(keymap)                 \
    for (size_t i = 0; i < RG_COUNT(keymap); ++i) \
        gamepad_mapped |= keymap[i].key;          \

#ifdef ESP_PLATFORM
static inline int adc_get_raw(adc_unit_t unit, adc_channel_t channel)
{
    if (unit == ADC_UNIT_1)
    {
        return adc1_get_raw(channel);
    }
    else if (unit == ADC_UNIT_2)
    {
        int adc_raw_value = -1;
        if (adc2_get_raw(channel, ADC_WIDTH_MAX - 1, &adc_raw_value) != ESP_OK)
            RG_LOGE("ADC2 reading failed, this can happen while wifi is active.");
        return adc_raw_value;
    }
    RG_LOGE("Invalid ADC unit %d", (int)unit);
    return -1;
}
#endif

bool rg_input_read_battery_raw(rg_battery_t *out)
{
    uint32_t raw_value = 0;
    bool present = true;
    bool charging = false;

#if RG_BATTERY_DRIVER == 1
    for (int i = 0; i < 4; ++i)
    {
        int value = adc_get_raw(RG_BATTERY_ADC_UNIT, RG_BATTERY_ADC_CHANNEL);
        if (value < 0)
            return false;
        raw_value += esp_adc_cal_raw_to_voltage(value, &adc_chars);
    }
    raw_value /= 4;
#elif RG_BATTERY_DRIVER == 2
    uint8_t data[5];
    if (!rg_i2c_read(0x20, -1, &data, 5))
        return false;
    raw_value = data[4];
    charging = data[4] == 255;
#else
    return false;
#endif

    if (!out)
        return true;

    *out = (rg_battery_t){
        .level = RG_MAX(0.f, RG_MIN(100.f, RG_BATTERY_CALC_PERCENT(raw_value))),
        .volts = RG_BATTERY_CALC_VOLTAGE(raw_value),
        .present = present,
        .charging = charging,
    };
    return true;
}

bool rg_input_read_gamepad_raw(uint32_t *out)
{
    uint32_t state = 0;

#if defined(RG_GAMEPAD_ADC_MAP)
    static int old_adc_values[RG_COUNT(keymap_adc)];
    for (size_t i = 0; i < RG_COUNT(keymap_adc); ++i)
    {
        const rg_keymap_adc_t *mapping = &keymap_adc[i];
        int value = adc_get_raw(mapping->unit, mapping->channel);
        if (value >= mapping->min && value <= mapping->max)
        {
            if (abs(old_adc_values[i] - value) < RG_GAMEPAD_ADC_FILTER_WINDOW)
                state |= mapping->key;
            old_adc_values[i] = value;
        }
    }
#endif

#if defined(RG_GAMEPAD_GPIO_MAP)
    for (size_t i = 0; i < RG_COUNT(keymap_gpio); ++i)
    {
        const rg_keymap_gpio_t *mapping = &keymap_gpio[i];
        if (gpio_get_level(mapping->num) == mapping->level)
            state |= mapping->key;
    }
#endif

#if defined(RG_GAMEPAD_I2C_MAP)
    uint32_t buttons = 0;
    uint8_t pcf_data = 0xFF;
    if (rg_i2c_read(0x20, -1, &pcf_data, 1))
    {
        buttons = pcf_data;
        for (size_t i = 0; i < RG_COUNT(keymap_i2c); ++i)
        {
            const rg_keymap_i2c_t *mapping = &keymap_i2c[i];
            if (((buttons >> mapping->num) & 1) == mapping->level)
                state |= mapping->key;
        }
    }
#endif

#if defined(RG_GAMEPAD_KBD_MAP)
#ifdef RG_TARGET_SDL2
    int numkeys = 0;
    const uint8_t *keys = SDL_GetKeyboardState(&numkeys);
    for (size_t i = 0; i < RG_COUNT(keymap_kbd); ++i)
    {
        const rg_keymap_kbd_t *mapping = &keymap_kbd[i];
        if (mapping->src < 0 || mapping->src >= numkeys)
            continue;
        if (keys[mapping->src])
            state |= mapping->key;
    }
#else
#warning "not implemented"
#endif
#endif

#if defined(RG_GAMEPAD_SERIAL_MAP)
    gpio_set_level(RG_GPIO_GAMEPAD_LATCH, 0);
    rg_usleep(5);
    gpio_set_level(RG_GPIO_GAMEPAD_LATCH, 1);
    rg_usleep(1);
    uint32_t buttons = 0;
    for (int i = 0; i < 16; i++)
    {
        buttons |= gpio_get_level(RG_GPIO_GAMEPAD_DATA) << (15 - i);
        gpio_set_level(RG_GPIO_GAMEPAD_CLOCK, 0);
        rg_usleep(1);
        gpio_set_level(RG_GPIO_GAMEPAD_CLOCK, 1);
        rg_usleep(1);
    }
    for (size_t i = 0; i < RG_COUNT(keymap_serial); ++i)
    {
        const rg_keymap_serial_t *mapping = &keymap_serial[i];
        if (((buttons >> mapping->num) & 1) == mapping->level)
            state |= mapping->key;
    }
#endif

#if defined(RG_GAMEPAD_VIRT_MAP)
    for (size_t i = 0; i < RG_COUNT(keymap_virt); ++i)
    {
        if (state == keymap_virt[i].src)
            state = keymap_virt[i].key;
    }
#endif

#ifdef ESP_PLATFORM
    state |= touch_get_scroll_keys();
#endif

    if (out)
        *out = state;
    return true;
}

static void input_task(void *arg)
{
    uint8_t debounce[RG_KEY_COUNT];
    uint32_t local_gamepad_state = 0;
    uint32_t state;
    int64_t next_battery_update = 0;

    memset(debounce, 0xFF, sizeof(debounce));
    input_task_running = true;

    while (input_task_running)
    {
        if (rg_input_read_gamepad_raw(&state))
        {
            for (int i = 0; i < RG_KEY_COUNT; ++i)
            {
                uint32_t val = ((debounce[i] << 1) | ((state >> i) & 1));
                debounce[i] = val & 0xFF;

                if ((val & ((1 << RG_GAMEPAD_DEBOUNCE_PRESS) - 1)) == ((1 << RG_GAMEPAD_DEBOUNCE_PRESS) - 1))
                {
                    local_gamepad_state |= (1 << i);
                }
                else if ((val & ((1 << RG_GAMEPAD_DEBOUNCE_RELEASE) - 1)) == 0)
                {
                    local_gamepad_state &= ~(1 << i);
                }
            }
            gamepad_state = local_gamepad_state;
        }

        if (rg_system_timer() >= next_battery_update)
        {
            rg_battery_t temp = {0};
            if (rg_input_read_battery_raw(&temp))
            {
                if (fabsf(battery_state.level - temp.level) < RG_BATTERY_UPDATE_THRESHOLD)
                    temp.level = battery_state.level;
                if (fabsf(battery_state.volts - temp.volts) < RG_BATTERY_UPDATE_THRESHOLD_VOLT)
                    temp.volts = battery_state.volts;
            }
            battery_state = temp;
            next_battery_update = rg_system_timer() + 2 * 1000000;
        }

        rg_task_delay(10);
    }

    input_task_running = false;
    gamepad_state = -1;
}

void rg_input_init(void)
{
    RG_ASSERT(!input_task_running, "Input already initialized!");

#if defined(RG_GAMEPAD_ADC_MAP)
    RG_LOGI("Initializing ADC gamepad driver...");
    adc1_config_width(ADC_WIDTH_MAX - 1);
    for (size_t i = 0; i < RG_COUNT(keymap_adc); ++i)
    {
        const rg_keymap_adc_t *mapping = &keymap_adc[i];
        if (mapping->unit == ADC_UNIT_1)
        {
            adc1_config_channel_atten(mapping->channel, ADC_ATTEN_DB_11);
        }
        else if (mapping->unit == ADC_UNIT_2)
        {
            adc2_config_channel_atten(mapping->channel, ADC_ATTEN_DB_11);
        }
    }
    UPDATE_GLOBAL_MAP(keymap_adc);
#endif

#if defined(RG_GAMEPAD_GPIO_MAP)
    RG_LOGI("Initializing GPIO gamepad driver...");
    for (size_t i = 0; i < RG_COUNT(keymap_gpio); ++i)
    {
        const rg_keymap_gpio_t *mapping = &keymap_gpio[i];
        gpio_set_direction(mapping->num, GPIO_MODE_INPUT);
        if (mapping->pullup)
            gpio_set_pull_mode(mapping->num, GPIO_PULLUP_ONLY);
        else if (mapping->pulldown)
            gpio_set_pull_mode(mapping->num, GPIO_PULLDOWN_ONLY);
    }
    UPDATE_GLOBAL_MAP(keymap_gpio);
#endif

#if defined(RG_GAMEPAD_I2C_MAP)
RG_LOGI("Initializing I2C gamepad driver...");
rg_i2c_init();
UPDATE_GLOBAL_MAP(keymap_i2c);
#endif
#if defined(RG_GAMEPAD_KBD_MAP)
RG_LOGI("Initializing KBD gamepad driver...");
UPDATE_GLOBAL_MAP(keymap_kbd);
#endif
#if defined(RG_GAMEPAD_SERIAL_MAP)
RG_LOGI("Initializing SERIAL gamepad driver...");
gpio_set_direction(RG_GPIO_GAMEPAD_CLOCK, GPIO_MODE_OUTPUT);
gpio_set_direction(RG_GPIO_GAMEPAD_LATCH, GPIO_MODE_OUTPUT);
gpio_set_direction(RG_GPIO_GAMEPAD_DATA, GPIO_MODE_INPUT);
gpio_set_level(RG_GPIO_GAMEPAD_LATCH, 1);
gpio_set_level(RG_GPIO_GAMEPAD_CLOCK, 1);
UPDATE_GLOBAL_MAP(keymap_serial);
#endif
#if defined(RG_GAMEPAD_VIRT_MAP)
UPDATE_GLOBAL_MAP(keymap_virt);
#endif
rg_task_create("rg_input", &input_task, NULL, 2 * 1024, RG_TASK_PRIORITY_5, 1);
while (!input_task_running)
rg_task_delay(1);
RG_LOGI("Input ready. state=" PRINTF_BINARY_16, PRINTF_BINVAL_16(gamepad_mapped));
}
void rg_input_deinit(void)
{
input_task_running = false;
while (gamepad_state != -1)
rg_task_delay(1);
RG_LOGI("Input terminated.");
}
bool rg_input_key_is_present(rg_key_t key)
{
return (gamepad_mapped & key) != 0;
}
uint32_t rg_input_read_gamepad(void)
{
return gamepad_state;
}
bool rg_input_wait_for_key(rg_key_t key, bool pressed, int timeout_ms)
{
int64_t expiration = timeout_ms < 0 ? INT64_MAX : (rg_system_timer() + timeout_ms * 1000);
while (rg_input_read_gamepad() != -1)
{
bool state = (rg_input_read_gamepad() & key) != 0;
if (state == pressed)
return true;
if (rg_system_timer() >= expiration)
break;
rg_task_delay(10);
}
return false;
}
rg_battery_t rg_input_read_battery(void)
{
return battery_state;
}
