// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "lcd.h"
#include "board_api.h"

#if TINYUF2_DISPLAY

static const char *TAG = "LCD";

/*!< Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA. */
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    // Memory Data Access Control
    {0x36, {DISPLAY_MADCTL}, 1},

    // Vertical Scroll Start Address of RAM
    {0x37, {DISPLAY_VSCSAD >> 8, DISPLAY_VSCSAD & 0x00FF}, 2},

    /* Interface Pixel Format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
#if 0 // another init sequence
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x35}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x19}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    //{0xC2, {0x01, 0xff}, 2},
    {0xC2, {0x01}, 1},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x12}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 1},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2b, 0x3f, 0x54, 0x4c, 0x18, 0x0D, 0x0B, 0x1f, 0x23}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x04, 0x0c, 0x11, 0x13, 0x2c, 0x3f, 0x44, 0x51, 0x2f, 0x1f, 0x1f, 0x20, 0x23}, 14},
#else
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x45}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x2B}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    {0xC2, {0x01, 0xff}, 2},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x11}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 1},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
#endif
    // Inversion ON
    {0x21, {0}, 0x00},
    /* Sleep Out */
    {0x11, {0}, 0x80},
    /* Display On */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}
};

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[] = {
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access control, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {DISPLAY_MADCTL}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       /*!< Zero out the transaction */
    t.length = 8;                   /*!< Command is 8 bits */
    t.tx_buffer = &cmd;             /*!< The data is the cmd itself */
    t.user = (void *)0;             /*!< D/C needs to be set to 0 */
    ret = spi_device_polling_transmit(spi, &t); /*!< Transmit! */
    assert(ret == ESP_OK);          /*!< Should have had no issues. */
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;

    if (len == 0) {
        return;    /*!< no need to send anything */
    }

    memset(&t, 0, sizeof(t));       /*!< Zero out the transaction */
    t.length = len * 8;             /*!< Len is in bytes, transaction length is in bits. */
    t.tx_buffer = data;             /*!< Data */
    t.user = (void *)1;             /*!< D/C needs to be set to 1 */
    ret = spi_device_polling_transmit(spi, &t); /*!< Transmit! */
    assert(ret == ESP_OK);          /*!< Should have had no issues. */
}

/*!<  This function is called (in irq context!) just before a transmission starts. It will */
/*!<  set the D/C line to the value indicated in the user field. */
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(DISPLAY_PIN_DC, dc);
}

#ifdef CONFIG_LCD_TYPE_AUTO
/*!<  read lcd id number */
static uint32_t lcd_get_id(spi_device_handle_t spi)
{
    /*!< get_id cmd */
    lcd_cmd(spi, 0x04);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *)1;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);

    return *(uint32_t *)t.rx_data;
}
#endif

/*!<  Initialize the display */
esp_err_t lcd_init(spi_device_handle_t spi)
{
    assert(spi != NULL);
    int cmd = 0;
    const lcd_init_cmd_t *lcd_init_cmds;

    /*!<  Initialize non-SPI GPIOs */
    gpio_config_t cfg =
    {
      .pin_bit_mask = 0,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };

    cfg.pin_bit_mask = (1ull << DISPLAY_PIN_DC);
    gpio_config(&cfg);

    cfg.pin_bit_mask = (1ull << DISPLAY_PIN_RST);
    gpio_config(&cfg);

    cfg.pin_bit_mask = (1ull << DISPLAY_PIN_BL);
    gpio_config(&cfg);

    gpio_set_direction(DISPLAY_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(DISPLAY_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(DISPLAY_PIN_BL, GPIO_MODE_OUTPUT);

#ifdef DISPLAY_PIN_POWER
    /*!< /Enable display */
    gpio_set_direction(DISPLAY_PIN_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(DISPLAY_PIN_POWER, DISPLAY_POWER_ON);
#endif

    /*!<  Reset the display */
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    int lcd_type;

#ifdef CONFIG_LCD_TYPE_AUTO
    /*!<  detect LCD type */
    uint32_t lcd_id = lcd_get_id(spi);
    int lcd_detected_type = 0;

    ESP_LOGI(TAG, "LCD ID: %08X", lcd_id);

    if (lcd_id == 0) {
        /*!< zero, ili */
        lcd_detected_type = LCD_TYPE_ILI;
        ESP_LOGI(TAG, "ILI9341 detected.");
    } else {
        /*!<  none-zero, ST */
        lcd_detected_type = LCD_TYPE_ST;
        ESP_LOGI(TAG, "ST7789V detected.");
    }

    lcd_type = lcd_detected_type;

#elif defined( CONFIG_LCD_TYPE_ST7789V )
    ESP_LOGI(TAG, "kconfig: force CONFIG_LCD_TYPE_ST7789V.");
    lcd_type = LCD_TYPE_ST;

#elif defined( CONFIG_LCD_TYPE_ILI9341 )
    ESP_LOGI(TAG, "kconfig: force CONFIG_LCD_TYPE_ILI9341.");
    lcd_type = LCD_TYPE_ILI;

#endif

    if (lcd_type == LCD_TYPE_ST) {
        ESP_LOGI(TAG, "ST7789V initialization.");
        lcd_init_cmds = st_init_cmds;
    } else {
        ESP_LOGI(TAG, "ILI9341 initialization.");
        lcd_init_cmds = ili_init_cmds;
    }

    /*!< Send all the commands */
    while (lcd_init_cmds[cmd].databytes != 0xff) {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);

        if (lcd_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }

        cmd++;
    }

    /*!< /Enable backlight */
    gpio_set_level(DISPLAY_PIN_BL, DISPLAY_BL_ON);

    return ESP_OK;
}


static void send_line_finish(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;

    /*!< Wait for all 6 transactions to be done and get back the results. */
    for (int x = 0; x < 6; x++) {
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
        /*!< We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though. */
    }
}

/* To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
 * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
 * because the D/C line needs to be toggled in the middle.)
 * This routine queues these commands up as interrupt transactions so they get
 * sent faster (compared to calling spi_device_transmit several times), and at
 * the mean while the lines for next transactions can get calculated.
 */
void lcd_draw_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata)
{
    esp_err_t ret;
    int x;
    /*!< Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this */
    /*!< function is finished because the SPI driver needs access to it even while we're already calculating the next line. */
    static spi_transaction_t trans[6];

    /*!< In theory, it's better to initialize trans and data only once and hang on to the initialized */
    /*!< variables. We allocate them on the stack, so we need to re-init them each call. */
    for (x = 0; x < 6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));

        if ((x & 1) == 0) {
            /*!< Even transfers are commands */
            trans[x].length = 8;
            trans[x].user = (void *)0;
        } else {
            /*!< Odd transfers are data */
            trans[x].length = 8 * 4;
            trans[x].user = (void *)1;
        }

        trans[x].flags = SPI_TRANS_USE_TXDATA;
    }

    ypos += DISPLAY_ROW_OFFSET;

    trans[0].tx_data[0] = 0x2A;         /*!< Column Address Set */
    trans[1].tx_data[0] = (DISPLAY_COL_OFFSET) >> 8;            /*!< Start Col High */
    trans[1].tx_data[1] = (DISPLAY_COL_OFFSET) & 0xff;            /*!< Start Col Low */
    trans[1].tx_data[2] = (320) >> 8;   /*!< End Col High */
    trans[1].tx_data[3] = (320) & 0xff; /*!< End Col Low */

    trans[2].tx_data[0] = 0x2B;         /*!< Page address set */
    trans[3].tx_data[0] = ypos >> 8;    /*!< Start page high */
    trans[3].tx_data[1] = ypos & 0xff;  /*!< start page low */
    trans[3].tx_data[2] = (ypos + PARALLEL_LINES) >> 8; /*!< end page high */
    trans[3].tx_data[3] = (ypos + PARALLEL_LINES) & 0xff; /*!< end page low */

    trans[4].tx_data[0] = 0x2C;         /*!< memory write */
    trans[5].tx_buffer = linedata;      /*!< finally send the line data */
    trans[5].length = DISPLAY_HEIGHT * 2 * 8 * PARALLEL_LINES;  /*!< Data length, in bits */
    trans[5].flags = 0; /*!< undo SPI_TRANS_USE_TXDATA flag */

    /*!< Queue all transactions. */
    for (x = 0; x < 6; x++) {
        ret = spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret == ESP_OK);
    }

    /*!< When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens */
    /*!< mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to */
    /*!< finish because we may as well spend the time calculating the next line. When that is done, we can call */
    /*!< send_line_finish, which will wait for the transfers to be done and check their status. */

    send_line_finish(spi);
}

#if 0
/*!< Simple routine to generate some patterns and send them to the LCD. Don't expect anything too */
/*!< impressive. Because the SPI driver handles transactions in the background, we can calculate the next line */
/*!< while the previous one is being sent. */
void display_pretty_colors(spi_device_handle_t spi)
{
    uint16_t *lines[2];

    /*!< Allocate memory for the pixel buffers */
    for (int i = 0; i < 2; i++) {
        lines[i] = heap_caps_malloc(320 * PARALLEL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
        assert(lines[i] != NULL);
    }

    int frame = 0;
    /*!< Indexes of the line currently being sent to the LCD and the line we're calculating. */
    int sending_line = -1;
    int calc_line = 0;

    while (1) {
        frame++;

        for (int y = 0; y < 240; y += PARALLEL_LINES) {
            /*!<Calculate a line.*/
#if defined ( CONFIG_LCD_DYNAMIC_IMAGE )
            pretty_effect_calc_lines(lines[calc_line], y, frame, PARALLEL_LINES);
#elif defined ( CONFIG_LCD_STATIC_IMAGE )
            pretty_effect_static_lines(lines[calc_line], y, frame, PARALLEL_LINES);
#endif

            /*!<Finish up the sending process of the previous line, if any*/
            if (sending_line != -1) {
                send_line_finish(spi);
            }

            /*!<Swap sending_line and calc_line*/
            sending_line = calc_line;
            calc_line = (calc_line == 1) ? 0 : 1;
            /*!<Send the line we currently calculated.*/
            send_lines(spi, y, lines[sending_line]);
            /*!<The line set is queued up for sending now; the actual sending happens in the */
            /*!<background. We can go on to calculate the next line set as long as we do not */
            /*!<touch line[sending_line]; the SPI sending process is still reading from that.*/
        }
    }
}
#endif

#endif
