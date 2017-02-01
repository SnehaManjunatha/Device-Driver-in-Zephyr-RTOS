#include <nanokernel.h>
#include <board.h>
#include <device.h>
#include <init.h>
#include <misc/shell.h>
#include <zephyr.h>
#include <i2c.h>
#include <gpio.h>
#include <pinmux.h>
#include <hcsr04.h>
#include <string.h>

#if defined(CONFIG_STDOUT_CONSOLE)
  #include <stdio.h>
  #define PRINT           printf
  #else
  #include <misc/printk.h>
  #define PRINT           printk
#endif

#define DEV_OK  0
#define DEVICE_NAME "test shell"
#define I2C_SLV_ADDR	0x54
#define STACKSIZE 2000
#define SLEEPTIME  100
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 1000)

struct device *gpio;
struct device *gpio_sus;
struct device *i2c_dev, *i2c;
struct device *pinmux;
struct device *exp0;
struct device *hcsr04;
struct nano_sem fiber_hcsr;
struct nano_sem fiber_eeprom;
char __stack fiberStack[STACKSIZE];

uint8_t addr_highbyte = 0x00;
uint8_t addr_lowbyte = 0x00;
uint32_t buffer1[16];
uint32_t buffer2[16];
uint32_t write_buff[16];
uint32_t read_buff[16];
uint32_t temp_buff[16];
int write_count1 =0 ;
int set_flag_write = 0;
int read_count1 =0 ;
int set_flag_read= 0;
int buff_count1 =0;
int current_page =0;
int w_page;
int r_page1;
int r_page2;
int set_get =0;
/* String to integer conversion */

int string_to_int(char *string)
{	
	int  i, len;
	int result=0;
	len = strlen(string);
	for(i=0; i<len; i++){
		result = result * 10 + ( string[i] - '0' );
	}
	return result;	
}

/* Page write address calculation */
 
void address_calc(int page_num)
{
	int count = 0;
	int temp_byte;
	int addr_bytes;
	addr_highbyte = 0x00;
	addr_lowbyte = 0x00;
	while(count != page_num)
	{
		addr_bytes = addr_highbyte;
		temp_byte = addr_lowbyte;
		addr_bytes = (addr_bytes << 8 ) | (temp_byte);
		addr_bytes = addr_bytes + 64;
		addr_lowbyte = (addr_bytes & 0xFF);	//Low Byte
		addr_highbyte = (addr_bytes & 0xFF00) >> 8 ;	//High Byte
		count+=1;
	}
}

/* EEPROM write function */

int eeprom_write(uint32_t *buffer_data, int count)  //count is number of pages to write
{
	int pagecount = 0;
	uint8_t buff[66];
	uint8_t data[64];
	int bufindex, rc1;
	int i;

	memcpy(data , (uint8_t*)buffer_data , 64);
		
	for(pagecount=0;pagecount < count; pagecount++)
	{
		address_calc(current_page);
		buff[0] = addr_highbyte;
		buff[1] = addr_lowbyte;
		PRINT("------->Setting current position to page %d address = 0x%02x%02x \n", current_page, addr_highbyte, addr_lowbyte);
		bufindex = 2;
		for(i=pagecount*64; i<(pagecount*64)+64; i++)
		{
			buff[bufindex] = data[i];
			bufindex++;
		}
		
		rc1=i2c_write(i2c_dev, buff, 66, I2C_SLV_ADDR );
		if(!rc1) PRINT("return i2c write %d\n", rc1);
		else PRINT("return i2c write %d\n", rc1);
		fiber_sleep(SLEEPTICKS);
		if(current_page == 511)
			current_page = 0;
		else
			current_page++;
	}
	return 0;
}

/* EEPROM read function */

int eeprom_read(uint32_t *read_buffer_data, int count)
{
	int pagecount = 0;
	int rc1 ;
	uint8_t buff[5];
	int i;	
	
	for(pagecount=0;pagecount < count; pagecount++)
	{
		address_calc(r_page1++);
		buff[0] = addr_highbyte;
		buff[1] = addr_lowbyte;
		PRINT("-------> page %d address = 0x%02x%02x \n", current_page, addr_highbyte, addr_lowbyte);
		rc1=i2c_write(i2c_dev,buff,2, I2C_SLV_ADDR );
		if(!rc1) PRINT("return i2c write %d\n", rc1);
		else PRINT("return i2c write %d\n", rc1);
	
		fiber_sleep(SLEEPTICKS);
	
		rc1=i2c_read(i2c_dev,(uint8_t *)read_buffer_data, 64,I2C_SLV_ADDR );
		if(!rc1) PRINT("return i2c read %d\n", rc1);
		else PRINT("return i2c read %d\n", rc1);

		fiber_sleep(SLEEPTICKS);
	
		if(current_page == 511)
			current_page = 0;
		else
			current_page++;
		for(i=0; i< 8; i=i+2)
			PRINT("read = %d    timestamp = %d\n", read_buffer_data[i] , read_buffer_data[i+1]);	
		PRINT("\n\n" );	

	}
	
	return 0;
}



static int shell_cmd_enable(int argc, char *argv[])
{
	int enable = string_to_int(argv[1]);
	PRINT("enable = %d\n", enable);
	if(enable==0)
	{
		PRINT("None of the devices are enabled\n");
		enable=0;
	}
	else if(enable==1)
	{
		PRINT("Device HCSR0 is enabled\n");
		enable=1;
		hcsr04 = device_get_binding(CONFIG_HCSR04_DW_0_NAME);
		if(!hcsr04)
		PRINT("not binding\n");
	}
	else if(enable==2)
	{
		PRINT("Device HCSR1 is enabled\n");
		enable=2;
		hcsr04 = device_get_binding(CONFIG_HCSR04_DW_1_NAME);
		if(!hcsr04)
		PRINT("not binding\n");
	}
	else
		PRINT("Enter either 0,1 or 2\n");
return 0;
}

void eeprom_w(void)
{

PRINT("eeprom_w enterred 1\n");
int i,ret;
	for(i=0; i<w_page; i++){
	
	if(set_flag_read ==0)
	{
		PRINT("blocked 1\n");
		nano_fiber_sem_take(&fiber_hcsr, TICKS_UNLIMITED);
		PRINT("blocked released 1\n");
		ret = eeprom_write(write_buff, 1);
		if(ret == 0)
		{set_flag_read =1;
			while(set_get!=1);
		nano_fiber_sem_give(&fiber_eeprom);
		}
	}

	}	
	

}

static int shell_cmd_start(int argc, char *argv[])
{
	
	 w_page= string_to_int(argv[1]);
PRINT("start  %d\n",w_page);
	
	

nano_sem_init(&fiber_hcsr);
nano_sem_init(&fiber_eeprom);
fiber_fiber_start(&fiberStack[0], STACKSIZE,
			(nano_fiber_entry_t) eeprom_w, 0, 0, 7, 0);

uint32_t  tsc1;
uint32_t  tsc2;
int i,rc1;
uint32_t val;
int k;

for(k =0; k< w_page; k++){	
	for(i=0; i<8; i++){
		PRINT("for enterred\n");
tsc1 = sys_cycle_get_32();
			rc1 = hcsr04_write(hcsr04, CONFIG_GPIO_INT_PIN_1);
			if (rc1 != 0) 
				PRINT("hc-sr04_write config error %d!!\n", rc1);
				
			fiber_sleep(20);
			
			rc1 = hcsr04_read(hcsr04, 7, &val);
tsc2 = sys_cycle_get_32();
			if (rc1 != 0)
     				PRINT("hc-sr04_read config error %d!!\n", rc1);
 			else
			PRINT("val = %d\n", val);


			
			if(write_count1 < 16)
				{
	  
					write_buff[write_count1] = val;
					write_buff[write_count1+1] = tsc2-tsc1;
					write_count1 = write_count1 +2;
				}
			
			if(write_count1 == 16)
			{
				
				set_flag_write = 1;
			 	
				
			
				nano_fiber_sem_give(&fiber_hcsr);
				
				
				nano_fiber_sem_take(&fiber_eeprom, 20 );
				set_get =1;
				
				if((set_flag_write == 1) && (set_flag_read ==1))
				{
				
					memcpy(temp_buff, write_buff, sizeof(write_buff));
					memcpy(write_buff, read_buff, sizeof(read_buff));
					memcpy(write_buff, temp_buff, sizeof(temp_buff));
					set_flag_write =0; set_flag_read = 0; write_count1 = 0;set_get =0;
				
				}
			}
			
			
		}
write_count1 = 0;
nano_sem_init(&fiber_hcsr);
nano_sem_init(&fiber_eeprom);
}
return 0;
	
}

static int shell_cmd_dump(int argc, char *argv[])
{
	PRINT("dump\n");
	r_page1=string_to_int(argv[1]);
	r_page2=string_to_int(argv[2]);
	uint32_t read_buffer_data[16];	
	eeprom_read(read_buffer_data, r_page2-r_page1);
return 0;
}


const struct shell_cmd commands[] = {
	{ "enable", shell_cmd_enable },
	{ "start", shell_cmd_start },
	{ "dump", shell_cmd_dump },
	{ NULL, NULL }
};




void main(void)
{
	int rc1;
	gpio = device_get_binding(CONFIG_GPIO_PCAL9535A_2_DEV_NAME);
	i2c = device_get_binding("I2C_0");
	if (!i2c) {
		PRINT("I2C not found!!\n");
	}
	i2c_dev = device_get_binding(CONFIG_GPIO_PCAL9535A_1_I2C_MASTER_DEV_NAME);
		
	if (!gpio) {
		PRINT("GPIO not found!!\n");
	} else {
		rc1 = gpio_pin_write(gpio, 12, 0);  	
		if (rc1 != DEV_OK){ 
			PRINT("GPIO config error %d!!\n", rc1);
		}
		rc1 = gpio_pin_configure(gpio, 12, GPIO_DIR_OUT);  	
		if (rc1 != DEV_OK){ 
			PRINT("GPIO config error %d!!\n", rc1);
		}
	}
  	
	if (!i2c_dev) {
		PRINT("I2C not found!!\n");
	} else {
		rc1 = i2c_configure(i2c_dev, (I2C_SPEED_FAST << 1) | I2C_MODE_MASTER);
		if (rc1 != DEV_OK) {
			PRINT("I2C configuration error: %d\n", rc1);
		}
		else {
			PRINT("I2C configuration : %d\n", rc1);
		}
	}

	memset(buffer1, 0, sizeof(buffer1));
	memset(buffer2, 0, sizeof(buffer2));
	memcpy(write_buff, buffer1, sizeof(buffer1));  
	memcpy(read_buff, buffer2, sizeof(buffer1));  

	shell_init("shell> ", commands);

	}
