#include <linux/module.h>

static int __init gpio_event_init(void)
{
    pr_info("gpio_event_driver loaded\n");
    return 0;
}

static void __exit gpio_event_exit(void)
{
    pr_info("gpio_event_driver unloaded\n");
}

module_init(gpio_event_init);
module_exit(gpio_event_exit);

MODULE_LICENSE("GPL");