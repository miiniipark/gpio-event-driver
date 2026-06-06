#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/gpio.h>

#define DEV_NAME "gpio_event"
#define GPIO_BASE 512
#define LED_GPIO_BCM 17
#define LED_GPIO (GPIO_BASE + LED_GPIO_BCM)

static dev_t gpio_event_devt;
static struct cdev gpio_event_cdev;
static struct class *gpio_event_class;
static struct device *gpio_event_device;
static char data[32];

static ssize_t gpio_event_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    if (*ppos >= sizeof(data))
        return 0;

    if (count > sizeof(data) - *ppos)
        count = sizeof(data) - *ppos;

    if (copy_to_user(buf, data + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static ssize_t gpio_event_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char data;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&data, buf, 1))
        return -EFAULT;

    if (data == '1')
        gpio_set_value(LED_GPIO, 1);
    else if (data == '0')
        gpio_set_value(LED_GPIO, 0);
    else
        return -EINVAL;

    return count;
}

static const struct file_operations gpio_event_fops = {
    .owner = THIS_MODULE,
    .read = gpio_event_read,
    .write = gpio_event_write,
};

static int __init gpio_event_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&gpio_event_devt, 0, 1, DEV_NAME);
    if (ret)
        return ret;
    
    cdev_init(&gpio_event_cdev, &gpio_event_fops);

    ret = cdev_add(&gpio_event_cdev, gpio_event_devt, 1);
    if (ret)
        goto err_unregister_chrdev;

    gpio_event_class = class_create(DEV_NAME);
    if (IS_ERR(gpio_event_class)) {
        ret = PTR_ERR(gpio_event_class);
        goto err_cdev_del;
    }

    ret = gpio_request(LED_GPIO, "gpio_event_led");
    if (ret)
        goto err_class_destroy;
    
    ret = gpio_direction_output(LED_GPIO, 0);
    if (ret)
        goto err_gpio_free;

    gpio_event_device = device_create(gpio_event_class, NULL, gpio_event_devt, NULL, DEV_NAME);
    if (IS_ERR(gpio_event_device)) {
        ret = PTR_ERR(gpio_event_device);
        goto err_gpio_free;
    }

    return 0;

err_gpio_free:
    gpio_free(LED_GPIO);
err_class_destroy:
    class_destroy(gpio_event_class);
err_cdev_del:
    cdev_del(&gpio_event_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(gpio_event_devt, 1);
    return ret;
}

static void __exit gpio_event_exit(void)
{
    device_destroy(gpio_event_class, gpio_event_devt);
    gpio_free(LED_GPIO);
    class_destroy(gpio_event_class);
    cdev_del(&gpio_event_cdev);
    unregister_chrdev_region(gpio_event_devt, 1);
}

module_init(gpio_event_init);
module_exit(gpio_event_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("miiniipark");
MODULE_DESCRIPTION("GPIO Event Driver");