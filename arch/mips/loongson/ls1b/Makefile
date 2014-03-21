#
# Makefile for loongson based machines.
#

obj-y += setup.o init.o cmdline.o env.o time.o reset.o irq.o \
     ls1x-irq.o mem.o platform.o


#
# Serial port support
#
obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

#
# Enable CS5536 Virtual Support Module(VSM) to virtulize the PCI configure
# space
#

#
# Suspend Support
#

obj-$(CONFIG_LOONGSON_SUSPEND) += pm.o
