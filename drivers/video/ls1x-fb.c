/*
 *  linux/drivers/video/ls1x_fb.c -- Virtual frame buffer device
 *
 *      Copyright (C) 2002 James Simmons
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <ls1x_board.h>
#include <linux/dma-mapping.h>
/* LCD Register define */

#define DEFAULT_VIDEO_MODE "1024x768-16@60"
 
static char *mode_option;
    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */


static void *videomemory;
static	dma_addr_t dma_A;

static u_long videomemorysize = 0;
module_param(videomemorysize, ulong, 0);

static struct fb_var_screeninfo ls1x_fb_default __initdata = {
	.xres =		640,
	.yres =		480,
	.xres_virtual =	640,
	.yres_virtual =	480,
	.bits_per_pixel = 32,
	.red =		{ 11, 5 ,0},
     	.green =	{ 5, 6, 0 },
     	.blue =		{ 0, 5, 0 },
     	.activate =	FB_ACTIVATE_NOW,
      	.height =	-1,
      	.width =	-1,
      	.pixclock =	20000,
      	.left_margin =	64,
      	.right_margin =	64,
      	.upper_margin =	32,
      	.lower_margin =	32,
      	.hsync_len =	64,
      	.vsync_len =	2,
      	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo ls1x_fb_fix __initdata = {
	.id =		"Virtual FB",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1,
	.accel =	FB_ACCEL_NONE,
};

static int ls1x_fb_enable __initdata = 0;	/* disabled by default */
module_param(ls1x_fb_enable, bool, 0);

static int ls1x_fb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info);
static int ls1x_fb_set_par(struct fb_info *info);
static int ls1x_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info);
static int ls1x_fb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info);

static struct fb_ops ls1x_fb_ops = {
	.fb_check_var	= ls1x_fb_check_var,
	.fb_set_par	= ls1x_fb_set_par,
	.fb_setcolreg	= ls1x_fb_setcolreg,
	.fb_pan_display	= ls1x_fb_pan_display,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

    /*
     *  Internal routines
     */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

    /*
     *  Setting the video mode has been split into two parts.
     *  First part, xxxfb_check_var, must not write anything
     *  to hardware, it should only verify and adjust var.
     *  This means it doesn't alter par but it does use hardware
     *  data from it to check this var. 
     */

static int ls1x_fb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (videomemorysize &&  line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it 
	 * work. This way we let the user know what is acceptable.
	 */
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:		/* RGBA 5551 */
		if (var->transp.length) {
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 10;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
		} else {	/* BGR 565 */
			var->red.offset = 11;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
		break;
	case 24:		/* RGB 888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:		/* RGBA 8888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

#define LS1X_LCD_ADDR KSEG1ADDR(LS1X_LCD_BASE)

#define SB_FB_BUF_CONFIG_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+n*16)
#define SB_FB_BUF_ADDRESS_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32+n*16)
#define SB_FB_BUF_STRIDE_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*2+n*16)
#define SB_FB_BUF_ORIGIN_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*3+n*16)

#define SB_FB_OVLY_CONFIG_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*4+n*16)
#define SB_FB_OVLY_ADDRESS_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*5++n*16)
#define SB_FB_OVLY_STRIDE_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*6++n*16)
#define SB_FB_OVLY_TOPLEFT_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*7+n*16)
#define SB_FB_OVLY_BOTRIGHT_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*8+n*16)

#define SB_FB_DITHER_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*9+n*16)
#define SB_FB_DITHER_TABLE_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*10+n*16)

#define SB_FB_PANEL_CONFIG_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*11+n*16)
#define SB_FB_PANEL_TIMING_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*12+n*16)

#define SB_FB_FIFOCONTR_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*13+n*16)

#define SB_FB_HDISP_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*14+n*16)
#define SB_FB_HSYNC_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*15+n*16)
#define SB_FB_HCNT1_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*16+n*16)
#define SB_FB_HCNT2_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*17+n*16)

#define SB_FB_VDISP_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*18+n*16)
#define SB_FB_VSYNC_REG(n) *(volatile unsigned int *)(LS1X_LCD_ADDR+32*19+n*16)




/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int ls1x_fb_set_par(struct fb_info *info)
{
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
	return 0;
}

    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int ls1x_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
	if (regno >= 256)	/* no. of hw registers */
		return 1;
	/*
	 * Program hardware... do anything you want with transp
	 */

	/* grayscale works only partially under directcolor */
	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue =
		    (red * 77 + green * 151 + blue * 28) >> 8;
	}

	/* Directcolor:
	 *   var->{color}.offset contains start of bitfield
	 *   var->{color}.length contains length of bitfield
	 *   {hardwarespecific} contains width of RAMDAC
	 *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) | (X << blue.offset)
	 *   RAMDAC[X] is programmed to (red, green, blue)
	 * 
	 * Pseudocolor:
	 *    uses offset = 0 && length = RAMDAC register width.
	 *    var->{color}.offset is 0
	 *    var->{color}.length contains widht of DAC
	 *    cmap is not used
	 *    RAMDAC[X] is programmed to (red, green, blue)
	 * Truecolor:
	 *    does not use DAC. Usually 3 are present.
	 *    var->{color}.offset contains start of bitfield
	 *    var->{color}.length contains length of bitfield
	 *    cmap is programmed to (red << red.offset) | (green << green.offset) |
	 *                      (blue << blue.offset) | (transp << transp.offset)
	 *    RAMDAC does not exist
	 */
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		red = CNVT_TOHW(red, info->var.red.length);
		green = CNVT_TOHW(green, info->var.green.length);
		blue = CNVT_TOHW(blue, info->var.blue.length);
		transp = CNVT_TOHW(transp, info->var.transp.length);
		break;
	case FB_VISUAL_DIRECTCOLOR:
		red = CNVT_TOHW(red, 8);	/* expect 8 bit DAC */
		green = CNVT_TOHW(green, 8);
		blue = CNVT_TOHW(blue, 8);
		/* hey, there is bug in transp handling... */
		transp = CNVT_TOHW(transp, 8);
		break;
	}
#undef CNVT_TOHW
	/* Truecolor has hardware independent palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) |
		    (blue << info->var.blue.offset) |
		    (transp << info->var.transp.offset);
		switch (info->var.bits_per_pixel) {
		case 8:
			break;
		case 16:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		case 24:
		case 32:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		}
		return 0;
	}
	return 0;
}

    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */

static int ls1x_fb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
		    var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

static int power = -1;

#ifndef MODULE
static int __init ls1x_fb_setup(char *options)
{
	char *this_opt;

	ls1x_fb_enable = 1;

	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		if (!strncmp(this_opt, "disable", 7))
			ls1x_fb_enable = 0;
		else if (!strncmp(this_opt, "power=", 6))
		{
			power = simple_strtoul(this_opt+6,0,0);
		}
		else
			mode_option = this_opt;
	}
	return 1;
}
#endif  /*  MODULE  */

int detect_resolution(int i,int *width, int *height,int *depth)
{
	int cfg;

	*width = SB_FB_HDISP_REG(i) & 0xffff;
	*height = SB_FB_VDISP_REG(i) & 0xffff;
	cfg	=   SB_FB_BUF_CONFIG_REG(i);

	switch(cfg&7)
	{
		case 4:
			*depth = 24;
			break;
		case 3:
			*depth = 16;
			break;
		case 2:
			*depth = 15;
			break;
		default:
			*depth = 16;
			break;
	}

	return 0;
}


    /*
     *  Initialisation
     */

static int __init ls1x_fb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	int retval = -ENOMEM;
	struct page *map = NULL;
	int i;
	int ret;
	struct fb_var_screeninfo var;

	var = ls1x_fb_default;



	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		return retval;

	info->fix = ls1x_fb_fix;
	info->node = -1;
	info->fbops = &ls1x_fb_ops;
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;
	var = info->var = ls1x_fb_default;

	if(power==-1)
	{
		power = 0;
		power |= (SB_FB_BUF_CONFIG_REG(0) & 0x100)?1:0;
		power |= (SB_FB_BUF_CONFIG_REG(1) & 0x100)?2:0;
	}
	else
	{

		if(power&1)
			SB_FB_BUF_CONFIG_REG(0) |= 0x100; 
		else
			SB_FB_BUF_CONFIG_REG(0) &= ~0x100; 

		if(power&2)
			SB_FB_BUF_CONFIG_REG(1) |= 0x100; 
		else
			SB_FB_BUF_CONFIG_REG(1) &= ~0x100; 
	}

	if(mode_option)
	{
		retval = fb_find_mode(&info->var, info, mode_option,
				NULL, 0, NULL, 32);
	}
	else if(power)
	{
	 int xres, yres, depth;
	 if(power&1)
	  detect_resolution(0, &xres, &yres, &depth);

	 if(power&2)
	  detect_resolution(1, &xres, &yres, &depth);

	 info->var.xres_virtual = info->var.xres = xres;
	 info->var.yres_virtual = info->var.yres = yres;
	 info->var.bits_per_pixel = depth;
	}



    if (!videomemorysize) {
    	videomemorysize = info->var.xres_virtual *
			  info->var.yres_virtual *
			  info->var.bits_per_pixel / 8;
    }

	/*
	 * For real video cards we use ioremap.
	 */
	videomemory = dma_alloc_coherent(&dev->dev,videomemorysize+PAGE_SIZE, &dma_A,GFP_ATOMIC); //use only bank A
	if (!videomemory)
		goto err;
	printk("videomemorysize=%x\n",videomemorysize);

	memset(videomemory, 0, videomemorysize);

	info->screen_base = (char __iomem *)videomemory;
	info->fix.smem_start = dma_A;
	info->fix.smem_len = videomemorysize;

	retval = fb_alloc_cmap(&info->cmap, 32, 0);
	if (retval < 0)
		goto err1;

	printk("videomemory:%x::%x\n",videomemory,dma_A);
	SB_FB_BUF_ADDRESS_REG(0)=dma_A;
	SB_FB_BUF_ADDRESS_REG(1)=dma_A;
	/*set double flip buffer address*/
	*(volatile int *)0xbc301580 = dma_A;
	*(volatile int *)0xbc301590 = dma_A;
	/*disable fb0,board only use fb1 now*/

	
	ls1x_fb_check_var(&var,info);
	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

	printk(KERN_INFO
	       "fb%d: Virtual frame buffer device, using %ldK of video memory\n",
	       info->node, videomemorysize >> 10);
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	dma_free_coherent(&dev->dev,videomemorysize+PAGE_SIZE,videomemory,dma_A);
err:
	framebuffer_release(info);
	return retval;
}

static int ls1x_fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		unregister_framebuffer(info);
		dma_free_coherent(&dev->dev,videomemorysize+PAGE_SIZE,videomemory,dma_A);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver ls1x_fb_driver = {
	.probe	= ls1x_fb_probe,
	.remove = ls1x_fb_remove,
	.driver = {
		.name	= "ls1x-fb",
	},
};

static struct platform_device *ls1x_fb_device;

static int __init ls1x_fb_init(void)
{
	int ret = 0;

	printk("\nls1x_fb init\n");
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("ls1x_fb", &option))
		return -ENODEV;
	ls1x_fb_setup(option);
#endif

	if (!ls1x_fb_enable)
		return -ENXIO;

	ret = platform_driver_register(&ls1x_fb_driver);

	return ret;
}

module_init(ls1x_fb_init);

#ifdef MODULE
static void __exit ls1x_fb_exit(void)
{
	platform_driver_unregister(&ls1x_fb_driver);
}

module_exit(ls1x_fb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */


static int param_set_var(const char *val, struct kernel_param *kp)
{
	int x;
	x = simple_strtoul(val,0,0);

	if(x&1)
	 SB_FB_BUF_CONFIG_REG(0) |= 0x100; 
	else
	 SB_FB_BUF_CONFIG_REG(0) &= ~0x100; 

	if(x&2)
	 SB_FB_BUF_CONFIG_REG(1) |= 0x100; 
	else
	 SB_FB_BUF_CONFIG_REG(1) &= ~0x100; 

	return 0;
}

static int param_get_var(char *buffer, struct kernel_param *kp)
{
	int x = 0;
	 x |= (SB_FB_BUF_CONFIG_REG(0) & 0x100)?1:0;
	 x |= (SB_FB_BUF_CONFIG_REG(1) & 0x100)?2:0;

	return sprintf(buffer, "%d", x);
}

module_param_call(power, param_set_var, param_get_var, (void *)0, 0644);
