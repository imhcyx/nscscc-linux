/*
 * Copyright (C) 2000 RidgeRun, Inc.
 * Author: RidgeRun, Inc.
 *   glonnon@ridgerun.com, skranz@ridgerun.com, stevej@ridgerun.com
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 * Copyright (C) 2000, 2001 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 2003 ICT CAS (guoyi@ict.ac.cn)
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
 *
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/irq.h>
#include <linux/ptrace.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/i8259.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <ls1x_board.h>
#include <ls1x_board_int.h>
#include <ls1x_board_dbg.h>

#define LS232_CP0_CAUSE_TI  (1UL    <<  30)
#define LS232_CP0_CAUSE_PCI (1UL    <<  26)
#define LS232_COUNTER1_OVERFLOW     (1ULL    << 31)
#define LS232_COUNTER2_OVERFLOW     (1ULL   << 31)

static struct ls1x_board_intc_regs volatile *ls1x_board_hw0_icregs
	= (struct ls1x_board_intc_regs volatile *)(KSEG1ADDR(LS1X_BOARD_INTREG_BASE));

void ls1x_board_hw_irqdispatch(int n);
void ls1x_board_irq_init();

asmlinkage void plat_irq_dispatch(struct pt_regs *regs)
{
	// unsigned int pending = read_c0_cause() & read_c0_status() & ST0_IM;
	unsigned int cause_reg = read_c0_cause() ;
	unsigned int status_reg = read_c0_status() ;
	unsigned volatile int cause = cause_reg & ST0_IM;
	unsigned volatile int status = status_reg & ST0_IM;
	unsigned int pending = cause & status;
	uint64_t volatile counter1, counter2;
   	 if (pending & CAUSEF_IP7) {
#if 0
        	counter1 = read_c0_perfcntr0();
	        counter2 = read_c0_perfcntr1();
	       if (((counter1 & LS232_COUNTER1_OVERFLOW)||(counter2 & LS232_COUNTER2_OVERFLOW)) && (cause_reg & LS232_CP0_CAUSE_PCI))
        	{
               		 do_IRQ(MIPS_CPU_IRQ_BASE+6);
        	}
	        if (cause_reg & LS232_CP0_CAUSE_TI)
        	{
	                 do_IRQ(MIPS_CPU_IRQ_BASE+7);
	
        	}
#else
	                 do_IRQ(MIPS_CPU_IRQ_BASE+7);
#endif
        }
    else if (pending & CAUSEF_IP2) {
	    do_IRQ(MIPS_CPU_IRQ_BASE+2);
	}
    else if (pending & CAUSEF_IP3) {
	    do_IRQ(MIPS_CPU_IRQ_BASE+3);
	}
    else if (pending & CAUSEF_IP4) {
	    do_IRQ(MIPS_CPU_IRQ_BASE+4);
	}
    else if (pending & CAUSEF_IP5) {
	    do_IRQ(MIPS_CPU_IRQ_BASE+5);
	}
    else if (pending & CAUSEF_IP6) {
	    do_IRQ(MIPS_CPU_IRQ_BASE+6);
    } else {
        spurious_interrupt();
    }

}

static struct irqaction cascade_irqaction = {
	.handler	= no_action,
//        .mask		= CPU_MASK_NONE,
	.name		= "cascade",
};

void __init arch_init_irq(void)
{
	int i;
    unsigned long flags;

	clear_c0_status(ST0_IM | ST0_BEV);
	local_irq_disable();


	mips_cpu_irq_init();	

}
