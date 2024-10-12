/**
 * @file      utilities.h
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#pragma once

// ===================
// Select camera model
// ===================
#define LILYGO_ESP32S3_CAM_PIR_VOICE // Has PSRAM
//#define LILYGO_ESP32S3_CAM_BIGSCREEN


// Set this to true if using AP mode
#define USING_AP_MODE       true


// ===================
// Pins
// ===================
#ifdef I2C_SDA
#undef I2C_SDA
#endif

#ifdef I2C_SCL
#undef I2C_SCL
#endif


#if defined(LILYGO_ESP32S3_CAM_PIR_VOICE)

#define PWDN_GPIO_NUM               (-1)
#define RESET_GPIO_NUM              (39)
#define XCLK_GPIO_NUM               (38)
#define SIOD_GPIO_NUM               (5)
#define SIOC_GPIO_NUM               (4)
#define VSYNC_GPIO_NUM              (8)
#define HREF_GPIO_NUM               (18)
#define PCLK_GPIO_NUM               (12)
#define Y9_GPIO_NUM                 (9)
#define Y8_GPIO_NUM                 (10)
#define Y7_GPIO_NUM                 (11)
#define Y6_GPIO_NUM                 (13)
#define Y5_GPIO_NUM                 (21)
#define Y4_GPIO_NUM                 (48)
#define Y3_GPIO_NUM                 (47)
#define Y2_GPIO_NUM                 (14)

#define I2C_SDA                     (7)
#define I2C_SCL                     (6)

#define PIR_INPUT_PIN               (17)
#define PMU_INPUT_PIN               (2)


#define IIS_WS_PIN                  (42)
#define IIS_DIN_PIN                 (41)
#define IIS_SCLK_PIN                (40)


#define EXTERN_PIN1                 (16)
#define EXTERN_PIN2                 (15)

#define BUTTON_CONUT                (1)
#define USER_BUTTON_PIN             (0)
#define BUTTON_ARRAY                {USER_BUTTON_PIN}

#define I2S_SAMPLE_RATE   16000
#define I2S_SAMPLE_BITS   I2S_BITS_PER_SAMPLE_16BIT
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN   64
#define I2S_INTR_ALLOC_FLAGS 0
#define VAD_SAMPLE_RATE_HZ              16000
#define VAD_FRAME_LENGTH_MS             30
#define VAD_BUFFER_LENGTH               (VAD_FRAME_LENGTH_MS * VAD_SAMPLE_RATE_HZ / 1000)
#define I2S_CH                          I2S_NUM_0

#define IR_LED_PIN                  (45)

#elif defined(LILYGO_ESP32S3_CAM_BIGSCREEN)


#define PWDN_GPIO_NUM               (-1)
#define RESET_GPIO_NUM              (-1)
#define XCLK_GPIO_NUM               (4)
#define SIOD_GPIO_NUM               (18)
#define SIOC_GPIO_NUM               (23)
#define VSYNC_GPIO_NUM              (5)
#define HREF_GPIO_NUM               (27)
#define PCLK_GPIO_NUM               (25)
#define Y9_GPIO_NUM                 (36)
#define Y8_GPIO_NUM                 (37)
#define Y7_GPIO_NUM                 (38)
#define Y6_GPIO_NUM                 (39)
#define Y5_GPIO_NUM                 (35)
#define Y4_GPIO_NUM                 (26)
#define Y3_GPIO_NUM                 (13)
#define Y2_GPIO_NUM                 (34)

#define I2C_SDA                     (18)
#define I2C_SCL                     (23)

#elif defined(LILYGO_ESP32S3_CAM_SIM7080G)


#define PWDN_GPIO_NUM               (-1)
#define RESET_GPIO_NUM              (18)
#define XCLK_GPIO_NUM               (8)
#define SIOD_GPIO_NUM               (2)
#define SIOC_GPIO_NUM               (1)
#define VSYNC_GPIO_NUM              (16)
#define HREF_GPIO_NUM               (17)
#define PCLK_GPIO_NUM               (12)
#define Y9_GPIO_NUM                 (9)
#define Y8_GPIO_NUM                 (10)
#define Y7_GPIO_NUM                 (11)
#define Y6_GPIO_NUM                 (13)
#define Y5_GPIO_NUM                 (21)
#define Y4_GPIO_NUM                 (48)
#define Y3_GPIO_NUM                 (47)
#define Y2_GPIO_NUM                 (14)

#define I2C_SDA                     (15)
#define I2C_SCL                     (7)

#define PMU_INPUT_PIN               (6)

#define BUTTON_CONUT                (1)
#define USER_BUTTON_PIN             (0)
#define BUTTON_ARRAY                {USER_BUTTON_PIN}

#define BOARD_MODEM_PWR_PIN         (41)
#define BOARD_MODEM_DTR_PIN         (42)
#define BOARD_MODEM_RI_PIN          (3)
#define BOARD_MODEM_RXD_PIN         (4)
#define BOARD_MODEM_TXD_PIN         (5)

#define USING_MODEM


#else
#error "Camera model not selected"
#endif
