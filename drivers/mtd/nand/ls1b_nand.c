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


#define DMA_ACCESS_ADDR     0x1fe78040
#define ORDER_REG_ADDR      (KSEG1ADDR(0x1fd01160))
#define MAX_BUFF_SIZE	4096
#define PAGE_SHIFT      12
#define NO_SPARE_ADDRH(x)   ((x) >> (32 - (PAGE_SHIFT - 1 )))   
#define NO_SPARE_ADDRL(x)   ((x) << (PAGE_SHIFT - 1))
#define SPARE_ADDRH(x)      ((x) >> (32 - (PAGE_SHIFT )))   
#define SPARE_ADDRL(x)      ((x) << (PAGE_SHIFT ))
#define ALIGN_DMA(x)       (((x)+ 3)/4)


#define NAND_REG_BASE       0xbfe78000
#define NAND_CMD_REG        (NAND_REG_BASE + 0)    
#define NAND_ADDRL_REG      (NAND_REG_BASE + 0x4)    
#define NAND_ADDRH_REG      (NAND_REG_BASE + 0x8)    
#define NAND_TIMING_REG     (NAND_REG_BASE + 0xc)    
#define NAND_IDL_REG        (NAND_REG_BASE + 0x10)    
#define NAND_IDH_REG        (NAND_REG_BASE + 0x14)    
#define NAND_PARAM_REG      (NAND_REG_BASE + 0x18)    
#define NAND_OPNUM_REG      (NAND_REG_BASE + 0x1c)    
#define NAND_CSMAP_REG      (NAND_REG_BASE + 0x20)    

//#define USE_POLL
#ifdef USE_POLL
#define complete(...)
#define wait_for_completion_timeout(...)
#define init_completion(...)
#define request_irq(...) (0)
#define free_irq(...) 
#endif


#define CHIP_DELAY_TIMEOUT (2*HZ/10)

#define STATUS_TIME_LOOP_R  60 
#define STATUS_TIME_LOOP_WS  200  
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


#define  _NAND_IDL       (*((volatile unsigned int*)(NAND_IDL_REG)))
#define  _NAND_IDH       (*((volatile unsigned int*)(NAND_IDH_REG)))
#define  _NAND_SET_REG(x,y)   do{*((volatile unsigned int*)(x)) = (y);}while(0)                           
#define  _NAND_READ_REG(x,y)  do{(y) =  *((volatile unsigned int*)(x));}while(0) 


enum{
    ERR_NONE        = 0,
    ERR_DMABUSERR   = -1,
    ERR_SENDCMD     = -2,
    ERR_DBERR       = -3,
    ERR_BBERR       = -4,
};
enum{
    STATE_READY = 0,
    STATE_BUSY  ,
};

struct ls1b_nand_platform_data{
        int enable_arbiter;
        struct mtd_partition *parts;
        unsigned int nr_parts;
};
struct ls1b_nand_cmdset {
        uint32_t    cmd_valid:1;
	uint32_t    read:1;
	uint32_t    write:1;
	uint32_t    erase_one:1;
	uint32_t    erase_con:1;
	uint32_t    read_id:1;
	uint32_t    reset:1;
	uint32_t    read_sr:1;
	uint32_t    op_main:1;
	uint32_t    op_spare:1;
	uint32_t    done:1;
        uint32_t    resv1:5;//11-15 reserved
        uint32_t    nand_rdy:4;//16-19
        uint32_t    nand_ce:4;//20-23
        uint32_t    resv2:8;//24-32 reserved
};
struct ls1b_nand_dma_desc{
        uint32_t    orderad;
        uint32_t    saddr;
        uint32_t    daddr;
        uint32_t    length;
        uint32_t    step_length;
        uint32_t    step_times;
        uint32_t    cmd;
};
struct ls1b_nand_dma_cmd{
        uint32_t    dma_int_mask:1;
        uint32_t    dma_int:1;
        uint32_t    dma_sl_tran_over:1;
        uint32_t    dma_tran_over:1;
        uint32_t    dma_r_state:4;
        uint32_t    dma_w_state:4;
        uint32_t    dma_r_w:1;
        uint32_t    dma_cmd:2;
        uint32_t    revl:17;
};
struct ls1b_nand_desc{
        uint32_t    cmd;
        uint32_t    addrl;
        uint32_t    addrh;
        uint32_t    timing;
        uint32_t    idl;//readonly
        uint32_t    status_idh;//readonly
        uint32_t    param;
        uint32_t    op_num;
        uint32_t    cs_rdy_map;
};
struct ls1b_nand_info {
	struct nand_chip	nand_chip;

	struct platform_device	    *pdev;
        /* MTD data control*/
	unsigned int 		buf_start;
	unsigned int		buf_count;
        /* NAND registers*/
	void __iomem		*mmio_base;
        struct ls1b_nand_desc   nand_regs;
        unsigned int            nand_addrl;
        unsigned int            nand_addrh;
        unsigned int            nand_timing;
        unsigned int            nand_op_num;
        unsigned int            nand_cs_rdy_map;
        unsigned int            nand_cmd;

	/* DMA information */

        struct ls1b_nand_dma_desc  dma_regs;
        unsigned int            order_reg_addr;  
        unsigned int            dma_orderad;
        unsigned int            dma_saddr;
        unsigned int            dma_daddr;
        unsigned int            dma_length;
        unsigned int            dma_step_length;
        unsigned int            dma_step_times;
        unsigned int            dma_cmd;
        int			drcmr_dat;//dma descriptor address;
	dma_addr_t 		drcmr_dat_phys;
        size_t                  drcmr_dat_size;
	unsigned char		*data_buff;//dma data buffer;
	dma_addr_t 		data_buff_phys;
	size_t			data_buff_size;
        unsigned long           cac_size;
        unsigned long           num;
        unsigned long           size;
        struct timer_list       test_timer;
        unsigned int            dma_ask;
        unsigned int            dma_ask_phy;

	/* relate to the command */
	unsigned int		state;
//	int			use_ecc;	/* use HW ECC ? */
	size_t			data_size;	/* data size in FIFO */
        unsigned int            cmd;
        unsigned int            page_addr;
        struct completion 	cmd_complete;
        unsigned int            seqin_column;
        unsigned int            seqin_page_addr;
        unsigned int            timing_flag;
        unsigned int            timing_val;
};


static struct nand_ecclayout hw_largepage_ecclayout = {
	.eccbytes = 24,
	.eccpos = {
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 38} }
};
#define show_data_debug  0
#define show_debug(x,y)     show_debug_msk(x,y)
#define show_debug_msk(x,y)   do{ if(show_data_debug) {printk(KERN_ERR "%s:\n",__func__);show_data(x,y);} }while(0)

static void show_data(void * base,int num)
{
    int i=0;
    unsigned char *arry=( unsigned char *) base;
    for(i=0;i<num;i++){
        if(!(i % 32)){
            printk(KERN_ERR "\n");
        }
        if(!(i % 16)){
            printk("  ");
        }
        printk("%02x ",arry[i]);
    }
    printk(KERN_ERR "\n");
    
}


static int ls1b_nand_init_buff(struct ls1b_nand_info *info)
{
        struct platform_device *pdev = info->pdev;
    	info->data_buff = dma_alloc_coherent(&pdev->dev, MAX_BUFF_SIZE,
				&info->data_buff_phys, GFP_KERNEL);
        info->data_buff = CAC_ADDR(info->data_buff);
	if (info->data_buff == NULL) {
		dev_err(&pdev->dev, "failed to allocate dma buffer\n");
		return -ENOMEM;
	}
       	info->data_buff_size = MAX_BUFF_SIZE;
        return 0;
}
static int ls1b_nand_ecc_calculate(struct mtd_info *mtd,
		const uint8_t *dat, uint8_t *ecc_code)
{
	return 0;
}
static int ls1b_nand_ecc_correct(struct mtd_info *mtd,
		uint8_t *dat, uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct ls1b_nand_info *info = mtd->priv;
	/*
	 * Any error include ERR_SEND_CMD, ERR_DBERR, ERR_BUSERR, we
	 * consider it as a ecc error which will tell the caller the
	 * read fail We have distinguish all the errors, but the
	 * nand_read_ecc only check this function return value
	 */
	return 0;
}

static void ls1b_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	return;
}




static int ls1b_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
    return 0;
}
static void ls1b_nand_select_chip(struct mtd_info *mtd, int chip)
{
	return;
}
static int ls1b_nand_dev_ready(struct mtd_info *mtd)
{
	return 1;
}
static void ls1b_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
        struct ls1b_nand_info *info = mtd->priv;
        int i,real_len = min_t(size_t, len, info->buf_count - info->buf_start);
	memcpy(buf, info->data_buff + info->buf_start, real_len);

        show_debug(info->data_buff,0x40);

        info->buf_start += real_len;
}
static u16 ls1b_nand_read_word(struct mtd_info *mtd)
{
        struct ls1b_nand_info *info = mtd->priv;
        u16 retval = 0xFFFF;
        if(!(info->buf_start & 0x1) && info->buf_start < info->buf_count){
            retval = *(u16 *)(info->data_buff + info->buf_start);
        }
        info->buf_start += 2;
        return retval;


}
static uint8_t ls1b_nand_read_byte(struct mtd_info *mtd)
{
        struct ls1b_nand_info *info = mtd->priv;
	char retval = 0xFF;
	if (info->buf_start < info->buf_count)
		retval = info->data_buff[(info->buf_start)++];
       // show_debug(info->data_buff,6);
	return retval;
}
static void ls1b_nand_write_buf(struct mtd_info *mtd,const uint8_t *buf, int len)
{
        int i;
        struct ls1b_nand_info *info = mtd->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(info->data_buff + info->buf_start, buf, real_len);
            show_debug(info->data_buff,0x20);
 //           show_debug(info->data_buff+2048,0x20);
	info->buf_start += real_len;
}
static int ls1b_nand_verify_buf(struct mtd_info *mtd,const uint8_t *buf, int len)
{
       int i=0; 
        struct ls1b_nand_info *info = mtd->priv;
            show_debug(info->data_buff,0x20);
        while(len--){
            if(buf[i++] != ls1b_nand_read_byte(mtd) ){
//                printk("?????????????????????????????????????????????????????verify error...\n\n");
                return -1;
            }
        }
	return 0;
}
static void ls1b_nand_cmdfunc(struct mtd_info *mtd, unsigned command,int column, int page_addr);
static void ls1b_nand_init_mtd(struct mtd_info *mtd,struct ls1b_nand_info *info)
{
	struct nand_chip *this = &info->nand_chip;

	//this->options = NAND_CACHEPRG|NAND_SKIP_BBTSCAN;//(f->flash_width == 16) ? NAND_BUSWIDTH_16: 0;
	this->options = NAND_CACHEPRG;//(f->flash_width == 16) ? NAND_BUSWIDTH_16: 0;

	this->waitfunc		= ls1b_nand_waitfunc;
	this->select_chip	= ls1b_nand_select_chip;
	this->dev_ready		= ls1b_nand_dev_ready;
	this->cmdfunc		= ls1b_nand_cmdfunc;
	this->read_word		= ls1b_nand_read_word;
	this->read_byte		= ls1b_nand_read_byte;
	this->read_buf		= ls1b_nand_read_buf;
	this->write_buf		= ls1b_nand_write_buf;
	this->verify_buf	= ls1b_nand_verify_buf;

#if 0
        this->ecc.mode		= NAND_ECC_NONE;
#else
	this->ecc.mode		= NAND_ECC_SOFT;
#endif
	this->ecc.hwctl		= ls1b_nand_ecc_hwctl;
	this->ecc.calculate	= ls1b_nand_ecc_calculate;
	this->ecc.correct	= ls1b_nand_ecc_correct;

	this->ecc.layout = &hw_largepage_ecclayout;
        mtd->owner = THIS_MODULE;
}
static void show_dma_regs(void *dma_regs,int flag)
{
    return ;
    unsigned int *regs=dma_regs;
    printk("\n");
    printk("0x%08x:0x%08x\n",regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    printk("0x%08x:0x%08x\n",++regs,*regs);
    if(flag)
    printk("ORDER_REG_ADDR:0x%08x\n",*(volatile unsigned int *)ORDER_REG_ADDR);
}
static unsigned ls1b_nand_status(struct ls1b_nand_info *info)
{
    return(*((volatile unsigned int*)NAND_CMD_REG) & (0x1<<10));
}
#define write_z_cmd  do{                                    \
            *((volatile unsigned int *)(NAND_CMD_REG)) = 0;   \
            *((volatile unsigned int *)(NAND_CMD_REG)) = 400; \
    }while(0)
static irqreturn_t ls1b_nand_irq(int irq,void *devid)
{
    int status_time;
    struct ls1b_nand_info *info = devid;
    struct ls1b_nand_dma_desc *dma_regs = (volatile struct ls1b_nand_dma_desc *)(info->drcmr_dat);
    struct ls1b_nand_dma_cmd *dma_cmd = (struct ls1b_nand_dma_cmd *)(&(dma_regs->cmd));
    switch(info->cmd){
        case NAND_CMD_READOOB:
        case NAND_CMD_READ0:
            status_time=STATUS_TIME_LOOP_R;
            while(!(ls1b_nand_status(info))){
                if(!(status_time--)){
                    write_z_cmd;
                    break;
                }
                udelay(1);
            }
            break;
        case NAND_CMD_PAGEPROG:
            status_time=STATUS_TIME_LOOP_WS;
            while(!(ls1b_nand_status(info))){
                if(!(status_time--)){
                    write_z_cmd;
                    break;
                }
                udelay(2);
            }
            break;
        default:
       //     printk(KERN_ERR "this never happend!!!^^^^^^^^^^^^^^&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
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
static void dma_setup(unsigned int flags,struct ls1b_nand_info *info)
{
    struct ls1b_nand_dma_desc *dma_base = (volatile struct ls1b_nand_dma_desc *)(info->drcmr_dat);
    int status_time;
    dma_base->orderad = (flags & DMA_ORDERAD)== DMA_ORDERAD ? info->dma_regs.orderad : info->dma_orderad;
    dma_base->saddr = (flags & DMA_SADDR)== DMA_SADDR ? info->dma_regs.saddr : info->dma_saddr;
    dma_base->daddr = (flags & DMA_DADDR)== DMA_DADDR ? info->dma_regs.daddr : info->dma_daddr;
    dma_base->length = (flags & DMA_LENGTH)== DMA_LENGTH ? info->dma_regs.length: info->dma_length;
    dma_base->step_length = (flags & DMA_STEP_LENGTH)== DMA_STEP_LENGTH ? info->dma_regs.step_length: info->dma_step_length;
    dma_base->step_times = (flags & DMA_STEP_TIMES)== DMA_STEP_TIMES ? info->dma_regs.step_times: info->dma_step_times;
    dma_base->cmd = (flags & DMA_CMD)== DMA_CMD ? info->dma_regs.cmd: info->dma_cmd;

    if((dma_base->cmd)&(0x1 << 12)){ /*bit:12 direct this is a write op*/    
        dma_cache_wback((unsigned long)(info->data_buff),info->cac_size);
    }
    dma_cache_wback((unsigned long)(info->drcmr_dat),0x20);

    {
    long flags;
    local_irq_save(flags);
    *(volatile unsigned int *)info->order_reg_addr = ((unsigned int )info->drcmr_dat_phys) | 0x1<<3;
    while (*(volatile unsigned int *)info->order_reg_addr & 0x8 );
#ifdef USE_POLL
    while(!(ls1b_nand_status(info)));
    info->state = STATE_READY;
#endif
    local_irq_restore(flags);
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
static void nand_setup(unsigned int flags ,struct ls1b_nand_info *info)
{
    int i,val1,val2,val3;
    struct ls1b_nand_desc *nand_base = (struct ls1b_nand_desc *)(info->mmio_base);
    nand_base->cmd = 0;
    nand_base->addrl = (flags & NAND_ADDRL)==NAND_ADDRL ? info->nand_regs.addrl: info->nand_addrl;
    nand_base->addrh = (flags & NAND_ADDRH)==NAND_ADDRH ? info->nand_regs.addrh: info->nand_addrh;
    nand_base->timing = (flags & NAND_TIMING)==NAND_TIMING ? info->nand_regs.timing: info->nand_timing;
    nand_base->op_num = (flags & NAND_OP_NUM)==NAND_OP_NUM ? info->nand_regs.op_num: info->nand_op_num;
    nand_base->cs_rdy_map = (flags & NAND_CS_RDY_MAP)==NAND_CS_RDY_MAP ? info->nand_regs.cs_rdy_map: info->nand_cs_rdy_map;
    if(flags & NAND_CMD){
            nand_base->cmd = (info->nand_regs.cmd) & (~0xff);
            nand_base->cmd = info->nand_regs.cmd;
    }
    else
        nand_base->cmd = info->nand_cmd;
}
static void ls1b_nand_cmdfunc(struct mtd_info *mtd, unsigned command,int column, int page_addr)
{
        struct ls1b_nand_info *info = mtd->priv;
        unsigned cmd_prev;
        int status_time,page_prev;
        int timeout = CHIP_DELAY_TIMEOUT;
        init_completion(&info->cmd_complete);
        cmd_prev = info->cmd;
        page_prev = info->page_addr;

        info->cmd = command;
        info->page_addr = page_addr;
        switch(command){
            case NAND_CMD_READOOB:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY; 
                info->buf_count = mtd->oobsize;
                info->buf_start = 0;
                info->cac_size = info->buf_count;
                if(info->buf_count <=0 )
                    break;
                dma_cache_wback_inv((unsigned long)(info->data_buff),info->cac_size);
		info->dma_regs.cmd = 0;
                /*nand regs set*/
                info->nand_regs.addrh =  SPARE_ADDRH(page_addr);
                info->nand_regs.addrl = SPARE_ADDRL(page_addr) + mtd->writesize;
                info->nand_regs.op_num = info->buf_count;
               /*nand cmd set */ 
                info->nand_regs.cmd = 0; 
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->read = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->op_spare = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->cmd_valid = 1;
                /*dma regs config*/
                info->dma_regs.length =ALIGN_DMA(info->buf_count);
                ((struct ls1b_nand_dma_cmd *)&(info->dma_regs.cmd))->dma_int_mask = 1;
                /*dma GO set*/       
                nand_setup(NAND_ADDRL|NAND_ADDRH|NAND_OP_NUM|NAND_CMD,info);
                dma_setup(DMA_LENGTH|DMA_CMD,info);
                break;
            case NAND_CMD_READ0:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY;
                info->buf_count = mtd->oobsize + mtd->writesize ;
                info->buf_start =  0 ;
                info->cac_size = info->buf_count;
                if(info->buf_count <=0 )
                    break;
                dma_cache_wback_inv((unsigned long)(info->data_buff),info->cac_size);
                info->nand_regs.addrh = SPARE_ADDRH(page_addr);
                info->nand_regs.addrl = SPARE_ADDRL(page_addr);
                info->nand_regs.op_num = info->buf_count;
               /*nand cmd set */ 
                info->nand_regs.cmd = 0; 
                info->dma_regs.cmd = 0;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->read = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->op_spare = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->op_main = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->cmd_valid = 1; 
                /*dma regs config*/
                info->dma_regs.length = ALIGN_DMA(info->buf_count);
                ((struct ls1b_nand_dma_cmd *)&(info->dma_regs.cmd))->dma_int_mask = 1;
                nand_setup(NAND_ADDRL|NAND_ADDRH|NAND_OP_NUM|NAND_CMD,info);
                dma_setup(DMA_LENGTH|DMA_CMD,info);
                break;
            case NAND_CMD_SEQIN:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
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
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY;
                if(cmd_prev != NAND_CMD_SEQIN){
                    printk("Prev cmd don't complete...\n");
                    break;
                }
                if(info->buf_count <= 0 )
                    break;
                info->cac_size = info->buf_count;
                /*nand regs set*/
                info->nand_regs.addrh =  SPARE_ADDRH(info->seqin_page_addr);
                info->nand_regs.addrl =  SPARE_ADDRL(info->seqin_page_addr) + info->seqin_column;
                info->nand_regs.op_num = info->buf_start;
                /*nand cmd set */ 
                info->nand_regs.cmd = 0; 
                info->dma_regs.cmd = 0;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->write = 1;
                if(info->seqin_column < mtd->writesize)
                    ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->op_main = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->op_spare = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->cmd_valid = 1; 
                /*dma regs config*/
                info->dma_regs.length = ALIGN_DMA(info->buf_start);
                ((struct ls1b_nand_dma_cmd *)&(info->dma_regs.cmd))->dma_int_mask = 1;
                ((struct ls1b_nand_dma_cmd *)&(info->dma_regs.cmd))->dma_r_w = 1;
                nand_setup(NAND_ADDRL|NAND_ADDRH|NAND_OP_NUM|NAND_CMD,info);
                dma_setup(DMA_LENGTH|DMA_CMD,info);
                break;
            case NAND_CMD_RESET:
                /*Do reset op anytime*/
//                info->state = STATE_BUSY;
               /*nand cmd set */ 
                info->nand_regs.cmd = 0; 
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->reset = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->cmd_valid = 1; 
                nand_setup(NAND_CMD,info);
                status_time = STATUS_TIME_LOOP_R;
                while(!ls1b_nand_status(info)){
                    if(!(status_time--)){
                        write_z_cmd;
                        break;
                    }
                    udelay(50);
                }
                complete(&info->cmd_complete);
                break;
            case NAND_CMD_ERASE1:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY;
                /*nand regs set*/
                info->nand_regs.addrh =  NO_SPARE_ADDRH(page_addr);
                info->nand_regs.addrl =  NO_SPARE_ADDRL(page_addr) ;
               /*nand cmd set */
                info->nand_regs.cmd = 0; 
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->erase_one = 1;
                ((struct ls1b_nand_cmdset*)&(info->nand_regs.cmd))->cmd_valid = 1;
                nand_setup(NAND_ADDRL|NAND_ADDRH|NAND_OP_NUM|NAND_CMD,info);
                status_time = STATUS_TIME_LOOP_E;
//                udelay(3000);    
                while(!ls1b_nand_status(info)){
                    if(!(status_time--)){
                        write_z_cmd;
                        break;
                    }
                    udelay(50);    
                }
                complete(&info->cmd_complete);
                break;
            case NAND_CMD_STATUS:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY;
                info->buf_count = 0x1;
                info->buf_start = 0x0;
                *(unsigned char *)info->data_buff = 0x80;//we use WP bit only here
                complete(&info->cmd_complete);
                break;
            case NAND_CMD_READID:
                if(info->state == STATE_BUSY){
                    printk("nandflash chip if busy...\n");
                    return;
                }
                info->state = STATE_BUSY;
                info->buf_count = 0x5;
                info->buf_start = 0;
                info->cac_size = info->buf_count;
               {
                   
                   unsigned char *data = (unsigned char *)(info->data_buff);
                   unsigned int id_val_l=0,id_val_h=0;
#ifdef  LS1B01                  
                   unsigned int timing = 0;
                   _NAND_READ_REG(NAND_TIMING_REG,timing);
                   _NAND_SET_REG(NAND_TIMING_REG,0x30f0); 
                   _NAND_SET_REG(NAND_CMD_REG,0x21); 

                   do id_val_h = _NAND_IDH;
                   while(((id_val_l |= _NAND_IDL) & 0xff)  == 0);

                   _NAND_SET_REG(NAND_TIMING_REG,timing);
#else
                   _NAND_SET_REG(NAND_CMD_REG,0x21); 
                   udelay(1000); 
                   id_val_l = _NAND_IDL;
                   id_val_h = _NAND_IDH;
#endif                   
                   data[0]  = (id_val_h & 0xff);
                   data[1]  = (id_val_l & 0xff000000)>>24;
                   data[2]  = (id_val_l & 0x00ff0000)>>16;
                   data[3]  = (id_val_l & 0x0000ff00)>>8;
                   data[4]  = (id_val_l & 0x000000ff);

               }
                complete(&info->cmd_complete);
                break;
            case NAND_CMD_ERASE2:
            case NAND_CMD_READ1:
                complete(&info->cmd_complete);
                break;
            case NAND_CMD_RNDOUT:
                 
                info->buf_start =  column ;
                break;
            default :
                printk(KERN_ERR "non-supported command.\n");
                complete(&info->cmd_complete);
		break;
        }
        wait_for_completion_timeout(&info->cmd_complete,timeout);
        if(info->cmd == NAND_CMD_READ0 || info->cmd == NAND_CMD_READOOB || info->cmd == NAND_CMD_READID){
            dma_cache_inv((unsigned long)(info->data_buff),info->cac_size);
        }
        info->state = STATE_READY;

}
static int ls1b_nand_detect(struct mtd_info *mtd)
{
        return (mtd->writesize != 1<<11 || mtd->oobsize != 1<<6);

}

static void ls1b_nand_init_info(struct ls1b_nand_info *info)
{


    *((volatile unsigned int *)NAND_PARAM_REG) = 0x400;
    info->timing_flag = 1;/*0:read; 1:write;*/
    info->num=0;
    info->size=0;
    info->cac_size = 0; 
    info->state = STATE_READY;
    info->cmd = -1;
    info->page_addr = -1;
    info->nand_addrl = 0x0;
    info->nand_addrh = 0x0;
    info->nand_timing = 0x4<<8 | 0x12;
    info->nand_op_num = 0x0;
    info->nand_cs_rdy_map = 0x88442200;
    info->nand_cmd = 0;

    info->dma_orderad = 0x0;
    info->dma_saddr = info->data_buff_phys;
    info->dma_daddr = DMA_ACCESS_ADDR;
    info->dma_length = 0x0;
    info->dma_step_length = 0x0;
    info->dma_step_times = 0x1;
    info->dma_cmd = 0x0;

    info->order_reg_addr = ORDER_REG_ADDR;
}
static int ls1b_nand_probe(struct platform_device *pdev)
{	
        struct ls1b_nand_platform_data *pdata;
	struct ls1b_nand_info *info;
	struct nand_chip *this;
	struct mtd_info *mtd;
	struct resource *r;
	int ret = 0, irq;
#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
#endif

	pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -ENODEV;
	}

	mtd = kzalloc(sizeof(struct mtd_info) + sizeof(struct ls1b_nand_info),
			GFP_KERNEL);
	if (!mtd) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	info = (struct ls1b_nand_info *)(&mtd[1]);
	info->pdev = pdev;

	this = &info->nand_chip;
	mtd->priv = info;
        info->drcmr_dat =(unsigned int ) dma_alloc_coherent(&pdev->dev, MAX_BUFF_SIZE,
				&info->drcmr_dat_phys, GFP_KERNEL);
        info->drcmr_dat = CAC_ADDR(info->drcmr_dat);
        info->dma_ask =(unsigned int ) dma_alloc_coherent(&pdev->dev, MAX_BUFF_SIZE,
				&info->dma_ask_phy, GFP_KERNEL);

        if(!info->drcmr_dat){
                dev_err(&pdev->dev,"fialed to allocate memory\n");
                return ENOMEM;
        }
        r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		ret = -ENODEV;
		goto fail_put_clk;
	}

	r = request_mem_region(r->start, r->end - r->start + 1, pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto fail_put_clk;
	}

	info->mmio_base = ioremap(r->start, r->end - r->start + 1);
	if (info->mmio_base == NULL) {
		dev_err(&pdev->dev, "ioremap() failed\n");
		ret = -ENODEV;
		goto fail_free_res;
	}
        ret = ls1b_nand_init_buff(info);
	if (ret)
		goto fail_free_io;
        
        irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}
	ret = request_irq(irq, ls1b_nand_irq, IRQF_DISABLED,pdev->name, info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto fail_free_buf;
	}

        ls1b_nand_init_mtd(mtd, info);

         ls1b_nand_init_info(info);
        platform_set_drvdata(pdev, mtd);   

        if (nand_scan(mtd, 1)) {
		dev_err(&pdev->dev, "failed to scan nand\n");
		ret = -ENXIO;
		goto fail_free_irq;
	}
        if(ls1b_nand_detect(mtd)){
                dev_err(&pdev->dev, "driver don't support the Flash!\n");
                ret = -ENXIO;
                goto fail_free_irq;
        }
#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
	mtd->name = "nand-flash";
	num_partitions = parse_mtd_partitions(mtd, part_probes,
					      &partitions, 0);
#endif
	if (num_partitions <= 0 )
		{
		  partitions = pdata->parts;
		  num_partitions = pdata->nr_parts;
			
		}
         return add_mtd_partitions(mtd, partitions , num_partitions);
#else
		 return add_mtd_device(mtd);
#endif
fail_free_irq:
	free_irq(13, info);
fail_free_buf:
    	dma_free_coherent(&pdev->dev, info->data_buff_size,info->data_buff, info->data_buff_phys);
fail_free_io:
	iounmap(info->mmio_base);
fail_free_res:
	release_mem_region(r->start, r->end - r->start + 1);
fail_put_clk:
fail_free_mtd:
	kfree(mtd);
	return ret;
}

static int ls1b_nand_remove(struct platform_device *pdev)
{
    	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct ls1b_nand_info *info = mtd->priv;

	platform_set_drvdata(pdev, NULL);

	del_mtd_device(mtd);
	del_mtd_partitions(mtd);
	free_irq(13, info);
        kfree(info->drcmr_dat);
        kfree(mtd);

	return 0;
}
static int ls1b_nand_suspend(struct platform_device *pdev)
{
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct ls1b_nand_info *info = mtd->priv;

	if (info->state != STATE_READY) {
		dev_err(&pdev->dev, "driver busy, state = %d\n", info->state);
		return -EAGAIN;
	}

	return 0;
}
static int ls1b_nand_resume(struct platform_device *pdev)
{
        return 0;
}
static struct platform_driver ls1b_nand_driver = {
	.driver = {
		.name	= "ls1b-nand",
                .owner	= THIS_MODULE,
	},
	.probe		= ls1b_nand_probe,
	.remove		= ls1b_nand_remove,
	.suspend	= ls1b_nand_suspend,
	.resume		= ls1b_nand_resume,
};

static int __init ls1b_nand_init(void)
{
    int ret = 0;

    ret = platform_driver_register(&ls1b_nand_driver);
    if(ret){
        printk(KERN_ERR "failed to register loongson_1g_nand_driver");
    }
    return ret;
}
static void __exit ls1b_nand_exit(void)
{
    platform_driver_unregister(&ls1b_nand_driver);
}
module_init(ls1b_nand_init);
module_exit(ls1b_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang Gangqiang <yanggangqiang@loongson.cn>");
MODULE_DESCRIPTION("Loongson_1g NAND controller driver");
