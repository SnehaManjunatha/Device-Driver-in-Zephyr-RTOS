#include <nanokernel.h>
#include <gpio.h>
#include <board.h>
#include <sys_io.h>
#include <init.h>
#include <misc/util.h>
#include <misc/__assert.h>
#include <clock_control.h>
#include <misc/util.h>
#include <device.h>
#include <sys_clock.h>
#include <limits.h>

//#include <802.15.4/cc2520.h>


#ifdef CONFIG_SHARED_IRQ
#include <shared_irq.h>
#endif

#ifdef CONFIG_IOAPIC
#include <drivers/ioapic.h>
#endif

#if defined(CONFIG_STDOUT_CONSOLE)
  #include <stdio.h>
  #define PRINT           printf
  #else
  #include <misc/printk.h>
  #define PRINT           printk
#endif

#include <hcsr04.h>

//#define CONFIG_GPIO_INT_PIN_1   7		//IO12
//#define GPIO_TRIGGER_PIN_1  8   //IO8
//#define CONFIG_GPIO_INT_PIN_2   2		//IO10
//#define GPIO_TRIGGER_PIN_2  	6   //IO7
//#define SLEEPTICKS	USEC(25)
#define SLEEPTIME  20
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 1000000)


uint32_t  tsc1;
uint32_t  tsc2;

#define HW_CYCLES_TO_USEC(__hw_cycle__) \
        ( \
                ((uint64_t)(__hw_cycle__) * (uint64_t)sys_clock_us_per_tick) / \
                ((uint64_t)sys_clock_hw_cycles_per_tick) \
        )
/*driver data*/

struct hcsr_dw_runtime {
	device_sync_call_t	sync;
	//struct semaphore lock;
	int time;
	
	uint32_t per_dev_buffer;
	char buffer[64];
	//ruct ring_buf *Pring_buf;
	uint32_t trigger_pin;
	uint32_t echo_pin;
	//spinlock_t m_Lock;
	int ongoing;
	struct gpio_callback gpio_cb;
	struct nano_sem nanofibre;
};

/*config data*/

struct hcsr_dw_config {
uint32_t irq_pin;
uint32_t trigger_pin_config;
uint32_t echo_pin_config;
struct device *int_pin_1;
struct device *exp1;
};
int hcsr_dw_initialize(struct device *port);


void gpio_callback(struct device *port,
		   struct gpio_callback *cb, uint32_t pins)
{
	struct device *hcsr04_local = NULL;
	struct hcsr_dw_config *config;
	struct hcsr_dw_runtime *dev ;
	
	if(cb->pin_mask == BIT(CONFIG_GPIO_INT_PIN_2))
		hcsr04_local = device_get_binding(CONFIG_HCSR04_DW_1_NAME);

	if(cb->pin_mask == BIT(CONFIG_GPIO_INT_PIN_1))
		hcsr04_local = device_get_binding(CONFIG_HCSR04_DW_0_NAME);

	//PRINT("gpio_callback entered\n");
	 config = hcsr04_local->config->config_info;
		dev= hcsr04_local->driver_data;
	int int_value;
	int rc1;
	gpio_pin_read(port, config->irq_pin, &int_value);
	PRINT("gpio_callback value = %d............", int_value);
	
	if(int_value == 1)
	{//PRINT("%d edge high interrupt triggered\n", config->irq_pin);
		tsc1 = sys_cycle_get_32();	
		
	rc1 = gpio_pin_configure(port,config->irq_pin , (GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE
			 | GPIO_INT_ACTIVE_LOW| GPIO_INT_DEBOUNCE|GPIO_PUD_PULL_DOWN));

		if (rc1 != 0) {
			PRINT("Low GPIO config error %d!!\n", rc1);
		}
	}

	else
	{//PRINT("%d edge low interrupt triggered\n", config->irq_pin);
	rc1 = gpio_pin_configure(port,config->irq_pin , (GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE
	 | GPIO_INT_ACTIVE_HIGH| GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_DOWN));

	if (rc1 != 0) {
	PRINT("High GPIO config error %d!!\n", rc1);
	}
	  tsc2 = sys_cycle_get_32();	
//PRINT("%d is tc1\n", tsc1);
		//PRINT("%d clock cycles\n", tsc2-tsc1);
		dev->per_dev_buffer = (HW_CYCLES_TO_USEC((tsc2)-(tsc1))*17)/1000 ;                     //400 * 58
		//PRINT("distance = %d \n", dev->per_dev_buffer);
	dev->ongoing =0;
	}
	
	
	//device_sync_call_complete(&dev->sync);
	nano_isr_sem_give(&dev->nanofibre);
	
//PRINT("left interrupt\n");
}



static inline int hcsr_dw_write(struct device *port, uint32_t pin)
{
	int rc1;
	//PRINT("WRITE entered\n");
#if 0
struct nano_timer timer;
void *timer_data[1];
nano_timer_init(&timer, timer_data);
#endif
	struct hcsr_dw_config *config = port->config->config_info;
	struct hcsr_dw_runtime * dev = port->driver_data;
	//PRINT("time  = %d \n",sys_clock_us_per_tick);
	dev->per_dev_buffer =0;
	
	//if(dev->ongoing !=1)
	{
	dev->ongoing =1;
	rc1=gpio_pin_write(config->exp1,dev->trigger_pin, 1);
			if (rc1 != 0) {
			PRINT("gpio_pin_write error %d!!\n", rc1);
				}
				task_sleep(SLEEPTICKS);
			
			rc1=gpio_pin_write(config->exp1,dev->trigger_pin, 0);
			if (rc1 != 0) {
			PRINT("gpio_pin_write error %d!!\n", rc1);
			}
				task_sleep(SLEEPTICKS);
	}
	return 0;

}



int hcsr_dw_read(struct device *port, uint32_t ticks, uint32_t *value)
{
	struct hcsr_dw_config *config = port->config->config_info;
	struct hcsr_dw_runtime * dev = port->driver_data;
	int rc1;
#if 0
struct nano_timer timer;
void *timer_data[1];
nano_timer_init(&timer, timer_data);
#endif

	//PRINT("READ entered\n");
	//PRINT("distance = %d \n", dev->per_dev_buffer);
	if(dev->per_dev_buffer == 0)
	{	
		if(dev->ongoing !=1)
		{
			PRINT("ongoing meassurement\n");
						
			//device_sync_call_wait(&dev->sync);
			
		//}
		//else
		//{
		dev->ongoing =1;
		rc1=gpio_pin_write(config->exp1,dev->trigger_pin, 1);
			if (rc1 != 0) {
			PRINT("gpio_pin_write error %d!!\n", rc1);
				}
				task_sleep(SLEEPTICKS);
			
			rc1=gpio_pin_write(config->exp1,dev->trigger_pin, 0);
			if (rc1 != 0) {
			PRINT("gpio_pin_write error %d!!\n", rc1);
			}
				task_sleep(SLEEPTICKS);
				
				if (nano_fiber_sem_take(&dev->nanofibre, ticks)!= 1) //user
					{PRINT("not available");
					return -1;}
		*value = dev->per_dev_buffer;			
		}
	//*value = dev->per_dev_buffer;
		
	}

	else
	*value = dev->per_dev_buffer;
	
	nano_sem_init(&dev->nanofibre);
	dev->per_dev_buffer=0;
	//PRINT("read out\n");
	return 0;

}



int hcsr_dw_reset(struct device *port)
{
	struct hcsr_dw_config *config = port->config->config_info;
	struct hcsr_dw_runtime * const dev = port->driver_data;
	
	if (config->trigger_pin_config == CONFIG_GPIO_TRIGGER_PIN_1)
	{config->irq_pin = CONFIG_GPIO_INT_PIN_1;
	config->trigger_pin_config = CONFIG_GPIO_TRIGGER_PIN_1;
	config->echo_pin_config = CONFIG_GPIO_INT_PIN_1;
	}
	else if (config->trigger_pin_config == CONFIG_GPIO_TRIGGER_PIN_2)
	{
	config->irq_pin = CONFIG_GPIO_INT_PIN_2;
	config->trigger_pin_config = CONFIG_GPIO_TRIGGER_PIN_2;
	config->echo_pin_config = CONFIG_GPIO_INT_PIN_2;
	
	}
	
	nano_sem_init(&dev->nanofibre);
	dev->trigger_pin = config->trigger_pin_config ;
	dev->echo_pin = config->echo_pin_config ;
	dev->ongoing = 0;
	dev->per_dev_buffer =0;
	
	
	
	return 0;
}


 struct hcsr04_driver_api api_funcs = {
	//.config = hcsr_dw_config,
	.write = hcsr_dw_write,
	.read = hcsr_dw_read,
	.reset = hcsr_dw_reset,
 };



static struct device *int_pin_configure(struct device *port)
{
	
	struct device *int_pin_1;
	int rc1, ret;
	
	struct hcsr_dw_config *config = port->config->config_info;
	struct hcsr_dw_runtime * const dev = port->driver_data;
PRINT("int_pin_configure  %d\n", config->irq_pin);
	int_pin_1 = device_get_binding(CONFIG_GPIO_DW_0_NAME);
	//spi_configure(spi, &spi_conf);
	if (!int_pin_1) 
		{
		PRINT("DW GPIO not found!!\n");
		} 
		else {
		rc1 = gpio_pin_configure(int_pin_1,config->irq_pin , (GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE
	 | GPIO_INT_ACTIVE_HIGH| GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_DOWN));


	if (rc1 != 0) {
			PRINT("Legacy GPIO config error %d!!\n", rc1);
		}
		}

	if(int_pin_1)
	{				
	gpio_init_callback(&dev->gpio_cb, gpio_callback, BIT(config->irq_pin));
	rc1 = gpio_add_callback(int_pin_1, &dev->gpio_cb);
	if (rc1) {
		PRINT("Cannot setup callback!\n");
	}



	rc1 = gpio_pin_enable_callback(int_pin_1, config->irq_pin);
	if (rc1) {
	PRINT("Error enabling callback!\n");
			}
	}
	return int_pin_1;
}





int hcsr_dw_initialize(struct device *port)
{
	int ret;
	PRINT("INIT2");

	struct hcsr_dw_config *config = port->config->config_info;
	struct hcsr_dw_runtime * const dev = port->driver_data;
	struct device *gpio;
	int rc1;
	config->int_pin_1 = int_pin_configure(port);
	device_sync_call_init(&dev->sync);
	dev->trigger_pin = config->trigger_pin_config ;
	dev->echo_pin = config->echo_pin_config ;
	dev->ongoing = 0;
	
	dev->per_dev_buffer =0;
	nano_sem_init(&dev->nanofibre);
	if(dev->echo_pin == CONFIG_GPIO_INT_PIN_2)
	{	
		gpio = device_get_binding(CONFIG_PWM_PCA9685_0_DEV_NAME);
		if (!gpio) {
		PRINT("GPIO not found!!\n");
		} 
		else {
		rc1 = gpio_pin_write(gpio, CONFIG_GPIO_MUX_PIN_2, 0);  	
		if (rc1 != 0){ 
			PRINT("GPIO config error %d!!\n", rc1);
		}
		rc1 = gpio_pin_configure(gpio, CONFIG_GPIO_MUX_PIN_2, GPIO_DIR_OUT);  	
		if (rc1 != 0){ 
			PRINT("GPIO config error %d!!\n", rc1);
		}
		}
	}
	//port->driver_api = &api_funcs;
	config->exp1 = device_get_binding(CONFIG_GPIO_PCAL9535A_1_DEV_NAME);

	if(!config->exp1) 
	{
	PRINT("GPIO not found!!\n");
	} 

	ret = gpio_pin_configure(config->exp1,dev->trigger_pin, GPIO_DIR_OUT);
	if (ret != 0) {
	PRINT("GPIO config error %d!!\n", ret);
	}

	ret = gpio_pin_write(config->exp1,dev->trigger_pin, 0);
	if (ret != 0) {
			PRINT("GPIO write1 error %d!!\n", ret);
		}
return 0;
}

struct hcsr_dw_config hcsr_config_0 = {
	.irq_pin = CONFIG_GPIO_INT_PIN_1,
	.trigger_pin_config = CONFIG_GPIO_TRIGGER_PIN_1,
	.echo_pin_config = CONFIG_GPIO_INT_PIN_1
};

struct hcsr_dw_config hcsr_config_1 = {
	.irq_pin = CONFIG_GPIO_INT_PIN_2,
	.trigger_pin_config = CONFIG_GPIO_TRIGGER_PIN_2,
	.echo_pin_config = CONFIG_GPIO_INT_PIN_2
};

struct hcsr_dw_runtime hcsr_0_runtime;
struct hcsr_dw_runtime hcsr_1_runtime;

DEVICE_AND_API_INIT(HCSR0, CONFIG_HCSR04_DW_0_NAME, hcsr_dw_initialize,
				&hcsr_0_runtime, &hcsr_config_0,
				APPLICATION, CONFIG_HCSR04_DW_INIT_PRIORITY, &api_funcs);   

DEVICE_AND_API_INIT(HCSR1, CONFIG_HCSR04_DW_1_NAME, hcsr_dw_initialize,
				&hcsr_1_runtime, &hcsr_config_1,
				APPLICATION, CONFIG_HCSR04_DW_INIT_PRIORITY, &api_funcs);  
