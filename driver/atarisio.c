/*
   atarisio V1.07
   a kernel module for handling the Atari 8bit SIO protocol

   Copyright (C) 2002-2019 Matthias Reichl <hias@horus.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#define ATARISIO_USE_HRTIMER
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#endif

#include <linux/fs.h>
#include <linux/serial.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <linux/sched/signal.h>
#endif

#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/smp_lock.h>
#define my_lock_kernel() lock_kernel()
#define my_unlock_kernel() unlock_kernel()
#else
#define my_lock_kernel()	do { } while (0)
#define my_unlock_kernel()	do { } while (0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
#include <linux/device.h>
#include <linux/printk.h>
#endif

#include <linux/poll.h>
#include <linux/serial_reg.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <asm/system.h>
#endif
#include <asm/io.h>
#include <asm/ioctls.h>

#include "atarisio.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23)
#define irqreturn_t void
#define IRQ_RETVAL(foo)
#endif

/* additional definitions that are not present in 2.2 kernels */
#ifndef UART_EFR
#define UART_EFR	0x02    /* Enhanced Feature Register */
#endif

#ifndef UART_ICR
#define UART_ICR	0x05    /* Index Control Register */
#endif

#ifndef UART_ACR
#define UART_ACR	0x00    /* Additional Control Register */
#endif

#ifndef UART_ACR_ICRRD
#define UART_ACR_ICRRD	0x40    /* ICR Read enable */
#endif

#ifndef UART_CPR
#define UART_CPR        0x01    /* Clock Prescalar Register */
#endif

#ifndef UART_TCR
#define UART_TCR        0x02    /* Times Clock Register */
#endif

#ifndef UART_CKS
#define UART_CKS        0x03    /* Clock Select Register */
#endif


#ifndef UART_TTL
#define UART_TTL        0x04    /* Transmitter Interrupt Trigger Level */
#endif 

#ifndef UART_RTL
#define UART_RTL        0x05    /* Receiver Interrupt Trigger Level */
#endif

#ifndef UART_FCL
#define UART_FCL        0x06    /* Flow Control Level Lower */
#endif

#ifndef UART_FCH
#define UART_FCH        0x07    /* Flow Control Level Higher */
#endif

#ifndef UART_ID1
#define UART_ID1        0x08    /* ID #1 */
#endif

#ifndef UART_ID2
#define UART_ID2        0x09    /* ID #2 */
#endif

#ifndef UART_ID3
#define UART_ID3        0x0A    /* ID #3 */
#endif

#ifndef UART_REV
#define UART_REV        0x0B    /* Revision */
#endif

#ifndef UART_CSR
#define UART_CSR        0x0C    /* Channel Software Reset */
#endif

#ifndef UART_MCR_CLKSEL
#define UART_MCR_CLKSEL         0x80 /* Divide clock by 4 (TI16C752, EFR[4]=1) */
#endif

#ifndef __iomem
#define __iomem
#endif

/* debug levels: */
#define DEBUG_STANDARD 1
#define DEBUG_NOISY 2
#define DEBUG_VERY_NOISY 3

/*
#define ATARISIO_DEBUG_TIMING
*/

/*
#define ATARISIO_PRINT_TIMESTAMPS
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#define PRINTK_NODEV(x...) printk(NAME ": " x)
#define PRINTK(x...) do { \
		printk(NAME "%d: ", dev->id); \
		printk(x); \
	} while(0)
#else
#define PRINTK_NODEV(x...) pr_info(NAME ": " x)
#define PRINTK(x...) dev_info(dev->miscdev->this_device, x)
#endif

#define IRQ_PRINTK(level, x...) \
	if (debug_irq >= level) { \
		PRINTK(x); \
	}

#define DBG_PRINTK(level, x...) \
	if (debug >= level) { \
		PRINTK(x); \
	}

#define DBG_PRINTK_NODEV(level, x...) \
	if (debug >= level) { \
		PRINTK_NODEV(x); \
	}

#ifdef ATARISIO_DEBUG_TIMING
#define PRINT_TIMESTAMP(x...) PRINTK_TIMESTAMP(x)
#else
#define PRINT_TIMESTAMP(x...) do { } while(0)
#endif

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Matthias Reichl <hias@horus.com>");
#endif
#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("Serial Atari 8bit SIO driver");
#endif

/* 
 * currently we use major 10, minor 240, which is a reserved local
 * number within the miscdevice block
 */

#define ATARISIO_DEFAULT_MINOR 240

/* maximum number of devices */
#define ATARISIO_MAXDEV 4

#define NAME "atarisio"

/*
 * module parameters:
 * io:  the base address of the 16550
 * irq: interrupt number
 */
static int minor = ATARISIO_DEFAULT_MINOR;
static char* port[ATARISIO_MAXDEV]       = { [0 ... ATARISIO_MAXDEV-1] = 0 };
static int   io[ATARISIO_MAXDEV]         = { [0 ... ATARISIO_MAXDEV-1] = 0 };
static long  mmio[ATARISIO_MAXDEV]	 = { [0 ... ATARISIO_MAXDEV-1] = 0 };
static int   irq[ATARISIO_MAXDEV]        = { [0 ... ATARISIO_MAXDEV-1] = 0 };
static int   baud_base[ATARISIO_MAXDEV]  = { [0 ... ATARISIO_MAXDEV-1] = 0 };
static int   ext_16c950[ATARISIO_MAXDEV] = { [0 ... ATARISIO_MAXDEV-1] = 1 };
static int   debug = 0;
static int   debug_irq = 0;
#ifdef ATARISIO_USE_HRTIMER
static int   hrtimer = 1;
#else
static int   hrtimer = 0;
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
MODULE_PARM(minor,"i");
MODULE_PARM(port,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "s");
MODULE_PARM(io,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "i");
MODULE_PARM(mmio,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "l");
MODULE_PARM(irq,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "i");
MODULE_PARM(baud_base,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "i");
MODULE_PARM(ext_16c950,"1-" __MODULE_STRING(ATARISIO_MAXDEV) "i");
MODULE_PARM(debug,"i");
MODULE_PARM(debug_irq,"i");
MODULE_PARM(hrtimer,"i");
#else
module_param(minor, int, S_IRUGO);
module_param_array(port, charp, 0, S_IRUGO);
module_param_array(io, int, 0, S_IRUGO);
module_param_array(mmio, long, 0, S_IRUGO);
module_param_array(irq, int, 0, S_IRUGO);
module_param_array(baud_base, int, 0, S_IRUGO);
module_param_array(ext_16c950, int, 0, S_IRUGO);
module_param(debug, int, S_IRUGO | S_IWUSR);
module_param(debug_irq, int, S_IRUGO | S_IWUSR);
module_param(hrtimer, int, S_IRUGO | S_IWUSR);
#endif

#ifdef MODULE_PARM_DESC
MODULE_PARM_DESC(port,"serial port (eg /dev/ttyS0, default: use supplied io/irq values)");
MODULE_PARM_DESC(io,"io address of 16550 UART (eg 0x3f8)");
MODULE_PARM_DESC(mmio,"mmio address of 16550 UART (eg 0xea401000)");
MODULE_PARM_DESC(irq,"irq of 16550 UART (eg 4)");
MODULE_PARM_DESC(baud_base,"base baudrate (default: 115200)");
MODULE_PARM_DESC(ext_16c950,"use extended 16C950 features (default: 1)");
MODULE_PARM_DESC(minor,"minor device number (default: 240)");
MODULE_PARM_DESC(debug,"debug level (default: 0)");
MODULE_PARM_DESC(debug_irq,"interrupt debug level (default: 0)");
MODULE_PARM_DESC(hrtimer,"use high resolution timers (default: 1, if available)");
#endif

/*
 * constants of the SIO protocol
 */

#define MAX_COMMAND_FRAME_RETRIES 13
#define COMMAND_FRAME_ACK_CHAR 0x41
#define COMMAND_FRAME_NAK_CHAR 0x4e

#define OPERATION_COMPLETE_CHAR 0x43
#define OPERATION_ERROR_CHAR 0x45
#define DATA_FRAME_ACK_CHAR 0x41
#define DATA_FRAME_NAK_CHAR 0x4e


/*
 * delays, according to the SIO specs, in uSecs
 */

#define DELAY_T0 900
#define DELAY_T1 850

/* BiboDos needs at least 50us delay before ACK */
#define DELAY_T2_MIN 100
#define DELAY_T2_MAX 20000
#define DELAY_T3_MIN 1000
#define DELAY_T3_MAX 1600
#define DELAY_T4_MIN 450
#define DELAY_T4_MAX 16000

/* the QMEG OS needs at least 300usec delay between ACK and complete */
/* #define DELAY_T5_MIN 250 */
#define DELAY_T5_MIN 300
#define DELAY_T5_MIN_SLOW 1000

/* QMEG OS 3 needs a delay of max. 150usec between complete and data */
#define DELAY_T3_PERIPH 150

/* this one is in mSecs, one jiffy (10 mSec on i386) is minimum,
 * try higher values for slower machines. The default of 50 mSecs
 * should be sufficient for most PCs
 */
#define DELAY_RECEIVE_HEADROOM 50
#define DELAY_SEND_HEADROOM 50

#define MODE_1050_2_PC 0
#define MODE_SIOSERVER 1

/*
 * IO buffers for the intterupt handlers
 */

#define IOBUF_LENGTH 8200
/*#define IOBUF_LENGTH 300*/

#define MAX_SIO_DATA_LENGTH (IOBUF_LENGTH-2)
/* circular buffers can hold IOBUF_LENGTH-1 chars, reserve one further
 * char for the checksum
 */


/*
 * send mode: don't wait at all, wait for buffer empty or wait for fifo/thre empty
 */
#define SEND_MODE_NOWAIT 0
#define SEND_MODE_WAIT_BUFFER 1
#define SEND_MODE_WAIT_ALL 2

/* device state information */
struct atarisio_dev {
	int busy; /* =0; */
	int id;
	char* devname;
	int use_mmio;
	int io;
	size_t mapbase;
	void __iomem *membase;
	int irq;
	char* port; /* = 0 */

	void (*serial_out)(struct atarisio_dev*, unsigned int offset, uint8_t value);
	uint8_t (*serial_in)(struct atarisio_dev*, unsigned int offset);

	/* flag if the UART is a 16C950, not a 16C550 */
	int is_16c950;

	/* flag if extended 16C950 functions should be used */
	int use_16c950_mode;

	/* global data structure and UART lock */
	spinlock_t lock;

	volatile int current_mode; /* = MODE_1050_2_PC; */

	/* timestamp at the very beginning of the IRQ */
	uint64_t irq_timestamp;
	
	/* value of modem status register at the last interrupt call */
	uint8_t last_msr; /* =0; */

	int enable_timestamp_recording; /* = 0; */
	int got_send_irq_timestamp;
	SIO_timestamps timestamps;

	/* receive buffer */
	volatile struct rx_buf_struct {
		uint8_t buf[IOBUF_LENGTH];
		unsigned int head;
		unsigned int tail;
		int wakeup_len;
	} rx_buf;

	/* transmit buffer */
	volatile struct tx_buf_struct {
		uint8_t buf[IOBUF_LENGTH];
		unsigned int head;
		unsigned int tail;
	} tx_buf;

	unsigned int current_cmdframe_serial_number;

	/* command frame buffer */
	volatile struct cmdframe_buf_struct {
		uint8_t buf[5];
		unsigned int is_valid;
		unsigned int receiving;
		unsigned int pos;

		unsigned int ok_chars; /* correctly received chars */
		unsigned int error_chars; /* received chars with (i.e. framing) errors */
		unsigned int break_chars; /* received breaks */

		uint64_t start_reception_time; /* in usec */
		uint64_t end_reception_time; /* in usec */

		unsigned int serial_number;
		unsigned int missed_count;
	} cmdframe_buf;

	/*
	 * wait queues
	 */

	wait_queue_head_t rx_queue;
	wait_queue_head_t tx_queue;
	wait_queue_head_t cmdframe_queue;

	/*
	 * configuration of the serial port
	 */

	volatile struct serial_config_struct {
		unsigned int baudrate;
		unsigned int exact_baudrate;
		unsigned long baud_base;
		uint8_t IER;
		uint8_t MCR;
		uint8_t LCR;
		uint16_t DL;  /* divisor */
		uint8_t FCR;
		uint8_t ACR;	/* for 16C950 UARTs */
		uint8_t TCR;
		uint8_t CPR;

		unsigned int just_switched_baud; /* true if we just switched baud rates */
		/* this flag is cleared after successful reception of a command frame */
	} serial_config;

	/* 
	 * configuration for SIOSERVER mode
	 */

	uint8_t sioserver_command_line; /* = UART_MSR_RI; */
	uint8_t sioserver_command_line_delta; /* = UART_MSR_TERI; */

	/* 
	 * configuration for 1050-2-PC mode
	 */

	/*
	 * The command line has to be set to LOW during the
	 * transmission of a command frame and HI otherwise
	 */
	uint8_t command_line_mask; /* = ~UART_MCR_RTS; */
	uint8_t command_line_low; /* = UART_MCR_RTS; */
	uint8_t command_line_high; /* = 0; */

	unsigned int default_baudrate; /* = ATARISIO_STANDARD_BAUDRATE; */
	unsigned int highspeed_baudrate; /* = ATARISIO_HIGHSPEED_BAUDRATE; */

	int do_autobaud; /* = 0; */
	int add_highspeedpause; /* = 0; */
	unsigned int tape_baudrate;

	/* new tape mode using start tape block, send raw frame nowait and end tape block */
	unsigned int tape_old_baudrate;
	int tape_old_autobaud;
	int tape_old_ier;
	int tape_mode_enabled; /* = 0; */

	uint8_t standard_lcr; /* = UART_LCR_WLEN8; */
	uint8_t slow_lcr; /* = UART_LCR_WLEN8 | UART_LCR_STOP | UART_LCR_PARITY | UART_LCR_SPAR; */

	/* serial port state */
	struct serial_struct orig_serial_struct;
	int need_reenable; /* = 0; */

	struct miscdevice* miscdev;
};

static struct atarisio_dev* atarisio_devices[ATARISIO_MAXDEV];

/*
 * helper functions to access the 16550
 */
static void io_serial_out(struct atarisio_dev* dev, unsigned int offset, uint8_t value)
{
	if (offset >=8) {
		DBG_PRINTK(DEBUG_STANDARD, "illegal offset in io_serial_out\n");
	} else {
		outb(value, dev->io+offset);
	}
}

static uint8_t io_serial_in(struct atarisio_dev* dev, unsigned int offset)
{
	if (offset >=8) {
		DBG_PRINTK(DEBUG_STANDARD, "illegal offset in io_serial_in\n");
		return 0;
	} else {
		return inb(dev->io+offset);
	}
}

static void mem_serial_out(struct atarisio_dev* dev, unsigned int offset, uint8_t value)
{
	if (offset >=8) {
		DBG_PRINTK(DEBUG_STANDARD, "illegal offset in mem_serial_out\n");
	} else {
		writeb(value, dev->membase+offset);
	}
}

static uint8_t mem_serial_in(struct atarisio_dev* dev, unsigned int offset)
{
	if (offset >=8) {
		DBG_PRINTK(DEBUG_STANDARD, "illegal offset in mem_serial_in\n");
		return 0;
	} else {
		return readb(dev->membase+offset);
	}
}

static void set_lcr(struct atarisio_dev* dev, uint8_t lcr)
{
	dev->serial_config.LCR = lcr;
	dev->serial_out(dev, UART_LCR, dev->serial_config.LCR);
}

static void set_dl(struct atarisio_dev* dev, uint16_t dl)
{
	dev->serial_config.DL = dl;
	dev->serial_out(dev, UART_LCR, dev->serial_config.LCR | UART_LCR_DLAB);
	dev->serial_out(dev, UART_DLL, dev->serial_config.DL & 0xff);
	dev->serial_out(dev, UART_DLM, dev->serial_config.DL >> 8);
	dev->serial_out(dev, UART_LCR, dev->serial_config.LCR);
}

/*
 * helper functions for additional 16c950 registers
 * according to the 16c950 application notes
 * http://www.oxsemi.com/products/serial/documents/HighPerformance_UARTs/OX16C950B/SER_AN_SWexamples.pdf
 */

static inline void write_icr(struct atarisio_dev* dev, uint8_t index, uint8_t value)
{
	dev->serial_out(dev, UART_SCR, index);
	dev->serial_out(dev, UART_ICR, value);
}

static inline uint8_t read_icr(struct atarisio_dev* dev, uint8_t index)
{
	uint8_t ret;

	/* first enable read access to the ICRs */
	write_icr(dev, UART_ACR, dev->serial_config.ACR | UART_ACR_ICRRD);

	/* access the ICR register */
	dev->serial_out(dev, UART_SCR, index);
	ret = dev->serial_in(dev, UART_ICR);

	/* disable read access to the ICRs */
	write_icr(dev, UART_ACR, dev->serial_config.ACR);

	return ret;
}

static void set_tcr(struct atarisio_dev* dev, uint8_t tcr)
{
	dev->serial_config.TCR = tcr;
	write_icr(dev, UART_TCR, dev->serial_config.TCR);
}

static void set_cpr(struct atarisio_dev* dev, uint8_t cpr)
{
	dev->serial_config.CPR = cpr;
	write_icr(dev, UART_CPR, dev->serial_config.CPR);
}


/*
 * try to detect a 16C950 chip
 */
static int detect_16c950(struct atarisio_dev* dev)
{
	uint8_t id1,id2,id3,rev;

	/* according to 8250.c of the Linux kernel enhanced mode must be enabled
	 * for the 16C952 rev B, otherwise detection will fail
	 */
	dev->serial_out(dev, UART_LCR, 0xBF);
	dev->serial_out(dev, UART_EFR, UART_EFR_ECB);
	dev->serial_out(dev, UART_LCR, 0);

	id1=read_icr(dev, UART_ID1);
	id2=read_icr(dev, UART_ID2);
	id3=read_icr(dev, UART_ID3);
	rev=read_icr(dev, UART_REV);

	if ((id1 == id2) && (id1 == id3) && (id1 == rev)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "not a 16C950, ID registers all contain %02x\n", id1);
		return 0;
	}
	if ((id1 == 0x16) && (id2 == 0xc9) && ( (id3 & 0xf0) == 0x50)) {
		PRINTK_NODEV("detected 16C950: id = %02X%02X%02X rev = %02X\n",
			id1, id2, id3, rev);
		return 1;
	} else {
		DBG_PRINTK_NODEV(DEBUG_STANDARD,"unknown (possible 16C950) chip: id = %02X%02X%02X rev = %02X\n",
			id1, id2, id3, rev);
		return 0;
	}
}


/*
 * timing / timestamp functions
 */
static inline uint64_t get_timestamp(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	struct timespec64 now;
	ktime_get_real_ts64(&now);

	return (uint64_t)(now.tv_sec)*1000000 + now.tv_nsec/1000;
#else
	struct timeval now;
	do_gettimeofday(&now);
	return (uint64_t)(now.tv_sec)*1000000 + now.tv_usec;
#endif
}

static inline void timestamp_entering_ioctl(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.system_entering = get_timestamp();
		dev->timestamps.transmission_start = dev->timestamps.system_entering;
		dev->timestamps.transmission_send_irq = dev->timestamps.system_entering;
		dev->timestamps.transmission_end = dev->timestamps.system_entering;
		dev->timestamps.transmission_wakeup = dev->timestamps.system_entering;
		dev->timestamps.uart_finished = dev->timestamps.system_entering;
		dev->timestamps.system_leaving = dev->timestamps.system_entering;
		dev->got_send_irq_timestamp = 0;
	}
}

static inline void timestamp_transmission_start(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.transmission_start = get_timestamp();
	}
}

static inline void timestamp_transmission_send_irq(struct atarisio_dev* dev)	/* note: only the first occurrence will be recorded */
{
	if (dev->enable_timestamp_recording && !dev->got_send_irq_timestamp) {
		dev->timestamps.transmission_send_irq = get_timestamp();
		dev->got_send_irq_timestamp = 1;
	}
}

static inline void timestamp_transmission_end(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.transmission_end = get_timestamp();
	}
}

static inline void timestamp_transmission_wakeup(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.transmission_wakeup = get_timestamp();
	}
}

static inline void timestamp_uart_finished(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.uart_finished = get_timestamp();
	}
}

static inline void timestamp_leaving_ioctl(struct atarisio_dev* dev)
{
	if (dev->enable_timestamp_recording) {
		dev->timestamps.system_leaving = get_timestamp();
	}
}

/* reset command frame buffer data so that a new reception can start */
static inline void reset_cmdframe_buf_data(struct atarisio_dev* dev)
{
	dev->cmdframe_buf.is_valid = 0;
	dev->cmdframe_buf.receiving = 0;
	dev->cmdframe_buf.pos = 0;
	dev->cmdframe_buf.ok_chars = 0;
	dev->cmdframe_buf.error_chars = 0;
	dev->cmdframe_buf.break_chars = 0;
	dev->cmdframe_buf.start_reception_time=0;
	dev->cmdframe_buf.end_reception_time=0;
}

#if 0
typedef struct {
	unsigned int baudrate;
	uint8_t tcr;
	uint8_t cpr;
	unsigned int divisor;
} baudrate_entry_16c950; 

/* pre-calculated baud-rate table for 16c950 chips running at 921600 bit/sec */
static baudrate_entry_16c950 baudrate_table_16c950[] = {
	/* pokey divisor 0: needs 2 stopbits on PAL */
	{ 126843, 15,  31,   2 }, /* ( 126674.82: -0.13%) ( 127840.91:  0.79%) */
	{ 126707,  7, 133,   1 }, /* ( 126674.82: -0.03%) ( 127840.91:  0.89%) */
	{ 126571,  4, 233,   1 }, /* ( 126674.82:  0.08%) ( 127840.91:  1.00%) */
	{ 126165, 11,  17,   5 }, /* ( 126674.82:  0.40%) ( 127840.91:  1.33%) */
	{ 126030, 13,   0,   9 }, /* ( 126674.82:  0.51%) ( 127840.91:  1.44%) */
	{ 125762, 14,  67,   1 }, /* ( 126674.82:  0.73%) ( 127840.91:  1.65%) */

	/* pokey divisor 0: working */
	{ 125494, 10,  47,   2 }, /* ( 126674.82:  0.94%) ( 127840.91:  1.87%) */

	/* pokey divisor 0: testing */
	{ 124962, 16,  59,   1 }, /* ( 126674.82:  1.37%) ( 127840.91:  2.30%) */
	{ 124830, 15,   9,   7 }, /* ( 126674.82:  1.48%) ( 127840.91:  2.41%) */
	{ 124698, 11,  43,   2 }, /* ( 126674.82:  1.59%) ( 127840.91:  2.52%) */
	{ 124435, 12,  79,   1 }, /* ( 126674.82:  1.80%) ( 127840.91:  2.74%) */
	{ 124304, 13,  73,   1 }, /* ( 126674.82:  1.91%) ( 127840.91:  2.85%) */
	{ 124173, 10,  19,   5 }, /* ( 126674.82:  2.01%) ( 127840.91:  2.95%) */

	{ 123912, 14,  17,   4 }, /* ( 126674.82:  2.23%) ( 127840.91:  3.17%) */
	{ 123652,  9,  53,   2 }, /* ( 126674.82:  2.44%) ( 127840.91:  3.39%) */
	{ 123265, 11,  29,   3 }, /* ( 126674.82:  2.77%) ( 127840.91:  3.71%) */
	/* sometimes errors */
	{ 123008,  7, 137,   1 }, /* ( 126674.82:  2.98%) ( 127840.91:  3.93%) */
	/* OK, but not 100% clean */
	{ 122880, 16,  10,   6 }, /* ( 126674.82:  3.09%) ( 127840.91:  4.04%) */
	{ 122624, 13,  37,   2 }, /* ( 126674.82:  3.30%) ( 127840.91:  4.25%) */
	{ 122497,  9, 107,   1 }, /* ( 126674.82:  3.41%) ( 127840.91:  4.36%) */

	{ 122116, 14,  23,   3 }, /* ( 126674.82:  3.73%) ( 127840.91:  4.69%) */
	/* error in reception */
	{ 121864, 11,   8,  11 }, /* ( 126674.82:  3.95%) ( 127840.91:  4.90%) */

	/* unclean reception */
	{ 121613, 10,  97,   1 }, /* ( 126674.82:  4.16%) ( 127840.91:  5.12%) */
	{ 121362, 12,   9,   9 }, /* ( 126674.82:  4.38%) ( 127840.91:  5.34%) */


	/* unable to receive command frame */
	{ 120989, 15,  13,   5 }, /* ( 126674.82:  4.70%) ( 127840.91:  5.66%) */
	{ 120865, 16,  61,   1 }, /* ( 126674.82:  4.81%) ( 127840.91:  5.77%) */

	/* pokey divisor 1: testing */

	/* pokey divisor 1: needs 2 stopbits on PAL */
	{ 112347, 15,  10,   7 }, /* ( 110840.47: -1.34%) ( 111860.80: -0.43%) */
	{ 112027, 13,   9,   9 }, /* ( 110840.47: -1.06%) ( 111860.80: -0.15%) */

	/* pokey divisor 1: works on NTSC, needs 2 stopbits on PAL */
	{ 111709, 16,  11,   6 }, /* ( 110840.47: -0.78%) ( 111860.80:  0.14%) */
	{ 111603,  7, 151,   1 }, /* ( 110840.47: -0.68%) ( 111860.80:  0.23%) */
	{ 111287, 10, 106,   1 }, /* ( 110840.47: -0.40%) ( 111860.80:  0.52%) */
	{ 111077,  9,  59,   2 }, /* ( 110840.47: -0.21%) ( 111860.80:  0.71%) */
	{ 110869, 14,  76,   1 }, /* ( 110840.47: -0.03%) ( 111860.80:  0.89%) */

	/* pokey divisor 1: working */
	{ 110765,  5, 213,   1 }, /* ( 110840.47:  0.07%) ( 111860.80:  0.99%) */

	/* pokey divisor 2: needs 2 stopbits on PAL */
	{  99130, 14,  17,   5 }, /* (  98524.86: -0.61%) (  99431.82:  0.30%) */

	/* pokey divisor 2: works on NTSC */
	{  99632, 16,  74,   1 }, /* (  98524.86: -1.11%) (  99431.82: -0.20%) */
	{  99548, 15,  79,   1 }, /* (  98524.86: -1.03%) (  99431.82: -0.12%) */

	/* pokey divisor 2: testing */
	{  98715,  5, 239,   1 }, /* (  98524.86: -0.19%) (  99431.82:  0.73%) */

	/* pokey divisor 2: working */
	{  98963,  4, 149,   2 }, /* (  98524.86: -0.44%) (  99431.82:  0.47%) */
	{  98797,  6, 199,   1 }, /* (  98524.86: -0.28%) (  99431.82:  0.64%) */
	{  98632, 13,  23,   4 }, /* (  98524.86: -0.11%) (  99431.82:  0.81%) */
	{  98550,  7,   9,  19 }, /* (  98524.86: -0.03%) (  99431.82:  0.89%) */

	{  98304, 15,   0,  10 }, /* (  98524.86:  0.22%) (  99431.82:  1.15%) */

	/* pokey divisor 3: working on NTSC, need 2 stopbits on PAL */
	{  89638, 14,  47,   2 }, /* (  88672.38: -1.08%) (  89488.64: -0.17%) */

	/* pokey divisor 3: working */
	{  89367, 15,   8,  11 }, /* (  88672.38: -0.78%) (  89488.64:  0.14%) */
	{  88828, 16,  83,   1 }, /* (  88672.38: -0.18%) (  89488.64:  0.74%) */
	{  88695, 14,  19,   5 }, /* (  88672.38: -0.03%) (  89488.64:  0.89%) */
	{  88628, 11,  11,  11 }, /* (  88672.38:  0.05%) (  89488.64:  0.97%) */
	{  88562, 12,  37,   3 }, /* (  88672.38:  0.12%) (  89488.64:  1.05%) */
	{  88297,  8, 167,   1 }, /* (  88672.38:  0.43%) (  89488.64:  1.35%) */
	{  88230,  7, 191,   1 }, /* (  88672.38:  0.50%) (  89488.64:  1.43%) */
	{  88033, 10,  67,   2 }, /* (  88672.38:  0.73%) (  89488.64:  1.65%) */
	{  87902, 11,  61,   2 }, /* (  88672.38:  0.88%) (  89488.64:  1.81%) */
	
	/* pokey divisor 3: testing */
	{  87771, 16,  12,   7 }, /* (  88672.38:  1.03%) (  89488.64:  1.96%) */
	{  87381, 15,  30,   3 }, /* (  88672.38:  1.48%) (  89488.64:  2.41%) */
	{  86738, 16,  17,   5 }, /* (  88672.38:  2.23%) (  89488.64:  3.17%) */
	{  86484, 11, 124,   1 }, /* (  88672.38:  2.53%) (  89488.64:  3.47%) */
	{  86421, 15,  13,   7 }, /* (  88672.38:  2.61%) (  89488.64:  3.55%) */
	{  85980, 14,  14,   7 }, /* (  88672.38:  3.13%) (  89488.64:  4.08%) */
	/* OK */
	{  85481, 15,  92,   1 }, /* (  88672.38:  3.73%) (  89488.64:  4.69%) */
	/* partially working */
	{  85234,  8, 173,   1 }, /* (  88672.38:  4.03%) (  89488.64:  4.99%) */
	/* partially working */
	{  84744, 16,  87,   1 }, /* (  88672.38:  4.64%) (  89488.64:  5.60%) */
	{  84562, 15,  31,   3 }, /* (  88672.38:  4.86%) (  89488.64:  5.83%) */
	{  83781, 16,   8,  11 }, /* (  88672.38:  5.84%) (  89488.64:  6.81%) */

	/* pokey divisor 6: works with 1050 Turbo */
	{  68266, 16,   9,  12 }, /* (  68209.52: -0.08%) (  68837.41:  0.84%) */
	{  68385, 15, 115,   1 }, /* (  68209.52: -0.26%) (  68837.41:  0.66%) */

	/* pokey divisor 9: compatible with Speedy 1050 */
	{  55434, 16,  19,   7 }, /* (  55420.23: -0.02%) (  55930.40:  0.90%) */

	/* pokey divisor 10: compatible with Happy 1050 */
	{  52150, 13,  29,   6 }, /* (  52160.22:  0.02%) (  52640.37:  0.94%) */

	/* pokey divisor 10: testing */
	{  52662, 16,  10,  14 }, /* (  52160.22: -0.95%) (  52640.37: -0.04%) */
	{  52289, 16, 141,   1 }, /* (  52160.22: -0.25%) (  52640.37:  0.67%) */

	/* pokey divisor 16: compatible with Happy 1050 Warp speed */
	{  38550, 15,  12,  17 }, /* (  38553.21:  0.01%) (  38908.10:  0.93%) */

	{  80577, 12,  61,   2 }, /* (  80611.25:  0.04%) (  81353.31:  0.96%) */

	{  58982,  8, 125,   2 },
	{ 110869,  8,  19,   7 },
	{ 110453,  6,  89,   2 },
	{  98385, 11, 109,   1 }, /* (  98524.86:  0.14%) (  99431.82:  1.06%) */
	{      0,  0,   0,   0 }
};

static int calc_16c950_baudrate(struct atarisio_dev* dev, unsigned int baudrate)
{
	unsigned int divisor;
	int i;
	int tcr;

	if (dev->serial_config.baud_base == 921600) {
		for (i=0; baudrate_table_16c950[i].baudrate; i++) {
			if (baudrate_table_16c950[i].baudrate == baudrate) {
				if (baudrate_table_16c950[i].cpr) {
					dev->serial_config.MCR |= UART_MCR_CLKSEL;
					write_icr(dev, UART_CPR, baudrate_table_16c950[i].cpr);
				} else {
					dev->serial_config.MCR &= ~UART_MCR_CLKSEL;
				}
				dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
				write_icr(dev, UART_TCR, baudrate_table_16c950[i].tcr);
				dev->serial_config.exact_baudrate = baudrate;
				DBG_PRINTK(DEBUG_STANDARD, "setting baudrate %d TCR=%d, CPR=%d, divisor %d\n",
					 baudrate,
					 baudrate_table_16c950[i].tcr,
					 baudrate_table_16c950[i].cpr,
					 baudrate_table_16c950[i].divisor);
				return baudrate_table_16c950[i].divisor;
			}
		}
	}

	dev->serial_config.MCR &= ~UART_MCR_CLKSEL; /* disable clock prescaler */
	dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);

	tcr = 4;
	/* calculate divisor for TCR = 4 */
	divisor = (dev->serial_config.baud_base * 4 + baudrate / 2) / baudrate;

	/* switch to TCR 8 or 16 if the divisor is even */
	while ( ((divisor & 1) == 0) && (tcr < 16) ) {
		divisor = divisor / 2;
		tcr = tcr * 2;
	}

	if (divisor == 0) {
		PRINTK("calc_16c950_baudrate: invalid baudrate %d\n", baudrate);
		return -EINVAL;
	}
	dev->serial_config.exact_baudrate = (dev->serial_config.baud_base*(16/tcr)) / divisor;

	write_icr(dev, UART_TCR, tcr & 0x0f);

	DBG_PRINTK(DEBUG_STANDARD, "setting baudrate %d (requested: %d) TCR=%d, CPR=0, divisor %d\n",
		dev->serial_config.exact_baudrate, baudrate, tcr, divisor);

	return divisor;
}
#endif

#define C950_PARAM(opt_baud, opt_tcr, opt_cpr, opt_div) \
	case opt_baud: \
		*tcr = opt_tcr; \
		*cpr = opt_cpr; \
		*div = opt_div; \
		return 0;

static unsigned int pokey_div_to_baud_16950_921600(unsigned int pokey_div)
{
	switch (pokey_div) {
	case 0:
		return 125494;
	case 1:
		return 110765;
	case 2:
		return 97010;
	case 3:
		return 87771;
	case 4:
		return 80139;
	case 5:
		return 73728;
	case ATARISIO_POKEY_DIVISOR_1050_TURBO:
		return 68266;
	case 7:
		return 62481;
	case ATARISIO_POKEY_DIVISOR_3XSIO:
		return 59130;
	case ATARISIO_POKEY_DIVISOR_SPEEDY:
		return 55434;
	case ATARISIO_POKEY_DIVISOR_HAPPY:
		return 52150;
	case ATARISIO_POKEY_DIVISOR_2XSIO_XF551:
		return 38400;
	case ATARISIO_POKEY_DIVISOR_STANDARD:
		return 19200;
	default: return 0;
	}
}


static unsigned int optimized_baudrate_16950_921600(unsigned int baudrate,
	uint8_t* tcr, uint8_t* cpr, uint16_t* div)
{
	switch(baudrate) {
	C950_PARAM(125494, 10,  47,   2); /* pokey divisor 0 */
	C950_PARAM(110765,  5, 213,   1); /* pokey divisor 1 */
	C950_PARAM( 97010,  8,   8,  19); /* pokey divisor 2 */
	C950_PARAM( 87771, 16,  12,   7); /* pokey divisor 3 */
	C950_PARAM( 68266, 16,   9,  12); /* pokey divisor 6, works with 1050 Turbo */
	C950_PARAM( 59130, 15,  19,   7); /* pokey divisor 8 */
	C950_PARAM( 55434, 16,  19,   7); /* pokey divisor 9, tested with Speedy 1050 */
	C950_PARAM( 52150, 13,  29,   6); /* pokey divisor 10, compatible with Happy 1050 */
	C950_PARAM( 38550, 15,  12,  17); /* pokey divisor 16 */
	C950_PARAM( 18856, 16,  17,  23); /* pokey divisor 40 */
	default: return 1;
	}
}

static unsigned int pokey_div_to_baud_16950_4000000(unsigned int pokey_div)
{
	switch (pokey_div) {
	case 0:
		return 125313;
	case 1:
		return 110815;
	case 2:
		return 98386;
	case 3:
		return 88778;
	case 4:
		return 80541;
	case 5:
		return 73877;
	case ATARISIO_POKEY_DIVISOR_1050_TURBO:
		return 68082;
	case 7:
		return 63323;
	case ATARISIO_POKEY_DIVISOR_3XSIO:
		return 59073;
	case ATARISIO_POKEY_DIVISOR_SPEEDY:
		return 55407;
	case ATARISIO_POKEY_DIVISOR_HAPPY:
		return 52083;
	case ATARISIO_POKEY_DIVISOR_2XSIO_XF551:
		return 38402;
	case ATARISIO_POKEY_DIVISOR_STANDARD:
		return 19201;
	default:
		return 0;
	}
}

static unsigned int optimized_baudrate_16950_4000000(unsigned int baudrate,
	uint8_t* tcr, uint8_t* cpr, uint16_t* div)
{
	switch(baudrate) {
	C950_PARAM(125313, 15,  14,  19); /* pokey divisor 0 */
	C950_PARAM(110815, 16,  47,   6); /* pokey divisor 1 */
	C950_PARAM( 98386, 14, 121,   3); /* pokey divisor 2 */
	C950_PARAM( 88778, 16,   8,  44); /* pokey divisor 3 */
	C950_PARAM( 80541, 16,  97,   4); /* pokey divisor 4 */
	C950_PARAM( 73877, 16,   9,  47); /* pokey divisor 5 */
	C950_PARAM( 68082, 16,  27,  17); /* pokey divisor 6 */
	C950_PARAM( 63323, 14,  12,  47); /* pokey divisor 7 */
	C950_PARAM( 59073, 16,  23,  23); /* pokey divisor 8 */
	C950_PARAM( 55407, 16,  12,  47); /* pokey divisor 9 */
	C950_PARAM( 52083, 16,   8,  75); /* pokey divisor 10 */
	C950_PARAM( 38402, 15,  14,  62); /* pokey divisor 16, 38400 */
	C950_PARAM( 38580, 16,   9,  90); /* pokey divisor 16 */
	C950_PARAM( 19201, 15,  14, 124); /* pokey divisor 40, 19200 */
	C950_PARAM( 18870, 16,   9, 184); /* pokey divisor 40 */
	default: return 1;
	}
}

static unsigned int pokey_div_to_baud_16950(struct atarisio_dev* dev, unsigned int pokey_div)
{
	int baudrate = 0;

	switch (dev->serial_config.baud_base) {
	case 921600:
		baudrate = pokey_div_to_baud_16950_921600(pokey_div);
		break;
	case 4000000:
		baudrate = pokey_div_to_baud_16950_4000000(pokey_div);
		break;
	default:
		break;
	}
	if (baudrate == 0) {
		baudrate = ATARISIO_ATARI_FREQUENCY_PAL / (2 * (pokey_div + 7));
	}
	return baudrate;
}

static int pokey_div_to_baud(struct atarisio_dev* dev, unsigned int pokey_div)
{
	if (pokey_div >= 256) {
		return -EINVAL;
	}
	if (dev->is_16c950 && dev->use_16c950_mode) {
		return pokey_div_to_baud_16950(dev, pokey_div);
	}

	/* 16550 can only use standard speeds */
	switch (pokey_div) {
	case ATARISIO_POKEY_DIVISOR_STANDARD:
		return 19200;
	case ATARISIO_POKEY_DIVISOR_2XSIO_XF551:
		return 38400;
	case ATARISIO_POKEY_DIVISOR_3XSIO:
		return 57600;
	default:
		return 0;
	}
}

static int set_baudrate_16950(struct atarisio_dev* dev, unsigned int baudrate)
{
	unsigned int got_optimized = 1;
	uint8_t tcr = 16, cpr = 8;
	uint16_t div = 0;
	unsigned int baud_base = dev->serial_config.baud_base;
	unsigned int real_baud;

	switch (baud_base) {
	case 921600:
		got_optimized = optimized_baudrate_16950_921600(baudrate, &tcr, &cpr, &div);
		break;
	case 4000000:
		/* PCIe cards actually have 62.5MHz/16 base clock */
		baud_base = 3906250;
		/* fallthrough */
	case 3906250:
		got_optimized = optimized_baudrate_16950_4000000(baudrate, &tcr, &cpr, &div);
		break;
	default:
		break;
	}

	if (got_optimized) {
		/* try to match baudrate +/- 0.5% */
		unsigned int min_baud = baudrate * 995 / 1000;
		unsigned int max_baud = baudrate * 1005 / 1000;

		cpr = 8; /* no clock prescaling */
		tcr = 16;
		while (tcr >= 4) {
			div = (baud_base * 16 / tcr + baudrate / 2) / baudrate;
			real_baud = baud_base * 16 / tcr / div;
			if (real_baud >= min_baud && real_baud <= max_baud) {
				break;
			}
			if (tcr == 4) {
				break;
			}
			tcr--;
		}
	}

	if (div == 0) {
		return -EINVAL;
	}

	real_baud = baud_base * 16 * 8 / cpr / tcr / div;

	DBG_PRINTK(DEBUG_STANDARD, "set_baudrate_16950(%d): CPR %d TCR %d DIV %d real_baud %d\n",
		baudrate, cpr, tcr, div, real_baud);

	if (cpr == 8) {
		dev->serial_config.MCR &= ~UART_MCR_CLKSEL; /* disable clock prescaler */
	} else {
		dev->serial_config.MCR |= UART_MCR_CLKSEL; /* enable clock prescaler */
	}

	set_cpr(dev, cpr);
	dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
	set_tcr(dev, tcr);
	set_dl(dev,div);

	return real_baud;
}

static int set_baudrate_16550(struct atarisio_dev* dev, unsigned int baudrate)
{
	uint16_t divisor;
	unsigned int real_baud;

	divisor = (dev->serial_config.baud_base + baudrate / 2) / baudrate;

	if (divisor == 0) {
		DBG_PRINTK(DEBUG_STANDARD, "set_baudrate_16550(%d): divisor is zero\n",
			baudrate);
		return -EINVAL;
	}

	real_baud = dev->serial_config.baud_base / divisor;

	DBG_PRINTK(DEBUG_STANDARD, "set_baudrate_16550(%d): divisor %d real_baud %d\n",
		baudrate, divisor, real_baud);

	set_dl(dev, divisor);
	return real_baud;
}

static int force_set_baudrate(struct atarisio_dev* dev, unsigned int baudrate)
{
	int ret;

	if (dev->is_16c950 && dev->use_16c950_mode) {
		ret = set_baudrate_16950(dev, baudrate);
	} else {
		ret = set_baudrate_16550(dev, baudrate);
	}
	if (ret < 0) {
		return ret;
	}

	dev->serial_config.baudrate = baudrate;
	dev->serial_config.just_switched_baud = 1;
	dev->serial_config.exact_baudrate = ret;

	return 0;
}

#if 0
static int force_set_baudrate_old(struct atarisio_dev* dev, unsigned int baudrate)
{
	int divisor;
	int ret = 0;

	if (baudrate == 0) {
		PRINTK("force_set_baudrate: invalid baudrate 0\n");
		return -EINVAL;
	}

	divisor = (dev->serial_config.baud_base + baudrate / 2) / baudrate;

	if (divisor == 0 ) {
		PRINTK("force_set_baudrate: invalid baudrate %d\n", baudrate);
		return -EINVAL;
	}

	dev->serial_config.baudrate = baudrate;
	dev->serial_config.just_switched_baud = 1;
	dev->serial_config.exact_baudrate = dev->serial_config.baud_base / divisor;

	if (dev->is_16c950 && dev->use_16c950_mode) {
		if (dev->serial_config.exact_baudrate == baudrate) {
			dev->serial_config.MCR &= ~UART_MCR_CLKSEL; /* disable clock prescaler */
			dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
			/* configure standard mode: 16 clocks per bit */
			write_icr(dev, UART_TCR, 0);
			DBG_PRINTK(DEBUG_STANDARD, "setting baudrate %d TCR=16, CPR=0, divisor %d\n", baudrate, divisor);
		} else {
			divisor = calc_16c950_baudrate(dev, baudrate);
		}
	} else {
		DBG_PRINTK(DEBUG_STANDARD, "setting baudrate %d (requested: %d), divisor %d\n",
			dev->serial_config.exact_baudrate, baudrate, divisor);
	}

	if (divisor > 0) {
		dev->serial_out(dev, UART_LCR, dev->serial_config.LCR | UART_LCR_DLAB);
		dev->serial_out(dev, UART_DLL, divisor & 0xff);
		dev->serial_out(dev, UART_DLM, divisor >> 8);
		dev->serial_out(dev, UART_LCR, dev->serial_config.LCR);
	}

	return ret;
}
#endif

static int set_baudrate(struct atarisio_dev* dev, unsigned int baudrate, int do_locks)
{
	int ret=0;
	unsigned long flags=0;
	if (baudrate == 0) {
		PRINTK("set_baudrate: invalid baudrate 0\n");
		return -EINVAL;
	}
	if (do_locks) {
		spin_lock_irqsave(&dev->lock, flags);
	}
	if (dev->serial_config.baudrate != baudrate) {
		ret=force_set_baudrate(dev, baudrate);
	}
	if (do_locks) {
		spin_unlock_irqrestore(&dev->lock, flags);
	}
	return ret;
}

static inline uint8_t calculate_checksum(const uint8_t* circ_buf, int start, int len, int total_len)
{
	unsigned int chksum=0;
	int i;
	for (i=0;i<len;i++) {
		chksum += circ_buf[(start+i) % total_len];
		if (chksum >= 0x100) {
			chksum = (chksum & 0xff) + 1;
		}
	}
	return chksum;
}

static inline void reset_fifos(struct atarisio_dev* dev)
{
	IRQ_PRINTK(DEBUG_NOISY, "resetting 16550 fifos\n");

	dev->serial_out(dev, UART_FCR, 0);
	dev->serial_out(dev, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	dev->serial_out(dev, UART_FCR, dev->serial_config.FCR);

	/* read LSR and MSR to clear any remaining status bits */
	(void) dev->serial_in(dev, UART_LSR);
	(void) dev->serial_in(dev, UART_MSR);
}

static inline void receive_chars(struct atarisio_dev* dev)
{
	uint8_t c,lsr;
	int do_wakeup=0;
	int first = 1;

	while ( (lsr=dev->serial_in(dev, UART_LSR)) & UART_LSR_DR ) {
		if (first && (dev->serial_config.LCR != dev->standard_lcr) ) {
			IRQ_PRINTK(DEBUG_STANDARD, "wrong LCR in receive chars!\n");
			dev->serial_config.LCR = dev->standard_lcr;
			dev->serial_out(dev, UART_LCR, dev->serial_config.LCR);
		}
		first = 0;
		if (lsr & UART_LSR_OE) {
			IRQ_PRINTK(DEBUG_STANDARD, "overrun error\n");
		}
		c = dev->serial_in(dev, UART_RX);

		if (lsr & (UART_LSR_FE | UART_LSR_BI | UART_LSR_PE) ) {
			if (lsr & UART_LSR_FE) {
				IRQ_PRINTK(DEBUG_NOISY, "got framing error\n");
				if (dev->cmdframe_buf.receiving) {
					dev->cmdframe_buf.error_chars++;
				}
			}
			if (lsr & UART_LSR_PE) {
				IRQ_PRINTK(DEBUG_NOISY, "got parity error (?)\n");
				if (dev->cmdframe_buf.receiving) {
					dev->cmdframe_buf.error_chars++;
				}
			}
			if (lsr & UART_LSR_BI) {
				IRQ_PRINTK(DEBUG_NOISY, "got break\n");
				if (dev->cmdframe_buf.receiving) {
					dev->cmdframe_buf.break_chars++;
				}
			}
			IRQ_PRINTK(DEBUG_VERY_NOISY, "received (broken) character 0x%02x\n",c);
		} else {
			if (dev->cmdframe_buf.receiving) {
				if (dev->cmdframe_buf.pos<5) {
					dev->cmdframe_buf.buf[dev->cmdframe_buf.pos] = c;
					dev->cmdframe_buf.pos++;
					IRQ_PRINTK(DEBUG_VERY_NOISY, "received cmdframe character 0x%02x\n",c);
				} else {
					IRQ_PRINTK(DEBUG_VERY_NOISY, "sinking cmdframe character 0x%02x\n",c);
				}
				dev->cmdframe_buf.ok_chars++;
			} else {
				if ( (dev->rx_buf.head+1) % IOBUF_LENGTH != dev->rx_buf.tail ) {
					dev->rx_buf.buf[dev->rx_buf.head] = c;
					dev->rx_buf.head = (dev->rx_buf.head+1) % IOBUF_LENGTH;
					if ( (dev->rx_buf.head + IOBUF_LENGTH - dev->rx_buf.tail) % IOBUF_LENGTH == (unsigned int) dev->rx_buf.wakeup_len) {
						do_wakeup=1;
					}
					IRQ_PRINTK(DEBUG_VERY_NOISY, "received character 0x%02x\n",c);
				} else {
					IRQ_PRINTK(DEBUG_VERY_NOISY, "sinking character 0x%02x\n",c);
				}
			}
		}
	}
	if ( do_wakeup ) {
		wake_up(&dev->rx_queue);
	}
}

static inline void send_chars(struct atarisio_dev* dev)
{
	int do_wakeup=0;
	int count = 16; /* FIFO can hold 16 chars at maximum */
	uint8_t lsr;

	lsr = dev->serial_in(dev, UART_LSR);
	if (lsr & UART_LSR_OE) {
		IRQ_PRINTK(DEBUG_STANDARD, "overrun error\n");
	}
	if (lsr & UART_LSR_THRE) {
		timestamp_transmission_send_irq(dev);	/* note: only the first occurrence will be recorded */

		while ( (count > 0) && (dev->tx_buf.head != dev->tx_buf.tail) ) {
			IRQ_PRINTK(DEBUG_VERY_NOISY, "transmit char 0x%02x\n",dev->tx_buf.buf[dev->tx_buf.tail]);

			dev->serial_out(dev, UART_TX, dev->tx_buf.buf[dev->tx_buf.tail]);
			dev->tx_buf.tail = (dev->tx_buf.tail+1) % IOBUF_LENGTH;
			count--;
		};

		if ( dev->tx_buf.head == dev->tx_buf.tail ) {
			/* end of TX-buffer reached, disable further TX-interrupts */

			dev->serial_config.IER &= ~UART_IER_THRI;
			dev->serial_out(dev, UART_IER, dev->serial_config.IER);

			timestamp_transmission_end(dev);

			do_wakeup=1;
		}

		if (do_wakeup) {
			wake_up(&dev->tx_queue);
		}
	}
}

static inline void try_switchbaud(struct atarisio_dev* dev)
{
	unsigned int baud;

        baud= dev->serial_config.baudrate;

	if (dev->do_autobaud) {
		if (baud == dev->default_baudrate) {
			baud = dev->highspeed_baudrate;
		} else {
			baud = dev->default_baudrate;
		}

		IRQ_PRINTK(DEBUG_STANDARD, "switching to %d baud\n",baud);
	}

	set_baudrate(dev, baud, 0);
}

static inline void check_modem_lines_before_receive(struct atarisio_dev* dev, uint8_t new_msr)
{
	if (dev->current_mode == MODE_SIOSERVER) {
		if ( (new_msr & dev->sioserver_command_line) != (dev->last_msr & dev->sioserver_command_line)) {
			IRQ_PRINTK(DEBUG_VERY_NOISY, "msr changed from 0x%02x to 0x%02x\n",dev->last_msr, new_msr);
			if (new_msr & dev->sioserver_command_line) {
				PRINT_TIMESTAMP("start of command frame\n");
				/* start of a new command frame */
				if (dev->cmdframe_buf.is_valid) {
					IRQ_PRINTK(DEBUG_STANDARD, "invalidating command frame (detected new frame) %d\n", 
						dev->cmdframe_buf.missed_count);
					dev->cmdframe_buf.missed_count++;
				}
				if (dev->cmdframe_buf.receiving) {
					IRQ_PRINTK(DEBUG_STANDARD, "restarted reception of command frame (detected new frame)\n");
				}
				reset_cmdframe_buf_data(dev);
				dev->cmdframe_buf.receiving = 1;
				dev->cmdframe_buf.start_reception_time = dev->irq_timestamp;
				dev->cmdframe_buf.serial_number++;
			}
		}
	}
}

#define DEBUG_PRINT_CMDFRAME_BUF(dev) \
	"cmdframe_buf: %02x %02x %02x %02x %02x rcv=%d pos=%d rx=%d er=%d brk=%d\n",\
	dev->cmdframe_buf.buf[0],\
	dev->cmdframe_buf.buf[1],\
	dev->cmdframe_buf.buf[2],\
	dev->cmdframe_buf.buf[3],\
	dev->cmdframe_buf.buf[4],\
	dev->cmdframe_buf.receiving,\
	dev->cmdframe_buf.pos,\
	dev->cmdframe_buf.ok_chars,\
	dev->cmdframe_buf.error_chars,\
	dev->cmdframe_buf.break_chars

enum command_frame_quality {
	command_frame_ok = 0,
	command_frame_minor_error = 1,	/* for example checksum error, ignore them */
	command_frame_normal_error = 2,	/* a few (framing) errors, countermeasure may be needed */
	command_frame_major_error = 3	/* severe error, take immediate contermeasures */
};

static enum command_frame_quality check_command_frame_quality(struct atarisio_dev* dev)
{
	if (!dev->cmdframe_buf.receiving) {
		/* no characters have been received, but COMMAND trailing edge detected */
		/* the UART might have locked up or the baudrate is way off */
		IRQ_PRINTK(DEBUG_NOISY, "indicated end of command frame, but I'm not currently receiving\n");
		return command_frame_major_error;
	}

	/* break characters may indicate that our speed is too high */
	if (dev->cmdframe_buf.break_chars > 1) {
		return command_frame_major_error;
	}

	/* 7 or more received charaters usually means our speed is too high */
	if (dev->cmdframe_buf.ok_chars + dev->cmdframe_buf.error_chars + dev->cmdframe_buf.break_chars >= 7) {
		return command_frame_major_error;
	}

	/* 3 or less characters may indicate our speed is too low */
	if (dev->cmdframe_buf.ok_chars + dev->cmdframe_buf.error_chars <= 3) {
		return command_frame_major_error;
	}

	/* a single break can mean anything, handle it as a normal error */
	if (dev->cmdframe_buf.break_chars) {
		return command_frame_normal_error;
	}

	/* 5 characters without errors, validate the checksum */
	if (dev->cmdframe_buf.ok_chars == 5 && dev->cmdframe_buf.error_chars == 0) {
		uint8_t checksum = calculate_checksum((uint8_t*)dev->cmdframe_buf.buf, 0, 4, 5);
		if (dev->cmdframe_buf.buf[4] == checksum) {
			IRQ_PRINTK(DEBUG_NOISY, "found command frame\n");
			return command_frame_ok;
		} else {
			IRQ_PRINTK(DEBUG_STANDARD, "command frame checksum error\n");
			return command_frame_minor_error;
		}
	}

	/* if we got up to here, we have received 4 to 6 characters in total */

	/* a single framing error, but 5 bytes in total - ignore it */
	if (dev->cmdframe_buf.ok_chars == 4 && dev->cmdframe_buf.error_chars == 1) {
		return command_frame_minor_error;
	}

	/* default: a normal error */
	return command_frame_normal_error;
}

/* check if the command frame is OK (return 0 in this case) and take possible counter-measures */
static inline int validate_command_frame(struct atarisio_dev* dev)
{
	int last_switchbaud;
	enum command_frame_quality quality;

	last_switchbaud = dev->serial_config.just_switched_baud;
	dev->serial_config.just_switched_baud = 0;

	quality = check_command_frame_quality(dev);

	if (quality == command_frame_ok) {
		return 0;
	}

	reset_fifos(dev);

	if (quality == command_frame_minor_error) {
		return 1;
	}

	if (quality == command_frame_normal_error) {
		/* don't switch baudrates too fast, wait until the next command frame */
		if (last_switchbaud) {
			return 1;
		}
	}

	try_switchbaud(dev);	/* this also sets just_switched_baud */

	reset_fifos(dev);

	return 1;
}

static inline void check_modem_lines_after_receive(struct atarisio_dev* dev, uint8_t new_msr)
{
	int do_wakeup = 0;

	if (dev->current_mode == MODE_SIOSERVER) {
		/*
		 * check if the UART indicated a change on the command line from
		 * high to low. If yes, a complete (5 byte) command frame should
		 * have been received.
		 */
		if ( (new_msr & dev->sioserver_command_line_delta) && (!(new_msr & dev->sioserver_command_line)) ) {
			if (dev->rx_buf.tail != dev->rx_buf.head) {
				IRQ_PRINTK(DEBUG_STANDARD, "flushing rx buffer (%d/%d)\n", dev->rx_buf.head, dev->rx_buf.tail);
			}
			if (dev->tx_buf.tail != dev->tx_buf.head) {
				IRQ_PRINTK(DEBUG_STANDARD, "flushing tx buffer (%d/%d)\n", dev->tx_buf.head, dev->tx_buf.tail);
			}

			dev->rx_buf.head = dev->rx_buf.tail; /* flush input buffer */
			dev->tx_buf.tail = dev->tx_buf.head; /* flush output buffer */

			IRQ_PRINTK(DEBUG_NOISY, DEBUG_PRINT_CMDFRAME_BUF(dev));
			if (validate_command_frame(dev) == 0) {
				do_wakeup = 1;
				dev->cmdframe_buf.is_valid = 1;
				dev->cmdframe_buf.end_reception_time = dev->irq_timestamp;
			} else {
				dev->cmdframe_buf.is_valid = 0;
			}

			dev->cmdframe_buf.receiving = 0;

			if (do_wakeup) {
				PRINT_TIMESTAMP("waking up cmdframe_queue\n");
				wake_up(&dev->cmdframe_queue);
			}
		}
	}
}

/*
 * the interrupt handler
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t atarisio_interrupt(int irq, void* dev_id, struct pt_regs* regs)
#else
static irqreturn_t atarisio_interrupt(int irq, void* dev_id)
#endif
{
	struct atarisio_dev* dev = dev_id;
	uint8_t iir,msr;
	unsigned int handled = 0;

	dev->irq_timestamp = get_timestamp();

	spin_lock(&dev->lock);

	while (! ((iir=dev->serial_in(dev, UART_IIR)) & UART_IIR_NO_INT)) {
		handled = 1;

		msr = dev->serial_in(dev, UART_MSR);
		IRQ_PRINTK(DEBUG_VERY_NOISY, "atarisio_interrupt: IIR = 0x%02x MSR = 0x%02x\n", iir, msr);

		check_modem_lines_before_receive(dev, msr);

		receive_chars(dev);

		check_modem_lines_after_receive(dev, msr);

		send_chars(dev);

		dev->last_msr = msr;
	}

	spin_unlock(&dev->lock);

	return IRQ_RETVAL(handled);
}

/*
 * code from here on is never called in an interrrupt context
 */

static inline void initiate_send(struct atarisio_dev* dev)
{
	unsigned long flags;
	DBG_PRINTK(DEBUG_VERY_NOISY, "initiate_send: tx-head = %d tx-tail = %d\n", dev->tx_buf.head, dev->tx_buf.tail);

	timestamp_transmission_start(dev);

	spin_lock_irqsave(&dev->lock, flags);

	if ( (dev->tx_buf.head != dev->tx_buf.tail)
	     && !(dev->serial_config.IER & UART_IER_THRI) ) {

		DBG_PRINTK(DEBUG_VERY_NOISY, "enabling TX-interrupt\n");

		dev->serial_config.IER |= UART_IER_THRI;
		dev->serial_out(dev, UART_IER, dev->serial_config.IER);
	}

	spin_unlock_irqrestore(&dev->lock, flags);
}

static inline int wait_send(struct atarisio_dev* dev, unsigned int len, unsigned int wait_mode)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	wait_queue_entry_t wait;
#else
	wait_queue_t wait;
#endif
	signed long timeout_jiffies;
	signed long expire;
	unsigned long max_jiffies;
	long timeout;

	if (wait_mode == SEND_MODE_NOWAIT) {
		return 0;
	}

	/* the fifo and thr may contain up to 17 bytes, therefore the +17 in len */
	timeout = 11*(len + 17)*1000 / dev->serial_config.exact_baudrate;
	timeout += DELAY_SEND_HEADROOM;

	timeout_jiffies = timeout*HZ/1000;
	if (timeout_jiffies <= 0) {
		timeout_jiffies = 1;
	}

	expire = timeout_jiffies;
	max_jiffies = jiffies + timeout_jiffies;

	/*
	 * first wait for the interrupt notification, telling us
	 * that the transmit buffer is empty
	 */
	init_waitqueue_entry(&wait, current);

	add_wait_queue(&dev->tx_queue, &wait);
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		if (dev->tx_buf.head == dev->tx_buf.tail) {
			break;
		}
		expire = schedule_timeout(expire);
		if (expire==0) {
			break;
		}
		if (signal_pending(current)) {
			current->state=TASK_RUNNING;
			remove_wait_queue(&dev->tx_queue, &wait);
			return -EINTR;
		}
	}
	current->state=TASK_RUNNING;
	remove_wait_queue(&dev->tx_queue, &wait);

	if ( dev->tx_buf.head != dev->tx_buf.tail ) {
		DBG_PRINTK(DEBUG_STANDARD, "timeout expired in wait_send\n");
		return -EATARISIO_COMMAND_TIMEOUT;
	}

	timestamp_transmission_wakeup(dev);

	/*
	 * workaround for buggy Moschip 9835 (and other?) UARTs:
	 * the 9835 sets TEMT for a short time after each byte,
	 * no matter if the FIFO contains data or not. So we have
	 * to check if both the FIFO (using the THRE flag) and
	 * the shift register are empty.
	 */
#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

	if (wait_mode == SEND_MODE_WAIT_ALL) {
		/*
		 * now wait until the transmission is completely finished
		 */
		while ((jiffies < max_jiffies) && ((dev->serial_in(dev, UART_LSR) & BOTH_EMPTY) != BOTH_EMPTY)) {
		}
	}
	timestamp_uart_finished(dev);

	if (jiffies >= max_jiffies) {
		DBG_PRINTK(DEBUG_STANDARD, "timeout expired in wait_send / TEMT\n");
		return -EATARISIO_COMMAND_TIMEOUT;
	} else {
		return 0;
	}
}

/* check if a new command frame was received, before sending a command ACK/NAK */
static inline int check_new_command_frame(struct atarisio_dev* dev)
{
	if (dev->current_mode != MODE_SIOSERVER) {
		return 0;
	}
	if (dev->cmdframe_buf.serial_number != dev->current_cmdframe_serial_number) {
		DBG_PRINTK(DEBUG_STANDARD, "new command frame has arrived...\n");
		return -EATARISIO_COMMAND_TIMEOUT;
	} else {
		return 0;
	}
}

static int wait_receive(struct atarisio_dev* dev, int len, int additional_timeout)
{
	unsigned long flags;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	wait_queue_entry_t wait;
#else
	wait_queue_t wait;
#endif

	int no_received_chars=0;
	long timeout;
	signed long timeout_jiffies;
	signed long expire;
	signed long jiffies_start = jiffies;

	if (len <= 0) {
		DBG_PRINTK(DEBUG_STANDARD, "error in wait_receive: invalid len parameter %d\n", len);
		return -EINVAL;
	}

	timeout = 11*len*1000 / dev->serial_config.exact_baudrate;
	timeout += additional_timeout + DELAY_RECEIVE_HEADROOM;

	timeout_jiffies = timeout*HZ/1000;
	if (timeout_jiffies <= 0) {
		timeout_jiffies = 1;
	}

	expire = timeout_jiffies;

	init_waitqueue_entry(&wait, current);

	dev->rx_buf.wakeup_len = len;
	add_wait_queue(&dev->rx_queue, &wait);
	while (1) {
		current->state = TASK_INTERRUPTIBLE;

		spin_lock_irqsave(&dev->lock, flags);
		no_received_chars=(dev->rx_buf.head+IOBUF_LENGTH - dev->rx_buf.tail) % IOBUF_LENGTH;
		spin_unlock_irqrestore(&dev->lock, flags);

		if (no_received_chars >=len) {
			break;
		}
		expire = schedule_timeout(expire);
		if (expire==0) {
			break;
		}
		if (signal_pending(current)) {
			current->state=TASK_RUNNING;
			remove_wait_queue(&dev->rx_queue, &wait);
			return -EINTR;
		}
	}
	current->state=TASK_RUNNING;
	remove_wait_queue(&dev->rx_queue, &wait);

	dev->rx_buf.wakeup_len=-1;

	if (no_received_chars < len) {
		DBG_PRINTK(DEBUG_STANDARD, "timeout in wait_receive [wanted %d got %d time %ld]\n",
				len, no_received_chars, jiffies - jiffies_start);
		if (no_received_chars) {
			DBG_PRINTK(DEBUG_NOISY, "wait_receive: flushing rx_buf\n");
			spin_lock_irqsave(&dev->lock, flags);
			dev->rx_buf.tail = dev->rx_buf.head;
			spin_unlock_irqrestore(&dev->lock, flags);
		}
		return -EATARISIO_COMMAND_TIMEOUT;
	}
	if (no_received_chars > len) {
		DBG_PRINTK(DEBUG_NOISY, "received more bytes than expected [expected %d got %d]\n",
				len, no_received_chars);
	}
	return 0;
}

static int receive_single_character(struct atarisio_dev* dev, unsigned int additional_timeout)
{
	unsigned long flags;
	int ret;
	int no_received_chars;

	if ((ret=wait_receive(dev, 1, additional_timeout))) {
		return ret;
	}

	spin_lock_irqsave(&dev->lock, flags);
	no_received_chars=(dev->rx_buf.head+IOBUF_LENGTH - dev->rx_buf.tail) % IOBUF_LENGTH;
	if (no_received_chars < 1) {
		DBG_PRINTK(DEBUG_NOISY,"receive_single_character: rx_buf flushed after wait_receive\n");
		ret = -EATARISIO_COMMAND_TIMEOUT;
	} else {
		ret = dev->rx_buf.buf[dev->rx_buf.tail];
		dev->rx_buf.tail = (dev->rx_buf.tail + 1) % IOBUF_LENGTH;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return ret;
}

static int copy_received_data_to_user(struct atarisio_dev* dev, unsigned int data_length, uint8_t* user_buffer)
{
	unsigned int len, remain;

	if ((data_length == 0) || (data_length >= MAX_SIO_DATA_LENGTH)) {
		return -EINVAL;
	}
	/* if we received any bytes, copy the to the user buffer, even
	 * if we ran into a timeout and got less than we wanted
	 */
	len = IOBUF_LENGTH - dev->rx_buf.tail;
	if (len > data_length) {
		len = data_length;
	}
	if ( copy_to_user(user_buffer,
			 (uint8_t*) (&(dev->rx_buf.buf[dev->rx_buf.tail])),
			 len) ) {
		return -EFAULT;
	}
	remain = data_length - len;
	if (remain>0) {
		if ( copy_to_user(user_buffer+len,
				 (uint8_t*) dev->rx_buf.buf,
				 remain) ) {
			return -EFAULT;
		}
	}
	return 0;
}

static int receive_block(struct atarisio_dev* dev,
	unsigned int block_len,
	uint8_t* user_buffer,
	unsigned int additional_timeout,
	int handle_checksum)
{
	unsigned long flags;

	int ret=0;
	int want_len;
	int no_received_chars;
	uint8_t checksum;

	if (block_len == 0) {
		return -EINVAL;
	}

	want_len = block_len;

	if (handle_checksum) {
		want_len++;
	}

	/* receive data block and checksum */
	if ((ret=wait_receive(dev, want_len, additional_timeout))) {
		return ret;
	}

	spin_lock_irqsave(&dev->lock, flags);

	no_received_chars=(dev->rx_buf.head+IOBUF_LENGTH - dev->rx_buf.tail) % IOBUF_LENGTH;
	if (no_received_chars < want_len) {
		DBG_PRINTK(DEBUG_NOISY,"receive_single_character: rx_buf flushed after wait_receive\n");
		ret = -EATARISIO_COMMAND_TIMEOUT;
	} else {
		ret = copy_received_data_to_user(dev, block_len, user_buffer);

		if (handle_checksum) {
			checksum = calculate_checksum((uint8_t*)(dev->rx_buf.buf),
					dev->rx_buf.tail, block_len, IOBUF_LENGTH);

			if (dev->rx_buf.buf[(dev->rx_buf.tail+block_len) % IOBUF_LENGTH ]
				!= checksum) {
				DBG_PRINTK(DEBUG_STANDARD, "checksum error : 0x%02x != 0x%02x\n",
					checksum,
					dev->rx_buf.buf[(dev->rx_buf.tail+block_len) % IOBUF_LENGTH]);
				if (! ret) {
					ret = -EATARISIO_CHECKSUM_ERROR;
				}
			}
		}
		dev->rx_buf.tail = (dev->rx_buf.tail + want_len) % IOBUF_LENGTH;
	}

	if (dev->rx_buf.head != dev->rx_buf.tail) {
		DBG_PRINTK(DEBUG_NOISY, "detected excess characters\n");
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return ret;
}

static int send_single_character(struct atarisio_dev* dev, uint8_t c)
{
	unsigned long flags;
	int w;
	
	spin_lock_irqsave(&dev->lock, flags);
	dev->tx_buf.buf[dev->tx_buf.head]=c;
	dev->tx_buf.head = (dev->tx_buf.head + 1) % IOBUF_LENGTH;
	spin_unlock_irqrestore(&dev->lock, flags);

	initiate_send(dev);

	w=wait_send(dev, 1, SEND_MODE_WAIT_ALL);
	return w;
}

static int setup_send_frame(struct atarisio_dev* dev, unsigned int data_length, uint8_t* user_buffer, int add_checksum)
{
	unsigned int len, remain;
	uint8_t checksum;

	if ((data_length == 0) || (data_length >= MAX_SIO_DATA_LENGTH)) {
		return -EINVAL;
	}

	dev->tx_buf.head = dev->tx_buf.tail;

	len = IOBUF_LENGTH - dev->tx_buf.head;
	if (len > data_length) {
		len = data_length;
	}
	if ( copy_from_user((uint8_t*) (&(dev->tx_buf.buf[dev->tx_buf.head])), 
			user_buffer,
			len) ) {
		return -EFAULT;
	}
	remain = data_length - len;
	if (remain>0) {
		if ( copy_from_user((uint8_t*) dev->tx_buf.buf,
				user_buffer+len,
				remain) ) {
			return -EFAULT;
		}
	}

	if (add_checksum) {
		checksum = calculate_checksum((uint8_t*)(dev->tx_buf.buf),
				dev->tx_buf.head, data_length, IOBUF_LENGTH);

		dev->tx_buf.buf[(dev->tx_buf.head+data_length) % IOBUF_LENGTH] = checksum;
		data_length++;
	}

	dev->tx_buf.head = (dev->tx_buf.head + data_length) % IOBUF_LENGTH;
	return 0;
}


static int send_block(struct atarisio_dev* dev,
        unsigned int block_len,
        uint8_t* user_buffer,
        int add_checksum,
	int wait_mode)

{
	unsigned long flags;
	int ret = 0;

	if (block_len) {
		spin_lock_irqsave(&dev->lock, flags);
		if ((dev->add_highspeedpause & ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY) && 
			( (dev->serial_config.baudrate != dev->default_baudrate) || (dev->current_mode == MODE_1050_2_PC) ) ) {
			set_lcr(dev, dev->slow_lcr);
		}
		ret = setup_send_frame(dev, block_len, user_buffer, add_checksum);
		spin_unlock_irqrestore(&dev->lock, flags);
		if (ret == 0) {
			PRINT_TIMESTAMP("send_block: initiate_send\n");
			initiate_send(dev);

			PRINT_TIMESTAMP("send_block: begin wait_send\n");
			if (add_checksum) {
				block_len++;
			}
			if ((ret=wait_send(dev, block_len, wait_mode))) {
				DBG_PRINTK(DEBUG_STANDARD, "wait_send returned %d\n", ret);
			} else {
				PRINT_TIMESTAMP("send_block: end wait_send\n");
			}
		}
		if ((dev->add_highspeedpause & ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY) && 
			( (dev->serial_config.baudrate != dev->default_baudrate) || (dev->current_mode == MODE_1050_2_PC) ) ) {
			spin_lock_irqsave(&dev->lock, flags);
			set_lcr(dev, dev->standard_lcr);
			spin_unlock_irqrestore(&dev->lock, flags);
		}
	}
	return ret;
}

/*
 * signal transmission of command frame
 */
static inline void set_command_line(struct atarisio_dev* dev)
{
	dev->serial_config.MCR = (dev->serial_config.MCR & dev->command_line_mask) | dev->command_line_low;
	dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
}

/*
 * signal end of command frame
 */
static inline void clear_command_line(struct atarisio_dev* dev)
{
	dev->serial_config.MCR = (dev->serial_config.MCR & dev->command_line_mask) | dev->command_line_high;
	dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
}

static int send_command_frame(struct atarisio_dev* dev, Ext_SIO_parameters* params)
{
	int retry = 0;
	int last_err = 0;
	unsigned long flags;
	int i;
	int w;
	int c;
	uint8_t highspeed_mode;

	uint8_t cmd_frame[5];

	highspeed_mode = params->highspeed_mode;

	cmd_frame[0] = params->device + params->unit - 1;
	cmd_frame[1] = params->command;
	cmd_frame[2] = params->aux1;
	cmd_frame[3] = params->aux2;

	switch (params->highspeed_mode) {
	case ATARISIO_EXTSIO_SPEED_NORMAL:
	case ATARISIO_EXTSIO_SPEED_ULTRA:
		break;
	case ATARISIO_EXTSIO_SPEED_TURBO:
		/* set bit 7 of daux2 */
		cmd_frame[3] |= 0x80;
		break;
	case ATARISIO_EXTSIO_SPEED_WARP:
		/* warp speed only supported for read/write sector */
		switch (params->command) {
		case 0x50:
		case 0x52:
		case 0x57:
			/* set bit 5 of command */
			cmd_frame[1] |= 0x20;
			break;
		default:
			highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;
		}
		break;
	case ATARISIO_EXTSIO_SPEED_XF551:
		/* XF551 sends response to format commands always in standard speed */
		if (params->command == 0x21 || params->command == 0x22) {
			highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;
		}
		/* set bit 7 of command */
		cmd_frame[1] |= 0x80;
		break;
	};

	cmd_frame[4] = calculate_checksum(cmd_frame, 0, 4, 5);

	while (retry < MAX_COMMAND_FRAME_RETRIES) {
		DBG_PRINTK(DEBUG_VERY_NOISY, "initiating command frame\n");

		spin_lock_irqsave(&dev->lock, flags);
		set_command_line(dev);
		if (highspeed_mode == ATARISIO_EXTSIO_SPEED_ULTRA) {
			set_baudrate(dev, dev->highspeed_baudrate, 0);
		} else {
			set_baudrate(dev, dev->default_baudrate, 0);
		}
		if (dev->add_highspeedpause & ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY) {
			set_lcr(dev, dev->slow_lcr);
		}
		spin_unlock_irqrestore(&dev->lock, flags);

		udelay(DELAY_T0);

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->tx_buf.head != dev->tx_buf.tail) {
			DBG_PRINTK(DEBUG_VERY_NOISY, "clearing tx bufffer\n");
			dev->tx_buf.head = dev->tx_buf.tail;
		}
		for (i=0;i<5;i++) {
			dev->tx_buf.buf[(dev->tx_buf.head+i) % IOBUF_LENGTH] = cmd_frame[i];
		}
		dev->tx_buf.head = (dev->tx_buf.head+5) % IOBUF_LENGTH;
		spin_unlock_irqrestore(&dev->lock, flags);

		initiate_send(dev);
		if ((w=wait_send(dev, 5, SEND_MODE_WAIT_ALL))) {
			if (w == -EATARISIO_COMMAND_TIMEOUT) {
				last_err = w;
				goto again;
			} else {
				DBG_PRINTK(DEBUG_STANDARD, "sending command frame returned %d\n",w);
				return w;
			}
		}

		spin_lock_irqsave(&dev->lock, flags);
		if (highspeed_mode == ATARISIO_EXTSIO_SPEED_TURBO) {
			set_baudrate(dev, dev->highspeed_baudrate, 0);
		}
		if (dev->add_highspeedpause & ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY) {
			set_lcr(dev, dev->standard_lcr);
		}

		dev->rx_buf.tail = dev->rx_buf.head; /* clear rx-buffer */
		spin_unlock_irqrestore(&dev->lock, flags);

		udelay(DELAY_T1);

		spin_lock_irqsave(&dev->lock, flags);
		clear_command_line(dev);
		spin_unlock_irqrestore(&dev->lock, flags);

		PRINT_TIMESTAMP("begin wait for command ACK\n");

		c = receive_single_character(dev, DELAY_T2_MAX/1000);

		if (highspeed_mode == ATARISIO_EXTSIO_SPEED_WARP || highspeed_mode == ATARISIO_EXTSIO_SPEED_XF551) {
			set_baudrate(dev, dev->highspeed_baudrate, 1);
		}

		PRINT_TIMESTAMP("end wait for command ACK\n");

		if (c < 0) {
			DBG_PRINTK(DEBUG_NOISY, "error waiting for command frame ACK: %d\n", c);
			last_err = -EATARISIO_COMMAND_TIMEOUT;
			goto again;
		}
		if (c==COMMAND_FRAME_ACK_CHAR) {
			break;
		} else if (c==COMMAND_FRAME_NAK_CHAR) {
			DBG_PRINTK(DEBUG_NOISY, "got command frame NAK\n");
			last_err = -EATARISIO_COMMAND_NAK;
		} else {
			DBG_PRINTK(DEBUG_STANDARD, "illegal response to command frame: 0x%02x\n",c);
			last_err = -EATARISIO_UNKNOWN_ERROR;
		}
		if (retry+1 < MAX_COMMAND_FRAME_RETRIES) {
			DBG_PRINTK(DEBUG_NOISY, "retrying command frame\n");
		}

again:
		spin_lock_irqsave(&dev->lock, flags);
		clear_command_line(dev);
		spin_unlock_irqrestore(&dev->lock, flags);

		udelay(100);

		retry++;
	}
		
	if (retry == MAX_COMMAND_FRAME_RETRIES) {
		if (last_err) {
			return last_err;
		} else {
			return -EATARISIO_UNKNOWN_ERROR;
		}
	} else {
		DBG_PRINTK(DEBUG_NOISY, "got ACK for command frame - ready to proceed\n");
		return 0;
	}
}


static int perform_ext_sio_send_data(struct atarisio_dev* dev, Ext_SIO_parameters * sio_params)
{
        int ret=0;
	int c;

	/*
	 * read data from disk
	 */
	udelay(DELAY_T3_MIN);

	if (sio_params->data_length) {
		if ( (ret=send_block(dev, sio_params->data_length, sio_params->data_buffer, 1, SEND_MODE_WAIT_ALL)) ) {
			return ret;
		}

		/*
		 * wait for data frame ack
		 */

		if ((c=receive_single_character(dev, DELAY_T4_MAX/1000)) < 0) {
			return c;
		}

		if (c != DATA_FRAME_ACK_CHAR) {
			if (c != DATA_FRAME_NAK_CHAR) {
				DBG_PRINTK(DEBUG_STANDARD, "wanted data ACK or ERROR, got: 0x%02x\n",c);
			}
			return -EATARISIO_DATA_NAK;
		}
	}
	return ret;
}

static inline int wait_ext_sio_command_complete(struct atarisio_dev* dev, Ext_SIO_parameters * sio_params)
{
	int ret = 0;
	int c;

	if ((c=receive_single_character(dev, sio_params->timeout*1000)) < 0) {
		return c;
	}

	if (c != OPERATION_COMPLETE_CHAR) {
		if (c == COMMAND_FRAME_ACK_CHAR) {
			DBG_PRINTK(DEBUG_STANDARD, "got command ACK instead of command complete\n");
		} else {
			if (c != OPERATION_ERROR_CHAR) {
				DBG_PRINTK(DEBUG_STANDARD, "wanted command complete, got: 0x%02x\n",c);
				ret = -EATARISIO_UNKNOWN_ERROR;
			} else {
				DBG_PRINTK(DEBUG_STANDARD, "got sio command error\n");
				ret = -EATARISIO_COMMAND_COMPLETE_ERROR;
			}
		}
	}
	return ret;
}

static inline int internal_perform_ext_sio(struct atarisio_dev* dev, Ext_SIO_parameters * sio_params)
{
	int ret = 0;
	int cmd_ret = 0;
	int sio_retry = 0;

	if (sio_params->data_length >= MAX_SIO_DATA_LENGTH) {
		return -EATARISIO_ERROR_BLOCK_TOO_LONG;
	}

	DBG_PRINTK(DEBUG_STANDARD, "SIO: device=0x%02x unit=0x%02x cmd=0x%02x aux1=0x%02x aux2=0x%02x dir=0x%02x timeout=%d datalen=%d highspeed=%d\n",
		sio_params->device,
		sio_params->unit,
		sio_params->command,
		sio_params->aux1, sio_params->aux2,
		sio_params->direction,
		sio_params->timeout,
		sio_params->data_length,
		sio_params->highspeed_mode);

	do {
		ret = 0;
		cmd_ret = 0;

		if (sio_retry) {
			DBG_PRINTK(DEBUG_NOISY, "retrying SIO\n");
		}
		if ((ret = send_command_frame(dev, sio_params))) {
			goto sio_error;
		}

		if (sio_params->data_length && (sio_params->direction & ATARISIO_EXTSIO_DIR_SEND)) {
			DBG_PRINTK(DEBUG_NOISY, "sending data block\n");
			if ((ret = perform_ext_sio_send_data(dev, sio_params))) {
				DBG_PRINTK(DEBUG_STANDARD, "sending data block failed: %d\n", ret);
				goto sio_error;
			} else {
				DBG_PRINTK(DEBUG_NOISY, "sending data block OK\n");
			}
		}

		DBG_PRINTK(DEBUG_NOISY, "receiving command complete\n");
		if ((cmd_ret = wait_ext_sio_command_complete(dev, sio_params))) {
			if (cmd_ret != -EATARISIO_COMMAND_COMPLETE_ERROR) {
				DBG_PRINTK(DEBUG_STANDARD, "wait for command complete failed: %d\n", ret);
				goto sio_error;
			} else {
				ret = 0;
			}
		}

		if (sio_params->data_length && (sio_params->direction & ATARISIO_EXTSIO_DIR_RECV)) {
			DBG_PRINTK(DEBUG_NOISY, "receiving data block\n");
			if ((ret = receive_block(dev, sio_params->data_length, sio_params->data_buffer, 0, 1))) {
				DBG_PRINTK(DEBUG_STANDARD, "receive data block failed: %d\n", ret);
				goto sio_error;
			} else {
				DBG_PRINTK(DEBUG_NOISY, "receive data block OK\n");
			}
		}

sio_error:
		if (cmd_ret && !ret) {
			/* report command error in case receiving data succeeded */
			ret = cmd_ret;
		}
		sio_retry++;
	} while (ret != 0 && sio_retry < 2);

	if (ret == 0) {
		DBG_PRINTK(DEBUG_NOISY, "SIO successfull\n");
	} else {
		DBG_PRINTK(DEBUG_STANDARD, "SIO error: %d\n", ret);
	}

	return ret;
}


static inline int perform_ext_sio(struct atarisio_dev* dev, Ext_SIO_parameters * user_sio_params)
{
	int len;
	Ext_SIO_parameters sio_params;

	len = sizeof(sio_params);
	if (copy_from_user(&sio_params, user_sio_params, sizeof(sio_params))) {
		DBG_PRINTK(DEBUG_STANDARD, "copy_from_user failed for SIO parameters\n");
		return -EFAULT;
	}

	return internal_perform_ext_sio(dev, &sio_params);
}

static inline int perform_sio(struct atarisio_dev* dev, SIO_parameters * user_sio_params)
{
	int len;
	SIO_parameters sio_params;
	Ext_SIO_parameters ext_sio_params;

	len = sizeof(sio_params);
	if (copy_from_user(&sio_params, user_sio_params, sizeof(sio_params))) {
		DBG_PRINTK(DEBUG_STANDARD, "copy_from_user failed for SIO parameters\n");
		return -EFAULT;
	}

	ext_sio_params.device = sio_params.device_id;
	ext_sio_params.unit = 1;
	ext_sio_params.command = sio_params.command;
	ext_sio_params.timeout = sio_params.timeout;
	ext_sio_params.aux1 = sio_params.aux1;
	ext_sio_params.aux2 = sio_params.aux2;
	ext_sio_params.data_length = sio_params.data_length;
	ext_sio_params.data_buffer = sio_params.data_buffer;

	if (sio_params.data_length) {
		if (sio_params.direction) {
			ext_sio_params.direction = ATARISIO_EXTSIO_DIR_SEND;
		} else {
			ext_sio_params.direction = ATARISIO_EXTSIO_DIR_RECV;
		}
	} else {
		ext_sio_params.direction = ATARISIO_EXTSIO_DIR_IMMEDIATE;
	}
	ext_sio_params.highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;

	return internal_perform_ext_sio(dev, &ext_sio_params);
}

static int perform_send_frame(struct atarisio_dev* dev, unsigned long arg, int add_checksum, int wait_mode)
{
	SIO_data_frame frame;

	if (copy_from_user(&frame, (SIO_data_frame*) arg, sizeof(SIO_data_frame)) ) {
		return -EFAULT;
	}

	return send_block(dev, frame.data_length, frame.data_buffer, add_checksum, wait_mode);
}


static int perform_receive_frame(struct atarisio_dev* dev, unsigned long arg, int handle_checksum)
{
	SIO_data_frame frame;

	if (copy_from_user(&frame, (SIO_data_frame*) arg, sizeof(SIO_data_frame)) ) {
		return -EFAULT;
	}

	return receive_block(dev, frame.data_length, frame.data_buffer, DELAY_T3_MAX/1000, handle_checksum);
}

static int start_tape_mode(struct atarisio_dev* dev)
{
	int ret = 0;

	if (dev->tape_mode_enabled) {
		DBG_PRINTK(DEBUG_STANDARD, "start tape mode: tape mode already enabled\n");
	} else {
		unsigned long flags = 0;
		spin_lock_irqsave(&dev->lock, flags);

		dev->tape_old_baudrate = dev->serial_config.baudrate;
		dev->tape_old_autobaud = dev->do_autobaud;
		dev->tape_old_ier = dev->serial_config.IER;

		/* disable all but the transmitter interrupt */
		dev->serial_config.IER &= UART_IER_THRI;
		dev->serial_out(dev, UART_IER, dev->serial_config.IER);

		/* disable autobauding and set the baudrate */
		dev->do_autobaud = 0;
		if ((ret = set_baudrate(dev, dev->tape_baudrate, 0))) {
			DBG_PRINTK(DEBUG_STANDARD, "setting tape baudrate %d failed\n", dev->tape_baudrate);
		} else {
			dev->tape_mode_enabled = 1;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
	}
	return ret;
}

static int end_tape_mode(struct atarisio_dev* dev)
{
	int ret = 0;
	if (!dev->tape_mode_enabled) {
		DBG_PRINTK(DEBUG_STANDARD, "end tape mode: tape mode not enabled\n");
	} else {
		unsigned long flags = 0;
		spin_lock_irqsave(&dev->lock, flags);

		dev->serial_config.IER = dev->tape_old_ier;
		dev->serial_out(dev, UART_IER, dev->serial_config.IER);

		set_baudrate(dev, dev->tape_old_baudrate, 0);
		dev->do_autobaud = dev->tape_old_autobaud;

		dev->tape_mode_enabled = 0;
		spin_unlock_irqrestore(&dev->lock, flags);
	}

	return ret;
}

static int perform_send_tape_block(struct atarisio_dev* dev, unsigned long arg)
{
	int r, ret = 0;

	PRINT_TIMESTAMP("start tape mode\n");
	if ((ret = start_tape_mode(dev))) {
		return ret;
	}

	PRINT_TIMESTAMP("send tape block\n");
	ret = perform_send_frame(dev, arg, 0, SEND_MODE_WAIT_BUFFER);

	PRINT_TIMESTAMP("wait_send\n");
	r = wait_send(dev, 0, SEND_MODE_WAIT_ALL);
	if (!ret) ret = r;

	PRINT_TIMESTAMP("end tape mode\n");
	r = end_tape_mode(dev);
	if (!ret) ret = r;

	return ret;
}

static int perform_send_fsk_data_busywait(struct atarisio_dev* dev, uint16_t* fsk_delays, unsigned int num_entries)
{
	int ret = 0;
	int bit = 0;
	int i = 0;
	unsigned long flags;
	uint8_t lcr;
	uint64_t current_time, start_time, end_time;
	long delay;

	PRINT_TIMESTAMP("start of send_fsk_data_udelay\n");
	end_time = get_timestamp();

	while (i < num_entries) {
		spin_lock_irqsave(&dev->lock, flags);
		if (bit) {
			lcr = dev->serial_config.LCR & (~UART_LCR_SBC);
			bit = 0;
		} else {
			lcr = dev->serial_config.LCR | UART_LCR_SBC;
			bit = 1;
		}
		set_lcr(dev, lcr);
		spin_unlock_irqrestore(&dev->lock, flags);

		start_time = end_time;
		end_time += fsk_delays[i] * 100;

		while ((current_time = get_timestamp()) < end_time) {
			if (current_time < start_time) {
				DBG_PRINTK(DEBUG_STANDARD,
					"send_fsk error: current time (%lld) < start time (%lld)!\n",
					current_time, start_time);
				ret = -EINVAL;
				goto exit_fsk;
			}
			delay = end_time - current_time;

			/* use schedule_timeout when delaying for more than 50ms */
			if (delay >= 50 * 1000) {
				/* leave at least 30ms slack time so we don't get caught by timing inaccuracy */
				long expire = (delay - 20 * 1000) * HZ / 1000000;
				PRINT_TIMESTAMP("send_fsk: using schedule_timeout(%ld) since delay is %ld\n", expire, delay);

				current->state = TASK_INTERRUPTIBLE;
				expire = schedule_timeout(expire);
				current->state=TASK_RUNNING;

				PRINT_TIMESTAMP("send_fsk: schedule_timeout() returned %ld\n", expire);

				if (signal_pending(current)) {
					current->state=TASK_RUNNING;
					ret = -EINTR;
					goto exit_fsk;
				}
			}
		}
		i++;
	}
exit_fsk:
	spin_lock_irqsave(&dev->lock, flags);
	set_lcr(dev, dev->serial_config.LCR & (~UART_LCR_SBC));
	spin_unlock_irqrestore(&dev->lock, flags);
	PRINT_TIMESTAMP("end of send_fsk_data_udelay\n");

	return ret;
}

#ifdef ATARISIO_USE_HRTIMER

static int perform_send_fsk_data_hrtimer(struct atarisio_dev* dev, uint16_t* fsk_delays, unsigned int num_entries)
{
	int ret = 0;
	int bit = 0;
	int i = 0;
	unsigned long flags;
	uint8_t lcr;
	uint64_t delay;
	struct timespec ts;
	ktime_t kt;

	ktime_get_ts(&ts);
	kt = timespec_to_ktime(ts);

	PRINT_TIMESTAMP("start of send_fsk_data_hrtimer\n");

	while (i < num_entries) {
		spin_lock_irqsave(&dev->lock, flags);
		if (bit) {
			lcr = dev->serial_config.LCR & (~UART_LCR_SBC);
			bit = 0;
		} else {
			lcr = dev->serial_config.LCR | UART_LCR_SBC;
			bit = 1;
		}
		set_lcr(dev, lcr);
		spin_unlock_irqrestore(&dev->lock, flags);

		/* delay is specified in units of 100usec */
		delay = ((uint64_t)fsk_delays[i]) * 100000;
		kt = ktime_add_ns(kt, delay);

		/*
		PRINT_TIMESTAMP("bit %d: delay=%ld timeout=%ld\n", i, (long) delay, (long) ktime_to_ns(kt));
		*/

		while (1) {
			current->state = TASK_INTERRUPTIBLE;
			ret = schedule_hrtimeout(&kt, HRTIMER_MODE_ABS);
			if (ret == 0) {
				break;
			}
			DBG_PRINTK(DEBUG_STANDARD, "send_fsk_data_hrtimer: schedule_hrtimeout returned %d\n", ret);
			if (signal_pending(current)) {
				current->state=TASK_RUNNING;
				ret = -EINTR;
				break;
			}
		}

		if (ret) {
			break;
		}
		i++;
	}
	spin_lock_irqsave(&dev->lock, flags);
	set_lcr(dev, dev->serial_config.LCR & (~UART_LCR_SBC));
	spin_unlock_irqrestore(&dev->lock, flags);

	ktime_get_ts(&ts);
	kt = timespec_to_ktime(ts);

	PRINT_TIMESTAMP("end of send_fsk_data_hrtimer\n");

	return ret;
}
#endif

static int perform_send_fsk_data(struct atarisio_dev* dev, unsigned long arg)
{
	FSK_data fsk;
	uint16_t* fsk_delays;
	int ret = 0;

	if (copy_from_user(&fsk, (FSK_data*) arg, sizeof(FSK_data)) ) {
		return -EFAULT;
	}

	if (fsk.num_entries == 0) {
		return -EINVAL;
	}

	fsk_delays = kmalloc(sizeof(uint16_t) * fsk.num_entries, GFP_KERNEL);
	if (fsk_delays == 0) {
		return -EFAULT;
	}

	if (copy_from_user(fsk_delays, fsk.bit_time, sizeof(uint16_t) * fsk.num_entries)) {
		kfree(fsk_delays);
		return -EFAULT;
	}

#ifdef ATARISIO_USE_HRTIMER
	if (hrtimer) {
		ret = perform_send_fsk_data_hrtimer(dev, fsk_delays, fsk.num_entries);
	} else {
		ret = perform_send_fsk_data_busywait(dev, fsk_delays, fsk.num_entries);
	}
#else
	ret = perform_send_fsk_data_busywait(dev, fsk_delays, fsk.num_entries);
#endif
	kfree(fsk_delays);
	return ret;
}

static int check_command_frame_time(struct atarisio_dev* dev, int do_lock)
{
	unsigned long flags = 0;
	uint64_t current_time;
	int ret = 0;
	unsigned int do_delay = 0;

	if (dev->current_mode != MODE_SIOSERVER) {
		return 0;
	}
	if (do_lock) {
		spin_lock_irqsave(&dev->lock, flags);
	}
	current_time = get_timestamp();

	if (current_time > dev->cmdframe_buf.end_reception_time+10000 ) {
		DBG_PRINTK(DEBUG_STANDARD, "command frame is too old (%lu usecs)\n",
				(unsigned long) (current_time - dev->cmdframe_buf.end_reception_time));
		ret = -EATARISIO_COMMAND_TIMEOUT;
	} else {
		DBG_PRINTK(DEBUG_NOISY, "command frame age is OK (%lu usecs)\n",
				(unsigned long) (current_time - dev->cmdframe_buf.end_reception_time));
		if (do_lock && (current_time - dev->cmdframe_buf.end_reception_time < DELAY_T2_MIN)) {
			do_delay = DELAY_T2_MIN-(current_time - dev->cmdframe_buf.end_reception_time);
		}
	}
	if (do_lock) {
		spin_unlock_irqrestore(&dev->lock, flags);
	}
	if (do_delay>0) {
		udelay(do_delay);
	}
	return ret;
}

/* init commandframe buffer and statistics like counters etc. */
static inline void init_cmdframe_buf(struct atarisio_dev* dev)
{
	/* clear all values */
	reset_cmdframe_buf_data(dev);

	/* reset statistics */
	dev->cmdframe_buf.serial_number=0;
	dev->cmdframe_buf.missed_count=0;
}


static void set_1050_2_pc_mode(struct atarisio_dev* dev, int prosystem, int do_locks)
{
	unsigned long flags=0;
	int have_to_wait;

	if (do_locks) {
		spin_lock_irqsave(&dev->lock, flags);
	}

	have_to_wait = !(dev->serial_config.MCR & UART_MCR_DTR);

	dev->current_mode = MODE_1050_2_PC;
	dev->command_line_mask = ~(UART_MCR_RTS | UART_MCR_DTR);
	dev->command_line_low = UART_MCR_DTR | UART_MCR_RTS;
	if (prosystem) {
		dev->command_line_high = UART_MCR_RTS;
	} else {
		dev->command_line_high = UART_MCR_DTR;
	}

	clear_command_line(dev);
	set_lcr(dev, dev->standard_lcr);

	dev->do_autobaud = 0;
	set_baudrate(dev, dev->default_baudrate, 0);
	dev->serial_config.IER &= ~UART_IER_MSI;
	dev->serial_out(dev, UART_IER, dev->serial_config.IER);

	init_cmdframe_buf(dev);

	if (do_locks) {
		spin_unlock_irqrestore(&dev->lock, flags);
		if (have_to_wait) {
			schedule_timeout(HZ/2); /* wait 0.5 sec. for voltage supply to stabilize */
		}
	}
}

/* clear DTR and RTS to make autoswitching Atarimax interface work */
static void clear_control_lines(struct atarisio_dev* dev)
{
	dev->serial_config.MCR &= ~(UART_MCR_DTR | UART_MCR_RTS);
	dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
}

#define SIOSERVER_COMMAND_RI 0
#define SIOSERVER_COMMAND_DSR 1
#define SIOSERVER_COMMAND_CTS 2

static void set_sioserver_mode(struct atarisio_dev* dev, int command_line)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	dev->current_mode = MODE_SIOSERVER;
	switch (command_line) {
	case SIOSERVER_COMMAND_DSR:
		dev->sioserver_command_line = UART_MSR_DSR;
		dev->sioserver_command_line_delta = UART_MSR_DDSR;
		break;
	case SIOSERVER_COMMAND_CTS:
		dev->sioserver_command_line = UART_MSR_CTS;
		dev->sioserver_command_line_delta = UART_MSR_DCTS;
		break;
	default:
		if (command_line != SIOSERVER_COMMAND_RI) {
			DBG_PRINTK(DEBUG_STANDARD, "set_sioserver_mode: illegal command_line value %d\n", command_line);
		}
		dev->sioserver_command_line = UART_MSR_RI;
		dev->sioserver_command_line_delta = UART_MSR_TERI;
		break;
	}

	set_lcr(dev, dev->standard_lcr);
	set_baudrate(dev, dev->default_baudrate, 0);
	clear_control_lines(dev);
	dev->serial_config.IER |= UART_IER_MSI;
	dev->serial_out(dev, UART_IER, dev->serial_config.IER);

	init_cmdframe_buf(dev);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static inline void reset_rx_buf(struct atarisio_dev* dev)
{
	if (dev->rx_buf.head!=dev->rx_buf.tail) {
		DBG_PRINTK(DEBUG_STANDARD, "reset_rx_buf: flushing %d characters\n",
			(dev->rx_buf.head+IOBUF_LENGTH - dev->rx_buf.tail) % IOBUF_LENGTH);
	}

	dev->rx_buf.head=dev->rx_buf.tail=0;
	dev->rx_buf.wakeup_len=-1;
}

static inline void reset_tx_buf(struct atarisio_dev* dev)
{
	if (dev->tx_buf.head!=dev->tx_buf.tail) {
		DBG_PRINTK(DEBUG_STANDARD, "reset_tx_buf: flushing %d characters\n",
			(dev->tx_buf.head+IOBUF_LENGTH - dev->tx_buf.tail) % IOBUF_LENGTH);
	}

	dev->tx_buf.head=dev->tx_buf.tail=0;
}


static int get_command_frame(struct atarisio_dev* dev, unsigned long arg)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	wait_queue_entry_t wait;
#else
	wait_queue_t wait;
#endif
	SIO_command_frame frame;
	signed long expire = HZ;
	signed long end_time = jiffies + HZ;
	unsigned long flags;

again:
	expire = end_time - jiffies;
	if (expire <= 0) {
		return -EATARISIO_COMMAND_TIMEOUT;
	}

	init_waitqueue_entry(&wait, current);

	add_wait_queue(&dev->cmdframe_queue, &wait);
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		if ( dev->cmdframe_buf.is_valid ) {
			PRINT_TIMESTAMP("found valid command frame\n");
			DBG_PRINTK(DEBUG_NOISY, "found valid command frame\n");
			break;
		}
		expire = schedule_timeout(expire);
		if (expire==0) {
			break;
		}
		if (signal_pending(current)) {
			current->state=TASK_RUNNING;
			remove_wait_queue(&dev->cmdframe_queue, &wait);
			DBG_PRINTK(DEBUG_STANDARD, "got signal while waiting for command frame\n");
			return -EINTR;
		}
	}
	current->state=TASK_RUNNING;
	remove_wait_queue(&dev->cmdframe_queue, &wait);

	spin_lock_irqsave(&dev->lock, flags);
	if (!dev->cmdframe_buf.is_valid ) {
		spin_unlock_irqrestore(&dev->lock, flags);
		DBG_PRINTK(DEBUG_STANDARD, "waiting for command frame timed out\n");
		return -EATARISIO_COMMAND_TIMEOUT;
	}

	if (check_command_frame_time(dev, 0)) {
		dev->cmdframe_buf.is_valid = 0;
		spin_unlock_irqrestore(&dev->lock, flags);
		goto again;
	}

	frame.device_id   = dev->cmdframe_buf.buf[0];
	frame.command     = dev->cmdframe_buf.buf[1];
	frame.aux1        = dev->cmdframe_buf.buf[2];
	frame.aux2        = dev->cmdframe_buf.buf[3];
	frame.reception_timestamp = dev->cmdframe_buf.end_reception_time;
	frame.missed_count = dev->cmdframe_buf.missed_count;
	dev->cmdframe_buf.missed_count=0;
	dev->current_cmdframe_serial_number = dev->cmdframe_buf.serial_number;
	dev->cmdframe_buf.is_valid = 0; /* already got that :-) */
	reset_tx_buf(dev);
	reset_rx_buf(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	if (copy_to_user((SIO_command_frame*) arg, &frame, sizeof(SIO_command_frame)) ) {
		return -EFAULT;
	}
	return 0;
}


static void print_status(struct atarisio_dev* dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	switch (dev->current_mode) {
	case MODE_1050_2_PC:
		PRINTK("current_mode: 1050-2-PC\n"); break;
	case MODE_SIOSERVER:
		PRINTK("current_mode: sioserver\n"); break;
	default:
		PRINTK("current_mode: unknown - %d\n", dev->current_mode);
		break;
	}

	PRINTK("rx_buf: head=%d tail=%d wakeup_len=%d\n",
		dev->rx_buf.head,
		dev->rx_buf.tail,
		dev->rx_buf.wakeup_len);

	PRINTK("tx_buf: head=%d tail=%d\n",
		dev->tx_buf.head,
		dev->tx_buf.tail);

	PRINTK("cmdframe_buf: valid=%d serial=%d missed=%d\n",
		dev->cmdframe_buf.is_valid, 
		dev->cmdframe_buf.serial_number,
		dev->cmdframe_buf.missed_count);

	PRINTK("cmdframe: receiving=%d pos=%d rx=%d er=%d brk=%d\n",
		dev->cmdframe_buf.receiving,
		dev->cmdframe_buf.pos,
		dev->cmdframe_buf.ok_chars,
		dev->cmdframe_buf.error_chars,
		dev->cmdframe_buf.break_chars);

	PRINTK("serial_config: baudrate=%d exact_baudrate=%d\n",
		dev->serial_config.baudrate,
		dev->serial_config.exact_baudrate);

	PRINTK("default_baudrate=%d highspeed_baudrate=%d do_autobaud=%d\n",
		dev->default_baudrate,
		dev->highspeed_baudrate,
		dev->do_autobaud);

	PRINTK("tape_baudrate=%d\n",
		dev->tape_baudrate);

	PRINTK("command_line=0x%02x delta=0x%02x\n",
		dev->sioserver_command_line,
		dev->sioserver_command_line_delta);

	PRINTK("UART RBR = 0x%02x\n", dev->serial_in(dev, UART_RX));
	PRINTK("UART IER = 0x%02x\n", dev->serial_in(dev, UART_IER));
	PRINTK("UART IIR = 0x%02x\n", dev->serial_in(dev, UART_IIR));
	PRINTK("UART LCR = 0x%02x\n", dev->serial_in(dev, UART_LCR));
	PRINTK("UART MCR = 0x%02x\n", dev->serial_in(dev, UART_MCR));
	PRINTK("UART LSR = 0x%02x\n", dev->serial_in(dev, UART_LSR));
	PRINTK("UART MSR = 0x%02x\n", dev->serial_in(dev, UART_MSR));

	spin_unlock_irqrestore(&dev->lock, flags);
}

#ifdef HAVE_UNLOCKED_IOCTL
static long atarisio_unlocked_ioctl(struct file* filp,
 	unsigned int cmd, unsigned long arg)
#else
static int atarisio_ioctl(struct inode* inode, struct file* filp,
 	unsigned int cmd, unsigned long arg)
#endif
{
	int ret=0;

	struct atarisio_dev* dev = filp->private_data;
	if (!dev) {
		PRINTK_NODEV("cannot get device information!\n");
		return -ENOTTY;
	}

	/*
	 * check for illegal commands
	 */
	if (_IOC_TYPE(cmd) != ATARISIO_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > ATARISIO_IOC_MAXNR) return -ENOTTY;

	if (cmd != ATARISIO_IOC_GET_TIMESTAMPS) {
		timestamp_entering_ioctl(dev);
	}

        DBG_PRINTK(DEBUG_VERY_NOISY, "ioctl 0x%x , 0x%lx\n", cmd, arg);

	switch (cmd) {
	case ATARISIO_IOC_GET_VERSION:
		ret = ATARISIO_VERSION;
		break;
	case ATARISIO_IOC_SET_MODE:
		switch (arg) {
		case ATARISIO_MODE_1050_2_PC:
			set_1050_2_pc_mode(dev, 0, 1);
			break;
		case ATARISIO_MODE_PROSYSTEM:
			set_1050_2_pc_mode(dev, 1, 1);
			break;
		case ATARISIO_MODE_SIOSERVER:
			set_sioserver_mode(dev, SIOSERVER_COMMAND_RI);
			break;
		case ATARISIO_MODE_SIOSERVER_DSR:
			set_sioserver_mode(dev, SIOSERVER_COMMAND_DSR);
			break;
		case ATARISIO_MODE_SIOSERVER_CTS:
			set_sioserver_mode(dev, SIOSERVER_COMMAND_CTS);
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case ATARISIO_IOC_SET_BAUDRATE:
		ret = set_baudrate(dev, arg, 1);
		break;
	case ATARISIO_IOC_SET_STANDARD_BAUDRATE:
		dev->default_baudrate = arg;
		break;
	case ATARISIO_IOC_SET_HIGHSPEED_BAUDRATE:
		dev->highspeed_baudrate = arg;
		break;
	case ATARISIO_IOC_SET_AUTOBAUD:
		dev->do_autobaud = arg;
		if (dev->do_autobaud == 0) {
			ret = set_baudrate(dev, dev->default_baudrate, 1);
		}
		break;
	case ATARISIO_IOC_DO_SIO:
		ret = perform_sio(dev, (SIO_parameters*)arg);
		break;
	case ATARISIO_IOC_GET_COMMAND_FRAME:
		PRINT_TIMESTAMP("start getting command frame\n");
		ret = get_command_frame(dev, arg);
		PRINT_TIMESTAMP("end getting command frame\n");
		break;
	case ATARISIO_IOC_SEND_COMMAND_ACK:
	case ATARISIO_IOC_SEND_COMMAND_ACK_XF551:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		if ((ret = check_command_frame_time(dev, 1))) {
			break;
		}
		PRINT_TIMESTAMP("start sending command ACK\n");
		ret = send_single_character(dev, COMMAND_FRAME_ACK_CHAR);
		PRINT_TIMESTAMP("end sending command ACK\n");
		if (cmd == ATARISIO_IOC_SEND_COMMAND_ACK_XF551) {
			set_baudrate(dev, ATARISIO_XF551_BAUDRATE, 1);
		}
		break;
	case ATARISIO_IOC_SEND_COMMAND_NAK:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		if ((ret = check_command_frame_time(dev, 1))) {
			break;
		}
		ret = send_single_character(dev, COMMAND_FRAME_NAK_CHAR);
		break;
	case ATARISIO_IOC_SEND_DATA_ACK:
		udelay(DELAY_T4_MIN);
		ret = send_single_character(dev, COMMAND_FRAME_ACK_CHAR);
		break;
	case ATARISIO_IOC_SEND_DATA_NAK:
		udelay(DELAY_T4_MIN);
		ret = send_single_character(dev, COMMAND_FRAME_NAK_CHAR);
		break;
	case ATARISIO_IOC_SEND_COMPLETE:
	case ATARISIO_IOC_SEND_COMPLETE_XF551:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		PRINT_TIMESTAMP("start sending complete\n");
		if ((dev->add_highspeedpause & ATARISIO_HIGHSPEEDPAUSE_FRAME_DELAY) || cmd == ATARISIO_IOC_SEND_COMPLETE_XF551) {
			udelay(DELAY_T5_MIN_SLOW);
		} else {
			udelay(DELAY_T5_MIN);
		}
		ret = send_single_character(dev, OPERATION_COMPLETE_CHAR);
		PRINT_TIMESTAMP("end sending complete\n");
		if (cmd == ATARISIO_IOC_SEND_COMPLETE_XF551) {
			set_baudrate(dev, dev->default_baudrate, 1);
		}
		break;
	case ATARISIO_IOC_SEND_ERROR:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		udelay(DELAY_T5_MIN_SLOW);
		ret = send_single_character(dev, OPERATION_ERROR_CHAR);
		break;
	case ATARISIO_IOC_SEND_DATA_FRAME:
	case ATARISIO_IOC_SEND_DATA_FRAME_XF551:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		PRINT_TIMESTAMP("begin send data frame\n");
		/* delay a short time before transmitting data frame */
		/* most SIO codes don't like it if data follows immediately after complete */
		udelay(DELAY_T3_PERIPH);

		ret = perform_send_frame(dev, arg, 1, SEND_MODE_WAIT_ALL);
		PRINT_TIMESTAMP("end send data frame\n");
		if (cmd == ATARISIO_IOC_SEND_DATA_FRAME_XF551) {
			set_baudrate(dev, dev->default_baudrate, 1);
		}
		break;
	case ATARISIO_IOC_RECEIVE_DATA_FRAME:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		ret = perform_receive_frame(dev, arg, 1);
		if (ret == 0 || ret == -EATARISIO_CHECKSUM_ERROR) {
			udelay(DELAY_T4_MIN);
			if (ret == 0) {
				ret=send_single_character(dev, DATA_FRAME_ACK_CHAR);
			} else {
				send_single_character(dev, DATA_FRAME_NAK_CHAR);
			}
		}
		break;
	case ATARISIO_IOC_SEND_RAW_FRAME:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		PRINT_TIMESTAMP("begin send raw frame\n");
		ret = perform_send_frame(dev, arg, 0, SEND_MODE_WAIT_ALL);
		PRINT_TIMESTAMP("end send raw frame\n");
		break;
	case ATARISIO_IOC_RECEIVE_RAW_FRAME:
		if ((ret = check_new_command_frame(dev))) {
			break;
		}
		ret = perform_receive_frame(dev, arg, 0);
		break;
	case ATARISIO_IOC_GET_BAUDRATE:
		ret = dev->serial_config.baudrate;
		break;
	case ATARISIO_IOC_SET_HIGHSPEEDPAUSE:
		dev->add_highspeedpause = arg;
		break;
	case ATARISIO_IOC_PRINT_STATUS:
		print_status(dev);
		break;
	case ATARISIO_IOC_ENABLE_TIMESTAMPS:
		dev->enable_timestamp_recording = (int) arg;
		break;
	case ATARISIO_IOC_GET_TIMESTAMPS:
		if (!dev->enable_timestamp_recording) {
			ret = -EINVAL;
		} else {
			if (copy_to_user((SIO_timestamps*)arg, (uint8_t*) &dev->timestamps, sizeof(SIO_timestamps))) {
				ret = -EFAULT;
			}
		}
		break;
	case ATARISIO_IOC_SET_TAPE_BAUDRATE:
		dev->tape_baudrate = arg;
		if (dev->tape_mode_enabled) {
			ret = set_baudrate(dev, arg, 1);
		}
		break;
	case ATARISIO_IOC_SEND_TAPE_BLOCK:
		PRINT_TIMESTAMP("begin send tape block\n");
		ret = perform_send_tape_block(dev, arg);
		PRINT_TIMESTAMP("end send tape block\n");
		break;
	case ATARISIO_IOC_DO_EXT_SIO:
		ret = perform_ext_sio(dev, (Ext_SIO_parameters*)arg);
		break;
	case ATARISIO_IOC_GET_EXACT_BAUDRATE:
		ret = dev->serial_config.exact_baudrate;
		break;
	case ATARISIO_IOC_START_TAPE_MODE:
		PRINT_TIMESTAMP("start tape mode\n");
		ret = start_tape_mode(dev);
		break;
	case ATARISIO_IOC_END_TAPE_MODE:
		PRINT_TIMESTAMP("end tape mode\n");
		ret = end_tape_mode(dev);
		break;
	case ATARISIO_IOC_SEND_RAW_DATA_NOWAIT:
		PRINT_TIMESTAMP("begin send raw data nowait\n");
		ret = perform_send_frame(dev, arg, 0, SEND_MODE_WAIT_BUFFER);
		PRINT_TIMESTAMP("end send raw data nowait\n");
		break;
	case ATARISIO_IOC_FLUSH_WRITE_BUFFER:
		PRINT_TIMESTAMP("begin flush write buffer\n");
		ret = wait_send(dev, 0, SEND_MODE_WAIT_ALL);
		PRINT_TIMESTAMP("end flush write buffer\n");
		break;
	case ATARISIO_IOC_SEND_FSK_DATA:
		if ((ret = wait_send(dev, 0, SEND_MODE_WAIT_ALL))) {
			break;
		}
		ret = perform_send_fsk_data(dev, arg);
		break;
	case ATARISIO_IOC_GET_BAUDRATE_FOR_POKEY_DIVISOR:
		ret = pokey_div_to_baud(dev, (unsigned int) arg);
		break;
	default:
		ret = -EINVAL;
	}

	if (cmd != ATARISIO_IOC_GET_TIMESTAMPS) {
		timestamp_leaving_ioctl(dev);
	}

	return ret;
}

static unsigned int atarisio_poll(struct file* filp, poll_table* wait)
{
	unsigned int mask=0;

	struct atarisio_dev* dev = filp->private_data;
	if (!dev) {
		PRINTK_NODEV("cannot get device information!\n");
		return -ENOTTY;
	}

	poll_wait(filp, &dev->cmdframe_queue, wait);

	if (dev->cmdframe_buf.is_valid) {
		mask |= POLLPRI;
	}

	return mask;
}

/* old IRQ flags deprecated since linux 2.6.22 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#define MY_IRQFLAGS (IRQF_SHARED)
#else
#define MY_IRQFLAGS (SA_SHIRQ)
#endif



static int atarisio_open(struct inode* inode, struct file* filp)
{
	unsigned long flags;
	struct atarisio_dev* dev = 0;
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
       	MOD_INC_USE_COUNT;
#endif
	if (MINOR(inode->i_rdev) < minor) {
		ret = -ENOTTY;
		goto exit_open;
	}
	if (MINOR(inode->i_rdev) >= minor + ATARISIO_MAXDEV) {
		ret = -ENOTTY;
		goto exit_open;
	}

	dev = atarisio_devices[MINOR(inode->i_rdev) - minor];

	if (!dev) {
		PRINTK_NODEV("cannot get device information!\n");
		ret = -ENOTTY;
		goto exit_open;
	}

	if (dev->busy) {
		ret = -EBUSY;
		goto exit_open;
	}
	dev->busy=1;
	filp->private_data = dev;

	spin_lock_irqsave(&dev->lock, flags);

	dev->current_mode = MODE_1050_2_PC;

	reset_rx_buf(dev);
	reset_tx_buf(dev);
	init_cmdframe_buf(dev);

	dev->do_autobaud = 0;
	dev->default_baudrate = pokey_div_to_baud(dev, ATARISIO_POKEY_DIVISOR_STANDARD);
	dev->highspeed_baudrate = pokey_div_to_baud(dev, ATARISIO_POKEY_DIVISOR_3XSIO);
	dev->tape_baudrate = ATARISIO_TAPE_BAUDRATE;
	dev->add_highspeedpause = ATARISIO_HIGHSPEEDPAUSE_OFF;
	dev->tape_mode_enabled = 0;

	if (dev->is_16c950) {
		dev->serial_config.ACR = 0;

		/* now reset the 16c950 so we are in a known state */
		write_icr(dev, UART_CSR, 0);

		if (dev->use_16c950_mode) {
			/* set bit 4 of EFT to enable 16C950 mode */
			dev->serial_out(dev, UART_LCR, 0xbf);
			dev->serial_out(dev, UART_EFR, 0x10);

			dev->serial_out(dev, UART_LCR, 0);

			/* enable 16C950 FIFO trigger levels */
			dev->serial_config.ACR = 0x20; 
			write_icr(dev, UART_ACR, dev->serial_config.ACR);

			/* set receiver and transmitter trigger levels to 1 */
			write_icr(dev, UART_RTL, 1);
			write_icr(dev, UART_TTL, 1);

			/* set clock prescaler to 4 instead of 16 */
			/* write_icr(dev, UART_TCR, 4); */

/*
			PRINTK("MCR = %02x CPR = %02x\n",
				dev->serial_in(dev, UART_MCR),
				read_icr(dev, UART_CPR));
*/

			/* TESTING CPR */
			/* set clock prescaler to 1 */
			/*
			dev->serial_config.MCR |= 0x80;
			dev->serial_out(dev, UART_MCR, dev->serial_config.MCR);
			write_icr(dev, UART_CPR, 8);
			*/

			/* PRINTK("CPR = %02x\n", read_icr(dev, UART_CPR)); */
		} else {
			DBG_PRINTK(DEBUG_STANDARD, "disabled extended 16C950 features\n");
			/* clear the EFR to set 16C550 mode */
			dev->serial_out(dev, UART_LCR, 0xbf);
			dev->serial_out(dev, UART_EFR, 0);
			dev->serial_out(dev, UART_LCR, 0);
		}
	}

	dev->serial_config.baudrate = dev->default_baudrate;
	dev->serial_config.exact_baudrate = dev->default_baudrate;
	dev->serial_config.just_switched_baud = 1;
	dev->serial_config.IER = UART_IER_RDI;
	dev->serial_config.MCR = UART_MCR_OUT2;
			/* OUT2 is essential for enabling interrupts - when it's required :-) */
	dev->serial_config.LCR = dev->standard_lcr; /* 1 stopbit, no parity */

	dev->serial_config.FCR = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;

	dev->serial_out(dev, UART_IER, 0);

	dev->serial_out(dev, UART_LCR, dev->serial_config.LCR);

	force_set_baudrate(dev, dev->serial_config.baudrate);

	set_1050_2_pc_mode(dev, 0, 0); /* this call sets MCR */

	reset_fifos(dev);

	/*
	 * clear any interrupts
	 */
        (void)dev->serial_in(dev, UART_LSR);
	(void)dev->serial_in(dev, UART_IIR);
	(void)dev->serial_in(dev, UART_RX);
	(void)dev->serial_in(dev, UART_MSR);

	spin_unlock_irqrestore(&dev->lock, flags);

	if (request_irq(dev->irq, atarisio_interrupt, MY_IRQFLAGS, dev->devname, dev)) {
		PRINTK("could not register interrupt %d\n",dev->irq);
		dev->busy=0;
		ret = -EFAULT;
		goto exit_open;
	}

	spin_lock_irqsave(&dev->lock, flags);

	/* enable UART interrupts */
	dev->serial_out(dev, UART_IER, dev->serial_config.IER);

	spin_unlock_irqrestore(&dev->lock, flags);

exit_open:

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	if (ret != 0) {
        	MOD_DEC_USE_COUNT;
	}
#endif
	return ret;
}

static int atarisio_release(struct inode* inode, struct file* filp)
{
	unsigned long flags;

	struct atarisio_dev* dev = filp->private_data;
	if (!dev) {
		PRINTK_NODEV("cannot get device information!\n");
		return -ENOTTY;
	}

	spin_lock_irqsave(&dev->lock, flags);

	dev->serial_out(dev, UART_MCR,0);
	dev->serial_out(dev, UART_IER,0);

	spin_unlock_irqrestore(&dev->lock, flags);

	free_irq(dev->irq, dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        MOD_DEC_USE_COUNT;
#endif

	dev->busy=0;
	return 0;
}

static struct file_operations atarisio_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	owner:		THIS_MODULE,
#endif
	poll:		atarisio_poll,
#ifdef HAVE_UNLOCKED_IOCTL
	unlocked_ioctl:	atarisio_unlocked_ioctl,
#else
	ioctl:		atarisio_ioctl,
#endif
	open:		atarisio_open,
	release:	atarisio_release
};

struct atarisio_dev* alloc_atarisio_dev(unsigned int id)
{
	struct atarisio_dev* dev;
	if (id >= ATARISIO_MAXDEV) {
		PRINTK_NODEV("alloc_atarisio_dev called with invalid id %d\n", id);
		return 0;
	}
	if (atarisio_devices[id]) {
		PRINTK_NODEV("device %d already allocated\n", id);
		return 0;
	}

	dev = kmalloc(sizeof(struct atarisio_dev), GFP_KERNEL);
	if (!dev) {
		goto alloc_fail;
	}
	memset(dev, 0, sizeof(struct atarisio_dev));

	dev->devname = kmalloc(20, GFP_KERNEL);
	if (!dev->devname) {
		kfree(dev);
		goto alloc_fail;
	}
	sprintf(dev->devname,"%s%d",NAME, id);

	dev->miscdev = kmalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!dev->miscdev) {
		kfree(dev->devname);
		kfree(dev);
		goto alloc_fail;
	}
	memset(dev->miscdev, 0, sizeof(struct miscdevice));

	atarisio_devices[id] = dev;
	dev->is_16c950 = 0;
	dev->use_16c950_mode = 1;
	dev->serial_config.ACR = 0;
	dev->busy = 0;
	dev->id = id;
	spin_lock_init(&dev->lock);
	dev->current_mode = MODE_1050_2_PC;
	dev->last_msr = 0;
	dev->enable_timestamp_recording = 0;
	dev->rx_buf.head = 0;
	dev->rx_buf.tail = 0;
	dev->sioserver_command_line = UART_MSR_RI;
	dev->sioserver_command_line_delta = UART_MSR_TERI;
	dev->command_line_mask =  ~UART_MCR_RTS;
	dev->command_line_low = UART_MCR_RTS;
	dev->command_line_high = 0;
	dev->default_baudrate = ATARISIO_STANDARD_BAUDRATE;
	dev->highspeed_baudrate = ATARISIO_HIGHSPEED_BAUDRATE;
	dev->tape_baudrate = ATARISIO_TAPE_BAUDRATE;
	dev->do_autobaud = 0;
	dev->tape_mode_enabled = 0;
	dev->add_highspeedpause = ATARISIO_HIGHSPEEDPAUSE_OFF;
	dev->standard_lcr = UART_LCR_WLEN8;
	dev->slow_lcr = UART_LCR_WLEN8 | UART_LCR_STOP | UART_LCR_PARITY | UART_LCR_SPAR;
/*
	dev->slow_lcr = UART_LCR_WLEN8 | UART_LCR_STOP;
*/
	dev->need_reenable = 0;

	init_waitqueue_head(&dev->rx_queue);
	init_waitqueue_head(&dev->tx_queue);
	init_waitqueue_head(&dev->cmdframe_queue);

	dev->miscdev->name = dev->devname;
	dev->miscdev->fops = &atarisio_fops;
	dev->miscdev->minor = minor + dev->id;

	return dev;

alloc_fail:
	PRINTK_NODEV("cannot allocate atarisio_dev structure %d\n", id);
	return 0;
}

int free_atarisio_dev(unsigned int id)
{
	struct atarisio_dev* dev;
	if (id >= ATARISIO_MAXDEV) {
		PRINTK_NODEV("free_atarisio_dev called with invalid id %d\n", id);
		return 0;
	}
	dev = atarisio_devices[id];
	if (!dev) {
		PRINTK_NODEV("attempt to free non-allocated atarisio_dev %d\n", id);
		return -EINVAL;
	}
	kfree(dev->miscdev);
	dev->miscdev = 0;

	kfree(dev->devname);
	dev->devname = 0;
	kfree(dev);
	atarisio_devices[id] = 0;
	return 0;
}

static long ioctl_wrapper(struct atarisio_dev* dev, struct file* f, unsigned int cmd, unsigned long arg)
{
#ifdef HAVE_UNLOCKED_IOCTL
	if (f->f_op->unlocked_ioctl) {
		return f->f_op->unlocked_ioctl(f, cmd, arg);
	}
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	return f->f_op->ioctl(f->f_dentry->d_inode, f, cmd, arg);
#else
	PRINTK("neither unlocked_ioctl nor ioctl available!\n");
	return -EINVAL;
#endif
}

static int disable_serial_port(struct atarisio_dev* dev)
{
	mm_segment_t fs;
	struct file* f;
	struct dentry* de;
	struct serial_struct ss;

	my_lock_kernel();

	fs = get_fs();
	set_fs(get_ds());

	f = filp_open(dev->port,O_RDWR | O_NONBLOCK,0);
	if (IS_ERR(f)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error opening serial port %s\n", dev->port);
		goto fail;
	}
#ifdef HAVE_UNLOCKED_IOCTL
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	if (f->f_op && (f->f_op->ioctl || f->f_op->unlocked_ioctl) ) {
#else
	if (f->f_op && (f->f_op->unlocked_ioctl) ) {
#endif
#else
	if (f->f_op && f->f_op->ioctl) {
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
		de = f->f_dentry;
#else
		de = f->f_path.dentry;
#endif
		if (de && de->d_inode) {
			if (ioctl_wrapper(dev, f, TIOCGSERIAL, (unsigned long) &ss)) {
				DBG_PRINTK_NODEV(DEBUG_STANDARD, "TIOCGSERIAL failed\n");
				goto fail_close;
			}
			if (ioctl_wrapper(dev, f, TIOCGSERIAL, (unsigned long) &dev->orig_serial_struct)) {
				DBG_PRINTK_NODEV(DEBUG_STANDARD, "TIOCGSERIAL failed\n");
				goto fail_close;
			}
			switch (ss.type) {
			case PORT_16550:
			case PORT_16550A:
#ifdef PORT_NS16550A
			case PORT_NS16550A:
#endif
#ifdef PORT_16C950
			case PORT_16C950:
#endif
				break;
			default:
				PRINTK_NODEV("illegal port type %d - only 16550(A) and 16C950 are supported\n", ss.type);
				goto fail_close;
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			DBG_PRINTK_NODEV(DEBUG_STANDARD, "ss.port = 0x%04x ss.iomem_base = 0x%lx ss.irq = %d ss.type = %d\n",
				ss.port, (long) ss.iomem_base, ss.irq, ss.type);

			if (dev->mapbase == 0) {
				dev->mapbase = (size_t) ss.iomem_base;
			}
#else
			DBG_PRINTK_NODEV(DEBUG_STANDARD, "ss.port = 0x%04x ss.irq = %d ss.type = %d\n",
				ss.port, ss.irq, ss.type);
#endif

			if (dev->io == 0) {
				dev->io = ss.port;
			}

			if (dev->irq == 0) {
				dev->irq = ss.irq;
			}

			if (dev->serial_config.baud_base == 0) {
				dev->serial_config.baud_base = ss.baud_base;
			}

			/* disable serial driver by setting the uart type to none */
			ss.type = PORT_UNKNOWN;

			if (ioctl_wrapper(dev, f, TIOCSSERIAL, (unsigned long) &ss)) {
				DBG_PRINTK_NODEV(DEBUG_STANDARD, "TIOCSSERIAL failed\n");
				goto fail_close;
			}
			dev->need_reenable = 1;
		} else {
			DBG_PRINTK_NODEV(DEBUG_STANDARD, "unable to get inode of %s\n", dev->port);
			goto fail_close;
		}
	} else {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "device doesn't provide ioctl function!\n");
		goto fail_close;
	}

	if (filp_close(f,NULL)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error closing serial port %s\n", dev->port);
		goto fail;
	}
	set_fs(fs);
	my_unlock_kernel();
	return 0;

fail_close:
	if (filp_close(f,NULL)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error closing serial port %s\n", dev->port);
		goto fail;
	}

fail:
	set_fs(fs);
	my_unlock_kernel();
	return 1;
}

static int reenable_serial_port(struct atarisio_dev* dev)
{
	mm_segment_t fs;
	struct file* f;
	struct dentry* de;

	if (!dev->need_reenable) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "unnecessary call to reenable_serial_port\n");
		return 0;
	}

	my_lock_kernel();
	fs = get_fs();
	set_fs(get_ds());

	f = filp_open(dev->port,O_RDWR,0);
	if (IS_ERR(f)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error opening serial port %s\n", dev->port);
		goto fail;
	}
#ifdef HAVE_UNLOCKED_IOCTL
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	if (f->f_op && (f->f_op->ioctl || f->f_op->unlocked_ioctl) ) {
#else
	if (f->f_op && (f->f_op->unlocked_ioctl) ) {
#endif
#else
	if (f->f_op && f->f_op->ioctl) {
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
		de = f->f_dentry;
#else
		de = f->f_path.dentry;
#endif
		if (de && de->d_inode) {
			if (ioctl_wrapper(dev, f, TIOCSSERIAL, (unsigned long) &dev->orig_serial_struct)) {
				DBG_PRINTK_NODEV(DEBUG_STANDARD, "TIOCSSERIAL failed\n");
				goto fail_close;
			}
		} else {
			DBG_PRINTK_NODEV(DEBUG_STANDARD, "unable to get inode of %s\n", dev->port);
			goto fail_close;
		}
	} else {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "device doesn't provide ioctl function!\n");
		goto fail_close;
	}

	if (filp_close(f,NULL)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error closing serial port %s\n", dev->port);
		goto fail;
	}
	set_fs(fs);
	my_unlock_kernel();
	return 0;

fail_close:
	if (filp_close(f,NULL)) {
		DBG_PRINTK_NODEV(DEBUG_STANDARD, "error closing serial port %s\n", dev->port);
		goto fail;
	}

fail:
	set_fs(fs);
	my_unlock_kernel();
	return 1;
}

static int request_resources(struct atarisio_dev* dev)
{
	if (dev->use_mmio) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
		if (check_mem_region(dev->mapbase, 8)) {
			PRINTK_NODEV("cannot access MMIO ports 0x%lx-0x%lx\n",
				(unsigned long)dev->mapbase,
				(unsigned long)(dev->mapbase+7));
			return -EBUSY;
		}
		request_mem_region(dev->io, 8, NAME);
#else
		if (!request_mem_region(dev->mapbase, 8, NAME)) {
			PRINTK_NODEV("cannot access MMIO ports 0x%lx-0x%lx\n",
				(unsigned long)dev->mapbase,
				(unsigned long)(dev->mapbase+7));
			return -EBUSY;
		}
#endif

		dev->membase = ioremap_nocache(dev->mapbase, 8);
		if (!dev->membase) {
			PRINTK_NODEV("cannot map MMIO ports 0x%lx-0x%lx\n",
				(unsigned long)dev->mapbase,
				(unsigned long)(dev->mapbase+7));
			release_mem_region(dev->mapbase, 8);
			return -ENOMEM;
		}
	} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
		if (check_region(dev->io, 8)) {
			PRINTK_NODEV("cannot access IO ports 0x%04x-0x%04x\n",dev->io,dev->io+7);
			return -EBUSY;
		}
		request_region(dev->io, 8, NAME);
#else
		if (!request_region(dev->io, 8, NAME)) {
			PRINTK_NODEV("cannot access IO ports 0x%04x-0x%04x\n",dev->io,dev->io+7);
			return -EBUSY;
		}
#endif
	}
	return 0;
}

static void release_resources(struct atarisio_dev* dev)
{
	if (dev->use_mmio) {
		iounmap(dev->membase);
		release_mem_region(dev->mapbase, 8);
	} else {
		release_region(dev->io, 8);
	}
}

static int check_register_atarisio(struct atarisio_dev* dev)
{
	int ret;

	if (dev->io) {
		dev->use_mmio = 0;
		dev->serial_in = io_serial_in;
		dev->serial_out = io_serial_out;
	} else if (dev->mapbase) {
		dev->use_mmio = 1;
		dev->serial_in = mem_serial_in;
		dev->serial_out = mem_serial_out;
	} else {
		PRINTK_NODEV("neither IO nor MMIO ports specified\n");
		ret = -EINVAL;
		goto failure;
	}

	ret = request_resources(dev);
	if (ret) {
		goto failure;
	}

	/*
	 * check for 16550:
	 * this is just a very simple test using the scratch register
	 */
	dev->serial_out(dev, UART_SCR,0xaa);
	if (dev->serial_in(dev, UART_SCR) != 0xaa) {
		PRINTK_NODEV("couldn't detect 16550/16C950\n");
		ret = -ENODEV;
		goto failure_release;
	}

	dev->serial_out(dev, UART_SCR,0x55);
	if (dev->serial_in(dev, UART_SCR) != 0x55) {
		PRINTK_NODEV("couldn't detect 16550/16C950\n");
		ret = -ENODEV;
		goto failure_release;
	}

	if (detect_16c950(dev)) {
		dev->is_16c950 = 1;
		if (dev->use_16c950_mode) {
			PRINTK_NODEV("using extended 16C950 features\n");
		}
	} else {
		dev->is_16c950 = 0;
	}

	/*
	 * checks are OK so far, try to register the device
	 */
	if (misc_register(dev->miscdev))
	{
		PRINTK_NODEV("failed to register device %s (minor %d)\n", dev->miscdev->name, dev->miscdev->minor);
		ret = -ENODEV;
		goto failure_release;
	}

	if (dev->port) {
		PRINTK("minor=%d port=%s %s=0x%lx irq=%d baud_base=%ld\n",
		dev->miscdev->minor, dev->port,
		dev->use_mmio ? "mmio" : "io",
		(unsigned long) (dev->use_mmio ? dev->mapbase : dev->io),
		dev->irq, dev->serial_config.baud_base);
	} else {
		PRINTK("minor=%d %s=0x%lx irq=%d baud_base=%ld\n",
		dev->miscdev->minor,
		dev->use_mmio ? "mmio" : "io",
		(unsigned long) (dev->use_mmio ? dev->mapbase : dev->io),
		dev->irq, dev->serial_config.baud_base);
	}
	return 0;

failure_release:
	release_resources(dev);

failure:
	return ret;
}

static int atarisio_is_initialized = 0;

static void atarisio_cleanup_module(void)
{
	int i;
	struct atarisio_dev* dev;

	for (i=0;i<ATARISIO_MAXDEV;i++) {
		if ((dev=atarisio_devices[i])) {
			misc_deregister(dev->miscdev);

			release_resources(dev);
			if (dev->need_reenable) {
				if (reenable_serial_port(dev)) {
					PRINTK_NODEV("error re-enabling serial port!\n");
				} else {
					DBG_PRINTK_NODEV(DEBUG_STANDARD, "successfully re-enabled serial port %s\n", dev->port);
				}
			}
			free_atarisio_dev(i);
		}
	}
	PRINTK_NODEV("module terminated\n");
	atarisio_is_initialized = 0;
}  

static int atarisio_init_module(void)
{
	int i;
	int numdev = 0;
	int numtried = 0;
	atarisio_is_initialized = 0;

	printk("AtariSIO kernel driver V%d.%02d (c) 2002-2019 Matthias Reichl\n",
		ATARISIO_MAJOR_VERSION, ATARISIO_MINOR_VERSION);

	for (i=0;i<ATARISIO_MAXDEV;i++) {
		if ((port[i] && port[i][0]) || io[i] || mmio[i] || irq[i]) {
			struct atarisio_dev* dev = alloc_atarisio_dev(i);
			numtried++;
			if (!dev) {
				atarisio_cleanup_module();
				return -ENOMEM;
			}

			dev->port = 0;
			dev->io = io[i];
			dev->mapbase = (size_t) mmio[i];
			dev->irq = irq[i];
			if (baud_base[i]) {
				dev->serial_config.baud_base = baud_base[i];
				DBG_PRINTK_NODEV(DEBUG_STANDARD, "using a baud_base of %ld\n", dev->serial_config.baud_base);
			} else {
				dev->serial_config.baud_base = 0;
			}
			dev->use_16c950_mode = ext_16c950[i];

			if (port[i] && port[i][0]) {
				dev->port = port[i];
				if (disable_serial_port(dev)) {
					PRINTK_NODEV("unable to disable serial port %s\n", dev->port);
					free_atarisio_dev(i);
					continue;
				} else {
					DBG_PRINTK_NODEV(DEBUG_STANDARD, "successfully disabled serial port %s\n", dev->port);
				}
			}

			if (dev->serial_config.baud_base == 0) {
				dev->serial_config.baud_base = 115200;
			}

			if (check_register_atarisio(dev)) {
				if (dev->need_reenable) {
					if (reenable_serial_port(dev)) {
						PRINTK_NODEV("error re-enabling serial port!\n");
					} else {
						DBG_PRINTK_NODEV(DEBUG_STANDARD, "successfully re-enabled serial port %s\n", dev->port);
					}
				}
				free_atarisio_dev(i);
				continue;
			}
			numdev++;
		}
	}
	if (!numtried) {
		PRINTK_NODEV("please supply port or io and irq parameters\n");
		return -EINVAL;
	}
	if (!numdev) {
		PRINTK_NODEV("failed to register any devices\n");
		return -ENODEV;
	}
#ifndef ATARISIO_USE_HRTIMER
	if (hrtimer) {
		PRINTK_NODEV("warning: high resolution timers are not available\n");
	}
#endif
	atarisio_is_initialized = 1;
	return 0;
}

static void atarisio_exit_module(void)
{
	if (!atarisio_is_initialized) {
		PRINTK_NODEV("internal error - atarisio is not initialized!\n");
		return;
	}

	atarisio_cleanup_module();
}

module_init(atarisio_init_module);
module_exit(atarisio_exit_module);

