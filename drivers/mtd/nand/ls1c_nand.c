#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <asm/dma.h>
#include <linux/slab.h>


#define DMA_ACCESS_ADDR     0x1fe78040
#define ORDER_REG_ADDR      (KSEG1ADDR(0x1fd01160))
#define LS1X_NAND_REG_BASE      (0xbfe78000)
#define MAX_BUFF_SIZE	4096
#define PAGE_SHIFT      12
#define LS1CSOC	1
#ifdef	CONFIG_LS1A_MACH 
	#define NO_SPARE_ADDRH(x)   ((x))   
	#define NO_SPARE_ADDRL(x)   ((x) << (PAGE_SHIFT))
	#define SPARE_ADDRH(x)      ((x))   
	#define SPARE_ADDRL(x)      ((x) << (PAGE_SHIFT))
#elif (defined LS1CSOC)
	#define NO_SPARE_ADDRH(x)   (x)
	#define NO_SPARE_ADDRL(x)   0
	#define SPARE_ADDRH(x)      (x)
	#define SPARE_ADDRL(x)      0
#else
	#define NO_SPARE_ADDRH(x)   ((x) >> (32 - (PAGE_SHIFT - 1 )))   
	#define NO_SPARE_ADDRL(x)   ((x) << (PAGE_SHIFT - 1))
	#define SPARE_ADDRH(x)      ((x) >> (32 - (PAGE_SHIFT )))   
	#define SPARE_ADDRL(x)      ((x) << (PAGE_SHIFT ))
#endif
#define ALIGN_DMA(x)       (((x)+ 3)/4)

#define	GPIO_CONF1	(ioremap(0x1fd010c4, 4))
#define	GPIO_CONF2	(ioremap(0x1fd010c8, 4))
#define	GPIO_MUX	(ioremap(0x1fd00420, 4))

/*
//#define USE_POLL
#undef	USE_POLL
#ifdef USE_POLL
#define complete(...)
#define init_completion(...)
#define wait_for_completion_timeout(...)
#define wait_for_completion(...)
#define request_irq(...) (0)
#define free_irq(...) 
#endif
*/


#define CHIP_DELAY_TIMEOUT (2*HZ/10)

#define STATUS_TIME_LOOP_R  100 
#define STATUS_TIME_LOOP_WS  300  
#define STATUS_TIME_LOOP_WM  60  
#define STATUS_TIME_LOOP_E  100  

#define NAND_CMD        0x1
#define NAND_ADDRL      0x2
#define NAND_ADDRH      0x4
#define NAND_TIMING     0x8
#define NAND_IDL        0x10
#define NAND_STATUS_IDL 0x20
#define NAND_PARAM      0x40
#define NAND_OP_NUM     0X80
#define NAND_CS_RDY_MAP 0x100

#define DMA_ORDERAD     0x1
#define DMA_SADDR       0x2
#define DMA_DADDR       0x4
#define DMA_LENGTH      0x8
#define DMA_STEP_LENGTH 0x10
#define DMA_STEP_TIMES  0x20
#define DMA_CMD         0x40

#define NAND_ECC_OFF    0
#define NAND_ECC_ON     1
#define RAM_OP_OFF      0
#define RAM_OP_ON       1


#if 0  /* read ids use gpio*/

#define NAND_GPIO_MUX  0x0a000000
#define GPIO_MUX_CTRL  0xbfd00420

#define GPIO_CONF3      0xbfd010c8
#define GPIO_IEN3        0xbfd010d8
#define GPIO_IDATA3      0xbfd010e8
#define GPIO_ODATA3      0xbfd010f8

#define NAND_D0         6  //70 - 64
#define NAND_D1         7  //71 - 64
#define NAND_D2         8  //72 - 64
#define NAND_D3         9  //73 - 64
#define NAND_D4         10 //74 - 64
#define NAND_D5         11 //75 - 64
#define NAND_CLE        12 //76 - 64
#define NAND_ALE        13 //77 - 64
#define NAND_RE         14 //78 - 64
#define NAND_WE         15 //79 - 64
#define NAND_CE         16 //80 - 64
#define NAND_RDY        17 //81 - 64
#define NAND_D6         18 //82 - 64 
#define NAND_D7         19 //83 - 64

#define _EN_PIN(x)      (*((volatile unsigned int*)(GPIO_CONF3)) = *((volatile unsigned int *)GPIO_CONF3) | (0x1 << (x)))          
#define _DIS_PIN(x)      (*((volatile unsigned int*)(GPIO_CONF3)) = *((volatile unsigned int *)GPIO_CONF3) & (~(0x1 << (x))))          
#define _EN_IN(x)      (*((volatile unsigned int*)(GPIO_IEN3)) = *((volatile unsigned int *)GPIO_IEN3) | (0x1 << (x)))          
#define _DIS_IN(x)      (*((volatile unsigned int*)(GPIO_IEN3)) = *((volatile unsigned int *)GPIO_IEN3) & (~(0x1 << (x))))          
#define _SET_ONE(x)      (*((volatile unsigned int*)(GPIO_ODATA3)) = *((volatile unsigned int *)GPIO_ODATA3) | (0x1 << (x)))          
#define _SET_ZERO(x)      (*((volatile unsigned int*)(GPIO_ODATA3)) = *((volatile unsigned int *)GPIO_ODATA3) & (~(0x1 << (x))))          
#define  _READ_GPIO3     (*(volatile unsigned int *)(GPIO_ODATA3))

#define _EN_OUT(x)      _DIS_IN(x)

#define _DIS_CTRL_IO            do{     \
            _DIS_PIN(NAND_CLE);         \
            _DIS_PIN(NAND_ALE);         \
            _DIS_PIN(NAND_RE);          \
            _DIS_PIN(NAND_WE);          \
            _DIS_PIN(NAND_CE);          \
            _DIS_PIN(NAND_RDY);         \
            }while(0)

#define _DIS_DATA_IO            do{     \
            _DIS_PIN(NAND_D0);           \
            _DIS_PIN(NAND_D1);           \
            _DIS_PIN(NAND_D2);           \
            _DIS_PIN(NAND_D3);           \
            _DIS_PIN(NAND_D4);           \
            _DIS_PIN(NAND_D5);           \
            _DIS_PIN(NAND_D6);           \
            _DIS_PIN(NAND_D7);           \
        }while(0)
#define DIS_GPIO_READ_ID    do{         \
            _DIS_CTRL_IO;               \
            _DIS_DATA_IO;               \
        }while(0)

#define _EN_DATA_IO             do{     \
            _EN_PIN(NAND_D0) ;           \
            _EN_PIN(NAND_D1) ;           \
            _EN_PIN(NAND_D2) ;           \
            _EN_PIN(NAND_D3) ;           \
            _EN_PIN(NAND_D4) ;           \
            _EN_PIN(NAND_D5) ;           \
            _EN_PIN(NAND_D6) ;           \
            _EN_PIN(NAND_D7) ;           \
        }while(0)

#define _EN_IN_DATA_IO          do{     \
            _EN_IN(NAND_D0) ;            \
            _EN_IN(NAND_D1) ;            \
            _EN_IN(NAND_D2) ;            \
            _EN_IN(NAND_D3) ;            \
            _EN_IN(NAND_D4) ;            \
            _EN_IN(NAND_D5) ;            \
            _EN_IN(NAND_D6) ;            \
            _EN_IN(NAND_D7) ;            \
        }while(0)
           
#define  _EN_OUT_DATA_IO            do{   \
            _EN_OUT(NAND_D0);            \
            _EN_OUT(NAND_D1);            \
            _EN_OUT(NAND_D2);            \
            _EN_OUT(NAND_D3);            \
            _EN_OUT(NAND_D4);            \
            _EN_OUT(NAND_D5);            \
            _EN_OUT(NAND_D6);            \
            _EN_OUT(NAND_D7);           \
        }while(0)    

#define DIS_DATA_IO    _DIS_DATA_IO

#define _DIS_GPIO3    *((volatile unsigned int *)GPIO_CONF3) = 0


#define _PUTODATA32(x)      *((volatile unsigned int *)GPIO_ODATA3) = (x)
#define _GETODATA32(x)      (x) = *((volatile unsigned int *)GPIO_ODATA3)  

#define _PUTIDATA32(x)      *((volatile unsigned int *)GPIO_IDATA3) = (x)
#define _GETIDATA32(x)      (x) = *((volatile unsigned int *)GPIO_IDATA3)  


#define SET_UP(x)    do{                \
                _EN_PIN((x));           \
                _EN_OUT((x));           \
                _SET_ONE((x));          \
                _READ_GPIO3;            \
            }while(0)

#define SET_DOWN(x)    do{                \
                _EN_PIN((x));           \
                _EN_OUT((x));           \
                _SET_ZERO((x));          \
                _READ_GPIO3;            \
            }while(0)
/*EN_SEND_DATA_IO*/
#define  EN_SEND_DATA_IO        do{      \
            _EN_DATA_IO     ;            \
            _EN_OUT_DATA_IO ;           \
            _READ_GPIO3;                \
        }while(0)
/*EN_GET_DATA_IO*/
#define  EN_GET_DATA_IO         do{     \
            _EN_DATA_IO;                 \
            _EN_IN_DATA_IO;              \
            _READ_GPIO3;               \
        }while(0)
 
static void nand_gpio_init(void)
{
//    *((unsigned int *)GPIO_MUX_CTRL) = NAND_GPIO_MUX;
//    *((unsigned int *)GPIO_MUX_CTRL) = NAND_GPIO_MUX;
#ifdef	CONFIG_LS1A_MACH
{
	int val;
#ifdef CONFIG_NAND_USE_LPC_PWM01 //NAND复用LPC PWM01
	val = __raw_readl(GPIO_MUX);
	val |= 0x2a000000;
	__raw_writel(val, GPIO_MUX);

	val = __raw_readl(GPIO_CONF2);
	val &= ~(0xffff<<6);			//nand_D0~D7 & nand_control pin
	__raw_writel(val, GPIO_CONF2);
#elif CONFIG_NAND_USE_SPI1_PWM23 //NAND复用SPI1 PWM23
	val = __raw_readl(GPIO_MUX);
	val |= 0x14000000;
	__raw_writel(val, GPIO_MUX);

	val = __raw_readl(GPIO_CONF1);
	val &= ~(0xf<<12);				//nand_D0~D3
	__raw_writel(val, GPIO_CONF1);

	val = __raw_readl(GPIO_CONF2);
	val &= ~(0xfff<<12);			//nand_D4~D7 & nand_control pin
	__raw_writel(val, GPIO_CONF2);
#endif
}
#endif
}

static void gpio_senddata8(unsigned char val)
{
    unsigned int d32=0;
//    EN_SEND_DATA_IO;
    _GETODATA32(d32);
    d32 = (d32 & (~(0x3f << 6)) | ((val & 0x3f)<<6));
    d32 = (d32 & (~(0x3 << 18)) | (((val >> 6) & 0x3)<<18));
    _PUTODATA32(d32);
    _READ_GPIO3;
    SET_DOWN(NAND_WE);
    SET_UP(NAND_WE);
    //delay;

//   DIS_DATA_IO;
}

static unsigned char gpio_getdata8(void)
{
    unsigned int d32=0;
    unsigned char val=0;
    //EN_GET_DATA_IO;
    //delay
    SET_DOWN(NAND_RE);
    _GETIDATA32(d32);
    SET_UP(NAND_RE);
 //   DIS_DATA_IO;
    val = (d32 >> 18) & 0x3;
    val = (val << 6) |(d32 >> 6) & 0x3f;
    return val;

}
static unsigned int nand_gpio_read_id(void)
{
    unsigned char val[4]={0}; //4 bytes
    nand_gpio_init();
    EN_SEND_DATA_IO;

    SET_DOWN(NAND_CE);

    /*send command*/
    SET_DOWN(NAND_ALE);
    SET_UP(NAND_RE);
    SET_UP(NAND_CLE);
    gpio_senddata8(NAND_CMD_READID);
    //delay
    SET_DOWN(NAND_CLE);

    /*send addr*/
    SET_UP(NAND_ALE);
    gpio_senddata8(0x00);
    SET_DOWN(NAND_ALE);
    //delay min 60ns
    /*read id*/
    EN_GET_DATA_IO;
    val[0] = gpio_getdata8();
//    printk("1th:%x\n",val[0]);
    val[1] = gpio_getdata8();
//    printk("2th:%x\n",val[1]);
    val[2] = gpio_getdata8();
//    printk("3th:%x\n",val[2]);
    val[3] = gpio_getdata8();
//    printk("4th:%x\n",val[3]);
    DIS_GPIO_READ_ID;
return *((unsigned int *)val);
}

#endif /* end:read ids use gpio*/

#define  _NAND_IDL      ( *((volatile unsigned int*)(0xbfe78010)))
#define  _NAND_IDH       (*((volatile unsigned int*)(0xbfe78014)))
#define  _NAND_BASE      0xbfe78000
#define  _NAND_SET_REG(x,y)   do{*((volatile unsigned int*)(_NAND_BASE+x)) = (y);}while(0)                           
#define  _NAND_READ_REG(x,y)  do{(y) =  *((volatile unsigned int*)(_NAND_BASE+x));}while(0) 

#define _NAND_TIMING_TO_READ    _NAND_SET_REG(0xc,0x205)
#define _NAND_TIMING_TO_WRITE   _NAND_SET_REG(0xc,0x205)

enum{
	ERR_NONE = 0,
	ERR_DMABUSERR = -1,
	ERR_SENDCMD = -2,
	ERR_DBERR = -3,
	ERR_BBERR = -4,
};
enum{
	STATE_READY = 0, 
	STATE_BUSY,
};

struct ls1a_nand_platform_data{
	int enable_arbiter;
	struct mtd_partition *parts;
	unsigned int nr_parts;
};
struct ls1a_nand_cmdset{
	uint32_t cmd_valid :1;
	uint32_t read :1;
	uint32_t write :1;
	uint32_t erase_one :1;
	uint32_t erase_con :1;
	uint32_t read_id :1;
	uint32_t reset :1;
	uint32_t read_sr :1;
	uint32_t op_main :1;
	uint32_t op_spare :1;
	uint32_t done :1;
#if defined LS1CSOC
	uint32_t ecc_rd :1; //11
	uint32_t ecc_wr :1; //12
	uint32_t int_en :1; //13
	uint32_t resv14 :1; //14
	uint32_t ram_op :1; //15
#else
	uint32_t resv1:5; //11-15 reserved
#endif
	uint32_t nand_rdy :4;//16-19
	uint32_t nand_ce :4;//20-23
#if defined LS1CSOC
	uint32_t ecc_dma_req :1; //24
	uint32_t nul_dma_req :1; //25
	uint32_t resv26 :6;//26-31 reserved
#else
	uint32_t resv24:8;//24-31 reserved
#endif        

};

struct ls1a_nand_dma_desc{
	uint32_t orderad;
	uint32_t saddr;
	uint32_t daddr;
	uint32_t length;
	uint32_t step_length;
	uint32_t step_times;
	uint32_t cmd;
};

struct ls1a_nand_dma_cmd{
	uint32_t dma_int_mask :1;
	uint32_t dma_int :1;
	uint32_t dma_sl_tran_over :1;
	uint32_t dma_tran_over :1;
	uint32_t dma_r_state :4;
	uint32_t dma_w_state :4;
	uint32_t dma_r_w :1;
	uint32_t dma_cmd :2;
	uint32_t revl :17;
};

struct ls1a_nand_desc{
	uint32_t cmd;
	uint32_t addrl;
	uint32_t addrh;
	uint32_t timing;
	uint32_t idl;//readonly
	uint32_t status_idh;//readonly
	uint32_t param;
	uint32_t op_num;
	uint32_t cs_rdy_map;
};

struct ls1a_nand_info{
	struct nand_chip nand_chip;
	struct platform_device *pdev;
	/* MTD data control*/
	unsigned int buf_start;
	unsigned int buf_count;
	/* NAND registers*/
	void __iomem *mmio_base;
	struct ls1a_nand_desc nand_regs;
	unsigned int nand_addrl;
	unsigned int nand_addrh;
	unsigned int nand_timing;
	unsigned int nand_op_num;
	unsigned int nand_cs_rdy_map;
	unsigned int nand_cmd;
	/* DMA information */
	struct ls1a_nand_dma_desc dma_regs;
	unsigned int order_reg_addr;
	unsigned int dma_orderad;
	unsigned int dma_saddr;
	unsigned int dma_daddr;
	unsigned int dma_length;
	unsigned int dma_step_length;
	unsigned int dma_step_times;
	unsigned int dma_cmd;
	int drcmr_dat;//dma descriptor address;
	dma_addr_t drcmr_dat_phys;
	size_t drcmr_dat_size;
	unsigned char *data_buff;//dma data buffer;
	dma_addr_t data_buff_phys;
	size_t data_buff_size;
	unsigned long cac_size;
	unsigned long num;
	unsigned long size;
	struct timer_list test_timer;
	unsigned int dma_ask;
	unsigned int dma_ask_phy;

	/* relate to the command */
	unsigned int state;
	//	int			use_ecc;	/* use HW ECC ? */
	size_t data_size; /* data size in FIFO */
	unsigned int cmd;
	unsigned int page_addr;
	struct completion cmd_complete;
	unsigned int seqin_column;
	unsigned int seqin_page_addr;
	unsigned int timing_flag;
	unsigned int timing_val;
};


/*static struct nand_ecclayout hw_largepage_ecclayout = {
	.eccbytes = 24,
	.eccpos = {
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 38} }
};*/
#define show_data_debug  0
#define show_debug(x,y)     show_debug_msk(x,y)
#define show_debug_msk(x,y)   do{ if(show_data_debug) {printk(KERN_ERR "%s:\n",__func__);show_data(x,y);} }while(0)

//static void show_data(void * base,int num)
//{
//    int i=0;
//    unsigned char *arry=( unsigned char *) base;
//    for(i=0;i<num;i++){
//        if(!(i % 32)){
//            printk(KERN_ERR "\n");
//        }
//        if(!(i % 16)){
//            printk("  ");
//        }
//        printk("%02x ",arry[i]);
//    }
//    printk(KERN_ERR "\n");
//
//}


static int ls1a_nand_init_buff(struct ls1a_nand_info *info)
{
	struct platform_device *pdev = info->pdev;
	info->data_buff = dma_alloc_coherent(&pdev->dev, MAX_BUFF_SIZE,
			&info->data_buff_phys, GFP_KERNEL);
	info->data_buff = CAC_ADDR(info->data_buff);
	if (info->data_buff == NULL)	{
		dev_err(&pdev->dev, "failed to allocate dma buffer\n");
		return -ENOMEM;
	}
	info->data_buff_size = MAX_BUFF_SIZE;
	return 0;
}
//static int ls1a_nand_ecc_calculate(struct mtd_info *mtd, const uint8_t *dat,
//		uint8_t *ecc_code)
//{
//	return 0;
//}
//static int ls1a_nand_ecc_correct(struct mtd_info *mtd, uint8_t *dat,
//		uint8_t *read_ecc, uint8_t *calc_ecc)
//{
//	/*
//	 * Any error include ERR_SEND_CMD, ERR_DBERR, ERR_BUSERR, we
//	 * consider it as a ecc error which will tell the caller the
//	 * read fail We have distinguish all the errors, but the
//	 * nand_read_ecc only check this function return value
//	 */
//	return 0;
//}

//static void ls1a_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
//{
//	return;
//}


static int ls1a_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	udelay(50);
	return 0;
}

static void ls1a_nand_select_chip(struct mtd_info *mtd, int chip)
{
	return;
}

static int ls1a_nand_dev_ready(struct mtd_info *mtd)
{
	return 1;
}

static void ls1a_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct ls1a_nand_info *info = mtd->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);
	memcpy(buf, info->data_buff + info->buf_start, real_len);
	info->buf_start += real_len;
}
static u16 ls1a_nand_read_word(struct mtd_info *mtd)
{
	struct ls1a_nand_info *info = mtd->priv;
	u16 retval = 0xFFFF;
	if (!(info->buf_start & 0x1) && info->buf_start < info->buf_count)	{
		retval = *(u16 *) (info->data_buff + info->buf_start);
	}
	info->buf_start += 2;
	return retval;
}
static uint8_t ls1a_nand_read_byte(struct mtd_info *mtd)
{
	struct ls1a_nand_info *info = mtd->priv;
	char retval = 0xFF;
	if (info->buf_start < info->buf_count){
		retval = info->data_buff[(info->buf_start)++];
	}
	return retval;
}

static void ls1a_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf,
		int len)
{
	struct ls1a_nand_info *info = mtd->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);
	memcpy(info->data_buff + info->buf_start, buf, real_len);
	info->buf_start += real_len;
}

static int ls1a_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf,
		int len)
{
	int i = 0;
	while (len--){
		if (buf[i++] != ls1a_nand_read_byte(mtd)){
			return -1;
		}
	}
	return 0;
}
static void ls1a_nand_cmdfunc(struct mtd_info *mtd, unsigned command,int column, int page_addr);
static void ls1a_nand_init_mtd(struct mtd_info *mtd,struct ls1a_nand_info *info)
{
	struct nand_chip *this = &info->nand_chip;
	this->options = 8;//(f->flash_width == 16) ? NAND_BUSWIDTH_16: 0;
	this->waitfunc		= ls1a_nand_waitfunc;
	this->select_chip	= ls1a_nand_select_chip;
	this->dev_ready		= ls1a_nand_dev_ready;
	this->cmdfunc		= ls1a_nand_cmdfunc;
	this->read_word		= ls1a_nand_read_word;
	this->read_byte		= ls1a_nand_read_byte;
	this->read_buf		= ls1a_nand_read_buf;
	this->write_buf		= ls1a_nand_write_buf;
	this->verify_buf	= ls1a_nand_verify_buf;
//  this->ecc.mode		= NAND_ECC_NONE;
 	this->ecc.mode		= NAND_ECC_NONE;
//	this->ecc.hwctl		= ls1a_nand_ecc_hwctl;
//	this->ecc.calculate	= ls1a_nand_ecc_calculate;
//	this->ecc.correct	= ls1a_nand_ecc_correct;
//	this->ecc.size		= 2048;
//  this->ecc.bytes         = 24;
//	this->ecc.layout = &hw_largepage_ecclayout;
	mtd->owner = THIS_MODULE;
}
static unsigned ls1a_nand_status(struct ls1a_nand_info *info)
{
	return (*((volatile unsigned int*) 0xbfe78000) & (0x1 << 10));
}

#define	clear_flag	do {*((volatile unsigned int *)(0xbfe78000)) &= 0x0;}while(0)
#define write_z_cmd  do{                                    \
	*((volatile unsigned int *)(0xbfe78000)) = 0;   \
	*((volatile unsigned int *)(0xbfe78000)) = 0;   \
	*((volatile unsigned int *)(0xbfe78000)) = 0x400; \
}while(0)

static irqreturn_t ls1a_nand_irq(int irq,void *devid)
{
	int status_time;
	struct ls1a_nand_info *info = devid;
	switch(info->cmd){
		case NAND_CMD_READOOB:
		case NAND_CMD_READ0:
            udelay(1);
            info->state = STATE_READY;
            break;
        case NAND_CMD_PAGEPROG:
            udelay(10);
            status_time=STATUS_TIME_LOOP_WS;
            while(!(ls1a_nand_status(info))){
                if(!(status_time--)){
                    write_z_cmd;
                    break;
                }
                udelay(50);
            }
            info->state = STATE_READY;
            break;
        default:
            break;
    }
    complete(&info->cmd_complete);
    return IRQ_HANDLED;
   
}
/*
 *  flags & 0x1   orderad
 *  flags & 0x2   saddr
 *  flags & 0x4   daddr
 *  flags & 0x8   length
 *  flags & 0x10  step_length
 *  flags & 0x20  step_times
 *  flags & 0x40  cmd
 ***/
static void dma_setup(unsigned int flags, struct ls1a_nand_info *info)
{
	long irqflags;
	struct ls1a_nand_dma_desc *dma_base = (volatile struct ls1a_nand_dma_desc *) (info->drcmr_dat);
	dma_base->orderad
			= (flags & DMA_ORDERAD) == DMA_ORDERAD ? info->dma_regs.orderad
					: info->dma_orderad;
	dma_base->saddr = (flags & DMA_SADDR) == DMA_SADDR ? info->dma_regs.saddr
			: info->dma_saddr;
	dma_base->daddr = (flags & DMA_DADDR) == DMA_DADDR ? info->dma_regs.daddr
			: info->dma_daddr;
	dma_base->length
			= (flags & DMA_LENGTH) == DMA_LENGTH ? info->dma_regs.length
					: info->dma_length;
	dma_base->step_length
			= (flags & DMA_STEP_LENGTH) == DMA_STEP_LENGTH ? info->dma_regs.step_length
					: info->dma_step_length;
	dma_base->step_times
			= (flags & DMA_STEP_TIMES) == DMA_STEP_TIMES ? info->dma_regs.step_times
					: info->dma_step_times;
	dma_base->cmd = (flags & DMA_CMD) == DMA_CMD ? info->dma_regs.cmd
			: info->dma_cmd;

	if ((dma_base->cmd) & (0x1 << 12))
	{
		dma_cache_wback((unsigned long)(info->data_buff), info->cac_size);
	}
	dma_cache_wback((unsigned long)(info->drcmr_dat),0x20);
	local_irq_save(irqflags);
	*(volatile unsigned int *) info->order_reg_addr
			= ((unsigned int) info->drcmr_dat_phys) | 0x1 << 3;
	while (*(volatile unsigned int *) info->order_reg_addr & 0x8){
		;
	}
 local_irq_restore(irqflags);
}

static void sync_dma(struct ls1a_nand_info *info)
{
	int status_time;
	status_time = STATUS_TIME_LOOP_WS;
	while (!(*((volatile unsigned int*) (LS1X_NAND_REG_BASE)) & (0x1 << 10))) {
		if (!(status_time--)) {
			write_z_cmd;
			break;
		}
		udelay(60);
	}
}
/**
 *  flags & 0x1     cmd
 *  flags & 0x2     addrl
 *  flags & 0x4     addrh
 *  flags & 0x8     timing
 *  flags & 0x10    idl
 *  flags & 0x20    status_idh
 *  flags & 0x40    param
 *  flags & 0x80    op_num
 *  flags & 0x100   cs_rdy_map
 ****/
static void nand_setup(unsigned int flags, struct ls1a_nand_info *info)
{
	volatile struct ls1a_nand_desc *nand_base =
			(struct ls1a_nand_desc *) (info->mmio_base);
	nand_base->cmd = 0;
	nand_base->addrl
			= (flags & NAND_ADDRL) == NAND_ADDRL ? info->nand_regs.addrl
					: info->nand_addrl;
	nand_base->addrh
			= (flags & NAND_ADDRH) == NAND_ADDRH ? info->nand_regs.addrh
					: info->nand_addrh;
	nand_base->timing
			= (flags & NAND_TIMING) == NAND_TIMING ? info->nand_regs.timing
					: info->nand_timing;
	nand_base->op_num
			= (flags & NAND_OP_NUM) == NAND_OP_NUM ? info->nand_regs.op_num
					: info->nand_op_num;
	nand_base->cs_rdy_map
			= (flags & NAND_CS_RDY_MAP) == NAND_CS_RDY_MAP ? info->nand_regs.cs_rdy_map
					: info->nand_cs_rdy_map;
	nand_base->param = ((nand_base->param) & 0xc000ffff) | (nand_base->op_num
			<< 16);
	if (flags & NAND_CMD){
		nand_base->cmd = (info->nand_regs.cmd & (~1));
		nand_base->cmd = info->nand_regs.cmd;
	}
	else{
		nand_base->cmd = info->nand_cmd;
	}
}

static void ls1a_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
		int column, int page_addr)
{
	struct ls1a_nand_info *info = mtd->priv;
	unsigned cmd_prev;
	int status_time, page_prev;
	/*int timeout = CHIP_DELAY_TIMEOUT;*/
	init_completion(&info->cmd_complete);
	cmd_prev = info->cmd;
	page_prev = info->page_addr;

	info->cmd = command;
	info->page_addr = page_addr;
	switch (command)
	{
	case NAND_CMD_READOOB:
		if (info->state == STATE_BUSY) {
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}

		info->state = STATE_BUSY;
		info->buf_count = mtd->oobsize;
		info->buf_start = 0;
		info->cac_size = info->buf_count;
		if (info->buf_count <= 0)
			break;
		//dma_cache_wback_inv((unsigned long)(info->data_buff), info->buf_count);
		/*nand regs set*/
		info->nand_regs.addrh = SPARE_ADDRH(page_addr);
		info->nand_regs.addrl = SPARE_ADDRL(page_addr) + mtd->writesize;
		info->nand_regs.op_num = info->buf_count;
		/*nand cmd set */
		info->nand_regs.cmd = 0;
		info->dma_regs.cmd = 0;
#if defined LS1CSOC
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->int_en = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ram_op
				= RAM_OP_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_rd
				= NAND_ECC_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_wr
				= NAND_ECC_OFF;
#endif
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->read = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->op_spare = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->cmd_valid = 1;
		/*dma regs config*/
		info->dma_regs.length = ALIGN_DMA(info->buf_count);
		((struct ls1a_nand_dma_cmd *) &(info->dma_regs.cmd))->dma_int_mask = 1;
		/*dma GO set*/
		nand_setup(NAND_ADDRL | NAND_ADDRH | NAND_OP_NUM | NAND_CMD, info);
		dma_setup(DMA_LENGTH | DMA_CMD, info);
		sync_dma(info);
		complete(&info->cmd_complete);
		//printk("NAND_CMD_READOOB %08X  %08X\n",info->nand_regs.addrh, info->nand_regs.addrl);
		break;
	case NAND_CMD_READ0:
		if (info->state == STATE_BUSY)
		{
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		info->buf_count = mtd->oobsize + mtd->writesize;
		info->buf_start = 0;
		info->cac_size = info->buf_count;
		if (info->buf_count <= 0)
			break;
		//dma_cache_wback_inv((unsigned long)(info->data_buff), info->buf_count);
		info->nand_regs.addrh = SPARE_ADDRH(page_addr);
		info->nand_regs.addrl = SPARE_ADDRL(page_addr);
		info->nand_regs.op_num = info->buf_count;
		//		printk ("READ0: column=0x%x, page_addr=0x%x !                 ", column, page_addr);
		//		printk ("lxy: READ0's addrl= 0x%x !\n", info->nand_regs.addrl);
		/*nand cmd set */
		info->nand_regs.cmd = 0;
		info->dma_regs.cmd = 0;
#if defined LS1CSOC
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->int_en = 0;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ram_op
				= RAM_OP_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_rd
				= NAND_ECC_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_wr
				= NAND_ECC_OFF;
#endif
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->read = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->op_spare = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->op_main = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->cmd_valid = 1;
		/*dma regs config*/
		info->dma_regs.length = ALIGN_DMA(info->buf_count);
		((struct ls1a_nand_dma_cmd *) &(info->dma_regs.cmd))->dma_int_mask = 0;
		nand_setup(NAND_ADDRL | NAND_ADDRH | NAND_OP_NUM | NAND_CMD, info);
		dma_setup(DMA_LENGTH | DMA_CMD, info);
		sync_dma(info);
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_SEQIN:
		if (info->state == STATE_BUSY)
		{
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		info->buf_count = mtd->oobsize + mtd->writesize - column;
		info->buf_start = 0;
		info->seqin_column = column;
		info->seqin_page_addr = page_addr;
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_PAGEPROG:
		//                info->coherent = 0;
		if (info->state == STATE_BUSY){
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		if (cmd_prev != NAND_CMD_SEQIN){
			printk(KERN_INFO "Prev cmd don't complete...\n");
			break;
		}
		if (info->buf_count <= 0)
			break;

#if 0                
		if(((info->num)++) % 512 == 0){
			printk("nand have write : %d M\n",(info->size)++);
		}
#endif
	
		/*nand regs set*/
		info->nand_regs.addrh = SPARE_ADDRH(info->seqin_page_addr);
		info->nand_regs.addrl = SPARE_ADDRL(info->seqin_page_addr)
				+ info->seqin_column;
		//		printk ("lxy: program's addrl= 0x%x !\n", info->nand_regs.addrl);
		info->nand_regs.op_num = info->buf_start;
		info->cac_size = info->buf_start;
		/*nand cmd set */
		info->nand_regs.cmd = 0;
		info->dma_regs.cmd = 0;
#if defined LS1CSOC
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->int_en = 0;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ram_op
				= RAM_OP_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_rd
				= NAND_ECC_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_wr
				= NAND_ECC_OFF;
#endif                
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->write = 1;
		if (info->seqin_column < mtd->writesize)
			((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->op_main = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->op_spare = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->cmd_valid = 1;
		/*dma regs config*/
		info->dma_regs.length = ALIGN_DMA(info->buf_start);
		((struct ls1a_nand_dma_cmd *) &(info->dma_regs.cmd))->dma_int_mask = 1;
		((struct ls1a_nand_dma_cmd *) &(info->dma_regs.cmd))->dma_r_w = 1;
		nand_setup(NAND_ADDRL | NAND_ADDRH | NAND_OP_NUM | NAND_CMD, info);
		dma_setup(DMA_LENGTH | DMA_CMD, info);
		sync_dma(info);
		complete(&info->cmd_complete);
		//printk("NAND_CMD_PAGEPROG %08X\n",info->seqin_page_addr);
		break;
	case NAND_CMD_RESET:
		/*Do reset op anytime*/
		//                info->state = STATE_BUSY;
		/*nand cmd set */
		info->nand_regs.cmd = 0;
#if defined LS1CSOC
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->int_en = 0;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ram_op
				= RAM_OP_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_rd
				= NAND_ECC_OFF;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ecc_wr
				= NAND_ECC_OFF;
#endif
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->reset = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->cmd_valid = 1;
		nand_setup(NAND_CMD, info);
		status_time = STATUS_TIME_LOOP_R;
		while (!ls1a_nand_status(info)){
			if (!(status_time--)){
				write_z_cmd;
				break;
			}
			udelay(50);
		}
		info->state = STATE_READY;
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_ERASE1:
		if (info->state == STATE_BUSY) {
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		/*nand regs set*/
		info->nand_regs.addrh = NO_SPARE_ADDRH(page_addr);
		info->nand_regs.addrl = NO_SPARE_ADDRL(page_addr);
		/*nand cmd set */
		info->nand_regs.cmd = 0;
#if defined LS1CSOC
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->int_en = 0;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->ram_op
				= RAM_OP_OFF;
#endif                
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->erase_one = 1;
		((struct ls1a_nand_cmdset*) &(info->nand_regs.cmd))->cmd_valid = 1;
		nand_setup(NAND_ADDRL | NAND_ADDRH | NAND_OP_NUM | NAND_CMD, info);
		status_time = STATUS_TIME_LOOP_E;
		udelay(2000); //lxy
		while (!ls1a_nand_status(info)){
			if (!(status_time--)){
				write_z_cmd;
				break;
			}
			udelay(50);
		}
		info->state = STATE_READY;
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_STATUS:
		if (info->state == STATE_BUSY){
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		info->buf_count = 0x1;
		info->buf_start = 0x0;
		*(unsigned char *) info->data_buff = ls1a_nand_status(info) | 0x80;
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_READID:
		if (info->state == STATE_BUSY){
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->state = STATE_BUSY;
		info->buf_count = 0x4;
		info->buf_start = 0;
		{

			unsigned int id_val_l = 0, id_val_h = 0;
			unsigned int timing = 0;
			unsigned char *data = (unsigned char *) (info->data_buff);
			_NAND_READ_REG(0xc,timing);
			_NAND_SET_REG(0xc,0x30f0);
			_NAND_SET_REG(0x0,0x21);

			while (((id_val_l |= _NAND_IDL) & 0xff) == 0){
				id_val_h = _NAND_IDH;
			}

			while (id_val_h == 0) //lxy
			{
				id_val_h = _NAND_IDH;
			}

			//while (((id_val_h = _NAND_IDH) & 0xff000000) == 0xec)
			//id_val_l = _NAND_IDL;

			_NAND_SET_REG(0xc,timing);
			udelay(50);
			data[0] = (id_val_h & 0xff);
			data[1] = (id_val_l & 0xff000000) >> 24;
			data[2] = (id_val_l & 0x00ff0000) >> 16;
			data[3] = (id_val_l & 0x0000ff00) >> 8;

		}
		//info->state = STATE_READY;
		complete(&info->cmd_complete);
		break;
	case NAND_CMD_ERASE2:
	case NAND_CMD_READ1:
		complete(&info->cmd_complete);
		break;

	case NAND_CMD_RNDOUT: //lxy
		if (info->state == STATE_BUSY){
			printk(KERN_INFO "nandflash chip if busy...\n");
			return;
		}
		info->buf_count = mtd->oobsize + mtd->writesize;
		info->buf_start = column;
		complete(&info->cmd_complete);
		break;

	default:
		printk(KERN_ERR "non-supported command.\n");
		complete(&info->cmd_complete);
		break;
	}
	wait_for_completion(&info->cmd_complete);
	//	clear_flag;	//lxy
	//        wait_for_completion_timeout(&info->cmd_complete,timeout);
	if (info->cmd == NAND_CMD_READ0 || info->cmd == NAND_CMD_READOOB){
		dma_cache_inv((unsigned long)(info->data_buff),info->cac_size);
	}
	info->state = STATE_READY;

}
static int ls1a_nand_detect(struct mtd_info *mtd)
{
        return (mtd->erasesize != 1<<17 || mtd->writesize != 1<<11 || mtd->oobsize != 1<<6);
}

//static void test_handler(unsigned long data)
//{
//    u32 val;
//    struct ls1a_nand_info *s = (struct ls1a_nand_info *)data;
//    mod_timer(&s->test_timer, jiffies+1);
//    val = s->dma_ask_phy | 0x4;
//    *((volatile unsigned int *)0xbfd01160) = val;
//    udelay(1000);
//}


static void ls1a_nand_init_info(struct ls1a_nand_info *info)
{
	//*((volatile unsigned int *)0xbfe78018) = 0x30000;
	info->timing_flag = 1;/*0:read; 1:write;*/
	info->num = 0;
	info->size = 0;
	//info->coherent = 0;
	info->cac_size = 0;
	info->state = STATE_READY;
	info->cmd = -1;
	info->page_addr = -1;
	info->nand_addrl = 0x0;
	info->nand_addrh = 0x0;
#if defined LS1CSOC  
	info->nand_timing = 0x209;
#else    
	info->nand_timing = 0x209;
#endif
	info->nand_op_num = 0x0;
	info->nand_cs_rdy_map = 0x00000000;
	info->nand_cmd = 0;

	info->dma_orderad = 0x0;
	info->dma_saddr = info->data_buff_phys;
	info->dma_daddr = DMA_ACCESS_ADDR;
	info->dma_length = 0x0;
	info->dma_step_length = 0x0;
	info->dma_step_times = 0x1;
	info->dma_cmd = 0x0;

//	init_timer(&info->test_timer);
//	info->test_timer.function = test_handler;
//	info->test_timer.expires = jiffies + 10;
//	info->test_timer.data = (unsigned long) info;

	info->order_reg_addr = ORDER_REG_ADDR;
}

static int ls1a_nand_probe(struct platform_device *pdev)
{
	struct ls1a_nand_platform_data *pdata;
	struct ls1a_nand_info *info;
	struct nand_chip *this;
	struct mtd_info *mtd;
	struct resource *r;
	int ret = 0, irq;
	//#ifdef CONFIG_MTD_PARTITIONS
	const char *part_probes[] =
	{ "cmdlinepart", NULL };
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
	//#endif
	//nand_gpio_init();
	pdata = pdev->dev.platform_data;
	if (!pdata)
	{
		dev_err(&pdev->dev, "no platform data defined\n");
		return -ENODEV;
	}
	mtd = kzalloc(sizeof(struct mtd_info) + sizeof(struct ls1a_nand_info),
			GFP_KERNEL);
	if (!mtd)
	{
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	info = (struct ls1a_nand_info *) (&mtd[1]);
	info->pdev = pdev;

	this = &info->nand_chip;
	mtd->priv = info;
	info->drcmr_dat = (unsigned int) dma_alloc_coherent(&pdev->dev,
			MAX_BUFF_SIZE, &info->drcmr_dat_phys, GFP_KERNEL);
	info->drcmr_dat = CAC_ADDR(info->drcmr_dat);
	info->dma_ask = (unsigned int) dma_alloc_coherent(&pdev->dev,
			MAX_BUFF_SIZE, &info->dma_ask_phy, GFP_KERNEL);

	if (!info->drcmr_dat)
	{
		dev_err(&pdev->dev, "fialed to allocate memory\n");
		return ENOMEM;
	}
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL)
	{
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		ret = -ENODEV;
		goto fail_put_clk;
	}

	r = request_mem_region(r->start, r->end - r->start + 1, pdev->name);
	if (r == NULL)
	{
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto fail_put_clk;
	}

	info->mmio_base = ioremap(r->start, r->end - r->start + 1);
	if (info->mmio_base == NULL)
	{
		dev_err(&pdev->dev, "ioremap() failed\n");
		ret = -ENODEV;
		goto fail_free_res;
	}
	ret = ls1a_nand_init_buff(info);
	if (ret)
		goto fail_free_io;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
	{
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}
	ret = request_irq(irq, ls1a_nand_irq, IRQF_DISABLED, pdev->name, info);
	if (ret < 0)
	{
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto fail_free_buf;
	}

	ls1a_nand_init_mtd(mtd, info);

	ls1a_nand_init_info(info);
	platform_set_drvdata(pdev, mtd);

	if (nand_scan(mtd, 1))
	{
		dev_err(&pdev->dev, "failed to scan nand\n");
		ret = -ENXIO;
		goto fail_free_irq;
	}
	if (ls1a_nand_detect(mtd))
	{
		dev_err(&pdev->dev, "driver don't support the Flash!\n");
		ret = -ENXIO;
		goto fail_free_irq;
	}
	//#ifdef CONFIG_MTD_PARTITIONS
	//#ifdef CONFIG_MTD_CMDLINE_PARTS
	mtd->name = "mtdparts";
	num_partitions = parse_mtd_partitions(mtd, part_probes, &partitions, 0);
	//#endif
	if (num_partitions <= 0)
	{
		partitions = pdata->parts;
		num_partitions = pdata->nr_parts;

	}
	         return add_mtd_partitions(mtd, partitions , num_partitions);
	//return mtd_device_register(mtd, partitions, num_partitions);
	//#else
	//		 return add_mtd_device(mtd);
	//#endif
fail_free_irq:
	free_irq(13, info);
fail_free_buf:
	dma_free_coherent(&pdev->dev, info->data_buff_size,
			info->data_buff, info->data_buff_phys);
fail_free_io:
	iounmap(info->mmio_base);
fail_free_res:
	release_mem_region(r->start, r->end - r->start + 1);
fail_put_clk:
/*fail_free_mtd:*/
	kfree(mtd);
	return ret;
}

static int ls1a_nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct ls1a_nand_info *info = mtd->priv;

	platform_set_drvdata(pdev, NULL);
	//	del_mtd_device(mtd);
	//	del_mtd_partitions(mtd);
	nand_release(mtd);
	free_irq(13, info);
	kfree((const void *)info->drcmr_dat);
	/*struct ls1a_nand_platform_data{
	 int enable_arbiter;
	 struct mtd_partition *parts;
	 unsigned int nr_parts;
	 };*/
	kfree(mtd);
	return 0;
}

//void nandwrite_testoob(struct mtd_info *nand, unsigned int off)/*cmd addr(L,page num) timing op_num(byte) */
//{
//	int i,retlen;
//	int ooboffset;
//	u_char datbuf[3000], oobbuf[100], *p;
//	struct ls1a_nand_info *info= (struct ls1a_nand_info *)nand->priv;
//	struct nand_chip * chip= &info->nand_chip;
//	struct mtd_oob_ops ops;
//	struct erase_info ei;
//	off &= ~(nand->writesize - 1);
//	loff_t addr = (loff_t) off;
//	memset(&ops, 0, sizeof(ops));
//	memset(datbuf,0, sizeof(datbuf));
//	memset(oobbuf,0, sizeof(oobbuf));
//
//	for(i = 0;i<2048;i++){
//		datbuf[i]=(u_char)(i&0xFF);
//	}
//	for(i=2;i<64;i++)
//	{
//		oobbuf[i] = (u_char)(i&0xFF);
//	}
//
//	ops.datbuf = datbuf;
//	ops.oobbuf = oobbuf;
//	ops.len = nand->writesize;
//	ops.ooblen = nand->oobsize;
//	ops.mode = MTD_OOB_AUTO;
//	retlen = nand->write_oob(nand, addr, &ops);
//	printk("nand->write_oob retlen %d\n",retlen);
//
//	memset(datbuf,0, sizeof(datbuf));
//	memset(oobbuf,0, sizeof(oobbuf));
//	ops.datbuf = datbuf;
//	ops.oobbuf = oobbuf;
//	ops.len = nand->writesize;
//	ops.ooblen = nand->oobsize;
//	ops.mode = MTD_OOB_RAW;
//	i = nand->read_oob(nand, addr, &ops);
//	printk("Page %08lx dump:\n", off);
//	i = nand->writesize >> 4;
//	p = datbuf;
//	while (i--) {
//			printk("%08lX:%02X %02X %02X %02X %02X %02X %02X %02X"
//			       "  %02X %02X %02X %02X %02X %02X %02X %02X\n",
//			       off, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
//			       p[8], p[9], p[10], p[11], p[12], p[13], p[14],
//			       p[15]);
//		p += 16;
//		off += 16;
//	}
//
//	printk("OOB:\n");
//	i = nand->oobsize >> 3;
//	ooboffset = 0;
//	p = ops.oobbuf;
//	while (i--) {
//		printk("%08X: %02X %02X %02X %02X %02X %02X %02X %02X\n",
//			ooboffset, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
//		p += 8;
//		ooboffset += 8;
//	}
//
//	memset(&ei, 0, sizeof(struct erase_info));
//	ei.mtd  = nand;
//	ei.addr = addr;
//	ei.len  = nand->erasesize;
//
//	retlen = nand->erase(nand, &ei);
//	printk("nand->erase retlen %d\n",retlen);
//
//	memset(datbuf,0, sizeof(datbuf));
//	memset(oobbuf,0, sizeof(oobbuf));
//	ops.datbuf = datbuf;
//	ops.oobbuf = oobbuf;
//	ops.len = nand->writesize;
//	ops.ooblen = nand->oobsize;
//	ops.mode = MTD_OOB_RAW;
//	i = nand->read_oob(nand, addr, &ops);
//	printk("Page %08lx dump:\n", addr);
//	off = addr;
//	i = nand->writesize >> 4;
//	p = datbuf;
//	while (i--) {
//			printk("%08lX:%02X %02X %02X %02X %02X %02X %02X %02X"
//			       "  %02X %02X %02X %02X %02X %02X %02X %02X\n",
//			       off, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
//			       p[8], p[9], p[10], p[11], p[12], p[13], p[14],
//			       p[15]);
//		p += 16;
//		off += 16;
//	}
//
//	printk("OOB:\n");
//	i = nand->oobsize >> 3;
//	ooboffset = 0;
//	p = ops.oobbuf;
//	while (i--) {
//		printk("%08X: %02X %02X %02X %02X %02X %02X %02X %02X\n",
//			ooboffset, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
//		p += 8;
//		ooboffset += 8;
//	}
//	panic("nandwrite_testoob\n");
//	return ;
//}

static int ls1a_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mtd_info *mtd = (struct mtd_info *) platform_get_drvdata(pdev);
	struct ls1a_nand_info *info = mtd->priv;

	if (info->state != STATE_READY){
		dev_err(&pdev->dev, "driver busy, state = %d\n", info->state);
		return -EAGAIN;
	}

	return 0;
}
static int ls1a_nand_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver ls1a_nand_driver = {
	.driver = {
		.name	= "ls1a-nand",
        .owner	= THIS_MODULE,
	},
	.probe		= ls1a_nand_probe,
	.remove		= ls1a_nand_remove,
	.suspend	= ls1a_nand_suspend,
	.resume		= ls1a_nand_resume,
};

static int __init ls1a_nand_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&ls1a_nand_driver);
	if (ret){
		printk(KERN_ERR "failed to register loongson_1g_nand_driver");
	}
	return ret;
}
static void __exit ls1a_nand_exit(void)
{
	platform_driver_unregister(&ls1a_nand_driver);
}
module_init(ls1a_nand_init);
module_exit(ls1a_nand_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Loongson_1g NAND controller driver");

