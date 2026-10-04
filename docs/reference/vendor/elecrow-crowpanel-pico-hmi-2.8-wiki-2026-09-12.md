# CrowPanel PICO HMI 2.8'' Display - Elecrow Wiki

**Source:** https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html
**Saved:** 2026-09-12T17:49:27.631Z

*Generated with [markdown-printer](https://github.com/levz0r/markdown-printer) (v1.2.0) by [Lev Gelfenbuim](https://lev.engineer)*

---

# CrowPanel PICO HMI 2.8'' Display[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#crowpanel-pico-hmi-28-display "Permanent link")

## Description[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#description "Permanent link")

* * *

CrowPanel Pico Display is an HMI module series that utilizes the powerful Raspberry Pi RP2040 (ARM Cortex-M0+) as its main controller. It is equipped with a 32-bit dual-core chip running at a clock frequency of up to 133 MHz. With 264kb of built-in SRAM and a 2MB flash memory chip, it integrates power supply, voltage regulation, and counter functions into a single microcontroller. This series of touch screens incorporate high performance, low cost, and user-friendly features.

The Pico 2.8” Display has a resolution of 320\*240 and comes with a touch pen for flexible screen manipulation. It has flexible I/O peripherals and a unique programmable input/output (PIO) subsystem, including practical communication interfaces such as I2C, UART, common IO ports, and USB. It also features a lithium battery interface and a buzzer alarm, enabling communication with almost any external device. This provides professional users with flexibility and powerful expandability, allowing seamless connection to the physical world and control of various aspects of smart homes.

The module comes with abundant resources, including development SDK, documentation. It has a very low entry barrier, making it suitable for beginners and hobbyist users. It not only can use Arduino IDE, MicroPython, PlatformIO and CircuitPython to program, but also supports the LVGL graphics library and Squareline Studio to customize the desired UI interface. It serves as an excellent platform for machine learning applications and is the preferred solution for Pico-like HMI interaction terminals.

**Model: [DIS01028P](https://www.elecrow.com/crowpanel-pico-display-2-8-inch-320-240-module-tft-lcd-touchscreen-with-rp2040-support-c-c-micropython-lvgl.html)**  
[![2.8](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/2.8.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/2.8.webp)

[![Alt text](https://www.elecrow.com/wiki/assets/images/common/Get_one_now.webp)](https://www.elecrow.com/crowpanel-pico-display-2-8-inch-320-240-module-tft-lcd-touchscreen-with-rp2040-support-c-c-micropython-lvgl.html)

## Feature[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#feature "Permanent link")

* * *

-   RP2040 Microcontroller: Equipped with a 32-bit dual-core chip, it achieves a maximum clock frequency of 133 MHz.
-   TN Panel: Offers a wide color gamut and exceptional brightness for optimal display performance.
-   Energy Efficiency: Supports low-power sleep and hibernation modes, promoting energy conservation and environmental consciousness.
-   Exceptional Extensibility: A wealth of interfaces, including I2C, UART, IO ports, USB, a lithium battery interface, and a buzzer alarm, ensures remarkable extensibility.
-   Broad Compatibility: Support multiple programming environments.
-   Versatile Applications: Ideally suited for a wide range of applications within the Internet of Things (IoT), intelligent home control, smart factories, intelligent agriculture, and as an HMI interactive terminal.

## Specification[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#specification "Permanent link")

* * *

-   Main Chip: RP2040
-   Processor: Dual-core 32-bit ARM Cortex-M0+ @ 133MHz
-   Memory: 264kB on-chip SRAM (supports up to 16MB of off-chip flash memory)
-   Screen Size: 2.8 inch
-   Resolution: 320\*240
-   Signal Interface: SPI
-   Touch Type: Resistive Touch
-   Panel Type: TFT LCD
-   Power Input: 5V-2A
-   Active Area: 43.2 \* 57.6mm(W \* H)
-   Dimensions: 57 \* 88.7 \* 13.4mm(W \* H \* D)

## PinOut[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#pinout "Permanent link")

* * *

[![1](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/1.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/1.webp)

| Pin Name | Description                                                                                          | Connect Type |
|----------|------------------------------------------------------------------------------------------------------|:------------:|
| PWR      | Power LED.                                                                                           |              |
| RST      | Reset button. Press it to reset the system.                                                          |              |
| BOOT     | Hold the BOOT button and press RST button to make the RP2040 enter flash mode.                       |              |
| TF       | Provide off-line save and extra storage space.                                                       |              |
| UART0    | Build the communication among Logic modules, including serial communication module and print module. |              |
| UART1    | Build the communication among Logic modules, including serial communication module and print module. | HY2.0-4P     |
| I2C      | Connecting microcontrollers and other peripheral devices.                                            | HY2.0-4P     |
| BAT      | Connect with the lithium battery. Connect USC-C port to charge the battery.                          | PH2.0-2P     |

| PICO 2.8-inch HMI Port |           Pin Number          |
|:----------------------:|:-----------------------------:|
|           I2C          |      GP20(SDA), GP21(SCL)     |
|          UART0         |        RX(GP1); TX(GP0)       |
|          UART1         |        RX(GP5); TX(GP4)       |
|        GPIO Pins       | GP0~GP7, GP19~GP21, GP26~GP28 |

## Schematic Diagram[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#schematic-diagram "Permanent link")

* * *

**RP2040 and TFT-display wiring pins(SPI)**

[![2](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/2.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/2.webp)

[![3](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/3.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/3.webp)

Definition in the pins\_arduino.h:

Note: Please refer to [Get Started With Arduino IDE](https://www.elecrow.com/wiki/Get_Started_with_Arduino_IDE.html) to install the RP2040 board in the Arduino IDE. After installation, the pins\_arduino. h file is located in the Arduino installation directory: ...\\Arduino\\hardware\\rp2040 ...\\vaiants rpipico

[![4](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/4.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/4.webp)

**RP2040 and touchscreen wiring pins**

[![image-20240521153232766](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/5.webp)](https://www.elecrow.com/wiki/assets/images/CrowPanel_Pico_HMI_Display-2.8/5.webp)

## Platforms Supported[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#platforms-supported "Permanent link")

* * *

**Arduino**

**SquareLine** Studio

[![Arduino.png](https://www.elecrow.com/wiki/image/thumb/6/63/Arduino.png/150px-Arduino.png)](https://www.elecrow.com/wiki/image/thumb/6/63/Arduino.png/150px-Arduino.png)

[![2a80b24a-38ae-4639-bbb1-af8b6c26891e.png](https://www.elecrow.com/wiki/image/thumb/9/9b/2a80b24a-38ae-4639-bbb1-af8b6c26891e.png/150px-2a80b24a-38ae-4639-bbb1-af8b6c26891e.png)](https://www.elecrow.com/wiki/image/thumb/9/9b/2a80b24a-38ae-4639-bbb1-af8b6c26891e.png/150px-2a80b24a-38ae-4639-bbb1-af8b6c26891e.png)

[![GetStarted](https://www.elecrow.com/wiki/assets/images/common/View-Tutorials.webp)](https://www.elecrow.com/wiki/Pico_HMI_2.8-inch_Arduino_Tutorial.html)

[![GetStarted](https://www.elecrow.com/wiki/assets/images/common/View-Tutorials.webp)](https://www.elecrow.com/wiki/Pico_HMI_2.8-inch_Arduino_Tutorial.html#design-ui-file-with-squareline-studio)

* * *

## FAQS[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#faqs "Permanent link")

* * *

-   You can list your question at [the forum](https://forum.elecrow.com/) or contact [techsupport@elecrow.com](mailto:techsupport@elecrow.com) for technology support.

## Resources Download[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#resources-download "Permanent link")

* * *

### Github link[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#github-link "Permanent link")

-   [CrowPanel-Pico-Display-2.8-inch-320240](https://github.com/Elecrow-RD/CrowPanel-Pico-Display-2.8-inch-320240)

### Hardware[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#hardware "Permanent link")

-   [Schematic & PCB\_Eagle\_File](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/CrowPanel_Pico_Display-2.8_v1.0.zip)

### Specification[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#specification_1 "Permanent link")

-   [LCD Specification](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/LCD_28A1044_specification .pdf)
-   [Touch Drive Specification](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS09024P/XPT2046.pdf)
-   [RP2040 Datasheet](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS09024P/RP2040.pdf)
-   [Dimension Figure](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/2.8_inch_dimension_figure.zip)

### Software[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#software "Permanent link")

##### Arduino IDE[¶](https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html#arduino-ide "Permanent link")

-   [Tutorial](https://www.elecrow.com/wiki/Pico_HMI_2.8-inch_Arduino_Tutorial.html)
-   [Libraries](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/Libraries.zip)
-   [Example Demo](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/basic_example.zip)
-   [UI Code&Material](https://www.elecrow.com/download/product/CrowPanel/PICO-HMI/DIS01028P/UI_Code&Material.zip)