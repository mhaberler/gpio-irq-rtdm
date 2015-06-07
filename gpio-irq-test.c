#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>
#include "gpio-irq.h"

RT_TASK demo_task;
int fd;
int pin = 69;
int timingpin = 0;
int tpretcode;

char *rtdm_driver = "gpio_irq";
#define EVERY 1000


static inline void toggle_timing_pin(void)
{
    if (!timingpin)
	return;
    tpretcode = rt_dev_ioctl(fd, GPIO_IRQ_PIN_TOGGLE, &timingpin);
}

void demo(void *arg)
{
    RTIME now, previous;
    int irqs = EVERY;
    struct gpio_irq_data rd = {
	.pin = pin,
	.falling =  0
    };

    if ((fd = rt_dev_ioctl(fd,  GPIO_IRQ_BIND, &rd)) < 0) {
	perror("rt_dev_ioctl GPIO_IRQ_BIND");
	return;
    }
    
    previous = rt_timer_read();
    int rc;
    
    while (1) {
	toggle_timing_pin();
	toggle_timing_pin();
	if ((rc = rt_dev_ioctl (fd,  GPIO_IRQ_PIN_WAIT, 0)) < 0) {
	    rt_printf("rt_dev_ioctl error! rc=%d\n", rc);
	    break;
	}
	irqs--;
	if (!irqs)  {
	    irqs = EVERY;
	    rt_printf("%d IRQs, tpretcode=%d\n",EVERY, tpretcode);
	}  
    }
}

void catch_signal(int sig)
{
    fprintf (stderr, "catch_signal sig=%d\n", sig);
    signal(SIGTERM,  SIG_DFL);
    signal(SIGINT, SIG_DFL);
    rt_task_delete(&demo_task);
}

int main(int argc, char* argv[])
{
    if (argc > 1)
	pin = atoi(argv[1]);
    if (argc > 2)
	timingpin = atoi(argv[2]);

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);

    /* Avoids memory swapping for this program */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    // Init rt_printf() system
    rt_print_auto_init(1);
	
    // Open RTDM driver
    if ((fd = rt_dev_open(rtdm_driver, 0)) < 0) {
	perror("rt_open");
	exit(-1);
    }

    rt_task_create(&demo_task, "trivial", 0, 99, 0);
    rt_task_start(&demo_task, &demo, NULL);
    pause();
    rt_task_delete(&demo_task);
    return 0;
}
