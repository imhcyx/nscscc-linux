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

struct ls1b_nand_platform_data{
	int enable_arbiter;
	struct mtd_partition *parts;
	unsigned int nr_parts;
};

static struct mtd_partition ls1b_nand_partitions[]={
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
static struct ls1b_nand_platform_data ls1b_nand_parts = {
	.enable_arbiter =   1,
	.parts          =   ls1b_nand_partitions,
	.nr_parts       =   ARRAY_SIZE(ls1b_nand_partitions),

};


//#define LS1A_UARTCLK 83000000
//#define LS1A_UARTCLK 66666666
#define LS1A_UARTCLK 33333333
struct plat_serial8250_port uart8250_data[] = {
	{ .uartclk=LS1A_UARTCLK, .mapbase=0x1fe40000,.membase=(void *)0xbfe40000,.irq=2,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
	//{ .uartclk=LS1A_UARTCLK, .mapbase=0xbfe41000,.membase=(void *)0xbfe41000,.irq=LS1X_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
	{ .uartclk=LS1A_UARTCLK, .mapbase=0x1fe44000,.membase=(void *)0xbfe44000,.irq=3,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
	{ .uartclk = LS1A_UARTCLK, .mapbase=0x1fe48000,.membase=(void *)0xbfe48000,.irq=4,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
	{ .uartclk = LS1A_UARTCLK, .mapbase=0x1fe4c000,.membase=(void *)0xbfe4c000,.irq=5,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
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

static struct resource ls1x_ohci_resources[] = { 
	[0] = {
		.start          = LS1X_USB_OHCI_BASE,
		.end            = (LS1X_USB_OHCI_BASE + 0x1000 - 1),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_BOARD_OHCI_IRQ,
		.end            = LS1X_BOARD_OHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct ls1x_usbh_data  ls1x_ohci_platform_data={
	.ports=1,
};

static struct platform_device ls1x_ohci_device = {
	.name           = "ls1x-ohci",
	.id             = -1,
	.dev = {
		.platform_data = &ls1x_ohci_platform_data,
		.dma_mask=&dma_mask,
	},
	.num_resources  = ARRAY_SIZE(ls1x_ohci_resources),
	.resource       = ls1x_ohci_resources,
};

/*
 * ehci
 */

static struct resource ls1x_ehci_resources[] = { 
	[0] = {
		.start          = LS1X_USB_EHCI_BASE,
		.end            = (LS1X_USB_EHCI_BASE + 0x6b),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_BOARD_EHCI_IRQ,
		.end            = LS1X_BOARD_EHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct ls1x_usbh_data  ls1x_ehci_platform_data={
	.ports=1,
};

static struct platform_device ls1x_ehci_device = {
	.name           = "ls1x-ehci",
	.id             = -1,
	.dev = {
		.platform_data = &ls1x_ehci_platform_data,
		.dma_mask=&dma_mask,
	},
	.num_resources  = ARRAY_SIZE(ls1x_ehci_resources),
	.resource       = ls1x_ehci_resources,
};

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
		.start          = LS1X_BOARD_GMAC1_BASE,
		.end            = (LS1X_BOARD_GMAC1_BASE + 0x6b),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_BOARD_GMAC1_IRQ,
		.end            = LS1X_BOARD_GMAC1_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_gmac1_device = {
	.name           = "ls1x-gmac",
	.id             = 1,
	.dev = {
		.dma_mask=&dma_mask,
	},
	.num_resources  = ARRAY_SIZE(ls1x_gmac1_resources),
	.resource       = ls1x_gmac1_resources,
};


static struct resource ls1x_gmac2_resources[] = { 
	[0] = {
		.start          = LS1X_BOARD_GMAC2_BASE,
		.end            = (LS1X_BOARD_GMAC2_BASE + 0x6b),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_BOARD_GMAC2_IRQ,
		.end            = LS1X_BOARD_GMAC2_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_gmac2_device = {
	.name           = "ls1x-gmac",
	.id             = 2,
	.dev = {
		.dma_mask=&dma_mask,
	},
	.num_resources  = ARRAY_SIZE(ls1x_gmac2_resources),
	.resource       = ls1x_gmac2_resources,
};

static struct resource ls1b_nand_resources[] = {
	[0] = {
		.start      =0,
		.end        =0,
		.flags      =IORESOURCE_DMA,    
	},
	[1] = {
		.start      =0x1fe78000,
		.end        =0x1fe78020,
		.flags      =IORESOURCE_MEM,
	},
	[2] = {
		.start      =0x1fd01160,
		.end        =0x1fd01160,
		.flags      =IORESOURCE_MEM,
	},
	[3] = {
		.start      =LS1X_BOARD_DMA0_IRQ,
		.end        =LS1X_BOARD_DMA0_IRQ,
		.flags      =IORESOURCE_IRQ,
	},
};

struct platform_device ls1b_nand_device = {
	.name       ="ls1b-nand",
	.id         =-1,
	.dev        ={
		.platform_data = &ls1b_nand_parts,
	},
	.num_resources  =ARRAY_SIZE(ls1b_nand_resources),
	.resource       =ls1b_nand_resources,
};

static struct resource ls1x_spi0_resources[]={
	[1]={
		.start      =   0x1fe80000,
		.end        =   0x1fe80006,
		.flags      =   IORESOURCE_MEM,
	},  
	[2]={
		.start      =   LS1X_BOARD_SPI0_IRQ,
		.end        =   LS1X_BOARD_SPI0_IRQ,
		.flags      =   IORESOURCE_IRQ,
	},
};
static struct resource ls1x_spi1_resources[]={
	[1]={
		.start      =   0x1fec0000,
		.end        =   0x1fec0006,
		.flags      =   IORESOURCE_MEM,
	},
	[2]={
		.start      =   LS1X_BOARD_SPI1_IRQ,
		.end        =   LS1X_BOARD_SPI1_IRQ,
		.flags      =   IORESOURCE_IRQ,
	},
};
static struct platform_device ls1x_spi0_device={
	.name   =   "ls1x-spi",
	.id     =       0,
	.num_resources  =ARRAY_SIZE(ls1x_spi0_resources),
	.resource       =ls1x_spi0_resources,
};
static struct platform_device ls1x_spi1_device={
	.name   =   "ls1x-spi",
	.id     =       1,
	.num_resources  =ARRAY_SIZE(ls1x_spi1_resources),
	.resource       =ls1x_spi1_resources,
};

/*I2C*/
static struct resource ls1x_i2c_resource[] = {
	[0]={
		.start  = LS1X_BOARD_I2C_BASE,
		.end    = (LS1X_BOARD_I2C_BASE + 0x4),
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c_device = {
	.name       = "ls1x-i2c",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1x_i2c_resource),
	.resource   = ls1x_i2c_resource,
};	

static struct platform_device ls1x_audio_device = { 
	.name       = "ls1x-audio",
	.id         = -1, 
};

static struct platform_device loongson232_cpufreq_device = {
	.name = "loongson2_cpufreq",
	.id = -1,
};


//for gmac1
static struct resource ls1b_eth0_stmac_resources[] = { 
	[0] = {
		.start          = LS1X_BOARD_GMAC1_BASE,
		.end            = (LS1X_BOARD_GMAC1_BASE + 0x6b),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.name	= "macirq",
		.start          = LS1X_BOARD_GMAC1_IRQ,
		.end            = LS1X_BOARD_GMAC1_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct stmmac_mdio_bus_data ls1b_eth0_mdio_bus_data = {
	.bus_id		= 0,
	.phy_mask	= 0,
};

static struct plat_stmmacenet_data ls1b_eth0_data = {
	.bus_id		= 0,
	.phy_addr	= -1,
	.mdio_bus_data	= &ls1b_eth0_mdio_bus_data,
	.pbl		= 32,
	.has_gmac	= 1,
	.tx_coe		= 1,
	.interface = PHY_INTERFACE_MODE_GMII,
};

struct platform_device ls1b_eth0_device = {
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ls1b_eth0_stmac_resources),
	.resource	= ls1b_eth0_stmac_resources,
	.dev		= {
		.platform_data = &ls1b_eth0_data,
	},
};

//for gmac2
static struct resource ls1b_eth1_stmac_resources[] = { 
	[0] = {
		.start          = LS1X_BOARD_GMAC2_BASE,
		.end            = (LS1X_BOARD_GMAC2_BASE + 0x6b),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.name	= "macirq",
		.start          = LS1X_BOARD_GMAC2_IRQ,
		.end            = LS1X_BOARD_GMAC2_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct stmmac_mdio_bus_data ls1b_eth1_mdio_bus_data = {
	.bus_id		= 1,
	.phy_mask	= 0,
};

static struct plat_stmmacenet_data ls1b_eth1_data = {
	.bus_id		= 1,
	.phy_addr	= -1,
	.mdio_bus_data	= &ls1b_eth1_mdio_bus_data,
	.pbl		= 32,
	.has_gmac	= 1,
	.tx_coe		= 1,
	.interface = PHY_INTERFACE_MODE_GMII,
};

struct platform_device ls1b_eth1_device = {
	.name		= "stmmaceth",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(ls1b_eth1_stmac_resources),
	.resource	= ls1b_eth1_stmac_resources,
	.dev		= {
		.platform_data = &ls1b_eth1_data,
	},
};

/*
 * CAN
 */
static struct resource ls1b_can1_resources[] = {
	{
		.start   = 0x1fe50000,
		.end     = 0x1fe53fff,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = 6,
		.end     = 6,
		.flags   = IORESOURCE_IRQ,
	},
};
static struct resource ls1b_can2_resources[] = {
	{
		.start   = 0x1fe54000,
		.end     = 0x1fe57fff,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = 7,
		.end     = 7,
		.flags   = IORESOURCE_IRQ,
	},
};

struct sja1000_platform_data ls1b_sja1000_platform_data = {
	.clock		= 66000000,
	.ocr		= 0x40 | 0x18,
	.cdr		= 0x40,
};

static struct platform_device ls1b_can1_device = {
	.name = "sja1000_platform",
	.id = 1,
	.dev = {
		.platform_data = &ls1b_sja1000_platform_data,
	},
	.resource = ls1b_can1_resources,
	.num_resources = ARRAY_SIZE(ls1b_can1_resources),
};
static struct platform_device ls1b_can2_device = {
	.name = "sja1000_platform",
	.id = 2,
	.dev = {
		.platform_data = &ls1b_sja1000_platform_data,
	},
	.resource = ls1b_can2_resources,
	.num_resources = ARRAY_SIZE(ls1b_can2_resources),
};

static struct mtd_partition ls1x_spiflash_parts[] = {
	{
		.name   ="LS1X spi flash",
		.offset =0,
		.size   =MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data ls1x_spiflash_data = {
	.name   = "m25p80",
	.parts  = NULL,//ls1x_spiflash_parts,
	.nr_parts   = 0,//ARRAY_SIZE(ls1x_spiflash_parts),
	.type   = "m25p80",
	//.type   = "sst25vf080b",
};

static struct spi_board_info spi_board_info[] = {
#if 1
	{
		.modalias       = "m25p80",
		//		.modalias       = "spidev",
		.max_speed_hz   = 1000000,
		.bus_num        = 0,
		.chip_select    = 0,
		.platform_data  = &ls1x_spiflash_data,
	},
#endif
	{
		.modalias       = "m25p80",
		//		.modalias       = "spidev",
		.max_speed_hz   = 1000000,
		.bus_num        = 1,
		.chip_select    = 0,
		.platform_data  = &ls1x_spiflash_data,
	},
};

/*
 *RTC
 */

static struct resource ls1x_rtc_resource[] = {
	[0]={
		.start      = 0x1fe64020,
		.end        = (0x1fe64020 + 0x54),
		.flags      = IORESOURCE_MEM,
	},
	[1]={
		.start      = 26,
		.end        = 26,
		.flags      = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_rtc_device = {
	.name       = "ls1x-rtc",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1x_rtc_resource),
	.resource   = ls1x_rtc_resource,
};

/*
 *I2C
 */


#if 1
struct key code1[5] = {{6, 0}, {14, 0}, {22, 0}, {30, 0}, {38, 0}};
struct key code2[5] = {{5, 0}, {13, 0}, {21, 0}, {29, 0}, {37, 0}};
struct key code3[5] = {{4, 0}, {12, 0}, {20, 0}, {28, 0}, {36, 0}};
struct key code4[5] = {{3, 0}, {11, 0}, {19, 0}, {27, 0}, {35, 0}};
struct key code5[5] = {{2, 0}, {10, 0}, {18, 0}, {26, 0}, {34, 0}};
struct key code6[5] = {{1, 0}, {9,  0}, {17, 0}, {25, 0}, {33, 0}};
struct key code7[5] = {{7, 0}, {15, 0}, {23, 0}, {31, 0}, {39, 0}};
struct key code8[5] = {{8, 0}, {16, 0}, {24, 0}, {32, 0}, {40, 0}};

struct gpio_keys_button gpio_buttons[] = {
	{
		.keycount = 5,
		.keycode = code1,
		.gpio = 34,
	},

	{
		.keycount = 5,
		.keycode = code2,
		.gpio = 35,
	},

	{
		.keycount = 5,
		.keycode = code3,
		.gpio = 36,
	},

	{
		.keycount = 5,
		.keycode = code4,
		.gpio = 37,
	},

	{
		.keycount = 5,
		.keycode = code5,
		.gpio = 38,
	},

	{
		.keycount = 5,
		.keycode = code6,
		.gpio = 39,
	},

	{
		.keycount = 5,
		.keycode = code7,
		.gpio = 40,
	},

	{
		.keycount = 5,
		.keycode = code8,
		.gpio = 41,
	},
};

struct gpio_keys_platform_data pdata = {
	.buttons = &gpio_buttons,
	.nbuttons = 8,    	
};

struct platform_device ls1x_gpio_keys_device = {
	.name = "gpio_keys",
	.id	  = 0,
	.dev.platform_data = &pdata,
	.num_resources = 0,
	.resource = NULL,
};
#endif

static struct platform_device *ls1x_platform_devices[] __initdata = {
	&ls1x_spi0_device,
	&ls1x_spi1_device,
	&uart8250_device,
	&ls1x_ohci_device,
	&ls1x_ehci_device,
//	&ls1x_gmac1_device,
//	&ls1x_gmac2_device,
	&ls1b_eth0_device,
	&ls1b_eth1_device,
	&ls1b_nand_device,
	&ls1x_dc_device,
	&loongson232_cpufreq_device,
	//&ls1x_audio_device,
	&ls1b_can1_device,
	//&ls1b_can2_device,
	//&ls1x_i2c_device,
	//&ls1x_rtc_device,
	//&ls1x_gpio_keys_device,
};

static struct mtd_partition ls2h_spi_parts[] = {
	[0] = {
		.name		= "spinand0",
		.offset		= 0,
		.size		= 32 * 1024 * 1024,
	},
	[1] = {
		.name		= "spinand1",
		.offset		= 32 * 1024 * 1024,
		.size		= 0,	
	},
};

struct spi_nand_platform_data ls2h_spi_nand = {
	.name		= "LS_Nand_Flash",
	.parts		= ls2h_spi_parts,
	.nr_parts	= ARRAY_SIZE(ls2h_spi_parts),
};

static struct spi_board_info ls_ls1x_spi_device[]={
#if 0
	{
		.modalias   = "mmc_spi",
		.platform_data   = NULL,
		.bus_num    =0,
		.chip_select =1,
		.mode       =0,
	},
#endif
	{	/* spi nand Flash chip */
		.modalias	= "mt29f",	
		.bus_num 		= 1,
		.chip_select	= 0,
		.max_speed_hz	= 60000000,
		.platform_data	= &ls2h_spi_nand,
	},
};

static struct i2c_board_info ls1x_i2c_info[] __initdata = {
	{	
		I2C_BOARD_INFO("ds1307", 0x68),
	},
	{	
		I2C_BOARD_INFO("rx8025", 0x64),
	},
	{
		I2C_BOARD_INFO("zt2083", 0x48),
	},
	{
		I2C_BOARD_INFO("pca9539", 0x74),		
	}
};
#define AHCI_PCI_BAR  5
extern unsigned long bus_clock;
int ls1b_platform_init(void)
{
	int ret, i;
#ifdef CONFIG_USB
	*(volatile int *)0xbfd00424 &= ~0x80000000;
	*(volatile int *)0xbfd00424;
	mdelay(1);
	/*ls1g usb reset stop*/
	*(volatile int *)0xbfd00424 |= 0x80000000;
#endif
	//spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	spi_register_board_info(ls_ls1x_spi_device, ARRAY_SIZE(ls_ls1x_spi_device));
	ret = i2c_register_board_info(0,&ls1x_i2c_info,4);
	if (ret <0 )
		printk(KERN_ERR " i2c-ls1x: can't register board I2C device!!\n");

	for(i = 0; i < ARRAY_SIZE(uart8250_data); i++)
		uart8250_data[i].uartclk = bus_clock/2;

	return platform_add_devices(ls1x_platform_devices, ARRAY_SIZE(ls1x_platform_devices));

}

arch_initcall(ls1b_platform_init);
