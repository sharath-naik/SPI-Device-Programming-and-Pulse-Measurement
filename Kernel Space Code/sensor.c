
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/errno.h>
#include <linux/math64.h>

#define DRIVER_NAME 		"sensor"
#define DEVICE_NAME 		"sensor"

static dev_t sensor_devnum;      //Allotted Device Number
static struct class *sensor_class;   //Tie with device class
static unsigned char edge_to_detect = 0;


typedef struct Pulse_Device_Tag
{
	struct cdev cdev;               //cdev structure
	char name[20];                  //Device Name
	unsigned int BUSY_FLAG;		  	//Flag to denote whent the sensor is waiting for an echo
	unsigned long long Time_Rising;		//Variable to record the Rising Edge Ocuurence Time
	unsigned long long Time_Falling;		//Variable to record the Falling Edge Ocuurence Time
	int irq;
} sensor_struct;

sensor_struct *sensor_dev;

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long) lo) | ((unsigned long long) hi)<<32;
}


static irqreturn_t change_state_interrupt(int irq, void *dev_id)
{
	if(edge_to_detect==0)
	{
		sensor_dev->Time_Rising = rdtsc();	//Rising edge detected, record the current time
	    irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);	//Program interrupt to detect falling edge
	    edge_to_detect=1;	//Configure the flag for falling edge detection handling
	}
	else
	{
		sensor_dev->Time_Falling = rdtsc();	//Falling edge detected, record the current time
	    irq_set_irq_type(irq, IRQF_TRIGGER_RISING);	//Program the interrupt to detect the rising edge
	    edge_to_detect=0;	//Configure the flag for rising edge detection handling
		sensor_dev->BUSY_FLAG = 0;
	}
	return IRQ_HANDLED;
}

int sensor_open(struct inode *inode, struct file *filp)	//Function that opens the sensor device
{
	int irq_line;
	int irq_req_res_rising;
	int retValue;
	sensor_dev->BUSY_FLAG = 0;
	sensor_dev = container_of(inode->i_cdev, sensor_struct, cdev);	//Get the device structure that contains the cdev
	filp->private_data = sensor_dev;
	//Set GPIO pins directions and values
	retValue= gpio_request_one(61, GPIOF_OUT_INIT_LOW , "gpio13");
	gpio_request_one(14, GPIOF_OUT_INIT_LOW , "gpio62");
	gpio_request_one(16, 1, "gpio34");
	gpio_request(77, "Trig_Mux1");
	gpio_request(76, "Echo_Mux1");
	gpio_request(64, "Trig_Mux2");
	//Set GPIO pins values
	gpio_set_value_cansleep(61, 0);
	gpio_set_value_cansleep(14, 0);
	gpio_set_value_cansleep(77, 0);
	gpio_set_value_cansleep(76, 0);
	gpio_set_value_cansleep(64, 0);
	gpio_free(14);
	gpio_request_one(14, GPIOF_IN , "gpio13");
	irq_line = gpio_to_irq(14);	//Initialise interrupt to detect from gpio14
	if(irq_line < 0)
	{
		printk("Gpio %d cannot be used as interrupt",14);
		retValue=-EINVAL;
	}
	//Initialise the device structure values
	sensor_dev->irq = irq_line;	
	sensor_dev->Time_Rising=0;
	sensor_dev->Time_Falling=0;
	irq_req_res_rising = request_irq(irq_line, change_state_interrupt, IRQF_TRIGGER_RISING, "gpio_change_state", sensor_dev);
	if(irq_req_res_rising)
	{
		printk("Unable to claim irq %d; error %d\n ", irq_line, irq_req_res_rising);
		return 0;
	}
	printk("Sensor Device Opened\n");
	return 0;
}

int sensor_close(struct inode *inode, struct file *filp)	//Function that closes the sensor device
{
	sensor_struct *local_sensor_dev;
	sensor_dev->BUSY_FLAG = 0;
	local_sensor_dev = filp->private_data;
	free_irq(sensor_dev->irq,sensor_dev);	//Free the interrupt handler
	//Free the gpio pins used
	gpio_free(61);
	gpio_free(14);
	gpio_free(77);
	gpio_free(76);
	gpio_free(64);
	gpio_free(16);
	
	printk("Sensor Device closing\n");
	return 0;
}
static ssize_t sensor_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)	//Function that sends the trigger pulse
{
	int retValue = 0;
	
	if(sensor_dev->BUSY_FLAG == 1)
	{
		retValue = -EBUSY;
		return -EBUSY;
	}
	
	gpio_set_value_cansleep(61,0);
	udelay(2);
	gpio_set_value_cansleep(61, 1);
	udelay(12);
	gpio_set_value_cansleep(61, 0);
	sensor_dev->BUSY_FLAG = 1;	//Set the flag to denote that the sensor is waiting for an echo
	return retValue;
}

static ssize_t sensor_read(struct file *file, char *buf, size_t count, loff_t *ptr)	//Function to detect the echo pulse recieved
{
	int retValue=0;
	unsigned int c;
	unsigned long long tempBuffer;
	if(sensor_dev->BUSY_FLAG == 1)
	{
		return -EBUSY;
	}
	else
	{
		if(sensor_dev->Time_Rising == 0 && sensor_dev->Time_Falling == 0)
		{
			printk("Please Trigger the measure first\n");
		}
		else
		{
			tempBuffer = sensor_dev->Time_Falling - sensor_dev->Time_Rising;	//Calculate the difference in time between rising and falling edges
			c = div_u64(tempBuffer,400);
			retValue = copy_to_user((void *)buf, (const void *)&c, sizeof(c));	//Sending the calculated distance to user space
		}
	}
	return retValue;
}
static struct file_operations sensor_fops =	//The file operation functions of the device
{
		.owner = THIS_MODULE,			/* Owner */
		.open = sensor_open,             /* Open method */
		.release = sensor_close,       /* Release method */
		.write = sensor_write,           /* Write method */
		.read = sensor_read				/* Read method */
};
static int __init sensor_driver_init(void)
{
	int retValue;
	if(alloc_chrdev_region(&sensor_devnum, 0, 0, DRIVER_NAME) < 0)	//Dynamic allocation of the device number
	{
		printk("Can't register device\n");
		return -1;
	}
	sensor_class = class_create(THIS_MODULE, DRIVER_NAME);	//Populate sysfs entries
	sensor_dev = kmalloc(sizeof(sensor_struct), GFP_KERNEL);	//Allocate memory for the device structure
	if(!sensor_dev)
	{
		printk("Bad Kmalloc sensor_dev\n");
		return -ENOMEM;
	}
	sprintf(sensor_dev->name, DRIVER_NAME);
	cdev_init(&sensor_dev->cdev, &sensor_fops);	//Connect the file operations with the cdev
	sensor_dev->cdev.owner = THIS_MODULE;
	retValue = cdev_add(&sensor_dev->cdev, MKDEV(MAJOR(sensor_devnum), 0), 1);	//Connect the major/minor number to the cdev
	if(retValue)
	{
		printk("Bad cdev for sensor_dev\n");
		return retValue;
	}
	device_create(sensor_class, NULL, MKDEV(MAJOR(sensor_devnum), 0), NULL, DEVICE_NAME);	
	printk("Sensor Driver Initialised\n");
	return 0;
}
static void __exit sensor_driver_close(void)
{
	device_destroy(sensor_class, MKDEV(MAJOR(sensor_devnum), 0));	//Destroy the created device
	cdev_del(&sensor_dev->cdev);
	kfree(sensor_dev);	//Free allocated memory in the kernel space for the device structure
	class_destroy(sensor_class);	//Destroy driver class
	unregister_chrdev_region(sensor_devnum, 1);	//Unregister the major number
	printk("Sensor Driver Closing\n");
}

module_init(sensor_driver_init);
module_exit(sensor_driver_close);
MODULE_LICENSE("GPL v2");

