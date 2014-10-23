/*
 *
 * BRIEF MODULE DESCRIPTION
 *	LS1X BOARD interrupt/setup routines.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * Part of this file was derived from Carsten Langgaard's 
 * arch/mips/ite-boards/generic/init.c.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/serial_reg.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <ls1x_board.h>
#include <ls1x_board_int.h>
#include <ls1x_board_dbg.h>

#undef DEBUG_IRQ
#ifdef DEBUG_IRQ
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#ifdef CONFIG_REMOTE_DEBUG
extern void breakpoint(void);
#endif

/* revisit */
#define EXT_IRQ0_TO_IP 2 /* IP 2 */
#define EXT_IRQ5_TO_IP 7 /* IP 7 */

#define ALLINTS_NOTIMER (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4)

void disable_ls1x_board_irq(unsigned int irq_nr);
void enable_ls1x_board_irq(unsigned int irq_nr);

extern void set_debug_traps(void);
// extern void mips_timer_interrupt(int irq, struct pt_regs *regs);
extern void mips_timer_interrupt(int irq);
extern asmlinkage void ls1x_board_IRQ(void);

static struct ls1x_board_intc_regs volatile *ls1x_board_hw0_icregs
	= (struct ls1x_board_intc_regs volatile *)(KSEG1ADDR(LS1X_BOARD_INTREG_BASE));


void ack_ls1x_board_irq(unsigned int irq_nr)
{
	DPRINTK("ack_ls1x_board_irq %d\n", irq_nr);
    	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_clr |= (1 << (irq_nr&0x1f));
}
void disable_ls1x_board_irq(unsigned int irq_nr)
{
	DPRINTK("disable_ls1x_board_irq %d\n", irq_nr);
    	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_en &= ~(1 << (irq_nr&0x1f));
}

void enable_ls1x_board_irq(unsigned int irq_nr)
{

	DPRINTK("enable_ls1x_board_irq %d\n", irq_nr);
    	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_en |= (1 << (irq_nr&0x1f));
}

static unsigned int startup_ls1x_board_irq(unsigned int irq)
{
	enable_ls1x_board_irq(irq);
	return 0; 
}

static int ls1x_board_irq_set_type(unsigned int irq, unsigned int flow_type)
{
	unsigned int irq_nr = irq;
	int mode;

	if (flow_type & IRQF_TRIGGER_PROBE)
		return 0;
	switch (flow_type & IRQF_TRIGGER_MASK) {
	case IRQF_TRIGGER_RISING:	mode = 3;	break;
	case IRQF_TRIGGER_FALLING:	mode = 2;	break;
	case IRQF_TRIGGER_HIGH:	mode = 1;	break;
	case IRQF_TRIGGER_LOW:	mode = 0;	break;
	default:
		return -EINVAL;
	}


	if(mode & 1)
	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_pol |= (1 << (irq_nr&0x1f));
	else
	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_pol &= ~(1 << (irq_nr&0x1f));

	if(mode & 2)
	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_edge |= (1 << (irq_nr&0x1f));
	else
	(ls1x_board_hw0_icregs+(irq_nr>>5))->int_edge &= ~(1 << (irq_nr&0x1f));

	return 0;
}
#define shutdown_ls1x_board_irq	disable_ls1x_board_irq
#define mask_and_ack_ls1x_board_irq    disable_ls1x_board_irq

static void end_ls1x_board_irq(unsigned int irq)
{
	if (!(irq_to_desc(irq)->status & (IRQ_DISABLED|IRQ_INPROGRESS))){
		(ls1x_board_hw0_icregs+(irq>>5))->int_clr |= 1 << (irq&0x1f); 
		//if(irq<LS1X_BOARD_GPIO_FIRST_IRQ) 
		enable_ls1x_board_irq(irq);
	}
}


static struct irq_chip ls1x_board_irq_chip = {
	.name = "LS1X BOARD",
	.ack = ack_ls1x_board_irq,
	.mask = disable_ls1x_board_irq,
	.unmask = enable_ls1x_board_irq,
	.eoi = enable_ls1x_board_irq,
	.end = end_ls1x_board_irq,
	.set_type = ls1x_board_irq_set_type
};

void prom_printf(char *fmt, ...);
// void ls1x_board_hw0_irqdispatch(struct pt_regs *regs)
void ls1x_board_hw_irqdispatch(int n)
{
	int irq;
	int intstatus = 0;
   	int status;

	/* Receive interrupt signal, compute the irq */
	status = read_c0_cause();
	intstatus = (ls1x_board_hw0_icregs+n)->int_isr;
	
	irq=ffs(intstatus);
//	prom_printf("irq=%d,n=%d,realirq=%d\n",irq,n,n*32+irq-1);
	
	if(!irq){
		printk("Unknow interrupt status %x intstatus %x \n" , status, intstatus);
		return; 
	}
	else do_IRQ(n*32+irq-1);
}

void ls1x_board_irq_init(u32 irq_base)
{
//	extern irq_desc_t irq_desc[];
	u32 i;
	for (i= 0; i<= LS1X_BOARD_LAST_IRQ; i++) {
		set_irq_chip_and_handler(i, &ls1x_board_irq_chip, handle_level_irq);
	}

}
