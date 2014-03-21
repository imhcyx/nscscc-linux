/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2013.1.24 zhaokai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include "gpio_keys.h"
#include "gpio.h"

static unsigned long gpio_out_cfg[]={GPIO_04,GPIO_05,GPIO_06,GPIO_07,GPIO_02};

void ls1b_gpio_state_init(void)
{
    ls1b_gpio_init_register_pin(GPIO_04, GPIO_OUTPUT, GPIO_LOW);    
    ls1b_gpio_init_register_pin(GPIO_05, GPIO_OUTPUT, GPIO_LOW);    
    ls1b_gpio_init_register_pin(GPIO_06, GPIO_OUTPUT, GPIO_LOW);    
    ls1b_gpio_init_register_pin(GPIO_07, GPIO_OUTPUT, GPIO_LOW);    
    ls1b_gpio_init_register_pin(GPIO_02, GPIO_OUTPUT, GPIO_LOW);
    ls1b_gpio_init_register_pin(GPIO_34, GPIO_INPUT, 0);    
    ls1b_gpio_init_register_pin(GPIO_35, GPIO_INPUT, 0);    
    ls1b_gpio_init_register_pin(GPIO_36, GPIO_INPUT, 0);
    ls1b_gpio_init_register_pin(GPIO_37, GPIO_INPUT, 0);
    ls1b_gpio_init_register_pin(GPIO_38, GPIO_INPUT, 0);
    ls1b_gpio_init_register_pin(GPIO_39, GPIO_INPUT, 0);
    ls1b_gpio_init_register_pin(GPIO_40, GPIO_INPUT, 0);
    ls1b_gpio_init_register_pin(GPIO_41, GPIO_INPUT, 0);
}

static void en_inter(unsigned int irq)
{
    (*(volatile unsigned long *)0xbfd01094) |= 1<<(irq - 64 - 32);
    enable_irq(irq);
}

static void dis_inter(unsigned int irq)
{
    (*(volatile unsigned long *)0xbfd01094) |= 1<<(irq - 64 - 32);
    disable_irq_nosync(irq);
}

static void en_all_inter(struct platform_device *pdev)
{
    int i;
    int irq;
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
    for (i = 0; i < pdata->nbuttons; i++) 
    {
        irq = ls1b_gpio_to_irq(pdata->buttons[i].gpio);
        en_inter(irq);
    }
}

static void dis_all_inter(struct platform_device *pdev)
{
    int i;
    int irq;
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
    for (i = 0; i < pdata->nbuttons; i++) 
    {
        irq = ls1b_gpio_to_irq(pdata->buttons[i].gpio);
        dis_inter(irq);
    }
}

static irqreturn_t gpio_keys_handler(int irq, void *dev_id)
{
    int i, j, keycode, gpio, value;
    struct platform_device *pdev = dev_id;
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
    struct input_dev *input = platform_get_drvdata(pdev);
    static int irqcount=0;
    dis_all_inter(pdev);
    //dis_inter(irq);
   //printk("<0>gpio_keys_handler %d\n", irqcount++);

    for (i = 0; i < pdata->nbuttons; i++) 
    {
        gpio = pdata->buttons[i].gpio;
        if (irq == ls1b_gpio_to_irq(gpio)) 
        {
            if (!(ls1b_gpio_read_input_pin(gpio)))
            {
                mdelay(15);
                if (!(ls1b_gpio_read_input_pin(gpio)))
                {
                    for (j = 0; j < 5; j++)
                        ls1b_gpio_init_register_pin(gpio_out_cfg[j], GPIO_OUTPUT, GPIO_HIGH);
                    for (j = 0; j < 5; j++)
                    {
                        ls1b_gpio_init_register_pin(gpio_out_cfg[j], GPIO_OUTPUT, GPIO_LOW);
                        if (!(ls1b_gpio_read_input_pin(gpio)))
                        {
                            keycode = pdata->buttons[i].keycode[j].key;
                            pdata->buttons[i].keycode[j].value = 1 - pdata->buttons[i].keycode[j].value;
                            value = pdata->buttons[i].keycode[j].value;
                            if (keycode > 0 && keycode < 41)
                            {
                                input_report_key(input, keycode, value);
                                ls1b_gpio_init_register_pin(GPIO_03, GPIO_OUTPUT, GPIO_HIGH);
                                mdelay(20);
                                ls1b_gpio_init_register_pin(GPIO_03, GPIO_OUTPUT, GPIO_LOW);
                                input_sync(input);
                            }
                            ls1b_gpio_init_register_pin(gpio_out_cfg[j], GPIO_OUTPUT, GPIO_HIGH);
                            break;
                        }
                        ls1b_gpio_init_register_pin(gpio_out_cfg[j], GPIO_OUTPUT, GPIO_HIGH);    
                    }
                    for (j = 0; j < 5; j++)
                        ls1b_gpio_init_register_pin(gpio_out_cfg[j], GPIO_OUTPUT, GPIO_LOW);
                    break;
                }
            }    
        }
    }
    en_all_inter(pdev);
    return IRQ_HANDLED;
}

static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
    struct input_dev *input;
    int i, error, j, code;
    
    printk("gpio keys probe !!\n");
    ls1b_gpio_state_init();
  
    input = input_allocate_device();
    if (!input)
        return -ENOMEM;

    platform_set_drvdata(pdev, input);

    input->evbit[0] = BIT(EV_KEY);

    input->name = pdev->name;
    input->phys = "gpio-keys/input0";
    input->dev.parent = &pdev->dev;
    //input->private = pdata;

    input->id.bustype = BUS_HOST;
    input->id.vendor = 0x0001;
    input->id.product = 0x0001;
    input->id.version = 0x0100;

    for (i = 0; i < pdata->nbuttons; i++) {
        int irq = ls1b_gpio_to_irq(pdata->buttons[i].gpio);
        ls1b_gpio_set_irq_type(pdata->buttons[i].gpio, LS1B_EDGE_FALL);
        //ls1b_gpio_set_irq_type(pdata->buttons[i].gpio, LS1B_LEVEL_LOW);

        error = request_irq(irq, gpio_keys_handler, IRQF_SAMPLE_RANDOM,
                pdata->buttons[i].desc ? pdata->buttons[i].desc : "gpio_keys",
                pdev);
        if (error) {
            printk(KERN_ERR "gpio-keys: unable to claim irq %d; error %d\n",
                    irq, error);
            goto fail;
        }

        for (j = 0; j < pdata->buttons[i].keycount; j++){
            code = pdata->buttons[i].keycode[j].key;
            set_bit(code, input->keybit);
        }
    }

    error = input_register_device(input);
    if (error) {
        printk(KERN_ERR "Unable to register gpio-keys input device\n");
        goto fail;
    }

    return 0;

 fail:
    for (i = i - 1; i >= 0; i--)
        free_irq(ls1b_gpio_to_irq(pdata->buttons[i].gpio), pdev);

    input_free_device(input);

    return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
    struct input_dev *input = platform_get_drvdata(pdev);
    int i;

    for (i = 0; i < pdata->nbuttons; i++) {
        int irq = ls1b_gpio_to_irq(pdata->buttons[i].gpio);
        free_irq(irq, pdev);
    }

    input_unregister_device(input);

    return 0;
}

struct platform_driver gpio_keys_device_driver = {
    .probe        = gpio_keys_probe,
    .remove        = __devexit_p(gpio_keys_remove),
    .driver        = {
        .name    = "gpio_keys",
    }
};

static int __init gpio_keys_init(void)
{
    return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{
    platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaokai <zhaokai@loongson.cn>");
MODULE_DESCRIPTION("gpio keyboard driver for loongson");
