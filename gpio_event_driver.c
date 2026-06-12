#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/poll.h>

#define DEV_NAME "gpio_event"
#define GPIO_BASE 512
#define LED_GPIO_BCM 17
#define BUTTON_GPIO_BCM 27
#define LED_GPIO (GPIO_BASE + LED_GPIO_BCM)
#define BUTTON_GPIO (GPIO_BASE + BUTTON_GPIO_BCM)
#define DEBOUNCE_MS 20
#define GPIO_EVENT_FIFO_SIZE 32

struct gpio_event {

	u32 seq;
	int value;

};

static DECLARE_KFIFO(gpio_event_fifo, struct gpio_event, GPIO_EVENT_FIFO_SIZE);
static DEFINE_SPINLOCK(gpio_event_lock);
static DECLARE_WAIT_QUEUE_HEAD(gpio_event_wq);
static u32 gpio_event_seq;
static dev_t gpio_event_devt;
static struct cdev gpio_event_cdev;
static struct class *gpio_event_class;
static struct device *gpio_event_device;
static int button_irq;
static struct delayed_work button_debounce_work;

static bool gpio_event_fifo_empty(void)
{
	unsigned long flags;
	bool empty;

	spin_lock_irqsave(&gpio_event_lock, flags);
	empty = kfifo_is_empty(&gpio_event_fifo);
	spin_unlock_irqrestore(&gpio_event_lock, flags);
	
    return empty;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    mod_delayed_work(system_wq, &button_debounce_work, msecs_to_jiffies(DEBOUNCE_MS));
    return IRQ_HANDLED;
}

static void button_debounce_work_func(struct work_struct *work)
{
    struct gpio_event event;
	unsigned long flags;
	unsigned int copied;

	event.value = gpio_get_value(BUTTON_GPIO);

	spin_lock_irqsave(&gpio_event_lock, flags);

	event.seq = ++gpio_event_seq;
	copied = kfifo_in(&gpio_event_fifo, &event, 1);

	spin_unlock_irqrestore(&gpio_event_lock, flags);
	
    if (copied != 1) {
		pr_warn("gpio_event: fifo full, event dropped\n");
		return;
	}

	wake_up_interruptible(&gpio_event_wq);

	pr_info("gpio_event: button event seq=%u value=%d\n",
		event.seq, event.value);
}

static ssize_t gpio_event_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct gpio_event event;
	unsigned long flags;
	unsigned int copied;
	int ret;

	if (count < sizeof(event))
		return -EINVAL;

	if (file->f_flags & O_NONBLOCK) {
		if (gpio_event_fifo_empty())
			return -EAGAIN;
	} else {
		ret = wait_event_interruptible(
			gpio_event_wq,
			!gpio_event_fifo_empty());

		if (ret)
			return ret;
	}

	spin_lock_irqsave(&gpio_event_lock, flags);
	copied = kfifo_out(&gpio_event_fifo, &event, 1);
	spin_unlock_irqrestore(&gpio_event_lock, flags);

	if (copied != 1)
		return -EAGAIN;

	if (copy_to_user(buf, &event, sizeof(event)))
		return -EFAULT;
    
	return sizeof(event);
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

static __poll_t gpio_event_poll(struct file *file, poll_table *wait)
{
    __poll_t mask = 0;

    poll_wait(file, &gpio_event_wq, wait);

    if (!gpio_event_fifo_empty())
        mask |= POLLIN | POLLRDNORM;

    return mask;
}

static const struct file_operations gpio_event_fops = {
    .owner = THIS_MODULE,
    .read = gpio_event_read,
    .write = gpio_event_write,
    .poll = gpio_event_poll,
};

static int __init gpio_event_init(void)
{
    int ret;

	INIT_KFIFO(gpio_event_fifo);
	gpio_event_seq = 0;

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
        goto err_gpio_free_led;

    ret = gpio_request(BUTTON_GPIO, "gpio_event_button");
    if (ret)
        goto err_gpio_free_led;

    ret = gpio_direction_input(BUTTON_GPIO);
    if (ret)
        goto err_gpio_free_button;

    INIT_DELAYED_WORK(&button_debounce_work, button_debounce_work_func);

    button_irq = gpio_to_irq(BUTTON_GPIO);
    if (button_irq < 0) {
        ret = button_irq;
        goto err_gpio_free_button;
    }

    ret = request_irq(button_irq, button_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_event_button_irq", NULL);
    if (ret)
        goto err_gpio_free_button;

    gpio_event_device = device_create(gpio_event_class, NULL, gpio_event_devt, NULL, DEV_NAME);
    if (IS_ERR(gpio_event_device)) {
        ret = PTR_ERR(gpio_event_device);
        goto err_free_button_irq;
    }

    return 0;

err_free_button_irq:
    free_irq(button_irq, NULL);
err_gpio_free_button:
    gpio_free(BUTTON_GPIO);
err_gpio_free_led:
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
    free_irq(button_irq, NULL);
    cancel_delayed_work_sync(&button_debounce_work);
    gpio_free(BUTTON_GPIO);
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