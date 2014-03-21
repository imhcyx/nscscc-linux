#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <asm/bitops.h>
#include "ls1x_board_int.h"
#include "gpio.h"

#define PCA9539_DEV_NAME  "pca9539"
#define PCA9539_IRQ_PIN   GPIO_00
#define PCA9539_ADDR      0xe8

#define IP0_REG  0x0
#define IP1_REG  0x1
#define OP0_REG  0x2
#define OP1_REG  0x3
#define PI0_REG  0x4
#define PI1_REG  0x5
#define COF0_REG 0x6
#define COF1_REG 0x7

struct key_event {
	unsigned int    row;
	unsigned int	col;
};

struct pca9539_data {
	struct input_dev	*input;
	struct i2c_client	*client;
	int			irq;
};
static struct i2c_client *pca9539_adcclient = NULL;
static value = 1;

static int _i2c_read_byte(unsigned char addr,unsigned char *data,unsigned long num)
{
    int err = 0;
    struct i2c_client *client = pca9539_adcclient;
    struct i2c_msg msg;
    msg.addr = addr;
    msg.len = num;
    msg.buf = data;
    msg.flags = I2C_M_RD;
    if((err = i2c_transfer(client->adapter,&msg,1))!=1){
        printk(KERN_ERR "msg failed!!!\n");
        return 0;
    }
   return err;
}
static int _i2c_write_byte(unsigned char addr,unsigned char *data,unsigned long num)
{
    int err = 0;
    struct i2c_client *client = pca9539_adcclient;
    struct i2c_msg msg;
    msg.addr = addr;
    msg.len = num;
    msg.buf = data;
    msg.flags = 0;
    if((err = i2c_transfer(client->adapter,&msg,1)) != 1){
        printk(KERN_ERR "msg failed!!!\n");
        return 0;
    }
   return err;
}
static int pca9539_get_key(void)
{
#if 1
    int err=0;
    unsigned char data[1];
    err =  _i2c_read_byte(PCA9539_ADDR,data,1);
    if(!err)
        printk(KERN_ERR "pca9539 get data failed!!!\n");
    return data[0];
#endif
}
static int pca9539_set_cmd(unsigned char reg, unsigned char val)
{
#if 1
    int err=0;
    unsigned char data[2];
    data[0] = reg;
	data[1] = val;
    err = _i2c_write_byte(PCA9539_ADDR,(unsigned char*)data,2); 
    if(!err)
        printk(KERN_ERR "pca9539 write reg failed!!!\n");
    return err;
#endif
}
static int pca9539_set_reg(unsigned char reg)
{
#if 1
    int err=0;
	unsigned char data[1];
	data[0] = reg;
    err = _i2c_write_byte(PCA9539_ADDR,(unsigned char*)data,1); 
    if(!err)
        printk(KERN_ERR "pca9539 write reg failed!!!\n");
    return err;
#endif
}

static void pca9539_get_values(struct key_event *tc)
{
	//////get row val////////
	//set col out
    pca9539_set_cmd(COF0_REG, 0x0);
    //set row in
	pca9539_set_cmd(COF1_REG, 0xff);
    //udelay(4);
	//ready to read row val
	pca9539_set_reg(IP1_REG);
    tc->row = pca9539_get_key();

	//////get col val////////
	//set row out
    pca9539_set_cmd(COF1_REG, 0x0);
	//set row low
    pca9539_set_cmd(OP1_REG, 0x0);
	//set col in
    pca9539_set_cmd(COF0_REG, 0xff);
    //udelay(4);
	pca9539_set_reg(IP0_REG);
    tc->col = pca9539_get_key();
    return ;
}

static void en_inter(unsigned int irq)
{
	//low level intr not save int the intr controler
	//so not need clear intr
	enable_irq(irq);
}

typedef struct keymap{
	struct key_event tc;
	int code;
	int value;
}keymap;


//8 x 8 {{row, col}, code, value}
static keymap key_map_table[64] = {
	{{0,0},  0, 1},{{0,1},  1, 1}, {{0,2},  2, 1},{{0,3},  3, 1},{{0,4},  4, 1},{{0,5},  5, 1},{{0,6},  6, 1},{{0,7},  7, 1},	
	{{1,0},  8, 1},{{1,1},  9, 1}, {{1,2}, 10, 1},{{1,3}, 11, 1},{{1,4}, 12, 1},{{1,5}, 13, 1},{{1,6}, 14, 1},{{1,7}, 15, 1},	
	{{2,0}, 16, 1},{{2,1}, 17, 1}, {{2,2}, 18, 1},{{2,3}, 19, 1},{{2,4}, 20, 1},{{2,5}, 21, 1},{{2,6}, 22, 1},{{2,7}, 23, 1},	
	{{3,0}, 24, 1},{{3,1}, 25, 1}, {{3,2}, 26, 1},{{3,3}, 27, 1},{{3,4}, 28, 1},{{3,5}, 29, 1},{{3,6}, 30, 1},{{3,7}, 31, 1},	
	{{4,0}, 32, 1},{{4,1}, 33, 1}, {{4,2}, 34, 1},{{4,3}, 35, 1},{{4,4}, 36, 1},{{4,5}, 37, 1},{{4,6}, 38, 1},{{4,7}, 39, 1},	
	{{5,0}, 40, 1},{{5,1}, 41, 1}, {{5,2}, 42, 1},{{5,3}, 43, 1},{{5,4}, 44, 1},{{5,5}, 45, 1},{{5,6}, 46, 1},{{5,7}, 47, 1},	
	{{6,0}, 48, 1},{{6,1}, 49, 1}, {{6,2}, 50, 1},{{6,3}, 51, 1},{{6,4}, 52, 1},{{6,5}, 53, 1},{{6,6}, 54, 1},{{6,7}, 55, 1},	
	{{7,0}, 56, 1},{{7,1}, 57, 1}, {{7,2}, 58, 1},{{7,3}, 59, 1},{{7,4}, 60, 1},{{7,5}, 61, 1},{{7,6}, 62, 1},{{7,7}, 63, 1},	
};

int pca9539_key_map(struct key_event *tc)
{
	int i = 0;
	unsigned char row = tc->row;//get by high col
	unsigned char col = ~(tc->col);//get by low row
	row = ffs(row) - 1;
	col = ffs(col) - 1;
	for (; i < 64; i++)
	{
		if ((key_map_table[i].tc.col == col) && (key_map_table[i].tc.row == row))
			return i;
	}

	return -1;
}

static irqreturn_t ts_interrupt(int irq,void *handle)
{
	struct pca9539_data *ts = (struct pca9539_data *)handle;
	unsigned int rt=0;
	struct key_event tc;
	int i;
	disable_irq_nosync(irq);
	
	pca9539_get_values(&tc);
	//input rising and falling can intr, so need giveup rising 
	if ((tc.col != 0xff) && (tc.row != 0x0))
	{
		i = pca9539_key_map(&tc);
		if (i != -1)
		{
			input_event(ts->input, EV_KEY, key_map_table[i].code, key_map_table[i].value);
			input_sync(ts->input);
			key_map_table[i].value = !key_map_table[i].value;
		}
	}
	en_inter(ts->irq);
	return IRQ_HANDLED;
}

/*
 * Initialization function
 */

static int pca9539_init_client(struct i2c_client *client)
{
	return 0;
}

static int __devinit pca9539_probe(struct i2c_client *client,
                   const struct i2c_device_id *id)
{
    int err, ret, i = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct pca9539_data *ts;
	struct input_dev *input;
	printk(KERN_INFO "----pca9539_probe----\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&adapter->dev, "doesn't support full I2C\n");
		err = -EIO;
		goto exit;
		} 

    if (!(ts = kzalloc(sizeof(*ts), GFP_KERNEL))) {
		dev_err(&adapter->dev, "failed to alloc memory\n");
		err = -ENOMEM;
		goto exit;
		}   

	ts->client = client;
	i2c_set_clientdata(client, ts);

    /*send cmd init*/
	if(!pca9539_adcclient){
		printk(KERN_ERR "pca9539_detect!!!!!!\n");
		pca9539_adcclient = client;
	}   

	/* Initialize the pca9539 chip */
	err = pca9539_init_client(client);
	if (err)
		goto exit_kfree;

    input = input_allocate_device();
    if (!input)
	{
   	    err = -ENOMEM;
		goto exit;
    }

	ts->input=input;
    ts->irq=ls1b_gpio_to_irq(PCA9539_IRQ_PIN);

	//open code bit in input subsystem
	for (; i < 64; i++)
		input_set_capability(input, EV_KEY, i);
    
	input->name = "PCA9539";
    input->id.bustype = BUS_HOST;
    err = input_register_device(input);
    if (err) {
   	    printk(KERN_ERR "Unable to register pca9539 ts input device\n");
        goto exit;
    }

    ls1b_gpio_set_interupt(PCA9539_IRQ_PIN);  
    ls1b_gpio_set_irq_type(PCA9539_IRQ_PIN,LS1B_LEVEL_LOW);
    
	ret |= request_irq(ts->irq,ts_interrupt,IRQF_DISABLED,PCA9539_DEV_NAME,ts);
    printk(PCA9539_DEV_NAME"\tinitialized\n");

    return 0;

    exit_kfree:
	    kfree(ts);
    exit:
	    return err;
}

static int __devexit pca9539_remove(struct i2c_client *client)
{
	int err;
	struct pca9539_data *ts = i2c_get_clientdata(client);

    if(client == pca9539_adcclient)
		pca9539_adcclient = NULL;
	
	input_unregister_device(ts->input);
	i2c_set_clientdata(client, NULL);
	kfree(ts);

	return 0;

    exit:
	    return err;
}

static const struct i2c_device_id pca9539_id[] = { 
	    { "pca9539", 0 },
		{ },
};


static struct i2c_driver pca9539_driver={
        .driver = {
            .name = "pca9539",
            .owner = THIS_MODULE,
        },
        .probe = pca9539_probe,
        .remove = __devexit_p(pca9539_remove),
		.id_table = pca9539_id,
};
static int __init pca9539_init(void)
{
	printk("\t\n*****pca9539_init*****\n");
    return i2c_add_driver(&pca9539_driver);
}

static int __exit pca9539_exit(void)
{
    int ret=0;
    i2c_del_driver(&pca9539_driver);       

    return 0;
}

late_initcall(pca9539_init);
module_exit(pca9539_exit);

MODULE_AUTHOR("ZHAOKAI <zhaokai@loongson.cn>");
MODULE_DESCRIPTION("LS1X pca9539 keyboard controler driver");
MODULE_LICENSE("GPL");
