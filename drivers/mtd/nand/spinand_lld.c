/*
 * Copyright (c) 2003-2013 Broadcom Corporation
 *
 * Copyright (c) 2009-2010 Micron Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/spinand.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>

#define BUFSIZE (10 * 64 * 2048)
#define NAND_CMD_PARAM		0xec

/*
 * OOB area specification layout:  Total 32 available free bytes.
 */
#ifdef CONFIG_MTD_SPINAND_ONDIEECC
static int enable_hw_ecc;
static int enable_read_hw_ecc;
#endif
static struct nand_ecclayout spinand_oob_64 = {
	.eccbytes = 24,
	.eccpos = {
		 3, 4, 5, 6,7,
		17, 18, 19, 20, 21, 22,23,
		33, 34, 35, 36, 37, 38,
		49, 50, 51, 52, 53, 54, },
	.oobavail = 32,
	.oobfree = {
		{.offset = 8,
		.length = 8},
		{.offset = 24,
		.length = 8},
		{.offset = 40,
		.length = 8},
		{.offset = 56,
		.length = 8}, }
};

static struct nand_ecclayout spinand_oob_128 = {
	.eccbytes = 24,
	.eccpos = {
		   40, 41, 42,43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55,
		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 2,
		 .length = 38},
		{.offset = 64,
		 .length = 64},
        }
                
};
/*
 * spinand_cmd - to process a command to send to the SPI Nand
 * Description:
 *    Set up the command buffer to send to the SPI controller.
 *    The command buffer has to initized to 0
 */

static int spinand_cmd(struct spi_device *spi, struct spinand_cmd *cmd)
{
	struct spi_message message;
	struct spi_transfer x[4];
	char cmdbuf[16];
	u8 dummy = 0xff;
	int ret;

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	x[0].len = 1;
	x[0].tx_buf = cmdbuf;
	cmdbuf[0] = cmd->cmd;

	if (cmd->n_addr) {
		x[0].len += cmd->n_addr;
		memcpy(&cmdbuf[1], cmd->addr, cmd->n_addr);
	}
	spi_message_add_tail(&x[0], &message);

	if (cmd->n_dummy) {
		x[2].len = cmd->n_dummy;
		x[2].tx_buf = &dummy;
		spi_message_add_tail(&x[2], &message);
	}

	if (cmd->n_tx) {
		x[3].len = cmd->n_tx;
		x[3].tx_buf = cmd->tx_buf;
		spi_message_add_tail(&x[3], &message);
	}

	if (cmd->n_rx) {
		x[3].len = cmd->n_rx;
		x[3].rx_buf = cmd->rx_buf;
		spi_message_add_tail(&x[3], &message);
	}

	ret = spi_sync(spi, &message);

	return ret;
}

/*
 * spinand_read_id- Read SPI Nand ID
 * Description:
 *    Read ID: read two ID bytes from the SPI Nand device
 */
static int spinand_read_id(struct spinand_info *info, u8 *id)
{
	int retval;
	u8 nand_id[3];
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_READ_ID;
	cmd.n_rx = 3;
	cmd.rx_buf = &nand_id[0];

	retval = spinand_cmd(info->spi, &cmd);
	if (retval != 0) {
		printk("error %d reading id\n", retval);
		return retval;
	}

	if(nand_id[0] == 0xc8/* && nand_id[1] == 0xb4*/) {
		info->gd_ctype = 1;
		id[0] = nand_id[0];
		id[1] = nand_id[1];
		info->dev_id = id[1];
	} else {
		id[0] = nand_id[1];
		id[1] = nand_id[2];
		info->dev_id = id[1];
	}

	return 0;
}

/*
 * spinand_read_status- send command 0xf to the SPI Nand status register
 * Description:
 *    After read, write, or erase, the Nand device is expected to set the
 *    busy status.
 *    This function is to allow reading the status of the command: read,
 *    write, and erase.
 *    Once the status turns to be ready, the other status bits also are
 *    valid status bits.
 */
static int spinand_read_status(struct spi_device *spi_nand, uint8_t *status)
{
	struct spinand_cmd cmd = {0};
	int ret;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_STATUS;
	cmd.n_rx = 1;
	cmd.rx_buf = status;

	ret = spinand_cmd(spi_nand, &cmd);
	if (ret != 0) {
		dev_err(&spi_nand->dev, "err: %d read status register\n", ret);
		return ret;
	}

	return 0;
}

#define MAX_WAIT_JIFFIES  (40 * HZ)
static int wait_till_ready(struct spi_device *spi_nand)
{
	unsigned long deadline;
	int retval;
	u8 stat = 0;

	deadline = jiffies + MAX_WAIT_JIFFIES;
	do {
		retval = spinand_read_status(spi_nand, &stat);
		if (retval < 0)
			return -1;
		else if (!(stat & 0x1))
			break;

		cond_resched();
	} while (!time_after_eq(jiffies, deadline));

	if ((stat & 0x1) == 0)
		return 0;

	return -1;
}

static int spinand_reset(struct spi_device *spi_nand)
{
	int retval;
	u8 nand_id[2];
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_RESET;

	retval = spinand_cmd(spi_nand, &cmd);
	if (retval != 0) {
		dev_err(&spi_nand->dev, "error %d reading id\n", retval);
		return retval;
	}
	if (wait_till_ready(spi_nand))
		dev_err(&spi_nand->dev, "WAIT timedout!!!\n");

	return 0;
}
/**
 * spinand_get_otp- send command 0xf to read the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_get_otp(struct spi_device *spi_nand, u8 *otp)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_OTP;
	cmd.n_rx = 1;
	cmd.rx_buf = otp;

	retval = spinand_cmd(spi_nand, &cmd);
	if (retval != 0) {
		dev_err(&spi_nand->dev, "error %d get otp\n", retval);
		return retval;
	}
	return 0;
}

/**
 * spinand_set_otp- send command 0x1f to write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_set_otp(struct spi_device *spi_nand, u8 *otp)
{
	int retval;
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_WRITE_REG,
	cmd.n_addr = 1,
	cmd.addr[0] = REG_OTP,
	cmd.n_tx = 1,
	cmd.tx_buf = otp,

	retval = spinand_cmd(spi_nand, &cmd);
	if (retval != 0) {
		dev_err(&spi_nand->dev, "error %d set otp\n", retval);
		return retval;
	}
	return 0;
}
#if 1
/**
 * spinand_enable_ecc- send command 0x1f to write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_enable_ecc(struct spi_device *spi_nand)
{
	int retval;
	u8 otp = 0;

	retval = spinand_get_otp(spi_nand, &otp);

	if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
		return 0;
	} else {
		otp |= OTP_ECC_MASK;
		retval = spinand_set_otp(spi_nand, &otp);
		retval = spinand_get_otp(spi_nand, &otp);
		return retval;
	}
}
#endif

static int spinand_disable_ecc(struct spi_device *spi_nand)
{
	int retval;
	u8 otp = 0;

	retval = spinand_get_otp(spi_nand, &otp);

	if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
		otp &= ~OTP_ECC_MASK;
		retval = spinand_set_otp(spi_nand, &otp);
		retval = spinand_get_otp(spi_nand, &otp);
		return retval;
	} else
		return 0;
}

static inline int spinand_driver_strength(struct spi_device *spi_nand)
{
	struct spinand_cmd cmd = {0};
	int ret;
	u8 otp = 0,lock = 0x60;

	ret = spinand_get_otp(spi_nand, &otp);

	cmd.cmd = CMD_WRITE_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_STRENGTH;
	cmd.n_tx = 1;
	cmd.tx_buf = &lock;
	
	ret = spinand_cmd(spi_nand, &cmd);
	if (ret != 0) {
		printk("error %d driver strength\n", ret);
		return ret;
	}
	return ret;
}

/**
 * spinand_write_enable- send command 0x06 to enable write or erase the
 * Nand cells
 * Description:
 *   Before write and erase the Nand cells, the write enable has to be set.
 *   After the write or erase, the write enable bit is automatically
 *   cleared (status register bit 2)
 *   Set the bit 2 of the status register has the same effect
 */
static int spinand_write_enable(struct spi_device *spi_nand)
{
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_WR_ENABLE;
	return spinand_cmd(spi_nand, &cmd);
}

static int spinand_read_page_to_cache(struct spi_device *spi_nand, int page_id)
{
	struct spinand_cmd cmd = {0};
	int row;

	row = page_id;
	cmd.cmd = CMD_READ;
	cmd.n_addr = 3;
	cmd.addr[0] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[2] = (u8)(row & 0x00ff);

	return spinand_cmd(spi_nand, &cmd);
}

/*
 * spinand_read_from_cache- send command 0x03 to read out the data from the
 * cache register(2112 bytes max)
 * Description:
 *   The read can specify 1 to 2112 bytes of data read at the coresponded
 *   locations.
 *   No tRd delay.
 */
static int spinand_read_from_cache(struct spinand_info *info, u16 byte_id,
		u16 len, u8 *rbuf)
{
	struct spinand_cmd cmd = {0};
	u16 column;

	column = byte_id;
            /*the a_type device requires 4 wrap mode configure bits wrap[3:0]
             * wrap[3]  wrap[2]   wrap[1]   wrap[0]  wrap length
             * 0          0         x         x         2112
             * 0          1         x         x         2048
             * 1          0         x         x         64
             * 1          1         x         x         16*/
	if(info->gd_ctype != 1){
        if(byte_id > 0)
        {
            column |= 0x8000;
        }
        }
        /*if the device is c type ,the CMD_READ_RDM is 0x03,otherwise the CMD_READ_RDM maybe  0x0B*/
	cmd.cmd = CMD_READ_RDM;
	cmd.n_addr = 3;
	if(info->gd_ctype == 1) {
		cmd.addr[0] = (u8)(0xff);
		cmd.addr[1] = (u8)((column & 0xff00) >> 8);
		cmd.addr[2] = (u8)(column & 0x00fe);
	} else {
		cmd.addr[0] = (u8)((column & 0xff00) >> 8);
		cmd.addr[1] = (u8)(column & 0x00ff);
		cmd.addr[2] = (u8)(0xff);
	}
	cmd.n_dummy = 0;
	cmd.n_rx = len;
	cmd.rx_buf = rbuf;

	return spinand_cmd(info->spi, &cmd);
}

/*
 * spinand_read_page-to read a page with:
 * @page_id: the physical page number
 * @offset:  the location from 0 to 2111
 * @len:     number of bytes to read
 * @rbuf:    read buffer to hold @len bytes
 *
 * Description:
 *   The read icludes two commands to the Nand: 0x13 and 0x03 commands
 *   Poll to read status to wait for tRD time.
 */
static int spinand_read_page(struct spinand_info *info, int page_id,
		u16 offset, u16 len, u8 *rbuf)
{
	int ret;
	u8 status = 0;

#ifdef CONFIG_MTD_SPINAND_ONDIEECC
	if (enable_read_hw_ecc) {
		if (spinand_enable_ecc(info->spi))
			dev_err(&info->spi->dev, "enable HW ECC failed!");
	}
#endif
	ret = spinand_read_page_to_cache(info->spi, page_id);
	if (wait_till_ready(info->spi))
		dev_err(&info->spi->dev, "WAIT timedout!!!\n");

	while (1) {
		ret = spinand_read_status(info->spi, &status);
		if (ret < 0) {
			dev_err(&info->spi->dev,
					"err %d read status register\n", ret);
			memset(rbuf,0,len);
			return ret;
		}

		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_ECC_MASK) == STATUS_ECC_ERROR) {
				dev_err(&info->spi->dev, "ecc error, page=%x\n",
						page_id);
				ret = spinand_disable_ecc(info->spi);
				memset(rbuf,0,len);
				return 0;
			}
			break;
		}
	}

	ret = spinand_read_from_cache(info, offset, len, rbuf);
	if (ret != 0)
		dev_err(&info->spi->dev, "read from cache failed!!\n");

#ifdef CONFIG_MTD_SPINAND_ONDIEECC
	if (enable_read_hw_ecc) {
		ret = spinand_disable_ecc(info->spi);
		enable_read_hw_ecc = 0;
	}
#endif
	return 0;
}

/*
 * spinand_program_data_to_cache--to write a page to cache with:
 * @byte_id: the location to write to the cache
 * @len:     number of bytes to write
 * @rbuf:    read buffer to hold @len bytes
 *
 * Description:
 *   The write command used here is 0x84--indicating that the cache is
 *   not cleared first.
 *   Since it is writing the data to cache, there is no tPROG time.
 */
static int spinand_program_data_to_cache(struct spi_device *spi_nand,
		u16 byte_id, u16 len, u8 *wbuf)
{
	struct spinand_cmd cmd = {0};
	u16 column;

	column = byte_id;
	cmd.cmd = CMD_PROG_PAGE_CLRCACHE;
	cmd.n_addr = 2;
	cmd.addr[0] = (u8)((column & 0xff00) >> 8);
	cmd.addr[1] = (u8)(column & 0x00ff);
	cmd.n_tx = len;
	cmd.tx_buf = wbuf;

	return spinand_cmd(spi_nand, &cmd);
}

/**
 * spinand_program_execute--to write a page from cache to the Nand array with
 * @page_id: the physical page location to write the page.
 *
 * Description:
 *   The write command used here is 0x10--indicating the cache is writing to
 *   the Nand array.
 *   Need to wait for tPROG time to finish the transaction.
 */
static int spinand_program_execute(struct spi_device *spi_nand, int page_id)
{
	struct spinand_cmd cmd = {0};
	int row;

	row = page_id;
	cmd.cmd = CMD_PROG_PAGE_EXC;
	cmd.n_addr = 3;
	cmd.addr[0] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[2] = (u8)(row & 0x00ff);

	return spinand_cmd(spi_nand, &cmd);
}

/**
 * spinand_program_page--to write a page with:
 * @page_id: the physical page location to write the page.
 * @offset:  the location from the cache starting from 0 to 2111
 * @len:     the number of bytes to write
 * @wbuf:    the buffer to hold the number of bytes
 *
 * Description:
 *   The commands used here are 0x06, 0x84, and 0x10--indicating that
 *   the write enable is first
 *   sent, the write cache command, and the write execute command
 *   Poll to wait for the tPROG time to finish the transaction.
 */
static int spinand_program_page(struct spinand_info *info,
		int page_id, u16 offset, u16 len, u8 *buf)
{
	int retval;
	u8 status = 0;
	uint8_t *wbuf;
#ifdef CONFIG_MTD_SPINAND_ONDIEECC
	unsigned int i, j;

	enable_read_hw_ecc = 0;
	wbuf = devm_kzalloc(&info->spi->dev, 2112, GFP_KERNEL);
	spinand_read_page(info, page_id, 0, 2112, wbuf);

	for (i = offset, j = 0; i < len; i++, j++)
		wbuf[i] &= buf[j];

	if (enable_hw_ecc)
		retval = spinand_enable_ecc(info->spi);
#else
	wbuf = buf;
#endif
	retval = spinand_write_enable(info->spi);
	if (wait_till_ready(info->spi))
		dev_err(&info->spi->dev, "wait timedout!!!\n");

	retval = spinand_program_data_to_cache(info->spi, offset, len, wbuf);
	retval = spinand_program_execute(info->spi, page_id);
	while (1) {
		retval = spinand_read_status(info->spi, &status);
		if (retval < 0) {
			dev_err(&info->spi->dev,
					"error %d reading status register\n",
					retval);
			return retval;
		}

		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_P_FAIL_MASK) == STATUS_P_FAIL) {
				dev_err(&info->spi->dev,
					"program error, page %d\n", page_id);
				return -1;
			} else
				break;
		}
	}
#ifdef CONFIG_MTD_SPINAND_ONDIEECC
	if (enable_hw_ecc) {
		retval = spinand_disable_ecc(info->spi);
		enable_hw_ecc = 0;
	}
#endif

	return 0;
}

/**
 * spinand_erase_block_erase--to erase a page with:
 * @block_id: the physical block location to erase.
 *
 * Description:
 *   The command used here is 0xd8--indicating an erase command to erase
 *   one block--64 pages
 *   Need to wait for tERS.
 */
static int spinand_erase_block_erase(struct spi_device *spi_nand, int block_id)
{
	struct spinand_cmd cmd = {0};
	int row;

	row = block_id;
	cmd.cmd = CMD_ERASE_BLK;
	cmd.n_addr = 3;
	cmd.addr[0] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[2] = (u8)(row & 0x00ff);

	return spinand_cmd(spi_nand, &cmd);
}

/**
 * spinand_erase_block--to erase a page with:
 * @block_id: the physical block location to erase.
 *
 * Description:
 *   The commands used here are 0x06 and 0xd8--indicating an erase
 *   command to erase one block--64 pages
 *   It will first to enable the write enable bit (0x06 command),
 *   and then send the 0xd8 erase command
 *   Poll to wait for the tERS time to complete the tranaction.
 */
static int spinand_erase_block(struct spi_device *spi_nand, int block_id)
{
	int retval;
	u8 status = 0;

	retval = spinand_write_enable(spi_nand);
	if (wait_till_ready(spi_nand))
		dev_err(&spi_nand->dev, "wait timedout!!!\n");

	retval = spinand_erase_block_erase(spi_nand, block_id);
	while (1) {
		retval = spinand_read_status(spi_nand, &status);
		if (retval < 0) {
			dev_err(&spi_nand->dev,
					"error %d reading status register\n",
					(int) retval);
			return retval;
		}

		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_E_FAIL_MASK) == STATUS_E_FAIL) {
				dev_err(&spi_nand->dev,
					"erase error, block %d\n", block_id);
				return -1;
			} else
				break;
		}
	}
	return 0;
}

#ifdef CONFIG_MTD_SPINAND_ONDIEECC
static int spinand_write_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	const uint8_t *p = buf;
	int eccsize = chip->ecc.size;
	int eccsteps = chip->ecc.steps;

	enable_hw_ecc = 1;
	chip->write_buf(mtd, p, eccsize * eccsteps);
	return 0;
}

static int spinand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
		uint8_t *buf, int oob_required, int page)
{
	u8 retval, status;
	uint8_t *p = buf;
	int eccsize = chip->ecc.size;
	int eccsteps = chip->ecc.steps;
	struct spinand_info *info = (struct spinand_info *)chip->priv;

	enable_read_hw_ecc = 1;

	chip->read_buf(mtd, p, eccsize * eccsteps);
	if (oob_required)
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	while (1) {
		retval = spinand_read_status(info->spi, &status);
		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_ECC_MASK) == STATUS_ECC_ERROR) {
				pr_info("spinand: ECC error\n");
				mtd->ecc_stats.failed++;
			} else if ((status & STATUS_ECC_MASK) ==
					STATUS_ECC_1BIT_CORRECTED)
				mtd->ecc_stats.corrected++;
			break;
		}
	}
	return 0;

}
#endif

static void spinand_select_chip(struct mtd_info *mtd, int dev)
{
}

static uint8_t spinand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct spinand_info *info = (struct spinand_info *)chip->priv;
	struct nand_state *state = (struct nand_state *)info->priv;
	u8 data;

	data = state->buf[state->buf_ptr];
	state->buf_ptr++;
	return data;
}

static int spinand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct spinand_info *info = (struct spinand_info *)chip->priv;

	unsigned long timeo = jiffies;
	int retval, state = chip->state;
	u8 status;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	while (time_before(jiffies, timeo)) {
		retval = spinand_read_status(info->spi, &status);
		if ((status & STATUS_OIP_MASK) == STATUS_READY)
			return 0;

		cond_resched();
	}
	return 0;
}

static void spinand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct spinand_info *info = (struct spinand_info *)chip->priv;
	struct nand_state *state = (struct nand_state *)info->priv;

	memcpy(state->buf+state->buf_ptr, buf, len);
	state->buf_ptr += len;
}

static void spinand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct spinand_info *info = (struct spinand_info *)chip->priv;
	struct nand_state *state = (struct nand_state *)info->priv;

	memcpy(buf, state->buf+state->buf_ptr, len);
	state->buf_ptr += len;
}

static void spinand_cmdfunc(struct mtd_info *mtd, unsigned int command,
		int column, int page)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct spinand_info *info = (struct spinand_info *)chip->priv;
	struct nand_state *state = (struct nand_state *)info->priv;

	switch (command) {
	/*
	 * READ0 - read in first  0x800 bytes
	 */
	case NAND_CMD_READ1:
	case NAND_CMD_READ0:
		state->buf_ptr = 0;
//		spinand_read_page(info, page, 0x0, 0x840, state->buf);
		spinand_read_page(info, page, 0x0, mtd->oobsize + mtd->writesize, state->buf);
		break;
	/* READOOB reads only the OOB because no ECC is performed. */
	case NAND_CMD_READOOB:
		state->buf_ptr = 0;
#if 0
		if(info->gd_ctype == 0)
		{
			if(page == 0xffc0 || page == 0x6440)
			{
				memset(state->buf,0,mtd->oobsize);
				break;
			}
		}
#endif
		spinand_read_page(info, page, mtd->writesize, mtd->oobsize, state->buf);
		break;
	case NAND_CMD_RNDOUT:
		state->buf_ptr = column;
		break;
	case NAND_CMD_READID:
		state->buf_ptr = 0;
		spinand_read_id(info, (u8 *)state->buf);
		break;
	case NAND_CMD_PARAM:
		state->buf_ptr = 0;
		break;
	/* ERASE1 stores the block and page address */
	case NAND_CMD_ERASE1:
		spinand_erase_block(info->spi, page);
		break;
	/* ERASE2 uses the block and page address from ERASE1 */
	case NAND_CMD_ERASE2:
		break;
	/* SEQIN sets up the addr buffer and all registers except the length */
	case NAND_CMD_SEQIN:
		state->col = column;
		state->row = page;
		state->buf_ptr = 0;
		break;
	/* PAGEPROG reuses all of the setup from SEQIN and adds the length */
	case NAND_CMD_PAGEPROG:
		spinand_program_page(info, state->row, state->col,
				state->buf_ptr, state->buf);
		break;
	case NAND_CMD_STATUS:
		spinand_get_otp(info->spi, state->buf);
		if (!(state->buf[0] & 0x80))
			state->buf[0] = 0x80;
		state->buf_ptr = 0;
		break;
	/* RESET command */
	case NAND_CMD_RESET:
		spinand_reset(info->spi);
		break;
	default:
		dev_err(&mtd->dev, "Unknown CMD: 0x%x\n", command);
	}
}

/**
 * spinand_lock_block- send write register 0x1f command to the Nand device
 *
 * Description:
 *    After power up, all the Nand blocks are locked.  This function allows
 *    one to unlock the blocks, and so it can be wriiten or erased.
 */
static int spinand_lock_block(struct spi_device *spi_nand, u8 lock)
{
	struct spinand_cmd cmd = {0};
	int ret;
	u8 otp = 0;

	ret = spinand_get_otp(spi_nand, &otp);

	cmd.cmd = CMD_WRITE_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_BLOCK_LOCK;
	cmd.n_tx = 1;
	cmd.tx_buf = &lock;

	ret = spinand_cmd(spi_nand, &cmd);
	if (ret != 0) {
		dev_err(&spi_nand->dev, "error %d lock block\n", ret);
		return ret;
	}
	return 0;
}

static int ls2h_nand_ecc_calculate(struct mtd_info *mtd,
				   const uint8_t * dat, uint8_t * ecc_code)
{
	return 0;
}

static int ls2h_nand_ecc_correct(struct mtd_info *mtd,
				 uint8_t * dat, uint8_t * read_ecc,
				 uint8_t * calc_ecc)
{
	/*
	 * Any error include ERR_SEND_CMD, ERR_DBERR, ERR_BUSERR, we
	 * consider it as a ecc error which will tell the caller the
	 * read fail We have distinguish all the errors, but the
	 * nand_read_ecc only check this function return value
	 */
	return 0;
}

static void ls2h_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	return;
}

/*
 * spinand_probe - [spinand Interface]
 * @spi_nand: registered device driver.
 *
 * Description:
 *   To set up the device driver parameters to make the device available.
 */
static int spinand_probe(struct spi_device *spi_nand)
{
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct spinand_info *info;
	struct nand_state *state;
	u8 spi_flash_id[3];
#if 0
	struct mtd_part_parser_data ppdata;
	int ret;
#else
	struct spi_nand_platform_data *plat = spi_nand->dev.platform_data;
#ifdef CONFIG_MTD_CMDLINE_PARTS
	const char *part_probes[] = { "cmdlinepart", NULL };
#endif
	struct mtd_partition *partitions = NULL;
	int nr_parts = 0;
	int ret;
#endif


	info  = devm_kzalloc(&spi_nand->dev, sizeof(struct spinand_info),
			GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->spi = spi_nand;

	spinand_reset(spi_nand);
	spinand_lock_block(spi_nand, BL_ALL_UNLOCKED);

	state = devm_kzalloc(&spi_nand->dev, sizeof(struct nand_state),
			GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	info->priv	= state;
	state->buf_ptr	= 0;
	state->buf	= devm_kzalloc(&spi_nand->dev, BUFSIZE, GFP_KERNEL);
	if (!state->buf)
		return -ENOMEM;

	chip = devm_kzalloc(&spi_nand->dev, sizeof(struct nand_chip),
			GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

#ifdef CONFIG_MTD_SPINAND_ONDIEECC
	chip->ecc.mode	= NAND_ECC_HW;
	chip->ecc.size	= 0x200;
	chip->ecc.bytes	= 0x6;
	chip->ecc.steps	= 0x4;

#if 0
	chip->ecc.strength = 1;
#endif
	chip->ecc.total	= chip->ecc.steps * chip->ecc.bytes;
	chip->ecc.layout = &spinand_oob_64;
	chip->ecc.read_page = spinand_read_page_hwecc;
	chip->ecc.write_page = spinand_write_page_hwecc;
#else
#if 0
	chip->ecc.mode	= NAND_ECC_SOFT;
	ret = spinand_disable_ecc(spi_nand);
#else
	chip->ecc.mode	= NAND_ECC_NONE;
	ret = spinand_enable_ecc(spi_nand);
#endif
#endif

	spinand_read_id(info, spi_flash_id);
	if(info->gd_ctype == 1) {
		spinand_driver_strength(info->spi);
		chip->ecc.layout = &spinand_oob_128;
		chip->ecc.size = 256;
		chip->ecc.bytes = 3;
	} else {
		chip->ecc.layout = &spinand_oob_64;
		chip->ecc.size = 256;
		chip->ecc.bytes = 3;
	}

	chip->priv	= info;
	chip->read_buf	= spinand_read_buf;
	chip->write_buf	= spinand_write_buf;
	chip->read_byte	= spinand_read_byte;
	chip->cmdfunc	= spinand_cmdfunc;
	chip->waitfunc	= spinand_wait;
	chip->options	|= NAND_CACHEPRG;
	chip->select_chip = spinand_select_chip;
#if 0
	chip->ecc.hwctl = ls2h_nand_ecc_hwctl;
	chip->ecc.calculate = ls2h_nand_ecc_calculate;
	chip->ecc.correct = ls2h_nand_ecc_correct;
#endif
	mtd = devm_kzalloc(&spi_nand->dev, sizeof(struct mtd_info), GFP_KERNEL);
	if (!mtd)
		return -ENOMEM;

	dev_set_drvdata(&spi_nand->dev, mtd);

	mtd->priv = chip;
	mtd->name = "spinand_flash";

	if(0xd4 == spi_flash_id[1])
		mtd->oobsize = 256;
	else if(0xdc == spi_flash_id[1])
		mtd->oobsize = 224;
	else if(info->gd_ctype == 1)
		mtd->oobsize = 128;
	else
		mtd->oobsize = 64;

	if (nand_scan(mtd, 1))
		return -1;

#if 0
	ppdata.of_node = spi_nand->dev.of_node;
	ret = mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
	if (!ret)
		return ret;

	return ret;
#endif
#ifdef CONFIG_MTD_CMDLINE_PARTS
	nr_parts = parse_mtd_partitions(mtd, part_probes,
					      &partitions, 0);
#endif
	if (nr_parts <= 0 )
	{
		partitions = plat->parts;
		nr_parts = plat->nr_parts;
		
	}
	return add_mtd_partitions(mtd, partitions, nr_parts);
}

/**
 * spinand_remove: Remove the device driver
 * @spi: the spi device.
 *
 * Description:
 *   To remove the device driver parameters and free up allocated memories.
 */
static int spinand_remove(struct spi_device *spi)
{
	struct mtd_info *mtd;
	struct nand_chip *chip = mtd->priv;
	struct spinand_info *info = chip->priv;
	struct nand_state *state = info->priv;

	mtd = dev_get_drvdata(&spi->dev);

#if 0
	mtd_device_unregister(mtd);
#else

	dev_set_drvdata(&spi->dev, NULL);

	del_mtd_device(mtd);
	del_mtd_partitions(mtd);

	kfree((void *)state->buf);
	kfree(state);
	kfree(info);
	kfree(chip);
	kfree(mtd);

#endif

	return 0;
}

static const struct of_device_id spinand_dt[] = {
	{ .compatible = "spinand,mt29f", },
};

/**
 * Device name structure description
 */
static struct spi_driver spinand_driver = {
	.driver = {
		.name		= "mt29f",
		.bus		= &spi_bus_type,
		.owner		= THIS_MODULE,
#if 0
		.of_match_table	= spinand_dt,
#endif
	},
	.probe		= spinand_probe,
	.remove		= spinand_remove,
};

/**
 * Device driver registration
 */
static int __init spinand_init(void)
{
	return spi_register_driver(&spinand_driver);
}

/**
 * unregister Device driver.
 */
static void __exit spinand_exit(void)
{
	spi_unregister_driver(&spinand_driver);
}
module_init(spinand_init);
module_exit(spinand_exit);

MODULE_DESCRIPTION("SPI NAND driver code");
MODULE_AUTHOR("Henry Pan <hspan@micron.com>, Kamlakant Patel <kamlakant.patel@broadcom.com>");
MODULE_LICENSE("GPL");
