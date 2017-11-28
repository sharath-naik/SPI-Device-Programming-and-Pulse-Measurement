Sharath Renjit Naik

Files in folder:
1.main3.c -is the main program.
2.Gpio_func.c-is the source program for gpio operations.
3.Gpio_func.h
4.Makefile


Steps for execution:

1.In the Makefile change the "IOT_HOME" and "PATH" line to the directory where the i586-poky-linux-gcc is placed in host machine.
2.Use make command to obtain the executable file, "spi".
3.Using scp command transfer the spi file to target device.
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
5.Use "chmod +x spi" to ensure "spi" is excecutable.
6.Run the excecutable using ./spi
 
  
