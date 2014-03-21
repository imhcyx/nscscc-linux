/*
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/pci.h>

#include <ls1x_board.h>

#ifdef CONFIG_DISABLE_PCI
static int disablepci=1;
#else
static int disablepci=0;
#endif
extern struct pci_ops ls1x_pci_pci_ops;


static struct resource loongson_pci_mem_resource = {
	.name   = "pci memory space",
	.start  = 0x14000000UL,
	.end    = 0x17ffffffUL,
	.flags  = IORESOURCE_MEM,
};

static struct resource loongson_pci_io_resource = {
	.name   = "pci io space",
	.start  = 0x00004000UL,
	.end    = IO_SPACE_LIMIT,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller  loongson_pci_controller = {
	.pci_ops        = &ls1x_pci_pci_ops,
	.io_resource    = &loongson_pci_io_resource,
	.mem_resource   = &loongson_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

static void __init setup_pcimap(void)
{
	/*
	 * local to PCI mapping for CPU accessing PCI space
	 * CPU address space [256M,448M] is window for accessing pci space
	 * we set pcimap_lo[0,1,2] to map it to pci space[0M,64M], [320M,448M]
	 *
	 * pcimap: PCI_MAP2  PCI_Mem_Lo2 PCI_Mem_Lo1 PCI_Mem_Lo0
	 * 	     [<2G]   [384M,448M] [320M,384M] [0M,64M]
	 */
	LS1X_PCIMAP = 0x46140;
}

static int __init pcibios_init(void)
{
	setup_pcimap();

	loongson_pci_controller.io_map_base = mips_io_port_base;

	if(!disablepci)
	register_pci_controller(&loongson_pci_controller);

	return 0;
}

arch_initcall(pcibios_init);

static int __init disablepci_setup(char *options)
{
    if (!options || !*options)
        return 0;
    if(options[0]=='0')disablepci=0;
    else disablepci=simple_strtoul(options,0,0);
    return 1;
}

__setup("disablepci=", disablepci_setup);

