/*
 * Loongson ls1x power management with ACPI support
 *
 *	 Copyright (C) 2010 Loongson Technology
 *   Author: Zeng Lu <zenglu@loongson.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/suspend.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/cacheflush.h>

//#include <loongson.h>
#include <ls1x_board.h>

extern void prom_printf(char *fmt, ...);

#define APB_BASE	0xbfe40000
#define ACPI_BASE	0x3c000+APB_BASE
#define GEN_PMCON_1 ACPI_BASE+0x30
#define GEN_PMCON_2 ACPI_BASE+0x34
#define GEN_PMCON_3 ACPI_BASE+0x38
#define PM1_STS		ACPI_BASE+0x0
#define PM1_EN		ACPI_BASE+0x4
#define PM1_CNT		ACPI_BASE+0x8
#define PM1_TMR		ACPI_BASE+0xc
#define PROC_CNT	ACPI_BASE+0x10

#define GPE_STS		ACPI_BASE+0x20
#define GPE_EN		ACPI_BASE+0x24

#define	INT0EN		0xbfd01044
#define	INT0POL		0xbfd01050
#define	INT0EDGE	0xbfd01054

#define	INT1EN		0xbfd0105c
#define	INT1POL		0xbfd01068
#define	INT1EDGE	0xbfd0106c

#define	INT2EN		0xbfd01074
#define	INT2POL		0xbfd01080
#define	INT2EDGE	0xbfd01084

#define	INT3EN		0xbfd0108c
#define	INT3POL		0xbfd01098
#define	INT3EDGE	0xbfd0109c

#define	INT4EN		0xbfd010ac
#define	INT4POL		0xbfd010b8
#define	INT4EDGE	0xbfd010bc
struct wakeup_state_t {
	unsigned long gen_regs[16];
	unsigned long cp0_status;
	unsigned long cp0_context;
	unsigned long cp0_pagemask;
};

static struct wakeup_state_t wakeup_state;

static unsigned long wakeup_state_address = 0xa01FFC00;

static unsigned int target_sleep_state = 0;
static unsigned int cached_local_int_flags;
static unsigned int cached_int0_en;  
static unsigned int cached_int0_pol;  
static unsigned int cached_int0_edge;  

static unsigned int cached_int1_en;  
static unsigned int cached_int1_pol;  
static unsigned int cached_int1_edge;  

static unsigned int cached_int2_en;  
static unsigned int cached_int2_pol;  
static unsigned int cached_int2_edge;  

static unsigned int cached_int3_en;  
static unsigned int cached_int3_pol;  
static unsigned int cached_int3_edge;  

static unsigned int cached_int4_en;  
static unsigned int cached_int4_pol;  
static unsigned int cached_int4_edge;  

void arch_suspend_disable_irqs(void)
{
    /* disable all mips events */
    //local_irq_disable();
	local_irq_save(cached_local_int_flags);

	cached_int0_en	= readl(INT0EN);
	cached_int0_pol = readl(INT0POL);
	cached_int0_edge= readl(INT0EDGE);

	cached_int1_en	= readl(INT1EN);
	cached_int1_pol = readl(INT1POL);
	cached_int1_edge= readl(INT1EDGE);

	cached_int2_en	= readl(INT2EN);
	cached_int2_pol = readl(INT2POL);
	cached_int2_edge= readl(INT2EDGE);

	cached_int3_en	= readl(INT3EN);
	cached_int3_pol = readl(INT3POL);
	cached_int3_edge= readl(INT3EDGE);

	cached_int4_en	= readl(INT4EN);
	cached_int4_pol = readl(INT4POL);
	cached_int4_edge= readl(INT4EDGE);

	writel(0,INT0EN);
	writel(0,INT1EN);
	writel(0,INT2EN);
	writel(0,INT3EN);
	writel(0,INT4EN);
#if 0
#ifdef CONFIG_I8259
    /* disable all events of i8259A */
    cached_slave_mask = inb(PIC_SLAVE_IMR);
    cached_master_mask = inb(PIC_MASTER_IMR);
    outb(0xff, PIC_SLAVE_IMR);
    inb(PIC_SLAVE_IMR);
    outb(0xff, PIC_MASTER_IMR);
    inb(PIC_MASTER_IMR);
#endif
    /* disable all events of bonito */
    LOONGSON_INTENCLR = 0xffff;
    (void)LOONGSON_INTENCLR;
#endif
}

void arch_suspend_enable_irqs(void)
{
    /* enable all mips events */
    //local_irq_enable();
	local_irq_restore(cached_local_int_flags);

	writel(cached_int0_en,	INT0EN);
	writel(cached_int0_pol, INT0POL);
	writel(cached_int0_edge,INT0EDGE);

	writel(cached_int1_en,	INT1EN);
	writel(cached_int1_pol, INT1POL);
	writel(cached_int1_edge,INT1EDGE);

	writel(cached_int2_en,	INT2EN);
	writel(cached_int2_pol, INT2POL);
	writel(cached_int2_edge,INT2EDGE);

	writel(cached_int3_en,	INT3EN);
	writel(cached_int3_pol, INT3POL);
	writel(cached_int3_edge,INT3EDGE);

	writel(cached_int4_en,	INT4EN);
	writel(cached_int4_pol, INT4POL);
	writel(cached_int4_edge,INT4EDGE);

#if 0
#ifdef CONFIG_I8259
    /* only enable the cached events of i8259A */
    outb(cached_slave_mask, PIC_SLAVE_IMR);
    outb(cached_master_mask, PIC_MASTER_IMR);
#endif
    /* enable all cached events of bonito */
    LOONGSON_INTENSET = cached_bonito_irq_mask;
    (void)LOONGSON_INTENSET;
#endif
}


static int ls1x_pm_valid_state(suspend_state_t state)
{
    switch (state) {
      case PM_SUSPEND_ON:
		  return 0;
	  case PM_SUSPEND_STANDBY:
		  return 1;
	  case PM_SUSPEND_MEM:
		  return 3;
	  default:
	      return 0;
	}
}

static void acpi_mem_checksum()
{
	volatile unsigned int * addr;
	volatile unsigned int sum=0;
	for(addr=(unsigned int *)0xa0200000;addr<(unsigned int *)0xa0400000;addr+=4)
	  sum+=(unsigned int)(*addr);
	
	printk("ACPI MEMORY CHECKSUM: %x\n", sum);
}


static int ls1x_pm_begin(suspend_state_t state)
{
	//acpi_mem_checksum();
	target_sleep_state = state;
	return 0;
}

static int ls1x_pm_prepare(void)
{
	if(target_sleep_state == 3){
		//flush_cache_all();
		mmiowb();
	}
	return 0;
}

void ls1x_save_state_and_wakeup_code(void)
{
	/* Save cpu state*/
	asm volatile(  
	"move	$8, %0\n\t"
	"la $9,	1f\n\t"
	"sw	$9, ($8)\n\t"
	"sw	$29, 4($8)\n\t"
	"sw	$30, 8($8)\n\t"
	"sw	$28, 12($8)\n\t"
	"sw	$16, 16($8)\n\t"
	"sw	$17, 20($8)\n\t"
    "sw	$18, 24($8)\n\t"
    "sw	$19, 28($8)\n\t"
    "sw	$20, 32($8)\n\t"
    "sw	$21, 36($8)\n\t"
	"sw	$22, 40($8)\n\t"
    "sw	$23, 44($8)\n\t"

    "sw	$26, 48($8)\n\t"//k0
    "sw	$27, 52($8)\n\t"//k1

    "sw	$2, 56($8)\n\t"//v0
    "sw	$3, 60($8)\n\t"//v1

	"mfc0	$9, $12\n\t"
	"mfc0	$10, $4\n\t"
	"mfc0	$11, $5\n\t"
	"sw	$9, 64($8)\n\t"
	"sw	$10, 68($8)\n\t"
	"sw	$11, 72($8)\n\t"
	"1:"
	:
	:"r" (&wakeup_state)
	:"$8","$31","$9","$10","$11"
	);
}

void ls1x_restore_state_and_wakeup_code(void)
{
	/* ###### This function is not implemented 
	   since PMON is resposible for the recovery task ###### */
}

static int ls1x_pm_enter(suspend_state_t state)
{
	u32 reg;
	unsigned long flags = 0;
	if(target_sleep_state == 3){
			/* Save cpu state*/
	asm volatile(  
	"move	$8, %0\n\t"
	"la $9,	1f\n\t"
	"sw	$9, ($8)\n\t"
	"sw	$29, 4($8)\n\t"
	"sw	$30, 8($8)\n\t"
	"sw	$28, 12($8)\n\t"
	"sw	$16, 16($8)\n\t"
	"sw	$17, 20($8)\n\t"
    "sw	$18, 24($8)\n\t"
    "sw	$19, 28($8)\n\t"
    "sw	$20, 32($8)\n\t"
    "sw	$21, 36($8)\n\t"
	"sw	$22, 40($8)\n\t"
    "sw	$23, 44($8)\n\t"

    "sw	$26, 48($8)\n\t"//k0
    "sw	$27, 52($8)\n\t"//k1

    "sw	$2, 56($8)\n\t"//v0
    "sw	$3, 60($8)\n\t"//v1

	"mfc0	$9, $12\n\t"
	"mfc0	$10, $4\n\t"
	"mfc0	$11, $5\n\t"
	"sw	$9, 64($8)\n\t"
	"sw	$10, 68($8)\n\t"
	"sw	$11, 72($8)\n\t"
	"1:"
	:
	:"r" (&wakeup_state)
	:"$8","$31","$9","$10","$11"
	);

	//	ls1x_save_state_and_wakeup_code();
		reg = readl((void *)PM1_CNT);
		reg &= (7<<10);
		if(reg!=0)
			goto finish;
		/* Save wakeup_state to low memory*/
		memcpy((void *)wakeup_state_address, &wakeup_state, sizeof(wakeup_state));
#if 0
		prom_printf("ACPI COPY CONTEXT\n");
		int i;
		unsigned int *tmp1;
		unsigned int tmp2;
		for(i=0;i<19;i++){
			tmp1 =  (unsigned int *)(wakeup_state_address+4*i);
			tmp2 =  *tmp1;
			prom_printf("%x: %x\n ",tmp1, tmp2);
		}
#endif

	}
	//local_irq_save(flags);
	/* Clear WAK_STS */
	reg = readl((void *)PM1_STS);
	reg |= (1<<15);
	writel(reg, (void *)PM1_STS);
	/* clear pm1_sts*/
	writel(0xffffffff,(void *)PM1_STS);
	/* get PM1A control */
	reg = readl((void *)PM1_CNT);
	/* Clear the SLP_EN and SLP_TYP fields */
	reg &= ~(15<<10);
	writel(reg, (void *)PM1_CNT);
	/* Insert the SLP_TYP bits */
	reg |= ((state+2)<<10);
	/* Write #1: write the SLP_TYP data to the PM1 Control registers */
	writel(reg, (void *)PM1_CNT);
	/* Insert the sleep enable (SLP_EN) bit */
	//reg |= (1<<13);
	/* Flush caches, as per ACPI specification */
	__flush_cache_all();
	/* Write #2: Write both SLP_TYP + SLP_EN */
	//writel(reg, (void *)PM1_CNT);
//	((void (*)(void))ioremap_nocache(0x1fc00480, 4)) ();
	if(target_sleep_state == 3)
		prom_printf("ENTER ACPI STR(S3) STATE\n");
	if(target_sleep_state == 4)
		prom_printf("ENTER ACPI STD(S4) STATE\n");
	#if 1
	asm volatile(
		"li	$26,	0xbfc00480\n\t"
		"jr	$26\n\t"
		"nop"
	);
	#endif

	/* Wait until we enter sleep state */
#if 0
	do{
		reg = readl((void *)PM1_STS)&(1<<15);
	}while(!reg);
#endif
	do{}while(1);
	/*Hibernation should stop here and won't be executed any more*/

	/*=====================WAKE UP===========================*/
finish:
	//local_irq_restore(flags);
	if(target_sleep_state == 3)
		prom_printf("LEAVE ACPI STR(S3) STATE\n");
	if(target_sleep_state == 4)
		prom_printf("LEAVE ACPI STD(S4) STATE\n");
	if(target_sleep_state == 3){
		ls1x_restore_state_and_wakeup_code();
	}
	return 0;
}

static void ls1x_pm_wake(void)
{
}

static void ls1x_pm_finish(void)
{
	writel(-1, (void *)PM1_STS);
	writel(-1, (void *)GPE_STS);
	writel(0, (void *)PM1_CNT);

	//acpi_mem_checksum();
#if 0
	/* Enable power button: clear PWRBTN_STS, enable PWRBTN_EN */
	u32 reg;
	reg = readl((void *)PM1_STS);
	reg |= (1<<8);
	writel(reg, (void *)PM1_STS);

	reg = readl((void *)PM1_EN);
	reg |= (1<<8);
	writel(reg, (void *)PM1_EN);

	/* Clear WAK_STS */
	reg = readl((void *)PM1_STS);
	reg |= (1<<15);
	writel(reg, (void *)PM1_STS);

	/*set SCI_EN*/
	reg = readl((void *)PM1_CNT);
	reg |= 1;
	writel(reg, (void *)PM1_CNT);
#endif
}

static void ls1x_pm_end(void)
{
	target_sleep_state = 0;
}

static struct platform_suspend_ops ls1x_suspend_ops = {
	.valid			= ls1x_pm_valid_state,
	.begin			= ls1x_pm_begin,
	.prepare_late	= ls1x_pm_prepare,/* ???? */
	.enter			= ls1x_pm_enter,
	.wake			= ls1x_pm_wake,
	.finish			= ls1x_pm_finish,
	.end			= ls1x_pm_end,
};
 
/*
 * The ACPI specification wants us to save NVS memory regions during hibernation
 * and to restore them during the subsequent resume.  However, it is not certain
 * if this mechanism is going to work on all machines, so we allow the user to
 * disable this mechanism using the 'acpi_sleep=s4_nonvs' kernel command line
 * option.
 */
static bool s4_no_nvs;

void __init acpi_s4_no_nvs(void)
{
	s4_no_nvs = true;
}

static int ls1x_hibernation_begin(void)
{
	int error;

	//error = s4_no_nvs ? 0 : hibernate_nvs_alloc();
	if (!error) {
		target_sleep_state = 4;
	}

	printk(KERN_INFO "***ls1x_hibernation_begin***\n");
	return error;
}

static int ls1x_hibernation_pre_snapshot(void)
{
	int error = ls1x_pm_prepare();

	if (!error)
		hibernate_nvs_save();

	printk(KERN_INFO "***ls1x_hibernation_pre_snapshot***\n");
	return error;
}

static int ls1x_hibernation_enter(void)
{
	int error = ls1x_pm_enter(4);
	printk(KERN_INFO "***ls1x_hibernation_enter***\n");
	return error;
}

static void ls1x_hibernation_finish(void)
{
	//hibernate_nvs_free();
	ls1x_pm_finish();
	printk(KERN_INFO "***ls1x_hibernation_finish***\n");
}

static void ls1x_hibernation_leave(void)
{
	/* Restore the NVS memory area */
	//hibernate_nvs_restore();
	printk(KERN_INFO "***ls1x_hibernation_leave***\n");
}

static int ls1x_hibernation_pre_restore(void)
{
	printk(KERN_INFO "***ls1x_hibernation_pre_restore***\n");
	return 0;
}
static void ls1x_hibernation_restore_cleanup(void)
{
	printk(KERN_INFO "***ls1x_hibernation_restore_cleanup***\n");
}
static struct platform_hibernation_ops ls1x_hibernation_ops = {
	.begin = ls1x_hibernation_begin,
	.end = ls1x_pm_end,
	.pre_snapshot = ls1x_hibernation_pre_snapshot,
	.finish = ls1x_hibernation_finish,
	.prepare = ls1x_pm_prepare,
	.enter = ls1x_hibernation_enter,
	.leave = ls1x_hibernation_leave,
	.pre_restore=ls1x_hibernation_pre_restore,
	.restore_cleanup=ls1x_hibernation_restore_cleanup,
};

static int __init ls1x_pm_init(void)
{
	suspend_set_ops(&ls1x_suspend_ops);
	hibernation_set_ops(&ls1x_hibernation_ops);
	return 0;
}
arch_initcall(ls1x_pm_init);
