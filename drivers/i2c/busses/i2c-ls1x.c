
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

/*register*/
#define REG_I2C_PRER_LO	0x0
#define REG_I2C_PRER_HI	0x1
#define REG_I2C_CTR		0x2
#define REG_I2C_TXR		0x3
#define REG_I2C_RXR		0x3
#define REG_I2C_CR		0X4
#define	REG_I2C_SR		0X4

/*control*/
#define I2C_C_START		0x80
#define I2C_C_STOP		0x40
#define	I2C_C_READ		0x20
#define I2C_C_WRITE		0x10
#define I2C_C_WACK		0x8
#define I2C_C_IACK		0x1

/*status*/
#define	I2C_S_RNOACK	0x80
#define I2C_S_BUSY		0x40
#define I2C_S_RUN		0x2
#define	I2C_S_IF		0x1


struct ls1x_i2c {
	void __iomem *base;
	u32 interrupt;
	wait_queue_head_t queue;
	struct i2c_adapter adap;
	int irq;
	u32 flags;
};

static char i2c_writeb(struct ls1x_i2c *i2c, unsigned int reg, unsigned char  data)
{
	
	(*(volatile unsigned char *)(i2c->base + reg)) = data;
}

static unsigned char i2c_readb (struct ls1x_i2c *i2c, unsigned char reg)
{
	unsigned char data;
	
	data = (*(volatile unsigned char *)(i2c->base + reg));

	return data;
}


static int ls1x_xfer_read(struct ls1x_i2c *i2c, unsigned char *buf, int len) 
{

	int x;

	for(x=0; x<len; x++) {

//send ACK last not send ACK
		if(x != (len -1)) 
			i2c_writeb(i2c, REG_I2C_CR,   I2C_C_READ);
		else
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_WACK |I2C_C_READ);

		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);

		buf[x] = i2c_readb(i2c,REG_I2C_TXR);

	}
	
	i2c_writeb(i2c,REG_I2C_CR, I2C_C_STOP);
		
	return 0;
}

static int ls1x_xfer_write(struct ls1x_i2c *i2c, unsigned char *buf, int len)
{

	int j;

	for(j=0; j< len; j++) {
			i2c_writeb(i2c, REG_I2C_TXR, buf[j]);
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_WRITE);
			while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
			if(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {

			i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);
				return len;
			}
 
		}

	i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);

	return 0;

}

static int ls1x_xfer(struct i2c_adapter *adap, struct i2c_msg *pmsg, int num)
{
	int i,j, ret, ret0;
	struct ls1x_i2c *i2c = adap->algo_data;

	dev_dbg(&adap->dev, "ls1x_xfer: processing %d messages:\n", num);

/*set slave addr*/
	for(i=0;i<num;i++) {

		char flags;
		
		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_BUSY);
		
		flags = (pmsg->flags & I2C_M_RD)?1:0;
		i2c_writeb(i2c, REG_I2C_TXR, ((pmsg->addr << 1 ) | flags));
		i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START | I2C_C_WACK));


		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);

		if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
			printk(" slave addr no ack !!\n");
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
			return 0;
		}


 		if(flags )
			ret = ls1x_xfer_read(i2c, pmsg->buf, pmsg->len);
  		else
			ret = ls1x_xfer_write(i2c, pmsg->buf, pmsg->len);

	++pmsg;

	}

	return num;
}

static u32 ls1x_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ls1x_algo = {
	.master_xfer = ls1x_xfer,
	.functionality = ls1x_functionality,
};

static struct i2c_adapter ls1x_ops = {
	.owner	= THIS_MODULE,
	.name	= "GS2FSB adapter",
	.algo	= &ls1x_algo,
	.timeout= 1,
	.retries= 1, 	
};

static irqreturn_t ls1x_i2c_isr(int irqno, void *dev_id)
{
	struct ls1x_i2c *i2c = dev_id;	

	i2c_writeb(i2c, REG_I2C_CR, 1);
	
	return IRQ_HANDLED;
}
static int
ls1x_i2c_probe(struct platform_device *pdev)
{
	int result = 0;
	struct ls1x_i2c *i2c;
	printk("in the ls1x_i2c_probe!!\n");
	struct resource *res =  platform_get_resource(pdev,	IORESOURCE_MEM, 0);  
	if (!(i2c = kzalloc(sizeof(*i2c), GFP_KERNEL))) {
		return -ENOMEM;
	}


	init_waitqueue_head(&i2c->queue);

	i2c->base = ioremap(res->start, res->end - res->start + 1);
	
	if (!i2c->base) {
		printk(KERN_ERR "i2c-ls1x - failed to map controller\n");
		result = -ENOMEM;
		goto fail_map;
	}
 
/*	if (i2c->irq != 0)
		if ((result = request_irq(i2c->irq, ls1x_i2c_isr,
					IRQF_SHARED, "i2c-ls1x",i2c)) < 0) {
			printk(KERN_ERR
				"i2c-ls1x - failed to attach interrupt\n");
			printk("i2c-ls1x - failed to attach interrupt\n");
			goto fail_irq;
		}*/

	platform_set_drvdata(pdev, i2c);

	i2c->adap = ls1x_ops;
	i2c_set_adapdata(&i2c->adap, i2c);
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
//set fre 
	i2c_writeb(i2c, REG_I2C_PRER_LO, 0x64);
	i2c_writeb(i2c, REG_I2C_PRER_HI, 0x0);
	i2c_writeb(i2c, REG_I2C_CTR, 0x80);


    result = i2c_add_numbered_adapter(&i2c->adap);
		if (result < 0) {
		printk( "i2c-ls1x - failed to add adapter\n");
		goto fail_add;
	}
	return result;

fail_irq:
fail_get_irq:
		if (i2c->irq != 0)
			free_irq(i2c->irq, NULL);
fail_map:
		iounmap(i2c->base);
fail_add:
		kfree(i2c);

	return result;

};
static int __exit ls1x_i2c_remove(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver ls1x_i2c_driver = {
	.probe		= ls1x_i2c_probe,
	.remove		= ls1x_i2c_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ls1x-i2c",
	},
};


static int __init
i2c_ls1x_init (void)
{
	return platform_driver_register(&ls1x_i2c_driver);
}

static void __exit
i2c_ls1x_exit(void)
{
	return platform_driver_unregister(&ls1x_i2c_driver);
}

module_init (i2c_ls1x_init);
module_exit (i2c_ls1x_exit);

MODULE_AUTHOR("ninglichen <ninglichen@loongson.cn>");
MODULE_DESCRIPTION("loongson 2F south borad i2c bus driver");
MODULE_LICENSE("GPL");
