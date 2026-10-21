> **Not authoritative.** Unmodified text of a schematic-derived pin table,
> kept as a reference. It disagrees with Pimoroni's shipping firmware and with
> pico-sdk's `pimoroni_presto.h` in several places (listed below), and assigns
> some GPIOs to two functions. Use [`../boards/pimoroni-presto.md`](../boards/pimoroni-presto.md)
> for pins. Contradictions found:
>
> | Item | This table | Firmware / SDK header |
> |---|---|---|
> | LCD RGB data | 18 bits on GPIO0-17 (R7 on GPIO0) | 16 RGB565 bits on GPIO1-16 (`PRESTO_LCD_D0_PIN`=1..D15=16), GPIO17/18 held low |
> | HSYNC / VSYNC / DE / DOTCLK | GPIO18 / 19 / 20 / 21 | GPIO19 / 20 / 21 / 22 |
> | SD clock | GPIO33 (same pin as the LED data line) | GPIO34 |
> | SD MOSI / MISO / DAT1 / DAT2 / CS | GPIO34 / 35 / 36 / 37 / 38 | GPIO35 / 36 / 37 / 38 / 39 |
> | CYW43 `WL_GP0/1/2` | GPIO22 / 21 / 20 (collide with LCD signals) | not RP2350 pins: the firmware only sets `CYW43_WL_GPIO_COUNT`=3 (CYW43-side GPIOs) -- interpretation, unverified |
>
> Everything else (touch I2C 30/31, INT 32, touch reset 42, LCD reset 44,
> backlight 45, LED 33, QW/ST 40/41, piezo 43, user switch 46, PSRAM CS 47,
> CYW43 23/24/25/29, LCD init SPI 26/27/28) agrees.

Based on the schematic provided in the document, here are the pinout connections between the **RP2350B** (MCU1) and each external device on the Pimoroni Presto board:

[Presto Github](https://github.com/pimoroni/presto)

---

### 1. RM2 Wi-Fi / Bluetooth Module (`A1`) / CYW43 

| RP2350 Pin / Signal | RM2 Pin & Function | Signal Name |
| --- | --- | --- |
| **GPIO23** | `B5` (WLON) `B6` (BTON) | WL_ON |
| **GPIO24** | `A5` (DI) / `A6` (DO) / `B3` (IRQ) | WL_D (Data) |
| **GPIO25** | `B2` (CS) | WL_CS (Chip Select) |
| **GPIO29** | `A3` (SCLK) | WL_CLK (Clock) |
| **GPIO22** | `B1` (GPIO0) | WL_GP0 |
| **GPIO21** | `C4` (GPIO1) | WL_GP1 |
| **GPIO20** | `C3` (GPIO2) | WL_GP2 |

---

### 2. Display / LCD Panel & Touch Interface (`FPC1` / FPC-40WAY)

- Screen controller: st7701
- Touch controller: FT6236

| RP2350 Pin / Signal | FPC1 Pin & Signal / Function |
| --- | --- |
| **GPIO0** | Pin 21 (`R7` - Red Data 7) |
| **GPIO1** | Pin 20 (`R6` - Red Data 6) |
| **GPIO2** | Pin 19 (`R5` - Red Data 5) |
| **GPIO3** | Pin 18 (`R4` - Red Data 4) |
| **GPIO4** | Pin 17 (`R3` - Red Data 3) |
| **GPIO5** | Pin 16 (`R2` - Red Data 2) |
| **GPIO6** | Pin 27 (`G7` - Green Data 7) |
| **GPIO7** | Pin 26 (`G6` - Green Data 6) |
| **GPIO8** | Pin 25 (`G5` - Green Data 5) |
| **GPIO9** | Pin 24 (`G4` - Green Data 4) |
| **GPIO10** | Pin 23 (`G3` - Green Data 3) |
| **GPIO11** | Pin 22 (`G2` - Green Data 2) |
| **GPIO12** | Pin 33 (`B7` - Blue Data 7) |
| **GPIO13** | Pin 32 (`B6` - Blue Data 6) |
| **GPIO14** | Pin 31 (`B5` - Blue Data 5) |
| **GPIO15** | Pin 30 (`B4` - Blue Data 4) |
| **GPIO16** | Pin 29 (`B3` - Blue Data 3) |
| **GPIO17** | Pin 28 (`B2` - Blue Data 2) |
| **GPIO18** | Pin 15 (`HSYNC` - Horizontal Sync) |
| **GPIO19** | Pin 14 (`VSYNC` - Vertical Sync) |
| **GPIO20** | Pin 13 (`LCD_DE` - Data Enable) |
| **GPIO21** | Pin 12 (`LCD_DOT_CLK` - Pixel Clock) |
| **GPIO26** | Pin 10 (`LCD_SPI_CLK` - SPI Clock) |
| **GPIO27** | Pin 9 (`LCD_SPI_DATA` - SPI Data) |
| **GPIO28** | Pin 11 (`LCD_SPI_CS` - SPI Chip Select) |
| **GPIO30** | Pin 36 (`TOUCH_SDA` - I2C Data) |
| **GPIO31** | Pin 37 (`TOUCH_SCL` - I2C Clock) |
| **GPIO32** | Pin 35 (`TOUCH_INT` - Touch Interrupt) |
| **GPIO42** | Pin 38 (`TOUCH_RESET` - Touch Reset) |
| **GPIO44** | Pin 6 (`LCD_RESET` - Display Reset) |
| **GPIO45** | Backlight Driver (`BACKLIGHT_EN` -> `U1 AP3031` CTRL Pin 4) |

---

### 3. MicroSD Card Connector

| RP2350 Pin / Signal | MicroSD Pin & Function |
| --- | --- |
| **GPIO34** | Pin 3 (`SD_CMD/MOSI` - Command / SPI MOSI) |
| **GPIO35** | Pin 7 (`SD_SD0/MISO` - Data 0 / SPI MISO) |
| **GPIO36** | Pin 8 (`SD_SD1` - Data 1) |
| **GPIO37** | Pin 1 (`SD_SD2` - Data 2) |
| **GPIO38** | Pin 2 (`SD_SD3/SD_CS` - Data 3 / SPI CS) |
| **GPIO33** (SD_SCLK) | Pin 5 (`SD_SCLK` - Clock) |

---

### 4. Flash Memory (`U3` - 16MB W25Q128JVPIQ)

| RP2350 Pin / Signal | Flash Memory (`U3`) Pin & Function |
| --- | --- |
| **FLASH_CS / QSPI_CS** | Pin 1 (`CS` - Chip Select) |
| **QSPI_SD1** | Pin 2 (`IO1 / MISO`) |
| **QSPI_SD3** | Pin 3 (`IO3 / /HOLD`) |
| **QSPI_SD2** | Pin 7 (`IO2 / /WP`) |
| **QSPI_SCLK** | Pin 6 (`CLK` - Serial Clock) |
| **QSPI_SD0** | Pin 5 (`IO0 / MOSI`) |

---

### 5. PSRAM (`U4` - 8MB APS6404L-3SQR-ZR)

| RP2350 Pin / Signal | PSRAM (`U4`) Pin & Function |
| --- | --- |
| **GPIO47** (PSRAM_CS) | Pin 1 (`CE` - Chip Select) |
| **QSPI_SD1** | Pin 2 (`SO / SIO1`) |
| **QSPI_SD3** | Pin 3 (`SIO3 / /HOLD`) |
| **QSPI_SCLK** | Pin 6 (`SCLK` - Clock) |
| **QSPI_SD0** | Pin 5 (`SI / SIO0`) |
| **QSPI_SD2** | Pin 7 (`SIO2`) |

---

### 6. I2C QW/ST Header (`J2`)

| RP2350 Pin / Signal | QW/ST Connector (`J2`) Pin & Function |
| --- | --- |
| **GPIO40** | Pin 3 (`I2C_SDA` - Data) |
| **GPIO41** | Pin 4 (`I2C_SCL` - Clock) |

---

### 7. RGB LEDs (Addressable Chain)

| RP2350 Pin / Signal | LED Chain Pin & Function |
| --- | --- |
| **GPIO33** | `LED_DATA` (Input to first WS2812/SK6812 LED) |

---

### 8. Piezo Speaker Circuit

| RP2350 Pin / Signal | Speaker Drive Circuit |
| --- | --- |
| **GPIO43** | `PIEZO_AUDIO` (Gate of Transistor `Q1` BSS138PW) |

---

### 9. USB-C Interface

| RP2350 Pin / Signal | USB-C Connector Signal |
| --- | --- |
| **USB_DP** (via 27.4Ω `R11`) | `USB_DP` (D+) |
| **USB_DM** (via 27.4Ω `R12`) | `USB_DM` (D-) |

---

### 10. Buttons & Controls

| RP2350 Pin / Signal | Button / Switch |
| --- | --- |
| **GPIO46** | User Switch (`USER_SW` / `SW_BOOT`) |
| **RUN** | Reset Button (`SW_RESET`) |
