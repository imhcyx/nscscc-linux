/*
 *arch/mips/loongson/ls1x-board/gpio.c
 *LOONSON LS1B GPIO driver 
 *
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include "gpio.h"
 
static inline void gpio_lock(unsigned long flags)
{
	local_irq_save(flags);
}

static inline void gpio_unlock(unsigned long flags)
{
	local_irq_restore(flags);
} 

static unsigned long access_map[2]={0x00000001, 0x00800000};
//static unsigned long access_map[2]={0};


int ls1b_gpio_register_pin(unsigned short pin)
{
    unsigned long flags,idx,offset,ret=-1;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);

    gpio_lock(flags);

    if(access_map[idx] & offset){
        printk(KERN_ERR "The pin is using...");
        goto out;
    }
    _reg_writel(idx*4+GPIO_EN,_reg_readl(idx*4 + GPIO_EN)|offset);
    access_map[idx]|=offset;

//    gpio_unlock(flags);
    
    ret=0;

out:
    gpio_unlock(flags);
    return ret;
}
EXPORT_SYMBOL(ls1b_gpio_register_pin);

//register and init direction and the init value
int ls1b_gpio_init_register_pin(unsigned short pin,int output,int value)
{
    unsigned long flags,idx,offset,tmp,val,ret=-1;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    val = value? GPIO_HIGH : GPIO_LOW;
    gpio_lock(flags);
    if(access_map[idx] & offset){
        printk(KERN_ERR "The pin is using...");
        goto out;
    }
    //set direction
    tmp = _reg_readl((idx*4) + GPIO_DIR);
    if(!output){
        _reg_writel((idx*4) + GPIO_DIR,tmp|offset);
    }else{
        _reg_writel((idx*4) + GPIO_DIR,tmp&(~offset));
    }
    //if output ; set out-value.   if input;NULL
    if(output){
        tmp = _reg_readl(idx*4 + GPIO_OUT_VAL);
        if(val)
            _reg_writel(idx*4 + GPIO_OUT_VAL,tmp|offset);
        else
            _reg_writel(idx*4 + GPIO_OUT_VAL,tmp&(~offset));
    }
    //enable the PIN-GPIO func  
    _reg_writel(idx*4+GPIO_EN,_reg_readl(idx*4 + GPIO_EN)|offset);
    access_map[idx]|=offset;
//    gpio_unlock(flags);
    ret=0;
out:
    gpio_unlock(flags);
    return ret;
}
EXPORT_SYMBOL(ls1b_gpio_init_register_pin);


int ls1b_gpio_unregister_pin(unsigned short pin)
{
    unsigned long flags,idx,offset,ret=-1;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
 
    gpio_lock(flags); 
    
    access_map[idx] &= (~offset);
    _reg_writel(idx*4+GPIO_EN,_reg_readl(idx*4 + GPIO_EN)&(~offset));
    
    ret=0;
    gpio_unlock(flags);
    return ret; 
}
EXPORT_SYMBOL(ls1b_gpio_unregister_pin);

/*ls1b_gpio_read_input_pin  only read the INPUT VALUE register*/
int ls1b_gpio_read_input_pin(unsigned short pin)
{
    unsigned long flags,idx,offset,tmp;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    if(!(access_map[idx] & offset)){
        printk(KERN_ERR "The pin is not using... +134");
        return -1;
    }
  
    gpio_lock(flags);  

    tmp = _reg_readl(idx*4 + GPIO_IN_VAL);

    gpio_unlock(flags);
  
    return ((tmp & offset)?GPIO_HIGH:GPIO_LOW);
}
EXPORT_SYMBOL(ls1b_gpio_read_input_pin);

/*ls1b_gpio_read_output_pin  only read the OUTPUT VALUE register*/
int ls1b_gpio_read_output_pin(unsigned short pin)
{
    unsigned long flags,idx,offset,tmp;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    if(!(access_map[idx] & offset)){
        printk(KERN_ERR "The pin is not using... +160");
        return -1;
    } 
    gpio_lock(flags);  

    tmp = _reg_readl(idx*4 + GPIO_OUT_VAL);

    gpio_unlock(flags);
  
    return ((tmp & offset)?GPIO_HIGH:GPIO_LOW);
}
EXPORT_SYMBOL(ls1b_gpio_read_output_pin);



/*ls1b_gpio_write_pin  only write the OUTPUT VALUE register*/
int ls1b_gpio_write_pin(unsigned short pin, int value)
{
    unsigned long flags,idx,offset,val,tmp;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR  "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    val = value? GPIO_HIGH : GPIO_LOW;
    if(!(access_map[idx] & offset)){
        printk(KERN_ERR "The pin is not using... +188");
        return -1;
    }
        
    gpio_lock(flags);  
    
    tmp = _reg_readl(idx*4 + GPIO_OUT_VAL);
    if(val)
        _reg_writel(idx*4 + GPIO_OUT_VAL,tmp|offset);
    else
        _reg_writel(idx*4 + GPIO_OUT_VAL,tmp&(~offset));
        
    gpio_unlock(flags);
    return 0;
}

EXPORT_SYMBOL(ls1b_gpio_write_pin);


/*0:input 1:output*/
int ls1b_gpio_set_pin_direction(unsigned short pin, int output)
{
    unsigned long flags,idx,offset,tmp,ret=-1;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    
    gpio_lock(flags);
    if(!(access_map[idx] & offset)){
        printk(KERN_ERR "The pin is not using... +222");
        printk("access_map[%d] = %d, offset = 0x%x\n", idx, access_map[idx], offset);
        goto out;
    }    
    if(!output){
        tmp = _reg_readl((idx*4) + GPIO_EN);
        _reg_writel((idx*4) + GPIO_EN,tmp|offset);
        tmp = _reg_readl((idx*4) + GPIO_DIR);
        _reg_writel((idx*4) + GPIO_DIR,tmp|offset);
        printk("gpio config to input, idx = %d, offset = 0x%x\n", idx, offset);
    }else{
        tmp = _reg_readl((idx*4) + GPIO_EN);
        _reg_writel((idx*4) + GPIO_EN,tmp|offset);
        tmp = _reg_readl((idx*4) + GPIO_DIR);
        _reg_writel((idx*4) + GPIO_DIR,tmp&(~offset));
    } 
    ret = 0;
out:
    gpio_unlock(flags);
    return ret; 
}
EXPORT_SYMBOL(ls1b_gpio_set_pin_direction);

/*0:input 1:output*/
int ls1b_gpio_read_pin_direction(unsigned short pin)
{
    unsigned long flags,idx,offset,tmp,ret=-1;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    idx=PIN_IDX(pin);
    offset=PIN_OFFSET(pin);
    if(!(access_map[idx] & offset)){
        printk(KERN_ERR "The pin is not using... +250");
        return -1;
    }
        
    gpio_lock(flags);
    
    tmp = _reg_readl((idx*4) + GPIO_DIR);
     
    gpio_unlock(flags);
    return ((tmp & offset)? GPIO_OUTPUT : GPIO_INPUT);

}
EXPORT_SYMBOL(ls1b_gpio_read_pin_direction);


int ls1b_gpio_to_irq(unsigned short pin)
{
    return pin+64;
}
EXPORT_SYMBOL(ls1b_gpio_to_irq);
#define GPIO_IRQ_STATUS  0x0
#define GPIO_IRQ_EN      0x4
#define GPIO_IRQ_SET     0x8
#define GPIO_IRQ_CLR     0xc
#define GPIO_IRQ_LEVEL   0x10
#define GPIO_IRQ_EDGE    0x14

#define gpio_irq_set(x,pin) do{                                                                 \
            tmp =   _reg_readl((PIN_IDX(pin)*0x18)+(x) + GPIO_IRQ_BASE);                        \
            _reg_writel((PIN_IDX(pin)*0x18)+(x)+ GPIO_IRQ_BASE,tmp|PIN_OFFSET(pin));            \
}while(0)
#define gpio_irq_clr(x,pin) do{                                                                 \
            tmp =   _reg_readl((PIN_IDX(pin)*0x18)+(x) + GPIO_IRQ_BASE);                        \
            _reg_writel((PIN_IDX(pin)*0x18)+(x)+ GPIO_IRQ_BASE,tmp&(~PIN_OFFSET(pin)));         \
}while(0)

//type:  0:edge_fall   1:edge_up  2:level_high  3:level_low
int ls1b_gpio_set_irq_type(unsigned short pin,unsigned char type)
{
    unsigned long tmp,flags;
    gpio_lock(flags);
    switch(type){
        case 0:
            gpio_irq_set(GPIO_IRQ_EDGE,pin);
            gpio_irq_clr(GPIO_IRQ_LEVEL,pin);
            gpio_irq_set(GPIO_IRQ_CLR,pin);
            gpio_irq_clr(GPIO_IRQ_SET,pin);
            gpio_irq_set(GPIO_IRQ_EN,pin);
            break;
        case 1:
            gpio_irq_set(GPIO_IRQ_EDGE,pin);
            gpio_irq_set(GPIO_IRQ_LEVEL,pin);
            gpio_irq_set(GPIO_IRQ_CLR,pin);
            gpio_irq_clr(GPIO_IRQ_SET,pin);
            gpio_irq_set(GPIO_IRQ_EN,pin);
            break;
        case 2:
            gpio_irq_clr(GPIO_IRQ_EDGE,pin);
            gpio_irq_set(GPIO_IRQ_LEVEL,pin);
            gpio_irq_set(GPIO_IRQ_CLR,pin);
            gpio_irq_clr(GPIO_IRQ_SET,pin);
            gpio_irq_set(GPIO_IRQ_EN,pin);
            break;
        case 3:
            gpio_irq_clr(GPIO_IRQ_EDGE,pin);
            gpio_irq_clr(GPIO_IRQ_LEVEL,pin);
            gpio_irq_set(GPIO_IRQ_CLR,pin);
            gpio_irq_clr(GPIO_IRQ_SET,pin);
            gpio_irq_set(GPIO_IRQ_EN,pin);
            break;
        default:
            break;
    }
    gpio_unlock(flags);
}
EXPORT_SYMBOL(ls1b_gpio_set_irq_type);

int ls1b_gpio_mask_intr(unsigned short pin)
{
    unsigned long tmp, flags;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    gpio_lock(flags);
    
	gpio_irq_clr(GPIO_IRQ_EN,pin);
	gpio_irq_set(GPIO_IRQ_CLR,pin);

    gpio_unlock(flags);
    return 0; 
}
EXPORT_SYMBOL(ls1b_gpio_mask_intr);

int ls1b_gpio_unmask_intr(unsigned short pin)
{
    unsigned long tmp, flags;
    if(unlikely((pin > GPIO_MAX) || (pin < GPIO_MIN)))
    {
        printk(KERN_ERR "NO find the GPIO PIN...");
        return -1;
    }
    gpio_lock(flags);
    
	gpio_irq_set(GPIO_IRQ_EN,pin);
	gpio_irq_set(GPIO_IRQ_CLR,pin);

    gpio_unlock(flags);
    return 0; 
}
EXPORT_SYMBOL(ls1b_gpio_unmask_intr);

void ls1b_gpio_change_edge(unsigned short irq)
{
    unsigned long tmp,idx,offset;
        if(irq<64 || irq>128)
            return;
        idx = PIN_IDX(irq-64);
        offset = PIN_OFFSET(irq-64);
        tmp = _reg_readl((idx*0x18)+GPIO_IRQ_EDGE + GPIO_IRQ_BASE);
        if(likely(tmp && offset)){
            tmp = _reg_readl((idx*0x18)+GPIO_IRQ_LEVEL + GPIO_IRQ_BASE);
            _reg_writel((idx*0x18)+GPIO_IRQ_LEVEL+ GPIO_IRQ_BASE ,tmp ^ offset);
        }
}
EXPORT_SYMBOL(ls1b_gpio_change_edge);

void ls1b_gpio_set_interupt(unsigned short pin)
{
    ls1b_gpio_set_pin_direction(pin,0);
}
EXPORT_SYMBOL(ls1b_gpio_set_interupt);


  

















