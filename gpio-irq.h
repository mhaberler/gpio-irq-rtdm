#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#include <stdbool.h>
#endif
struct gpio_irq_data  {
    int pin;      // kernel GPIO pin number
    bool falling; // 0..default to trigger on rising edge, GPIO_IRQ_BIND only
};

#define GPIO_IRQ_IOC_MAGIC    'r'

#define GPIO_IRQ_PIN_SET      _IO(GPIO_IRQ_IOC_MAGIC, 1)
#define GPIO_IRQ_PIN_TOGGLE   _IOW(GPIO_IRQ_IOC_MAGIC, 2, int)
#define GPIO_IRQ_BIND         _IOW(GPIO_IRQ_IOC_MAGIC, 3, struct gpio_irq_data)
#define GPIO_IRQ_PIN_READ     _IOR(GPIO_IRQ_IOC_MAGIC, 4, int)
#define GPIO_IRQ_PIN_WAIT     _IO(GPIO_IRQ_IOC_MAGIC,  5)

