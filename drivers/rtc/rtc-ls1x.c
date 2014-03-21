

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/io.h>

#define RTC_TOYIM	0x00
#define	RTC_TOYWLO	0x04
#define	RTC_TOYWHI	0x08
#define RTC_TOYRLO	0x0c
#define RTC_TOYRHI	0x10
#define	RTC_TOYMH0	0x14
#define	RTC_TOYMH1	0x18
#define	RTC_TOYMH2	0x1c
#define	RTC_CNTL	0x20
#define	RTC_RTCIM	0x40
#define	RTC_WRITE0	0x44
#define	RTC_READ0	0x48
#define	RTC_RTCMH0	0x4c
#define	RTC_RTCMH1	0x50
#define	RTC_RTCMH2	0x54

struct rtc_ls1x{
	struct rtc_device	*rtc;
	void __iomem		*regs;
	unsigned long 		alarm_time;
	unsigned long 		irq;

	spinlock_t			lock;

};

static int ls1x_rtc_read_register(struct rtc_ls1x *rtc, unsigned long reg)
{
	int data;

	data = (*(volatile unsigned int *)(rtc->regs + reg));

	return data;
}

static int ls1x_rtc_write_register(struct rtc_ls1x *rtc, unsigned long reg, 
			int data)
{
	(*(volatile unsigned int *)(rtc->regs + reg)) = data;
}
static int ls1x_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	struct rtc_ls1x	*rtc = dev_get_drvdata(dev);
	unsigned long now, now1;

	now = ls1x_rtc_read_register(rtc, RTC_TOYRLO);

	tm->tm_sec = ((now >> 4) & 0x3f);
	tm->tm_min = ((now >> 10) & 0x3f);
	tm->tm_hour = ((now >> 16) & 0x1f);
	tm->tm_mday = ((now >> 21) & 0x1f);
	tm->tm_mon = ((now >> 26) & 0x3f) -1;
		
	now1 = ls1x_rtc_read_register(rtc, RTC_TOYRHI);	

	tm->tm_year = (now1 -1900) ;

	return 0;
}

static int ls1x_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	struct rtc_ls1x *rtc = dev_get_drvdata(dev);
	unsigned long now;
	int ret;
	
	now = ((tm->tm_sec << 4) | (tm->tm_min << 10) | (tm->tm_hour << 16) | 
			(tm->tm_mday << 21) | (((tm->tm_mon+1)<< 26) ));
	spin_lock_irq(&rtc->lock);
	ls1x_rtc_write_register(rtc, RTC_TOYWLO, now);
//set year 	
	ls1x_rtc_write_register(rtc, RTC_TOYWHI, (tm->tm_year+1900) );
	spin_unlock_irq(&rtc->lock);
	
	return 0;
}

static int ls1x_rtc_readalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_ls1x *rtc = dev_get_drvdata(dev);
	unsigned long time;

	spin_lock_irq(&rtc->lock);
	time = ls1x_rtc_read_register(rtc, RTC_TOYMH0);
	spin_unlock_irq(&rtc->lock);	

	alrm->time.tm_sec = (time & 0x3f);
	alrm->time.tm_min = ((time >> 6) & 0x3f);
	alrm->time.tm_hour = ((time >> 12) & 0x1f);
	alrm->time.tm_mday = ((time >> 17) & 0x1f);
	alrm->time.tm_mon = ((time >> 22) & 0x1f);
	alrm->time.tm_year = ((time >> 26) & 0x3f);

	return 0;
}


static int ls1x_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_ls1x *rtc = dev_get_drvdata(dev);
	int time;
	
	time =(( alrm->time.tm_sec & 0x3f) | ((alrm->time.tm_min & 0x3f) << 6)
			| ((alrm->time.tm_hour & 0x1f) << 12) | ((alrm->time.tm_mday 
			& 0x1f) << 17) | ((alrm->time.tm_mon & 0xf) << 22) | 
			((alrm->time.tm_year & 0x3f) << 26));
	
	spin_lock_irq(&rtc->lock);
	ls1x_rtc_write_register(rtc, RTC_TOYMH0, time);
	spin_unlock_irq(&rtc->lock);

	return 0;
}

static int ls1x_rtc_ioctl(struct device *dev, unsigned int cmd,
			unsigned long arg)
{

	switch(cmd) {
		case RTC_PIE_ON:
			break;
		case RTC_PIE_OFF:
			break;
		case RTC_UIE_ON:
			break;
		case RTC_UIE_OFF:
			break;
		case RTC_AIE_ON:
			break;
		case RTC_AIE_OFF:
			break;
	
		default:
			return -ENOIOCTLCMD;
	}
	
	return 0;

}

static irqreturn_t ls1x_rtc_interrupt(int irq, void *dev_id)
{
	struct rtc_ls1x *rtc = (struct rtc_ls1x *)dev_id;
	unsigned long events = 0;
	
	spin_lock(&rtc->lock);

	events = RTC_AF | RTC_IRQF;
	rtc_update_irq(rtc->rtc, 1, events);
	spin_unlock(&rtc->lock);

}

static struct rtc_class_ops ls1x_rtc_ops = {
	.ioctl		= ls1x_rtc_ioctl,
	.read_time	= ls1x_rtc_readtime,
	.set_time	= ls1x_rtc_settime,
	.read_alarm	= ls1x_rtc_readalarm,
	.set_alarm	= ls1x_rtc_setalarm,
};


static int __init ls1x_rtc_probe(struct platform_device *pdev)
{
	struct resource *regs;
	struct rtc_ls1x *rtc;
	int    irq = -1;
	int    ret;

	rtc = kzalloc(sizeof(struct rtc_ls1x), GFP_KERNEL);
	if (!rtc) {
		dev_dbg(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM,0);
	if (!regs) {
		dev_dbg(&pdev->dev, "no nmio resource defined\n");
		ret = -ENXIO;
		goto out;
	}
	printk("the regs->start is 0x%x", regs->start);	
	irq = platform_get_irq(pdev, 0);
	printk("the irq is %d\n", irq);
	rtc->irq = irq;
	
	rtc->regs = ioremap(regs->start, regs->end - regs->start + 1);	
	if (!rtc->regs) {
		ret = -ENOMEM;
		dev_dbg(&pdev->dev, "could not map i/o memory\n");
		goto out;
	}
	spin_lock_init(&rtc->lock);

//next set control register
//	ls1x_rtc_write_register(rtc, RTC_CNTL, 0x800);  	
	(*(volatile int *)(0xbfe64040)) = 0x2d00;
	ret = request_irq(irq, ls1x_rtc_interrupt, IRQF_SHARED, "rtc", rtc);
	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq %d", irq);
		goto out_iounmap;
	}

	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev, 
				&ls1x_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc)) {
		dev_dbg(&pdev->dev, "could not register rtc device\n");
		ret = PTR_ERR(rtc->rtc);
		goto out_free_irq;
	}

	platform_set_drvdata(pdev, rtc);
	device_init_wakeup(&pdev->dev, 1);

	dev_info(&pdev->dev, "LS1X RTC  at %08lx irq %ld \n",
			(unsigned long)rtc->regs, rtc->irq);

	return 0;

out_free_irq:
	free_irq(irq, rtc);
out_iounmap:
	iounmap(rtc->regs);
out:
	kfree(rtc);
	return ret;
}


static int __exit ls1x_rtc_remove(struct platform_device *pdev)
{
	struct rtc_ls1x *rtc = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);

	free_irq(rtc->irq, rtc);
	iounmap(rtc->regs);
	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	platform_set_drvdata(pdev, NULL);

	return 0;
}


MODULE_ALIAS("platform:ls1x-rtc");

static struct platform_driver ls1x_rtc_driver = {
	.remove		= __exit_p(ls1x_rtc_remove),
	.driver		= {
		.name	= "ls1x-rtc",
		.owner	= THIS_MODULE,
	},

};

static int __init ls1x_rtc_init(void)
{
	return platform_driver_probe(&ls1x_rtc_driver, ls1x_rtc_probe);
}

module_init(ls1x_rtc_init);

static void __exit ls1x_rtc_exit(void)
{
	platform_driver_unregister(&ls1x_rtc_driver);

}
module_exit(ls1x_rtc_exit);

MODULE_AUTHOR("<ninglichen@loongson.cn>");
MODULE_DESCRIPTION("Real Time clock for loongson2fsb");
MODULE_LICENSE("GPL");
