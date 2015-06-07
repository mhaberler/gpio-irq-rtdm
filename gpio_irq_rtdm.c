//
//  RTDM driver for GPIO edge detection
//  loosely based on code by Pierre Ficheux
//  see https://github.com/pficheux/raspberry_pi/tree/master/Xenomai/RT_irq
//
//  Michael Haberler 6/2015

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/types.h>

// see https://lkml.org/lkml/2014/3/19/59
#define smp_mb__before_atomic smp_mb__before_clear_bit

#include <rtdm/rtdm_driver.h>
#include "gpio-irq.h"

// compile in code for irq handler timing pin and counters
#define STATS  1 

#define RTDM_SUBCLASS_GPIO_IRQ       4711
#define DEVICE_NAME                 "gpio_irq"

MODULE_DESCRIPTION("RTDM driver for GPIO edge detection");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Haberler");

struct device_ctx  {
    int gpio_irq;
    rtdm_irq_t irq_handle;
    rtdm_event_t gpio_event;
    char name[10];
};

static int timing_pin = 0;
module_param(timing_pin, int, 0644);

#if STATS
static int interrupts = 0;
module_param(interrupts, int, 0644);

static int completions = 0;
module_param(completions, int, 0644);

static inline void toggle_timing_pin(void)
{
    if (!timing_pin)
	return;
    gpio_set_value(timing_pin, !gpio_get_value(timing_pin));
}
#else
#define toggle_timing_pin()
#endif

static int irq_handler(rtdm_irq_t *irq_handle)
{
    struct device_ctx *ctx = rtdm_irq_get_arg (irq_handle, struct device_ctx);
#if STATS    
    interrupts++;
    toggle_timing_pin();
#endif    
    rtdm_event_signal (&ctx->gpio_event);
    return RTDM_IRQ_HANDLED;
}

int gpio_irq_open(struct rtdm_dev_context *context, rtdm_user_info_t *user_info, int oflags)
{
    struct device_ctx *ctx = (struct device_ctx *) context->dev_private;
    ctx->gpio_irq = -1;  // mark as unbound
    return 0;
}

int gpio_irq_close(struct rtdm_dev_context *context, rtdm_user_info_t *user_info)
{
    int rc;
    struct device_ctx *ctx = (struct device_ctx *) context->dev_private;

    if (ctx->gpio_irq < 0)  {
	printk(KERN_INFO "gpio_irq_close:  closing unbound fd\n");
	return 0;
    }
    rc = rtdm_irq_disable(&ctx->irq_handle);
    if (rc < 0) {
	printk(KERN_WARNING "rtdm_irq_disable:  %d\n", rc);
	return rc;
    }
    rc = rtdm_irq_free(&ctx->irq_handle);
    if (rc < 0) {
	printk(KERN_WARNING "rtdm_irq_free:  %d\n", rc);
	return rc;
    }
    rtdm_event_destroy(&ctx->gpio_event);
    return 0;
}


static ssize_t gpio_irq_ioctl_rt(struct rtdm_dev_context* context,
				 rtdm_user_info_t* user_info,
				 unsigned int request,
				 void __user* arg)
{
    struct device_ctx *ctx = (struct device_ctx *) context->dev_private;
    int rc;
    int pin, irq;
    struct gpio_irq_data gid;

    switch (request) {
	
    case GPIO_IRQ_BIND:
	rtdm_safe_copy_from_user(user_info, &gid, arg,
				 sizeof(struct gpio_irq_data));
	printk(KERN_INFO "gpio_irq_ioctl_rt(GPIO_IRQ_BIND) pin=%d falling=%d\n",
	       gid.pin, gid.falling);
	irq = gpio_to_irq(gid.pin);
	irq_set_irq_type(irq, gid.falling ?
			 IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
	snprintf(ctx->name, sizeof(ctx->name), "gpio%d", gid.pin);
	rc = rtdm_irq_request(&ctx->irq_handle,
			      irq,
			      irq_handler,
			      RTDM_IRQTYPE_EDGE,
			      ctx->name,
			      ctx);
	if (rc < 0) {
	    printk(KERN_WARNING "rtdm_irq_request: cant register irq %d for pin %d - %d\n",
		   irq, gid.pin, rc);
	    ctx->gpio_irq = -1;
	    return rc;
	}
	rtdm_event_init(&ctx->gpio_event, 0);
	rc = rtdm_irq_enable(&ctx->irq_handle);
	if (rc < 0) {
	    printk(KERN_WARNING "rtdm_irq_enable: cant enable irq for pin %d - %d\n", gid.pin, rc);
	    ctx->gpio_irq = -1;
	    return rc;
	}
	ctx->gpio_irq = irq;
	break;

    case GPIO_IRQ_PIN_SET:
	rtdm_safe_copy_from_user(user_info, &pin, arg, sizeof(pin));
	gpio_set_value(pin, 1);
	break;
	
    case GPIO_IRQ_PIN_READ:
	rtdm_safe_copy_from_user(user_info, &pin, arg, sizeof(pin));
	return gpio_get_value(pin);
	break;
	
    case GPIO_IRQ_PIN_TOGGLE:
	rtdm_safe_copy_from_user(user_info, &pin, arg, sizeof(pin));
	gpio_set_value(pin, 1 - gpio_get_value(pin));
	break;
	    
    case GPIO_IRQ_PIN_WAIT:
	if ((rc = rtdm_event_wait (&ctx->gpio_event)) < 0)
	    return rc;
#if STATS    
	completions++;
#endif	
	break;

    default:
	printk(KERN_WARNING "ioctl: invalid value %d\n", request);
	return -EINVAL;
    }
    return 0;
}

static struct rtdm_device device = {
 struct_version:         RTDM_DEVICE_STRUCT_VER,
 device_flags:           RTDM_NAMED_DEVICE,
 context_size:           sizeof(struct device_ctx),
 device_name:            DEVICE_NAME,
 open_rt:                NULL,
 open_nrt:               gpio_irq_open,

 ops:{
    close_rt:       NULL,
    close_nrt:      gpio_irq_close,
  
    ioctl_rt:       gpio_irq_ioctl_rt,
    ioctl_nrt:      NULL,
  
    read_rt:        NULL,
    read_nrt:       NULL,

    write_rt:       NULL,
    write_nrt:      NULL,   
    },

 device_class:           RTDM_CLASS_EXPERIMENTAL,
 device_sub_class:       RTDM_SUBCLASS_GPIO_IRQ,
 driver_name:            "gpio_irq_rtdm",
 driver_version:         RTDM_DRIVER_VER(0, 0, 0),
 peripheral_name:        "GPIO_IRQ RTDM",
 provider_name:          "machinekit",
 proc_name:              device.device_name,
};

int __init gpio_irq_init(void)
{
#if STATS
    if (timing_pin)
	printk(KERN_WARNING "timing_pin has no effect - #define STATS 1 to enable\n");
#endif
    return rtdm_dev_register(&device); 
}

void __exit gpio_irq_exit(void)
{
    rtdm_dev_unregister (&device, 1000);
}

module_init(gpio_irq_init);
module_exit(gpio_irq_exit); 

