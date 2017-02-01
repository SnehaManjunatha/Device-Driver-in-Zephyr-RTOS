Data Recording Application using HC-SR04 Device Driver in Zephyr RTOS

	Develop a Linux device driver for HC-SR04 ultrasonic distance sensors in Zephyr RTOS 
	running on Galileo Gen 2. The driver module should be placed in /Zephyr/drivers/hc-sr04 
	directory. When the driver is initialized, it should create two instances of HC-SR04 sensors.
	The application to should take the distance measure and records the measure plus timestamp in
	an EEPROM device.Use a shell module to accept commands from console:
		- Enable n (n=0, 1, or 2, to enable none, HCSR0, or HCSR1)
		- Start p (to start collecting and recording distance measures from page 0. The operation 
		  stops until p pages of EEPROM are filled, where p is less than 512.)
		- Dump p1 p2 (p1 is less than p2, to dump (print out) the distance measures recorded in 
		  pages p1 to p2 on console)
	
Getting Started
    These instructions will get you a copy of the project up and running on your local machine 
	for development and testing purposes.

	Prerequisites
		- Set up the Galileo Gen 2 board with the Zephyr RTOS.
		- Set the environment variables using the following commands in the folder where the Zephyr RTOS
		  is downloaded.
				export ZEPHYR_GCC_VARIANT=zephyr
				export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk/
		- Run this command inside the Zephyr RTOS folder
				source zephyr-env.sh
		- Connect the sensor's trigget and echo pins to IO7 and IO10(HCSR1) respectively or to IO8 and IO12
		  (HCSR0) respectively.
		- Make the required connections to the EEPROM device.
	
	Running the tests
		- Apply the patch "hcsr_zephyr" to the original zephyr rtos. Using the following commands
				patch -p1 < path/to/patch/hcsr_zephyr.patch
		- Change the directory to zephyr/samples/HCSR_app/ and enter the command
				make
		- Copy the "zephyr_script" image from HCSR_app/outdir to the kernel folder in the micro SD card.
		- Load the micro SD card on the Galileo Gen 2 board and power the board.
		- Enter the following functions into the console
				enable val					@val = 0,1 or 2. Configure none or the required device
				start p1					@p = number of pages to to written into the EEPROM
				dump p1 p2					@p1,p2 = pages read from p1 to p2 in the EEPROM 
