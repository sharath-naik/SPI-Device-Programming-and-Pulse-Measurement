
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/ioctl.h>

#define SPI_DEVICE_NAME "/dev/spidev1.1"
#define PULSE_DEVICE_NAME "/dev/sensor"


typedef struct //Arguments used for the thread
{
	int threadId;
	int fd;
}ThreadParams;


pthread_mutex_t mutex;

double distance;

int write_pulse(int fd)	//Function to give a pulse to triiger pin of sensor
{
	int retValue=0;
    char* writeBuffer;
	
	writeBuffer = (char *)malloc(10);
	while(1)
	{
		retValue = write(fd, writeBuffer, 10);
		if(retValue < 0);
		else
		{
			break;
		}
		usleep(100000);
	}
	free(writeBuffer);
	return retValue;
}

int read_pulse(int fd)	//Function to read the calculated distance from the sensor echoes
{
	int retValue=0;
    unsigned int writeBuffer =0;
	while(1)
	{
		retValue = read(fd, &writeBuffer, sizeof(writeBuffer));
		
		if(retValue < 0);
		else
		{
			break;
		}
		usleep(100000);
	}
	return writeBuffer;
}

int spi_led_write(int fd, unsigned int sequenceBuffer[20])	//Function to write the sequence into the SPI device
{
	int retValue=0;
	while(1)
	{
		retValue = write(fd, sequenceBuffer, sizeof(sequenceBuffer));
		if(retValue < 0);
		else
		{
			break;
		}
		usleep(10000);
	}
	return retValue;
}

int spi_led_ioctl(int fd, char patternBuffer[10][8])	//Function to control the IO of the SPI device
{
	int retValue=0;
	while(1)
	{
		ioctl(fd,(unsigned int) patternBuffer, sizeof(patternBuffer));
		if(retValue < 0)
		{
			printf("SPI LED IOCTL Failure\n");
		}
		else
		{
			break;
		}
		usleep(100000);
	}
	return 0;
}
void *thread_transmit_pattern(void *data)	//Function that determines the pattern to be transmitted depending on the calculated distance
{
	int fd,pattern=0;
	double distance_previous = 0, distance_current = 0, distance_diff = 0, distance_threshhold=0;
	char new_direction = 'L', old_direction = 'L';
	unsigned int Delay_Time = 0;
	unsigned int sequenceBuffer[10];
	char patternBuffer[12][8] = {
		//Maximum thickness towards left
		{0b00011100,0b00111110,0b01110111,0b00000000,0b00011100,0b00111110,0b01110111,0b00000000},
		{0b00000000,0b00011100,0b00111110,0b01110111,0b00000000,0b00011100,0b00111110,0b01110111},
		
		//Maximum thickness towards right
		{0b00000000,0b01110111,0b00111110,0b00011100,0b00000000,0b01110111,0b00111110,0b00011100},
		{0b01110111,0b00111110,0b00011100,0b00000000,0b01110111,0b00111110,0b00011100,0b00000000},
		
		//Medium thickness towards left
		{0b00011000,0b00111100,0b01100110,0b11000011,0b10011001,0b00111100,0b01100110,0b11000011},
		{0b11000011,0b10011001,0b00111100,0b01100110,0b11000011,0b00011000,0b00111100,0b01100110},
		
		//Medium thickness towards right
		{0b11000011,0b01100110,0b00111100,0b10011001,0b11000011,0b01100110,0b00111100,0b00011000},
		{0b01100110,0b00111100,0b00011000,0b11000011,0b01100110,0b00111100,0b10011001,0b11000011},

		//Least thickness towards left
		{0b00001000,0b00010100,0b00100010,0b01000001,0b00001000,0b00010100,0b00100010,0b01000001},
		{0b00010100,0b00100010,0b01000001,0b00001000,0b00010100,0b00100010,0b01000001,0b00001000},

		//Least thickness towards right
		{0b01000001,0b00100010,0b00010100,0b00001000,0b01000001,0b00100010,0b00010100,0b00001000},
		{0b00001000,0b01000001,0b00100010,0b00010100,0b00001000,0b01000001,0b00100010,0b00010100},
		
		};

	fd = open(SPI_DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	
	spi_led_ioctl(fd,patternBuffer);
	while(1)
	{
		pthread_mutex_lock(&mutex);
		distance_current = distance;
		pthread_mutex_unlock(&mutex);
		distance_diff = distance_current - distance_previous;
		distance_threshhold = distance_current / 10.0;

		if((distance_diff > -distance_threshhold) && (distance_diff < distance_threshhold))
		{
			new_direction = old_direction;
		}
		else if(distance_diff > distance_threshhold)
		{
			new_direction = 'R';
		}
		else if(distance_diff < -distance_threshhold)
		{
			new_direction = 'L';
		}
		
		
		if(distance_current > 10 && distance_current < 20)//pattern with medium thickness
		{
			Delay_Time = 450;
			pattern=1;

		}
		else if(distance_current >= 20)//pattern with minimum thickness
		{
			Delay_Time = 200;
			pattern=2;
		}
		else{
			Delay_Time = 50;
			pattern=0; //pattern with maximum thickness

		}
		
		if(new_direction == 'R' && pattern==0)
		{
			//printf("Moving Away... Move Right\n");
			sequenceBuffer[0] = 2;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);

			sequenceBuffer[0] = 3;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		else if(new_direction == 'L'&& pattern==0)
		{
			//printf("Moving Closer... Move Left\n");
			sequenceBuffer[0] = 0;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			
			sequenceBuffer[0] = 1;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		if(new_direction == 'R'&&pattern==1)
		{
			//printf("Moving Away... Move Right\n");
			sequenceBuffer[0] = 6;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);

			sequenceBuffer[0] = 7;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		else if(new_direction == 'L'&&pattern==1)
		{
			//printf("Moving Closer... Move Left\n");
			sequenceBuffer[0] = 4;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			
			sequenceBuffer[0] = 5;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		if(new_direction == 'R'&&pattern==2)
		{
			//printf("Moving Away... Move Right\n");
			sequenceBuffer[0] = 10;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);

			sequenceBuffer[0] = 11;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		else if(new_direction == 'L'&&pattern==2)
		{
			//printf("Moving Closer... Move Left\n");
			sequenceBuffer[0] = 8;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			
			sequenceBuffer[0] = 9;
			sequenceBuffer[1] = Delay_Time;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}

		distance_previous = distance_current;
		old_direction = new_direction;
		usleep(10000);
	}
	close(fd);
	pthread_exit(0);
}

void *thread_Ultrasonic_distance(void *data)	//Function that manages the sensor writing and reading
{
	int fd;
	int pulseWidth;
	fd = open(PULSE_DEVICE_NAME, O_RDWR);
	while(1)
	{
		write_pulse(fd);
		pulseWidth = read_pulse(fd);
		pthread_mutex_lock(&mutex);
		distance = pulseWidth * 0.017;
		pthread_mutex_unlock(&mutex);
		usleep(100000);
	}
	close(fd);
	pthread_exit(0);
}

int main(int argc, char **argv, char **envp)
{
	int retValue;
	pthread_t thread_id_spi, thread_id_dist;
	ThreadParams *tp_spi, *tp_dist;

	pthread_mutex_init(&mutex, NULL);
	
	tp_spi = malloc(sizeof(ThreadParams));
	tp_spi -> threadId = 100;


	retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_pattern, (void*)tp_spi);	//Thread that manages the SPI device
		
	if(retValue)
	{
		printf("ERROR; return code from pthread_create() is %d\n", retValue);
		exit(-1);
	}
	
	tp_dist = malloc(sizeof(ThreadParams));
	tp_dist -> threadId = 101;
	
	retValue = pthread_create(&thread_id_dist, NULL, &thread_Ultrasonic_distance, (void*)tp_dist);	//Thread that manages the sensor device
	
	pthread_join(thread_id_spi, NULL);
	pthread_join(thread_id_dist, NULL);
	
	free(tp_spi);
	free(tp_dist);
	
	return 0;
} 
