#include "pico_toolset/bootsel.h"

#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/platform.h"

namespace pico_toolset {

// Same technique as pico-examples' get_bootsel_button(), valid on RP2040 and RP2350.
bool __no_inline_not_in_flash_func(bootsel_pressed)() {
    const uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[1].ctrl, GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (int i = 0; i < 1000; ++i)
        __asm volatile("nop");
    const bool pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);
    hw_write_masked(&ioqspi_hw->io[1].ctrl, GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}

} // namespace pico_toolset
