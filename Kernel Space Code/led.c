#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#define DRIVER_NAME 		"spidev"
#define DEVICE_NAME 		"spidev1.1"
#define DEVICE_CLASS_NAME 	"spidev"
#define MINOR_NUMBER    0
#define MAJOR_NUMBER    154     /* assigned */


static DEFINE_MUTEX(device_list_lock);

struct spidev_data {		//Device Structure
	dev_t                   devt;	//Alloted Device nuumbers
	struct spi_device       *spi;
	char pattern_buffer[12][8];
	unsigned int sequence_buffer[10][2];
};
struct spi_device_id SpiLedDeviceID[] = {
	{"spidev",0},{}
	};
static struct spidev_data *spidevice;
static struct class *spidevclass;   	//Tie with device class
static unsigned bufsiz = 4096;
static unsigned int busyFlag=0;		//Flag to denote whether device is currently in another operation
static struct spi_message m;
static unsigned char ch_tx[2]={0};
static unsigned char ch_rx[2]={0};
static struct spi_transfer t = {	//Strucutre passed to spi device with every transmit
			.tx_buf = &ch_tx[0],
			.rx_buf = &ch_rx[0],
			.len = 2,
			.cs_change = 1,
			.bits_per_word = 8,
			.speed_hz = 500000,
			 };
static void spdev_send(unsigned char ch1, unsigned char ch2)	//Function that transmits message to SPI device
{
    int ret=0;
    ch_tx[0] = ch1;
    ch_tx[1] = ch2;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	gpio_set_value(15,0);	//Chip select
	ret = spi_sync(spidevice->spi, &m);
	gpio_set_value(15,1);
	return;
}

static int spidvc_open(struct inode *inode, struct file *filp)	//Function that opens the spi device 
{
	//Set GPIO pin directions
	gpio_request_one(24, GPIOF_OUT_INIT_LOW , "gpio13");
	gpio_request(42, "IO 2 MUX");
	gpio_request(30, "IO 2 MUX");
	gpio_request(44,"IO 2 MUX");
	gpio_request(46, "IO 2 MUX");
	gpio_request(72, "IO 2 MUX");
	gpio_request(15, "IO 2 MUX");
	gpio_direction_output(42,0);
	gpio_direction_output(30,0);
	gpio_direction_output(44,0);
	gpio_direction_output(46,0);
	gpio_direction_output(15,0);
	//Set GPIO pin values
	gpio_set_value_cansleep(24, 0);
	gpio_set_value_cansleep(42, 0);
	gpio_set_value_cansleep(30, 0);
	gpio_set_value_cansleep(44, 1);
	gpio_set_value_cansleep(46, 1);
	gpio_set_value_cansleep(72, 0);
	gpio_set_value_cansleep(15, 1);
	gpio_set_value_cansleep(72,0);
	busyFlag = 0;
	//Initialising the function registers of the led matrix
	spdev_send(0x0F, 0x00);	
	spdev_send(0x09, 0x00);
	spdev_send(0x0A, 0x01);
	spdev_send(0x0B, 0x07);
	spdev_send(0x0C, 0x01);

	return 0;
}

static int spidvc_close(struct inode *inode, struct file *filp)	//Function that closes the spi device
{
    int status = 0;
    unsigned char i=0;
    busyFlag = 0;
	for(i=1; i < 9; i++)	//Loop that clears all the data bits in the matrix
	{
		spdev_send(i, 0x00);
	}
	
	printk("Spidev is closing\n");
	return status;
}

int thread_spi_led_write(void *data)
{
	int i=0, j=0, k=0;
	
	if(spidevice->sequence_buffer[0][0] == 0 && spidevice->sequence_buffer[0][1] == 0)
	{
		for(k=1; k < 9; k++)
		{
			spdev_send(k, 0x00);
		}
		busyFlag = 0;
		goto sequenceEnd;
	}
				
	//If sequence pattern followed by 0,0 is present, then display the pattern in loop upto 0,0.
	for(j=0;j<10;j++) //loop for sequence order
	{
		for(i=0;i<12;i++)//loop for pattern number
		{
			if(spidevice->sequence_buffer[j][0] == i)
			{
				if(spidevice->sequence_buffer[j][0] == 0 && spidevice->sequence_buffer[j][1] == 0)
				{	
					for(k=1; k < 9; k++)//clear the display when 0,0 is reached
					{
						spdev_send(k, 0x00);
					}
					
					
					busyFlag = 0;
					goto sequenceEnd;
				}
				else
				{
					spdev_send(0x01, spidevice->pattern_buffer[i][0]);
					spdev_send(0x02, spidevice->pattern_buffer[i][1]);
					spdev_send(0x03, spidevice->pattern_buffer[i][2]);
					spdev_send(0x04, spidevice->pattern_buffer[i][3]);
					spdev_send(0x05, spidevice->pattern_buffer[i][4]);
					spdev_send(0x06, spidevice->pattern_buffer[i][5]);
					spdev_send(0x07, spidevice->pattern_buffer[i][6]);
					spdev_send(0x08, spidevice->pattern_buffer[i][7]);
					
					msleep(spidevice->sequence_buffer[j][1]);
				}
			}
		}
	}
	sequenceEnd:
	busyFlag = 0;
	return 0;
}
static ssize_t spidvc_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)	//Function to write the data to the spi device
{
	int retValue = 0, i=0, j=0;
	unsigned  int sequenceBuffer[20];
	struct task_struct *task;
	                                       
	if(busyFlag == 1)
	{
		return -EBUSY;
	}
	if (count > bufsiz)
	{
		return -EMSGSIZE;
	}
	retValue = copy_from_user((void *)&sequenceBuffer, (void *)buf, sizeof(sequenceBuffer));	//Command to copy data from the user space to the kernel space
	for(i=0;i<20;i=i+2)
	{
		j=i/2;
		spidevice->sequence_buffer[j][0] = sequenceBuffer[i];
		spidevice->sequence_buffer[j][1] = sequenceBuffer[i+1];
	}
	if(retValue != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",retValue);
	}
	
	busyFlag = 1;

    task = kthread_run(&thread_spi_led_write, (void *)sequenceBuffer,"kthread_spi_led");

	return retValue;
}

static long spidvc_ioctl(struct file *filp, unsigned int arg, unsigned long cmd)	//Function that helps with the I/O control of the spi device
{
	int i=0, j=0;
	char writeBuffer[10][8];
	int retValue;
	retValue = copy_from_user((void *)&writeBuffer, (void * )arg, sizeof(writeBuffer));
	if(retValue != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",retValue);
	}

	for(i=0;i<10;i++)
	{
		for(j=0;j<8;j++)
		{
			spidevice->pattern_buffer[i][j] = writeBuffer[i][j];
		}
	}
	return retValue;
}
static struct file_operations our_fops = {		//The file operations on the particular device
  .owner   			= THIS_MODULE,
  .write   			= spidvc_write,
  .open    			= spidvc_open,
  .release 			= spidvc_close,
  .unlocked_ioctl   = spidvc_ioctl,
};

static int spidev_probe(struct spi_device *spi)	//This is a probe function which is called when a SPI device is added or initialised 
{
	int status = 0;
	struct device *dev;
	spidevice = kzalloc(sizeof(*spidevice), GFP_KERNEL);
	if(!spidevice)
	{
		return -ENOMEM;
	}
	spidevice->spi = spi;

	spidevice->devt = MKDEV(MAJOR_NUMBER, MINOR_NUMBER);

    dev = device_create(spidevclass, &spi->dev, spidevice->devt, spidevice, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("Device Creation Failed\n");
		kfree(spidevice);
		return -1;
	}
	printk("SPI LED Driver Probed.\n");
	return status;
}

static int spidev_remove(struct spi_device *spi)	//Remove function called when the SPI device is remvoved
{
	int retValue=0;
	
	device_destroy(spidevclass, spidevice->devt);
	kfree(spidevice);
	printk("SPI LED Driver Removed.\n");
	return retValue;
}

static struct spi_driver spi_led_driver = {
		 .id_table=SpiLedDeviceID,
         .driver = {
                 .name =         "spidev",
                 .owner =        THIS_MODULE,
         },
         .probe =        spidev_probe,
         .remove =       spidev_remove,
};

static int __init spidrvr_init(void)
{
	int retValue;
	retValue = register_chrdev(MAJOR_NUMBER, DEVICE_NAME, &our_fops);	//Register the device with a fixed major and minor number and link it to the file operations structure
	if(retValue < 0)
	{
		printk("Device Registration Failed\n");
		return -1;
	}
	spidevclass = class_create(THIS_MODULE, DEVICE_CLASS_NAME);	//Populate sysfs entries
	if(spidevclass == NULL)
	{
		printk("Class Creation Failed\n");
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	retValue = spi_register_driver(&spi_led_driver);	//Register the spi driver with the file operations
	if(retValue < 0)
	{
		printk("Driver Registraion Failed\n");
		class_destroy(spidevclass);
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	printk("SPI LED Driver Initialized.\n");
	return retValue;
}

static void __exit spidrvr_exit(void)
{
	spi_unregister_driver(&spi_led_driver);	//Unregister the SPI device
	class_destroy(spidevclass);	//Destroy the driver class
	unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);	//Unregister the major and minor number associated
	printk("SPI LED Driver Uninitialized.\n");
}
MODULE_LICENSE("GPL v2");

module_init(spidrvr_init);
module_exit(spidrvr_exit);
