// Target definition for Custom ESP32-S3 N16R8 Handheld (480x320 SPI + Touch + PCF8574)
#define RG_TARGET_NAME             "MY-S3-HANDHELD"

// Storage (SD Card)
#define RG_STORAGE_ROOT            "/sd"
#define RG_STORAGE_SDSPI_HOST      SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED     SDMMC_FREQ_DEFAULT
#define RG_GPIO_SDSPI_MISO         GPIO_NUM_13
#define RG_GPIO_SDSPI_MOSI         GPIO_NUM_11
#define RG_GPIO_SDSPI_CLK          GPIO_NUM_12
#define RG_GPIO_SDSPI_CS           GPIO_NUM_38

// Audio (MAX98357 I2S)
#define RG_AUDIO_USE_INT_DAC       0
#define RG_AUDIO_USE_EXT_DAC       1
#define RG_GPIO_SND_I2S_BCK        GPIO_NUM_15
#define RG_GPIO_SND_I2S_WS         GPIO_NUM_16
#define RG_GPIO_SND_I2S_DATA       GPIO_NUM_17

// Video (3.5" 480x320 ILI9488 / ST7796 SPI)
#define RG_SCREEN_DRIVER           0
#define RG_SCREEN_HOST             SPI2_HOST
#define RG_SCREEN_SPEED            SPI_MASTER_FREQ_40M
#define RG_SCREEN_BACKLIGHT        1
#define RG_SCREEN_WIDTH            480
#define RG_SCREEN_HEIGHT           320
#define RG_SCREEN_ROTATE           0
#define RG_SCREEN_VISIBLE_AREA     {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA        {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                         \
    ILI9341_CMD(0x36, 0x28);                                                                                     \
    ILI9341_CMD(0x3A, 0x66);                                                                                     \
    ILI9341_CMD(0x21);

#define RG_GPIO_LCD_MISO           GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI           GPIO_NUM_11
#define RG_GPIO_LCD_CLK            GPIO_NUM_12
#define RG_GPIO_LCD_CS             GPIO_NUM_10
#define RG_GPIO_LCD_DC             GPIO_NUM_14
#define RG_GPIO_LCD_RST            GPIO_NUM_21
#define RG_GPIO_LCD_BCKL           GPIO_NUM_NC

// Input: I2C Bus for PCF8574 (0x20)
#define RG_GPIO_I2C_SDA            GPIO_NUM_8
#define RG_GPIO_I2C_SCL            GPIO_NUM_9

// Input: Direct GPIO Buttons (Active LOW with internal pull-ups)
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_L,      .num = GPIO_NUM_6,  .pullup = 1, .level = 0},\
    {RG_KEY_R,      .num = GPIO_NUM_7,  .pullup = 1, .level = 0},\
    {RG_KEY_L2,     .num = GPIO_NUM_18, .pullup = 1, .level = 0},\
    {RG_KEY_R2,     .num = GPIO_NUM_47, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_48, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_26, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_46, .pullup = 1, .level = 0},\
}

// Input: PCF8574 I2C Expander (8 Main Buttons)
#define RG_GAMEPAD_I2C_MAP {\
    {RG_KEY_UP,     .num = 0, .level = 0},\
    {RG_KEY_DOWN,   .num = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = 2, .level = 0},\
    {RG_KEY_RIGHT,  .num = 3, .level = 0},\
    {RG_KEY_A,      .num = 4, .level = 0},\
    {RG_KEY_B,      .num = 5, .level = 0},\
    {RG_KEY_X,      .num = 6, .level = 0},\
    {RG_KEY_Y,      .num = 7, .level = 0},\
}

// Input: Left PS2 Joystick (ADC1_CH0 = GPIO 1, ADC1_CH1 = GPIO 2)
#define RG_GAMEPAD_ADC_MAP {\
    {RG_KEY_UP,    .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_0, .min = 0,    .max = 1100},\
    {RG_KEY_DOWN,  .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_0, .min = 3000, .max = 4095},\
    {RG_KEY_LEFT,  .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_1, .min = 0,    .max = 1100},\
    {RG_KEY_RIGHT, .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_1, .min = 3000, .max = 4095},\
}
