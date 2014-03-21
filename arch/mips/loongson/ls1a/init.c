/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/bootmem.h>

#include <ls1x_board.h>

void __init prom_init(void)
{
	/* init base address of io space */
	set_io_port_base((unsigned long)
		ioremap(0x1c000000, 0x10000));


	prom_init_cmdline();
	prom_init_env();
	prom_init_memory();

}

void __init prom_free_prom_memory(void)
{
}
