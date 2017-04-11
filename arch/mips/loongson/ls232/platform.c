/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/serial_8250.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <ls1x_board.h>
#include <ls1x_board_int.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/can/platform/sja1000.h>
#include <linux/stmmac.h>
#include <linux/phy.h>
#include <../drivers/input/keyboard/gpio_keys.h>
#include <linux/delay.h>
#include <linux/mtd/spinand.h>

#define LS232_UART_BASE 0x1fe40000
#define LS232_MAC_BASE 0x1ff00000
#define LS232_NAND_BASE 0x1fe78000
#define LS232_SPI_BASE 0x1fe80000

#define LS232_MAC_IRQ  (MIPS_CPU_IRQ_BASE+2)
#define LS232_UART_IRQ  (MIPS_CPU_IRQ_BASE+3)
#define LS232_SPI_IRQ  (MIPS_CPU_IRQ_BASE+4)
#define LS232_NAND_IRQ  (MIPS_CPU_IRQ_BASE+5)
#define LS232_DMA_IRQ  (MIPS_CPU_IRQ_BASE+6)

struct ls1a_nand_platform_data{
	int enable_arbiter;
	struct mtd_partition *parts;
	unsigned int nr_parts;
};

static struct mtd_partition ls1a_nand_partitions[]={
	[0] = {
		.name   ="kernel",
		.offset =0,
		.size   =0x40000000,
		//        .mask_flags =   MTD_WRITEABLE,
	},
	[1] = {
		.name   ="swap",
		.offset = 0x40000000,
		.size   = 0,

	},
};
static struct ls1a_nand_platform_data ls1a_nand_parts = {
	.enable_arbiter =   1,
	.parts          =   ls1a_nand_partitions,
	.nr_parts       =   ARRAY_SIZE(ls1a_nand_partitions),

};


#define LS1A_UARTCLK 33333333
struct plat_serial8250_port uart8250_data[] = {
	{ .uartclk=LS1A_UARTCLK, .mapbase=LS232_UART_BASE,.membase=(void *)CKSEG1ADDR(LS232_UART_BASE),.irq=LS232_UART_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
	{}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = uart8250_data,
	}
};

/*
 * ohci
 */
static int dma_mask=-1;

/*
 * dc
 */

static struct platform_device ls1x_dc_device = {
	.name           = "ls1x-fb",
	.id             = -1,
};

/*
 * gmac
 */


static struct resource ls1x_gmac1_resources[] = { 
	[0] = {
		.start          = LS232_MAC_BASE,
		.end            = (LS232_MAC_BASE + 0xffff),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS232_MAC_IRQ,
		.end            = LS232_MAC_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_gmac1_device = {
	.name           = "dmfe",
	.id             = 1,
	.dev = {
		.dma_mask=&dma_mask,
	},
	.num_resources  = ARRAY_SIZE(ls1x_gmac1_resources),
	.resource       = ls1x_gmac1_resources,
};



static struct resource ls1a_nand_resources[] = {
	[0] = {
		.start      =0,
		.end        =0,
		.flags      =IORESOURCE_DMA,    
	},
	[1] = {
		.start      =LS232_NAND_BASE,
		.end        =LS232_NAND_BASE+0x3fff,
		.flags      =IORESOURCE_MEM,
	},
	[2] = {
		.start      =0x1fd01160,
		.end        =0x1fd01160,
		.flags      =IORESOURCE_MEM,
	},
	[3] = {
		.start      =LS232_DMA_IRQ,
		.end        =LS232_DMA_IRQ,
		.flags      =IORESOURCE_IRQ,
	},
};

struct platform_device ls1a_nand_device = {
	.name       ="ls1a-nand",
	.id         =-1,
	.dev        ={
		.platform_data = &ls1a_nand_parts,
	},
	.num_resources  =ARRAY_SIZE(ls1a_nand_resources),
	.resource       =ls1a_nand_resources,
};

static struct resource ls1x_spi0_resources[]={
	[1]={
		.start      =   LS232_SPI_BASE,
		.end        =   LS232_SPI_BASE+0xff,
		.flags      =   IORESOURCE_MEM,
	},  
	[2]={
		.start      =   LS232_SPI_IRQ,
		.end        =   LS232_SPI_IRQ,
		.flags      =   IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_spi0_device={
	.name   =   "ls1x-spi",
	.id     =       0,
	.num_resources  =ARRAY_SIZE(ls1x_spi0_resources),
	.resource       =ls1x_spi0_resources,
};



static struct flash_platform_data ls1x_spiflash_data = {
	.name   = "m25p80",
	.parts  = NULL,
	.nr_parts   = 0,
	.type   = "m25p80",
};

static struct spi_board_info spi_board_info[] = {
	{
		.modalias       = "m25p80",
		//		.modalias       = "spidev",
		.max_speed_hz   = 1000000,
		.bus_num        = 0,
		.chip_select    = 0,
		.platform_data  = &ls1x_spiflash_data,
	},
};

static struct platform_device *ls1x_platform_devices[] __initdata = {
	&ls1x_spi0_device,
	&uart8250_device,
	&ls1x_gmac1_device,
	&ls1a_nand_device,
	&ls1x_dc_device,
};


extern unsigned long bus_clock;
int ls1b_platform_init(void)
{
	int ret, i;
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));

	for(i = 0; i < ARRAY_SIZE(uart8250_data); i++)
		uart8250_data[i].uartclk = bus_clock/2;

	return platform_add_devices(ls1x_platform_devices, ARRAY_SIZE(ls1x_platform_devices));

}

arch_initcall(ls1b_platform_init);
