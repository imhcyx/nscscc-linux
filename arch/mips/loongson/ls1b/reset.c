/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 * Copyright (C) 2009 Lemote, Inc. & Institute of Computing Technology
 * Author: Zhangjin Wu, wuzj@lemote.com
 */
#include <linux/init.h>
#include <linux/pm.h>
#include <asm/io.h>

#include <asm/reboot.h>

#include <ls1x_board.h>

static void loongson_restart(char *command)
{

        writel(1,0xbfe5c060);
        writel(1,0xbfe5c064);
        writel(1,0xbfe5c068);

	/* do preparation for reboot */

	/* reboot via jumping to boot base address */
	((void (*)(void))ioremap_nocache(0x1fc00000, 4)) ();
}

static void loongson_halt(void)
{
	while (1)
		;
}

static int __init mips_reboot_setup(void)
{
	_machine_restart = loongson_restart;
	_machine_halt = loongson_halt;
	pm_power_off = loongson_halt;

	return 0;
}

arch_initcall(mips_reboot_setup);
