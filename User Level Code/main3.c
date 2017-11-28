#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <inttypes.h>
#include "Gpio_func.h"
#define DEVICE "/dev/spidev1.0" 
pthread_t polling_thread,display_thread;
pthread_mutex_t lock;
long distance = 1;
long delay;
int fd_spi;
uint8_t set_row[2];


uint8_t right_pos_1 [] = {	
	
		0x01, 0b00111100,
		0x02, 0b01111110,	
		0x03, 0b11111011,
		0x04, 0b11111111,
		0x05, 0b11101111,
		0x06, 0b11101111,
		0x07, 0b01101110,
		0x08, 0b00101100,
};

uint8_t right_pos_2 [] = {

		0x01, 0b00111100,
		0x02, 0b01111110,	
		0x03, 0b11111011,
		0x04, 0b11111111,
		0x05, 0b11111111,
		0x06, 0b11101111,
		0x07, 0b01000110,
		0x08, 0b00000000,
};
uint8_t left_pos_1 [] = {

		0x01, 0b01101100,
		0x02, 0b01101110,	
		0x03, 0b11101111,
		0x04, 0b11101111,
		0x05, 0b11111111,
		0x06, 0b11111011,
		0x07, 0b01111110,
		0x08, 0b00111100,
		
};

uint8_t left_pos_2 [] = {

		0x01, 0b00000000,
		0x02, 0b01000110,	
		0x03, 0b11101111,
		0x04, 0b11111111,
		0x05, 0b11111111,
		0x06, 0b11111011,
		0x07, 0b01111110,
		0x08, 0b00111100,
};

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


void sensor_set()	//Function to set up the Ultrasonic sesnor
{

	gpio_export(61);
	gpio_export(77);
	gpio_export(76);
	gpio_export(64);
	gpio_export(62);
	gpio_set_dir(62,0);
	gpio_set_dir(61,1);
	gpio_set_value(77,0);	
	gpio_set_value(76,0);
	gpio_set_value(64,0);	
	gpio_set_value(61,0);
			
}

void setdisplay()	//Function to setup the LED matrix
{
	//IO11(SPI_MOSI)
	gpio_export(24);
	gpio_export(44);
	gpio_export(72);
	gpio_set_dir(24,1);
	gpio_set_dir(44,1);
	gpio_set_value(24,0);	
	gpio_set_value(44,1);	
	gpio_set_value(72,0);
	//IO12(SPI_MISO)
	gpio_export(15);
	gpio_export(42);	
	gpio_set_dir(15,1);
	gpio_set_dir(42,1);
	gpio_set_value(15,0);
	gpio_set_value(42,0);
	//IO13 (SPI_SCK)
	gpio_export(30);
	gpio_export(46);
	gpio_set_dir(30,1);
	gpio_set_dir(46,1);
	gpio_set_value(30,0);
	gpio_set_value(46,1);
		
}

void* polling_function(void* arg)
{	
	long POLL_TIMEOUT = 100;	//Timeout value for poll function
	unsigned long long timeRising,timeFalling;
	char *buf[MAX_BUF];	
	struct pollfd fd_set={0};
	sensor_set();
	fd_set.fd = open("/sys/class/gpio/gpio62/value",O_RDONLY | O_NONBLOCK);
	fd_set.events = POLLPRI|POLLERR;
	while(1)
	{	
		lseek(fd_set.fd, 0, SEEK_SET);
		gpio_set_edge(62,"rising");	//Setting the edge detection to rising	
		gpio_set_value(61, 0);
		usleep(2);
		gpio_set_value(61,1);	//Giving trigger pulse
		timeRising = rdtsc();	//Storing the time 
		usleep(12);
		gpio_set_value(61,0);
		poll(&fd_set, 1, POLL_TIMEOUT);	//Polling the echo pin until edge detected or till timeout
		read(fd_set.fd, buf, 1);
		lseek(fd_set.fd, 0, SEEK_SET);
		if(fd_set.revents & POLLPRI)
		{	
			gpio_set_edge(62, "falling");	//Setting the edge detection to falling
			poll(&fd_set, 1, POLL_TIMEOUT);	//Polling the echo pin until edge detected or till timeout
			read(fd_set.fd, buf, 1);
			lseek(fd_set.fd, 0, SEEK_SET);	
			if(fd_set.revents & POLLPRI)
			{
				timeFalling = rdtsc();	//Storing time at falling edge
				pthread_mutex_lock(&lock);
				distance = ((timeFalling - timeRising) * 340.00) / (2.0 * 4000000);	//Calclation of the distance using the difference in times of edges
				pthread_mutex_unlock(&lock);
			}
		}
		usleep(600000);
	}
}


void* led_mat_disp(void* arg)
{
	int i,k = 0;
	int ret;
	double distance_previous = 0, distance_current = 0, distance_diff = 0, distance_threshhold=0;
	int new_direction = 0, old_direction = 0;
	setdisplay();	//Setting up the LED matrix
	struct spi_ioc_transfer tr = 	//Structure used to transfer into the SPI device
	{
		.tx_buf = (unsigned long)set_row,
		.rx_buf = 0,
		.len = 2,
		.delay_usecs = 1,
		.speed_hz = 10000000,
		.bits_per_word = 8,
		.cs_change = 1,
	};
	fd_spi= open(DEVICE,O_RDWR);
	if(fd_spi==-1)
	{
     	printf("file %s either does not exit or is currently used by an another user\n", DEVICE);
     	exit(-1);
	}
	usleep(100000);		
	set_row[0]=0x09;	//Setting up the device for no BCD decode for all pine
	set_row[1]=0x00;
	gpio_set_value(15,0);
	ret = ioctl(fd_spi,SPI_IOC_MESSAGE (1), &tr);
	gpio_set_value(15,1);
	usleep(10000);
	set_row[0]=0x0A;
	set_row[1]=0x04;
	gpio_set_value(15,0);
	ret = ioctl(fd_spi,SPI_IOC_MESSAGE (1), &tr); 
	gpio_set_value(15,1);
	usleep(10000);
	set_row[0]=0x0B;	//Setting scan limit register to display all digits
	set_row[1]=0x07;
	gpio_set_value(15,0);
	ret = ioctl(fd_spi,SPI_IOC_MESSAGE (1), &tr);
	gpio_set_value(15,1);
	usleep(10000);
	set_row[0]=0x0C;	//Setting up the Shutdown to normal operation
	set_row[1]=0x01;
	gpio_set_value(15,0);
	ret = ioctl(fd_spi,SPI_IOC_MESSAGE (1), &tr);
	gpio_set_value(15,1);
	usleep(10000);
	while(1)
	{		
		i = 0;
		k= 0;
		if (distance > 70)	// Lower limit for delay and hence animation speed
			delay = 625000;
		else if (distance <= 70)		// Upper limit
			delay = 75000;
		else
		{
			delay = (446250000/distance);				// Linear mapping
		}

		// Check direction condition according to change in distance
		pthread_mutex_lock(&lock);
		distance_current = distance;
		pthread_mutex_unlock(&lock);
		distance_diff = distance_current - distance_previous;
		distance_threshhold = distance_current / 10.0;

		if((distance_diff > -distance_threshhold) && (distance_diff < distance_threshhold))	//Same direction if object has not changed distance
		{
			new_direction = old_direction;
		}
		else if(distance_diff > distance_threshhold)	//Change direction if object has moved closer
		{
			new_direction = 1;
		}
		else if(distance_diff < -distance_threshhold)	//Change direction if object has moved further
		{
			new_direction = 0;
		}

		if (new_direction == 0)
		{
		while (i < 16)								
		{
			set_row[0] = right_pos_1 [i];
			set_row[1] = right_pos_1 [i+1];
			gpio_set_value(15,0);
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);	//Write into the spi device by copying the address and data into the device for each row
			gpio_set_value(15,1);
			i = i + 2; 
		
		}
		usleep(delay);
		
		while (k < 16)
		{
			set_row[0] = right_pos_2 [k];
			set_row[1] = right_pos_2 [k+1];
			gpio_set_value(15,0);
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			gpio_set_value(15,1);
			k = k + 2; 

		}
		usleep(delay);
	
		ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
		}
		else if (new_direction == 1)					
		{
		while (i < 16)
		{
			set_row[0] = left_pos_1 [i];
			set_row[1] = left_pos_1 [i+1];
			gpio_set_value(15,0);
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			gpio_set_value(15,1);
			i = i + 2; 
		
		}
		usleep(delay);
		
		while (k < 16)
		{
			set_row[0] = left_pos_2 [k];
			set_row[1] = left_pos_2 [k+1];
			gpio_set_value(15,0);
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			gpio_set_value(15,1);
			k = k + 2; 

		}
		usleep(delay);
		gpio_set_value(15,0);
		ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
		gpio_set_value(15,1);
		if(ret==1);
		}
			
	distance_previous = distance_current;
		old_direction = new_direction;	
	}
	
	close (fd_spi);
}




int main()
{

	int erc,err;
	if (pthread_mutex_init(&lock, NULL) != 0) 
	{
	    printf("\n mutex init failed\n");
	    return 1;
	}

	erc = pthread_create(&display_thread, NULL, &led_mat_disp, NULL);	//Create thread for the SPI device working function
	if (erc != 0)
	      printf("\ncan't create display thread\n");
	err = pthread_create(&polling_thread, NULL, &polling_function, NULL);	//Create thread for the ultrasonic sesnor working function
	if (err != 0)
	      printf("\ncan't create polling thread\n");
	pthread_join (display_thread, NULL);
	pthread_join (polling_thread, NULL);
	pthread_mutex_destroy(&lock);
	return 0;
}

