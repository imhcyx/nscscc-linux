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


//#define LS1A_UARTCLK 83000000
//#define LS1A_UARTCLK 66666666
#define LS1A_UARTCLK 33333333
struct plat_serial8250_port uart8250_data[] = {
//{ .uartclk=LS1A_UARTCLK, .mapbase=0xbfe41000,.membase=(void *)0xbfe41000,.irq=LS1X_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .uartclk = LS1A_UARTCLK, .mapbase=0x1fe4c000,.membase=(void *)0xbfe4c000,.irq=5,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .uartclk=LS1A_UARTCLK, .mapbase=0x1fe40000,.membase=(void *)0xbfe40000,.irq=2,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .uartclk=LS1A_UARTCLK, .mapbase=0x1fe44000,.membase=(void *)0xbfe44000,.irq=3,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .uartclk = LS1A_UARTCLK, .mapbase=0x1fe48000,.membase=(void *)0xbfe48000,.irq=4,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
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
 * ahci platform
 */

static struct resource ls1a_ahci_resources[] = { 
 [0] = {
   .start          = 0x1fe30000,
   .end            = 0x1fe30000+0x1ff,
   .flags          = IORESOURCE_MEM,
 },
 [1] = {
   .start          = 36,
   .end            = 36,
   .flags          = IORESOURCE_IRQ,
 },
};

static void __iomem *ls1a_ahci_map_table[6];

static struct platform_device ls1a_ahci_device = {
 .name           = "ls1x-ahci",
 .id             = -1,
 .dev = {
   .platform_data = ls1a_ahci_map_table,
 },
 .num_resources  = ARRAY_SIZE(ls1a_ahci_resources),
 .resource       = ls1a_ahci_resources,
};

/*
 * ohci
 */
static int dma_mask=-1;

static struct resource ls1a_ohci_resources[] = { 
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

static struct ls1x_usbh_data  ls1a_ohci_platform_data={
	.ports=4,
};

static struct platform_device ls1a_ohci_device = {
 .name           = "ls1x-ohci",
 .id             = -1,
 .dev = {
   .platform_data = &ls1a_ohci_platform_data,
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1a_ohci_resources),
 .resource       = ls1a_ohci_resources,
};

/*
 * ehci
 */

static struct resource ls1a_ehci_resources[] = { 
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

static struct ls1x_usbh_data  ls1a_ehci_platform_data={
	.ports=4,
};

static struct platform_device ls1a_ehci_device = {
 .name           = "ls1x-ehci",
 .id             = -1,
 .dev = {
   .platform_data = &ls1a_ehci_platform_data,
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1a_ehci_resources),
 .resource       = ls1a_ehci_resources,
};

/*
 * dc
 */

static struct platform_device ls1a_dc_device = {
 .name           = "ls1x-fb",
 .id             = -1,
};

/*
 * gmac
 */

static struct resource ls1a_gmac1_resources[] = { 
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

static struct platform_device ls1a_gmac1_device = {
 .name           = "ls1x-gmac",
 .id             = 1,
 .dev = {
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1a_gmac1_resources),
 .resource       = ls1a_gmac1_resources,
};


static struct resource ls1a_gmac2_resources[] = { 
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

static struct platform_device ls1a_gmac2_device = {
 .name           = "ls1x-gmac",
 .id             = 2,
 .dev = {
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1a_gmac2_resources),
 .resource       = ls1a_gmac2_resources,
};

static struct resource ls1a_nand_resources[] = {
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

struct platform_device ls1a_nand_device = {
    .name       ="ls1a-nand",
    .id         =-1,
    .dev        ={
        .platform_data = &ls1a_nand_parts,
    },
    .num_resources  =ARRAY_SIZE(ls1a_nand_resources),
    .resource       =ls1a_nand_resources,
};

static struct resource ls1a_spi0_resources[]={
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
static struct resource ls1a_spi1_resources[]={
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
static struct platform_device ls1a_spi0_device={
        .name   =   "ls1x-spi",
        .id     =       0,
        .num_resources  =ARRAY_SIZE(ls1a_spi0_resources),
        .resource       =ls1a_spi0_resources,
};
static struct platform_device ls1a_spi1_device={
        .name   =   "ls1x-spi",
        .id     =       1,
        .num_resources  =ARRAY_SIZE(ls1a_spi1_resources),
        .resource       =ls1a_spi1_resources,
};

/*I2C*/
static struct resource ls1a_i2c_resource[] = {
	 [0]={
		.start  = LS1X_BOARD_I2C_BASE,
		.end    = (LS1X_BOARD_I2C_BASE + 0x4),
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device ls1a_i2c_device = {
	.name       = "ls1x-i2c",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1a_i2c_resource),
	.resource   = ls1a_i2c_resource,
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
static struct resource ls1a_eth0_stmac_resources[] = { 
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

static struct stmmac_mdio_bus_data ls1a_eth0_mdio_bus_data = {
	.bus_id		= 0,
	.phy_mask	= 0,
};

static struct plat_stmmacenet_data ls1a_eth0_data = {
	.bus_id		= 0,
	.phy_addr	= -1,
	.mdio_bus_data	= &ls1a_eth0_mdio_bus_data,
	.pbl		= 32,
	.has_gmac	= 1,
	.tx_coe		= 1,
	.interface = PHY_INTERFACE_MODE_GMII,
};

struct platform_device ls1a_eth0_device = {
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ls1a_eth0_stmac_resources),
	.resource	= ls1a_eth0_stmac_resources,
	.dev		= {
		.platform_data = &ls1a_eth0_data,
	},
};

//for gmac2
static struct resource ls1a_eth1_stmac_resources[] = { 
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

static struct stmmac_mdio_bus_data ls1a_eth1_mdio_bus_data = {
	.bus_id		= 1,
	.phy_mask	= 0,
};

static struct plat_stmmacenet_data ls1a_eth1_data = {
	.bus_id		= 1,
	.phy_addr	= -1,
	.mdio_bus_data	= &ls1a_eth1_mdio_bus_data,
	.pbl		= 32,
	.has_gmac	= 1,
	.tx_coe		= 1,
	.interface = PHY_INTERFACE_MODE_GMII,
};

struct platform_device ls1a_eth1_device = {
	.name		= "stmmaceth",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(ls1a_eth1_stmac_resources),
	.resource	= ls1a_eth1_stmac_resources,
	.dev		= {
		.platform_data = &ls1a_eth1_data,
	},
};

/*
 * CAN
 */
static struct resource ls1a_can1_resources[] = {
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
static struct resource ls1a_can2_resources[] = {
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

struct sja1000_platform_data ls1a_sja1000_platform_data = {
	.clock		= 66000000,
	.ocr		= 0x40 | 0x18,
	.cdr		= 0x40,
};

static struct platform_device ls1a_can1_device = {
	.name = "sja1000_platform",
	.id = 1,
	.dev = {
		.platform_data = &ls1a_sja1000_platform_data,
	},
	.resource = ls1a_can1_resources,
	.num_resources = ARRAY_SIZE(ls1a_can1_resources),
};
static struct platform_device ls1a_can2_device = {
	.name = "sja1000_platform",
	.id = 2,
	.dev = {
		.platform_data = &ls1a_sja1000_platform_data,
	},
	.resource = ls1a_can2_resources,
	.num_resources = ARRAY_SIZE(ls1a_can2_resources),
};

static struct mtd_partition ls1a_spiflash_parts[] = {
	{
		.name   ="LS1X spi flash",
		.offset =0,
		.size   =MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data ls1a_spiflash_data = {
	.name   = "m25p80",
	.parts  = NULL,//ls1a_spiflash_parts,
	.nr_parts   = 0,//ARRAY_SIZE(ls1a_spiflash_parts),
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
		.platform_data  = &ls1a_spiflash_data,
	},
#endif
	{
		.modalias       = "m25p80",
//		.modalias       = "spidev",
		.max_speed_hz   = 1000000,
		.bus_num        = 1,
		.chip_select    = 0,
		.platform_data  = &ls1a_spiflash_data,
	},
};

/*
*RTC
*/

static struct resource ls1a_rtc_resource[] = {
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

static struct platform_device ls1a_rtc_device = {
	.name       = "ls1x-rtc",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1a_rtc_resource),
	.resource   = ls1a_rtc_resource,
};

/*
*I2C
*/

static struct platform_device *ls1x_platform_devices[] __initdata = {
	&ls1a_spi0_device,
	&ls1a_spi1_device,
	&uart8250_device,
	&ls1a_ohci_device,
	&ls1a_ehci_device,
	//&ls1a_gmac1_device,
	//&ls1a_gmac2_device,
	&ls1a_eth0_device,
	&ls1a_eth1_device,
        &ls1a_nand_device,
	&ls1a_ahci_device,
	&ls1a_dc_device,
	&loongson232_cpufreq_device,
	&ls1x_audio_device,
	&ls1a_can1_device,
//	&ls1a_can2_device,
	&ls1a_i2c_device,
	&ls1a_rtc_device,
};

static struct spi_board_info ls1a_spi_device[]={
    {
        .modalias   = "mmc_spi",
        .platform_data   = NULL,
        .bus_num    =0,
        .chip_select =1,
        .mode       =0,
    },
};

static struct i2c_board_info ls1a_i2c_info[] __initdata = {
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
int ls1x_platform_init(void)
{
    int ret, i;
    //spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
    spi_register_board_info(ls1a_spi_device, ARRAY_SIZE(ls1a_spi_device));
	ret = i2c_register_board_info(0,&ls1a_i2c_info,4);
	if (ret <0 )
			printk(KERN_ERR " i2c-gs2fsb: can't register board I2C device!!\n");
	ls1a_ahci_map_table[AHCI_PCI_BAR]=ioremap_nocache(ls1a_ahci_resources[0].start,0x200);
#if 1
/*sata clk 125M*/
	*(volatile int *)0xbfd00418 = (*(volatile int *)0xbfd00418 & ~((3<<26)|(0x1f<<20)|(3<<18)))|(2<<26)|(5<<20)|(0<<18);
#else

	*(volatile int *)0xbfd00424 |= 0x80000000;
//      *(volatile int *)0xbfd00418  = 0x38682650; //100MHZ
        *(volatile int *)0xbfd00418  = 0x38502650; //125MHZ
        *(volatile int *)0xbfe30000 &= 0x0;
#endif
	
	for(i = 0; i < ARRAY_SIZE(uart8250_data); i++)
		uart8250_data[i].uartclk = bus_clock/2;

	return platform_add_devices(ls1x_platform_devices, ARRAY_SIZE(ls1x_platform_devices));

}

arch_initcall(ls1x_platform_init);
