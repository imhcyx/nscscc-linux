#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/spi/spi.h>


/*define spi register */
#define	SPCR	0x00
#define	SPSR	0x01
#define FIFO	0x02
#define	SPER	0x03
#define	PARA	0x04
#define	SFCS	0x05
#define	TIMI	0x06

extern unsigned long bus_clock;
struct ls1x_spi {
	struct work_struct	work;
	spinlock_t			lock;

	struct	list_head	msg_queue;
	struct	spi_master	*master;
	void	__iomem		*base;
	int cs_active;
	struct workqueue_struct	*wq;
};


static char ls1x_spi_write_reg(struct ls1x_spi *spi, 
				unsigned char reg, unsigned char data)
{
	(*(volatile unsigned char *)(spi->base +reg)) = data;
}

static char ls1x_spi_read_reg(struct ls1x_spi *spi, 
				unsigned char reg)
{
	return(*(volatile unsigned char *)(spi->base + reg));
}
static int ls1x_spi_setup(struct spi_device *spi)
{
	unsigned char num, value;
	struct ls1x_spi *ls1x_spi;
	
	ls1x_spi = spi_master_get_devdata(spi->master);
	if (spi->bits_per_word %8)
		return -EINVAL;

	if(spi->chip_select >= spi->master->num_chipselect)
		return -EINVAL;

	return 0;
}

static int 
ls1x_spi_write_read_8bit( struct spi_device *spi,
  const u8 **tx_buf, u8 **rx_buf, unsigned int num)
{
	struct ls1x_spi *ls1x_spi;
	unsigned char value;
	int i;
	ls1x_spi = spi_master_get_devdata(spi->master);

	
	if (tx_buf && *tx_buf){
		ls1x_spi_write_reg(ls1x_spi, FIFO, *((*tx_buf)++));
 		while((ls1x_spi_read_reg(ls1x_spi, SPSR) & 0x1) == 1);
	}else{
		ls1x_spi_write_reg(ls1x_spi, FIFO, 0);
 		while((ls1x_spi_read_reg(ls1x_spi, SPSR) & 0x1) == 1);
	}

	if (rx_buf && *rx_buf) {
		*(*rx_buf)++ = ls1x_spi_read_reg(ls1x_spi, FIFO);
	}else{
		  ls1x_spi_read_reg(ls1x_spi, FIFO);
	}

	return 1;
}


static unsigned int
ls1x_spi_write_read(struct spi_device *spi, struct spi_transfer *xfer)
{
	struct ls1x_spi *ls1x_spi;
	unsigned int count;
	int word_len;
	const u8 *tx = xfer->tx_buf;
	u8 *rx = xfer->rx_buf;

	ls1x_spi = spi_master_get_devdata(spi->master);
	count = xfer->len;

	do {
		if (ls1x_spi_write_read_8bit(spi, &tx, &rx, count) < 0)
			goto out;
		count--;
	} while (count);

out:
	return xfer->len - count;
	//return count;

}

static inline int set_cs(struct ls1x_spi *ls1x_spi, struct spi_device  *spi, int val)
{
		int cs = ls1x_spi_read_reg(ls1x_spi, SFCS)&~(0x11 << spi->chip_select);
		ls1x_spi_write_reg(ls1x_spi, SFCS, (val?(0x11 << spi->chip_select):(0x1 << spi->chip_select))|cs);
}

static void ls1x_spi_work(struct work_struct *work)
{
	int i = 0;
	struct ls1x_spi *ls1x_spi = 
		container_of(work, struct ls1x_spi, work);
	int param;

	spin_lock_irq(&ls1x_spi->lock);
	param = ls1x_spi_read_reg(ls1x_spi, PARA);
	ls1x_spi_write_reg(ls1x_spi, PARA, param&~1);
	while (!list_empty(&ls1x_spi->msg_queue)) {

		struct spi_message *m;
		struct spi_device  *spi;
		struct spi_transfer *t = NULL;
		int par_override = 0;
		int status = 0, value = 0;

		m = container_of(ls1x_spi->msg_queue.next, struct spi_message, queue);

		list_del_init(&m->queue);
		spin_unlock_irq(&ls1x_spi->lock);

		spi = m->spi;

		/*in here set cs*/
		set_cs(ls1x_spi, spi, 0);



		list_for_each_entry(t, &m->transfers, transfer_list) {
		
			if (t->len)
		   		m->actual_length +=
					ls1x_spi_write_read(spi, t);
		}

msg_done:
	set_cs(ls1x_spi, spi, 1);
	m->complete(m->context);


	spin_lock_irq(&ls1x_spi->lock);
	}
	ls1x_spi_write_reg(ls1x_spi, PARA, param);

	spin_unlock_irq(&ls1x_spi->lock);
}



static int ls1x_spi_transfer(struct spi_device *spi, struct spi_message *m)
{

	struct ls1x_spi	*ls1x_spi;
	struct spi_transfer *t = NULL;
	unsigned long flags;
	
	m->actual_length = 0;
	m->status		 = 0;

	if (list_empty(&m->transfers) || !m->complete)
		return -EINVAL;

	ls1x_spi = spi_master_get_devdata(spi->master);

	list_for_each_entry(t, &m->transfers, transfer_list) {
		
		if (t->tx_buf == NULL && t->rx_buf == NULL && t->len) {
			dev_err(&spi->dev,
				"message rejected : "
				"invalid transfer data buffers\n");
			goto msg_rejected;
		}

	/*other things not check*/

	}

	spin_lock_irqsave(&ls1x_spi->lock, flags);
	list_add_tail(&m->queue, &ls1x_spi->msg_queue);
	queue_work(ls1x_spi->wq, &ls1x_spi->work);
	spin_unlock_irqrestore(&ls1x_spi->lock, flags);

	return 0;
msg_rejected:

	m->status = -EINVAL;
 	if (m->complete)
		m->complete(m->context);
	return -EINVAL;
}

static int __init ls1x_spi_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct ls1x_spi		*spi;
	struct resource		*res;
	int ret, spr, spre;
	
	master = spi_alloc_master(&pdev->dev, sizeof(struct ls1x_spi));
	
	if (master == NULL) {
		dev_dbg(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	if (pdev->id != -1)
		master->bus_num	= pdev->id;

		master->setup = ls1x_spi_setup;
		master->transfer = ls1x_spi_transfer;
		master->num_chipselect = 4;

	dev_set_drvdata(&pdev->dev, master);

	spi = spi_master_get_devdata(master);

	spi->wq	= create_singlethread_workqueue(pdev->name);
	
	spi->master = master;
	

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		ret = -ENOENT;
		goto free_master;
	}

	spi->base = ioremap(res->start, (res->end - res->start)+1);
	if (spi->base == NULL) {
		dev_err(&pdev->dev, "Cannot map IO\n");
		ret = -ENXIO;
		goto unmap_io;
	}

/*next we will set figure for controller eg: forbid interrupt*/
	//for gj ls1a 166MHZ ddr freq
	if (1/*bus_clock > 160000000*/)
	{
		ls1x_spi_write_reg(spi, SPCR, 0x51);
		ls1x_spi_write_reg(spi, SPER, 0x04);
	}
	else
	{
		ls1x_spi_write_reg(spi, SPCR, 0x50);
		ls1x_spi_write_reg(spi, SPER, 0x05);
	}

	ls1x_spi_write_reg(spi, TIMI, 0x01);
	//ls1x_spi_write_reg(spi, SPER, 0x04);
	//ls1x_spi_write_reg(spi, PARA, 0x42);
	ls1x_spi_write_reg(spi, PARA, 0x46);

	INIT_WORK(&spi->work, ls1x_spi_work);

	spin_lock_init(&spi->lock);
	INIT_LIST_HEAD(&spi->msg_queue);

	ret = spi_register_master(master);
	if (ret < 0)
		goto unmap_io;

	return ret;

unmap_io:
	iounmap(spi->base);
free_master:
	kfree(master);
put_master:
	spi_master_put(master);
err:
	return ret;

}

static struct platform_driver ls1x_spi_driver = {
	.remove	= __exit_p(ls1x_remove),
	.driver	= {
		.name	= "ls1x-spi",
		.owner	= THIS_MODULE,
	},
};


static int __init ls1x_spi_init(void)
{
	return platform_driver_probe(&ls1x_spi_driver, ls1x_spi_probe);
}

static void __exit ls1x_spi_exit(void)
{
	platform_driver_unregister(&ls1x_spi_driver);
}

module_init(ls1x_spi_init);
module_exit(ls1x_spi_exit);
