/*
 *
 * BRIEF MODULE DESCRIPTION
 *	FCR SOC system controller defines.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: ICT.
 *         	plj@ict.ac.cn
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LS1X_BOARD__H__
#define __LS1X_BOARD__H__

#include <asm/addrspace.h>
#include <asm/types.h>

/*
 * Configuration address and data registers
 */
#define CNF_ADDR        0x1e0
#define CNF_DATA        0x1e4

/* LS1X FPGA BOARD Memory control regs */
#define LS1X_BOARD_SD_BASE		0x1f000000
#define REG_SD_TIMING			0x0
#define REG_SD_MOD_SIZE			0x4

/* AHB BUS control regs */
#define LS1X_BOARD_AHB_MISC_BASE	 0x1f003200
#define AHB_MISC_CTRL		0x00
//#define AHB_CLK			33333333
#define AHB_CLK			50000000
/* Interrupt register */
#define REG_INT_EDGE		0x04
#define REG_INT_STEER		0x08
#define REG_INT_POL		0x0c
#define REG_INT_SET		0x10
#define REG_INT_CLR		0x14
#define REG_INT_EN		0x18
#define REG_INT_ISR		0x1c
#define LS1X_BOARD_INTC_BASE	LS1X_BOARD_AHB_MISC_BASE + REG_INT_EDGE
/* GPIO register */
#define REG_GPIO_OE_AHB		0x20
#define REG_GPIO_R_AHB		0x24
#define REG_GPIO_W_AHB		0x28

/* SPI regs */
#define LS1X_BOARD_SPI_BASE		 0x1f000000 
#define REG_SPCR			 0x00
#define REG_SPSR			 0x01
#define REG_SPDR			 0x02
#define REG_SPER			 0x03

/* UART regs */
#define LS1X_BOARD_GMAC1_BASE		 0x1fe10000
#define LS1X_BOARD_GMAC2_BASE         0x1fe20000
#define LS1X_BOARD_UART0_BASE		 0x1fe40000
#define LS1X_BOARD_UART1_BASE		 0x1fe44000
#define LS1X_BOARD_UART2_BASE		 0x1fe48000
#define LS1X_BOARD_UART3_BASE		 0x1fe4c000
#define LS1X_UART_SPLIT             0xbfe78038

/*watchdog*/
#define LS1X_BOARD_WAT_BASE			0x1f5c0060

/*RTC*/
#define LS1X_BOARD_RTC_BASE         0x1fe64020



/*I2C*/
#define LS1X_BOARD_I2C_BASE			0x1fe58000

/* APB BUS control regs */
#define LS1X_BOARD_APB_MISC_BASE	 0x1f004100
#define REG_GPIO_OE_APB 	0x00
#define REG_GPIO_R_APB		0x10
#define REG_GPIO_W_APB		0x20
#define REG_APB_MISC_CTL	0x40
#define APB_CLK			AHB_CLK
#define LS1X_GPIO_MUX_CTRL1 0xbfd00424
//AC97
#define LS1X_AC97_REGS_BASE 0x1fe74000
//PCI
#define LS1X_BOARD_PCI_REGS_BASE		 0x1f002000
struct ls1x_usbh_data {
    u8      ports;      /* number of ports on root hub */
    u8      vbus_pin[]; /* port power-control pin */
}; 

#define LS1X_USB_OHCI_BASE 0x1fe08000
#define LS1X_USB_EHCI_BASE 0x1fe00000
#define LS1X_LCD_BASE 0x1c301240

#define LS1X_PCICFG_BASE 0x1c100000
#define LS1X_PCIMAP (*(volatile int *)0xbfd01114)
#define LS1X_PCIMAP_CFG  (*(volatile int *)0xbfd01120)
#define LS1X_GPIO_CFG(x) (*(volatile int *)(0xbfd010c0+(x)*4))
#define LS1X_GPIO_OEN(x) (*(volatile int *)(0xbfd010d0+(x)*4))
#define LS1X_GPIO_IN(x) (*(volatile int *)(0xbfd010e0+(x)*4))
#define LS1X_GPIO_OUT(x) (*(volatile int *)(0xbfd010f0+(x)*4))


extern void __init prom_init_memory(void);
extern void __init prom_init_cmdline(void);
extern void __init prom_init_machtype(void);
extern void __init prom_init_env(void);
/* environment arguments from bootloader */
extern unsigned long bus_clock, cpu_clock_freq;
extern unsigned long memsize, highmemsize;
#endif

