#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Physical base addresses from Vivado Address Editor */
#define GPIO0_PHYS_ADDR          0x41200000UL
#define GPIO1_PHYS_ADDR          0x41210000UL
#define MAP_SIZE                 4096UL
#define MAP_MASK                 (MAP_SIZE - 1)

/* AXI GPIO Register Word Offsets (32-bit words) */
#define GPIO_CH1_DATA_OFFSET     0   /* 0x00: Channel 1 Data Register (LEDs) */
#define GPIO_CH2_DATA_OFFSET     2   /* 0x08: Channel 2 Data Register (Switches) */

/*
 * Set to 0 if RGB LEDs are on axi_gpio_0 Channel 1 bits [9:4] (Option A).
 * Set to 1 if RGB LEDs are on a separate axi_gpio_1 at 0x41210000 bits [5:0] (Option B).
 */
#define USE_SEPARATE_RGB_GPIO    0

/*
 * Maps SW(2 downto 0) to 3-bit RGB {Blue, Green, Red} per Lab 9 Table:
 * 000 -> OFF (0)
 * 001 -> Red (1)
 * 010 -> Green (2)
 * 100 -> Blue (4)
 * 011 -> Red+Green (3)
 * 101 -> Red+Blue / Purple (5)
 * 110 -> Blue+Green (6)
 * 111 -> Red+Blue+Green (7)
 */
static uint32_t sw_to_rgb(uint32_t sw_val)
{
    switch (sw_val & 0x07U) {
        case 0x0: return 0x0U; /* OFF */
        case 0x1: return 0x1U; /* Red */
        case 0x2: return 0x2U; /* Green */
        case 0x4: return 0x4U; /* Blue */
        case 0x3: return 0x3U; /* Red + Green */
        case 0x5: return 0x5U; /* Red + Blue (Purple) */
        case 0x6: return 0x6U; /* Blue + Green */
        case 0x7: return 0x7U; /* Red + Blue + Green */
        default:  return 0x0U;
    }
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Error opening /dev/mem (run as root or with sudo)");
        return EXIT_FAILURE;
    }

    /* Map axi_gpio_0 (0x41200000) into user space */
    void *map_base0 = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                           fd, GPIO0_PHYS_ADDR & ~MAP_MASK);
    if (map_base0 == MAP_FAILED) {
        perror("Error mapping GPIO0_PHYS_ADDR");
        close(fd);
        return EXIT_FAILURE;
    }
    volatile uint32_t *gpio0 = (volatile uint32_t *)((uint8_t *)map_base0 + (GPIO0_PHYS_ADDR & MAP_MASK));

#if USE_SEPARATE_RGB_GPIO
    /* Map axi_gpio_1 (0x41210000) if using a second AXI GPIO IP for RGB LEDs */
    void *map_base1 = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                           fd, GPIO1_PHYS_ADDR & ~MAP_MASK);
    if (map_base1 == MAP_FAILED) {
        perror("Error mapping GPIO1_PHYS_ADDR");
        munmap(map_base0, MAP_SIZE);
        close(fd);
        return EXIT_FAILURE;
    }
    volatile uint32_t *gpio1 = (volatile uint32_t *)((uint8_t *)map_base1 + (GPIO1_PHYS_ADDR & MAP_MASK));
#endif

    uint32_t led_count = 0;
    int tick = 0;

    printf("Lab 9 LED & RGB Controller running... Press Ctrl+C to exit.\n");

    while (1) {
        /* Read slider switches from axi_gpio_0 Channel 2 (offset 0x08) */
        uint32_t switches = gpio0[GPIO_CH2_DATA_OFFSET] & 0x07U;
        uint32_t rgb_3bit = sw_to_rgb(switches);

        /* Combine both RGB LEDs (LD6 and LD5 get the same 3-bit color) */
        uint32_t both_rgb_6bit = (rgb_3bit << 3) | rgb_3bit;

#if USE_SEPARATE_RGB_GPIO
        /* Write 4-bit counter to axi_gpio_0 and 6-bit RGB value to axi_gpio_1 */
        gpio0[GPIO_CH1_DATA_OFFSET] = (led_count & 0x0FU);
        gpio1[GPIO_CH1_DATA_OFFSET] = both_rgb_6bit;
#else
        /* Write 6-bit RGB [9:4] and 4-bit counter [3:0] to axi_gpio_0 Channel 1 */
        gpio0[GPIO_CH1_DATA_OFFSET] = (both_rgb_6bit << 4) | (led_count & 0x0FU);
#endif

        /* Sleep 100 ms so switch updates take effect within 0.1 s (< 1 s requirement) */
        usleep(100000);

        /* Increment the 4-bit LED counter every 5 ticks (5 * 100 ms = 500 ms -> 2 Hz) */
        tick++;
        if (tick >= 5) {
            tick = 0;
            led_count = (led_count + 1U) & 0x0FU;
        }
    }

#if USE_SEPARATE_RGB_GPIO
    munmap(map_base1, MAP_SIZE);
#endif
    munmap(map_base0, MAP_SIZE);
    close(fd);
    return EXIT_SUCCESS;
}