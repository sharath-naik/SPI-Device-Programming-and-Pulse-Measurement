CSE 438:Embedded System Programming 
Assignment 3-Part 2

Team 5
Rama Kumar Kana Sundara
ASU ID:1213347614
Sharath Renjit Naik
ASU ID:1213340750

Files in folder:
1.main.c -is the main program.
2.sensor.c - is the driver program for the ultrasonic sensor
3.led.c	- is the driver program for the spi device
4.Makefile
5.script1.sh - is a code for easy driver loading on the Galileo board


Steps for execution:

1.In the Makefile change the "IOT_HOME" and "PATH" line to the directory where the i586-poky-linux-gcc is placed in host machine.
2.Use make command to obtain the executable file "main" and the two kernel object files "sensor.ko" and "led.ko"
3.Using scp command transfer the three above mentioned files and the file script1.sh to target device.
4.Make the connections from the peripheral to Galileo Gen2 board as follows
Ultrasonic sensor 
	Vcc to 5V
	GND to GND
	Trigger to IO2
	Echo to IO3
LED Matrix
	Vcc to 5V
	GND to GND
	DIN to IO11
	CLK to IO13
	CS to IO12
5.Use "chmod +x script1.sh" to ensure it is excecutable.
6.Run the scirpt file using ./script1.sh which will insmod the kernel object files
7.Run the main program using ./main
 
  
