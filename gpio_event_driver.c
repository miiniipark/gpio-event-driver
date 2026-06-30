#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/sysfs.h>
#include <linux/compiler.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#define GPIO_EVENT_NAME			"gpio-event"
#define GPIO_EVENT_DEV_NAME		"gpio_event"
#define GPIO_EVENT_CLASS_NAME		"gpio_event"

#define GPIO_EVENT_BUTTON_GPIO		539
#define GPIO_EVENT_LED_GPIO		529

#define GPIO_EVENT_FIFO_SIZE		16

#define GPIO_EVENT_DEFAULT_DEBOUNCE_MS	20
#define GPIO_EVENT_MIN_DEBOUNCE_MS	1
#define GPIO_EVENT_MAX_DEBOUNCE_MS	1000

#define GPIO_EVENT_IOC_MAGIC		'g'
#define GPIO_EVENT_IOC_SET_DEBOUNCE_MS	_IOW(GPIO_EVENT_IOC_MAGIC, 1, unsigned int)
#define GPIO_EVENT_IOC_GET_DEBOUNCE_MS	_IOR(GPIO_EVENT_IOC_MAGIC, 2, unsigned int)

struct gpio_event_pdata {
	unsigned int button_gpio;
	unsigned int led_gpio;
};

struct gpio_event {
	struct device *dev;

	unsigned int button_gpio;
	unsigned int led_gpio;
	int irq;

	struct cdev cdev;
	struct device *chardev;

	wait_queue_head_t read_wq;
	spinlock_t fifo_lock;
	char fifo[GPIO_EVENT_FIFO_SIZE];
	unsigned int head;
	unsigned int tail;

	struct delayed_work debounce_work;
	unsigned int debounce_ms;
};

static dev_t gpio_event_devt;
static struct class *gpio_event_class;
static struct platform_device *gpio_event_pdev;

static const struct gpio_event_pdata gpio_event_pdata = {
	.button_gpio = GPIO_EVENT_BUTTON_GPIO,
	.led_gpio = GPIO_EVENT_LED_GPIO,
};

static bool gpio_event_fifo_empty(struct gpio_event *ge)
{
	return ge->head == ge->tail;
}

static bool gpio_event_fifo_full(struct gpio_event *ge)
{
	return ((ge->head + 1) % GPIO_EVENT_FIFO_SIZE) == ge->tail;
}

static bool gpio_event_has_data(struct gpio_event *ge)
{
	unsigned long flags;
	bool has_data;

	spin_lock_irqsave(&ge->fifo_lock, flags);
	has_data = !gpio_event_fifo_empty(ge);
	spin_unlock_irqrestore(&ge->fifo_lock, flags);

	return has_data;
}

static void gpio_event_fifo_push(struct gpio_event *ge, char val)
{
	unsigned long flags;

	spin_lock_irqsave(&ge->fifo_lock, flags);

	if (gpio_event_fifo_full(ge))
		ge->tail = (ge->tail + 1) % GPIO_EVENT_FIFO_SIZE;

	ge->fifo[ge->head] = val;
	ge->head = (ge->head + 1) % GPIO_EVENT_FIFO_SIZE;

	spin_unlock_irqrestore(&ge->fifo_lock, flags);
}

static int gpio_event_fifo_pop(struct gpio_event *ge, char *val)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ge->fifo_lock, flags);

	if (gpio_event_fifo_empty(ge)) {
		ret = -ENOENT;
		goto out;
	}

	*val = ge->fifo[ge->tail];
	ge->tail = (ge->tail + 1) % GPIO_EVENT_FIFO_SIZE;

out:
	spin_unlock_irqrestore(&ge->fifo_lock, flags);
	return ret;
}

static int gpio_event_open(struct inode *inode, struct file *file)
{
	struct gpio_event *ge;

	ge = container_of(inode->i_cdev, struct gpio_event, cdev);
	file->private_data = ge;

	return 0;
}

static int gpio_event_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t gpio_event_read(struct file *file,
			       char __user *buf,
			       size_t count,
			       loff_t *ppos)
{
	struct gpio_event *ge = file->private_data;
	char val;
	int ret;

	if (count < 1)
		return -EINVAL;

	for (;;) {
		ret = gpio_event_fifo_pop(ge, &val);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(ge->read_wq,
					       gpio_event_has_data(ge));
		if (ret)
			return ret;
	}

	if (copy_to_user(buf, &val, 1))
		return -EFAULT;

	return 1;
}

static __poll_t gpio_event_poll(struct file *file, poll_table *wait)
{
	struct gpio_event *ge = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &ge->read_wq, wait);

	if (gpio_event_has_data(ge))
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static long gpio_event_ioctl(struct file *file,
			     unsigned int cmd,
			     unsigned long arg)
{
	struct gpio_event *ge = file->private_data;
	unsigned int val;

	switch (cmd) {
	case GPIO_EVENT_IOC_SET_DEBOUNCE_MS:
		if (copy_from_user(&val, (unsigned int __user *)arg,
				   sizeof(val)))
			return -EFAULT;

		if (val < GPIO_EVENT_MIN_DEBOUNCE_MS ||
		    val > GPIO_EVENT_MAX_DEBOUNCE_MS)
			return -EINVAL;

		WRITE_ONCE(ge->debounce_ms, val);
		return 0;

	case GPIO_EVENT_IOC_GET_DEBOUNCE_MS:
		val = READ_ONCE(ge->debounce_ms);

		if (copy_to_user((unsigned int __user *)arg, &val,
				 sizeof(val)))
			return -EFAULT;

		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations gpio_event_fops = {
	.owner		= THIS_MODULE,
	.open		= gpio_event_open,
	.release	= gpio_event_release,
	.read		= gpio_event_read,
	.poll		= gpio_event_poll,
	.unlocked_ioctl	= gpio_event_ioctl,
};

static ssize_t debounce_ms_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct gpio_event *ge = dev_get_drvdata(dev);
	unsigned int val;

	val = READ_ONCE(ge->debounce_ms);

	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t debounce_ms_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t count)
{
	struct gpio_event *ge = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	if (val < GPIO_EVENT_MIN_DEBOUNCE_MS ||
	    val > GPIO_EVENT_MAX_DEBOUNCE_MS)
		return -EINVAL;

	WRITE_ONCE(ge->debounce_ms, val);

	return count;
}

static DEVICE_ATTR_RW(debounce_ms);

static void gpio_event_debounce_work(struct work_struct *work)
{
	struct gpio_event *ge;
	int val;

	ge = container_of(to_delayed_work(work),
			  struct gpio_event,
			  debounce_work);

	val = gpio_get_value(ge->button_gpio);
	gpio_set_value(ge->led_gpio, val);

	gpio_event_fifo_push(ge, val ? '1' : '0');
	wake_up_interruptible(&ge->read_wq);
}

static irqreturn_t gpio_event_irq(int irq, void *dev_id)
{
	struct gpio_event *ge = dev_id;
	unsigned int debounce_ms;

	debounce_ms = READ_ONCE(ge->debounce_ms);

	mod_delayed_work(system_wq,
			 &ge->debounce_work,
			 msecs_to_jiffies(debounce_ms));

	return IRQ_HANDLED;
}

static int gpio_event_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct gpio_event_pdata *pdata;
	struct gpio_event *ge;
	int ret;

	pdata = dev_get_platdata(dev);
	if (!pdata)
		return -EINVAL;

	ge = devm_kzalloc(dev, sizeof(*ge), GFP_KERNEL);
	if (!ge)
		return -ENOMEM;

	ge->dev = dev;
	ge->button_gpio = pdata->button_gpio;
	ge->led_gpio = pdata->led_gpio;
	ge->debounce_ms = GPIO_EVENT_DEFAULT_DEBOUNCE_MS;

	spin_lock_init(&ge->fifo_lock);
	init_waitqueue_head(&ge->read_wq);
	INIT_DELAYED_WORK(&ge->debounce_work, gpio_event_debounce_work);

	platform_set_drvdata(pdev, ge);

	ret = devm_gpio_request_one(dev,
				    ge->led_gpio,
				    GPIOF_OUT_INIT_LOW,
				    "gpio_event_led");
	if (ret) {
		dev_err(dev, "failed to request LED GPIO %u: %d\n",
			ge->led_gpio, ret);
		return ret;
	}

	ret = devm_gpio_request_one(dev,
				    ge->button_gpio,
				    GPIOF_IN,
				    "gpio_event_button");
	if (ret) {
		dev_err(dev, "failed to request button GPIO %u: %d\n",
			ge->button_gpio, ret);
		return ret;
	}

	ge->irq = gpio_to_irq(ge->button_gpio);
	if (ge->irq < 0) {
		dev_err(dev, "failed to get IRQ from GPIO %u: %d\n",
			ge->button_gpio, ge->irq);
		return ge->irq;
	}

	ret = devm_request_irq(dev,
			       ge->irq,
			       gpio_event_irq,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			       GPIO_EVENT_NAME,
			       ge);
	if (ret) {
		dev_err(dev, "failed to request IRQ %d: %d\n",
			ge->irq, ret);
		return ret;
	}

	cdev_init(&ge->cdev, &gpio_event_fops);
	ge->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ge->cdev, gpio_event_devt, 1);
	if (ret) {
		dev_err(dev, "failed to add cdev: %d\n", ret);
		return ret;
	}

	ge->chardev = device_create(gpio_event_class,
				    dev,
				    gpio_event_devt,
				    ge,
				    GPIO_EVENT_DEV_NAME);
	if (IS_ERR(ge->chardev)) {
		ret = PTR_ERR(ge->chardev);
		dev_err(dev, "failed to create device: %d\n", ret);
		goto err_cdev_del;
	}

	ret = device_create_file(ge->chardev, &dev_attr_debounce_ms);
	if (ret) {
		dev_err(dev, "failed to create debounce_ms sysfs: %d\n", ret);
		goto err_device_destroy;
	}

	dev_info(dev, "probed: button_gpio=%u led_gpio=%u irq=%d\n",
		 ge->button_gpio, ge->led_gpio, ge->irq);

	return 0;

err_device_destroy:
	device_destroy(gpio_event_class, gpio_event_devt);

err_cdev_del:
	cdev_del(&ge->cdev);

	return ret;
}

static void gpio_event_remove(struct platform_device *pdev)
{
	struct gpio_event *ge = platform_get_drvdata(pdev);

	if (!ge)
		return;

	devm_free_irq(&pdev->dev, ge->irq, ge);
	cancel_delayed_work_sync(&ge->debounce_work);

	device_remove_file(ge->chardev, &dev_attr_debounce_ms);
	device_destroy(gpio_event_class, gpio_event_devt);
	cdev_del(&ge->cdev);

	dev_info(&pdev->dev, "removed\n");
}

static struct platform_driver gpio_event_driver = {
	.probe = gpio_event_probe,
	.remove = gpio_event_remove,
	.driver = {
		.name = GPIO_EVENT_NAME,
	},
};

static int __init gpio_event_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&gpio_event_devt,
				  0,
				  1,
				  GPIO_EVENT_DEV_NAME);
	if (ret)
		return ret;

	gpio_event_class = class_create(GPIO_EVENT_CLASS_NAME);
	if (IS_ERR(gpio_event_class)) {
		ret = PTR_ERR(gpio_event_class);
		goto err_unregister_chrdev;
	}

	ret = platform_driver_register(&gpio_event_driver);
	if (ret)
		goto err_class_destroy;

	gpio_event_pdev = platform_device_register_data(NULL,
							GPIO_EVENT_NAME,
							PLATFORM_DEVID_NONE,
							&gpio_event_pdata,
							sizeof(gpio_event_pdata));
	if (IS_ERR(gpio_event_pdev)) {
		ret = PTR_ERR(gpio_event_pdev);
		goto err_unregister_driver;
	}

	pr_info("loaded\n");

	return 0;

err_unregister_driver:
	platform_driver_unregister(&gpio_event_driver);

err_class_destroy:
	class_destroy(gpio_event_class);

err_unregister_chrdev:
	unregister_chrdev_region(gpio_event_devt, 1);

	return ret;
}

static void __exit gpio_event_exit(void)
{
	platform_device_unregister(gpio_event_pdev);
	platform_driver_unregister(&gpio_event_driver);
	class_destroy(gpio_event_class);
	unregister_chrdev_region(gpio_event_devt, 1);

	pr_info("unloaded\n");
}

module_init(gpio_event_init);
module_exit(gpio_event_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("miiniipark");
MODULE_DESCRIPTION("GPIO Event platform driver");