#ifndef ATARISIO_H
#define ATARISIO_H

/*
   atarisio V1.07
   a kernel module for handling the Atari 8bit SIO protocol

   Copyright (C) 2002-2009 Matthias Reichl <hias@horus.com>

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

#include <linux/types.h>

#define ATARISIO_MAJOR_VERSION 1
#define ATARISIO_MINOR_VERSION 7
#define ATARISIO_VERSION_MAGIC 42

#define ATARISIO_VERSION ( ( (ATARISIO_VERSION_MAGIC) << 16) | ( (ATARISIO_MAJOR_VERSION) << 8) | (ATARISIO_MINOR_VERSION) )

/*
   baudrate definitions for standard and high speed SIO
*/

#define ATARISIO_STANDARD_BAUDRATE 19200
#define ATARISIO_XF551_BAUDRATE 38400
#define ATARISIO_HIGHSPEED_BAUDRATE 57600
#define ATARISIO_TAPE_BAUDRATE 600

/*
   Atari system frequencies in Hz
*/
#define ATARISIO_ATARI_FREQUENCY_PAL  1773447
#define ATARISIO_ATARI_FREQUENCY_NTSC 1789772

/*
   SIO parameter struct passed to DO_SIO ioctl.
   The entries of this struct have the same meaning like
   in the standard Atari SIO call $E459.
   Note: be sure that the data_buffer is allocated and can
   store data_length bytes!
*/
typedef struct SIO_param_struct {
	uint8_t  device_id;
	uint8_t  command;
	uint8_t  direction; /* 0=receive, 1=send data block */
	uint8_t  timeout; /* in seconds */
	uint8_t  aux1; /* auxiliary data, eg. sector number */
	uint8_t  aux2; 
	unsigned int   data_length;
	uint8_t* data_buffer;
} SIO_parameters;


#define ATARISIO_EXTSIO_DIR_IMMEDIATE 0
#define ATARISIO_EXTSIO_DIR_RECV 0x40
#define ATARISIO_EXTSIO_DIR_SEND 0x80

#define ATARISIO_EXTSIO_SPEED_NORMAL 0
#define ATARISIO_EXTSIO_SPEED_ULTRA 1
#define ATARISIO_EXTSIO_SPEED_WARP 2
#define ATARISIO_EXTSIO_SPEED_XF551 3
#define ATARISIO_EXTSIO_SPEED_TURBO 4

/*
   SIO parameter struct passed to DO_EXT_SIO ioctl.
   The entries of this struct have the same meaning like
   in the standard Atari SIO call $E459.
   Note: be sure that the data_buffer is allocated and can
   store data_length bytes!
*/
typedef struct Ext_SIO_param_struct {
	uint8_t  device;
	uint8_t  unit;
	uint8_t  command;
	uint8_t  direction; /* 0=none, 0x40 = read from device, 0x80 = write to device */
	uint8_t  timeout; /* in seconds */
	uint8_t  aux1; /* auxiliary data, eg. sector number */
	uint8_t  aux2; 
	unsigned int   data_length;
	uint8_t* data_buffer;
	uint8_t  highspeed_mode;	/* use ATARISIO_EXTSIO_* */
} Ext_SIO_parameters;

/*
   command frame struct returned by GET_COMMAND_FRAME ioctl.
   Please refer to some good Atari XL book on how to interpret
   the device_id, command, aux1, and aux2 members.
   missed_count is/was used for debugging purposes and contains
   the number of command frames that have been received between
   the last call to GET_COMMAND_FRAME and the returned command
   frame. Usually, this member should be 0.
*/
typedef struct SIO_command_frame_struct {
	uint8_t device_id;
	uint8_t command;
	uint8_t aux1;
	uint8_t aux2;
	uint64_t reception_timestamp;
	unsigned int  missed_count;
} SIO_command_frame;

/*
   the following struct is used by SEND_DATA_FRAME and by
   RECEIVE_DATA_FRAME. data_buffer must be allocated to
   (at least) data_length bytes.
*/
typedef struct SIO_data_frame_struct {
	uint8_t* data_buffer;
	unsigned int   data_length;
} SIO_data_frame;

/*
   definition of IOCTLs
*/

#define ATARISIO_IOC_MAGIC 0xa8

/* 
   return version of kernel driver. The lower 8 bits of the (24bit)
   version number contain the minor revision.
   Bits 8-15 contiain the major version number. A change in major
   version will usually indicate some severe changes either to the
   interface or to operation mode. Be sure the major number matches
   the version of atarisio you used when writing a program!
   
   When using atarisio, please check if the major number matches
   and if the minor number returned from this ioctl is higher or equal
   to the number used at the compiletime of your program.
*/

#define ATARISIO_IOC_GET_VERSION	_IO(  ATARISIO_IOC_MAGIC, 0)

/*
   parameters for SET_MODE
*/
#define ATARISIO_MODE_1050_2_PC 0
#define ATARISIO_MODE_PROSYSTEM 1
#define ATARISIO_MODE_SIOSERVER 2
#define ATARISIO_MODE_SIOSERVER_RI 2
#define ATARISIO_MODE_SIOSERVER_DSR 3
#define ATARISIO_MODE_SIOSERVER_CTS 4

/*
   set operation mode of kernel driver. At the moment two basic modes
   are supported: SIOSERVER and 1050-2-PC. The 1050-2-PC mode has two
   sub-variants: 1050-2-PC (when used with a 1050-2-PC cable) and
   PROSYSTEM (if an APE ProSystem cable is used).

   In 1050-2-PC mode you may use DO_SIO to talk to a connected
   1050 (or other disk drive).

   In SIOSERVER mode, you can use select to wait for a received
   command frame. Just call select with the file-descriptor of
   an opened /dev/atarisio in the exception set to wait for
   the next command frame.

   Then you have to call GET_COMMAND_FRAME to fetch the newly
   received frame. Next, analyze and process the command frame
   and use the SEND-ACK/NAK/COMPLETE/ERROR and SEND/RECEIVE 
   DATA_FRAME functions to implement the SIO protocol.

   The SIOSERVER_DSR mode acts like the SIOSERVER mode, but
   with the command line from the Atari going to the DSR
   (instead of RI) input of the PC's serial port.
*/

#define ATARISIO_IOC_SET_MODE		_IOW( ATARISIO_IOC_MAGIC, 1, unsigned int)

/*
   Set the current baudrate (the default is 19200 bit/sec)
*/
#define ATARISIO_IOC_SET_BAUDRATE	_IOW( ATARISIO_IOC_MAGIC, 2, unsigned int)

/*
   set autobaud enable for SIOSERVER mode.
   If set to "1", atarisio will automatically switch between 19200
   and 57600 bit/sec if errors occurred while receiving a command frame.
   If set to "0", atarisio will stay at the currently set baudrate.
*/

#define ATARISIO_IOC_SET_AUTOBAUD	_IOW( ATARISIO_IOC_MAGIC, 3, unsigned int)

/*
   Talk to a connected Atari device (like $E459)
*/
#define ATARISIO_IOC_DO_SIO		_IOWR(ATARISIO_IOC_MAGIC, 4, SIO_parameters *)

/*
   Try to fetch a received command frame. If no valid command frame
   is waiting (eg. if none was received yet, or if the received frame
   has "expired"), GET_COMMAND_FRAME will wait up to one second for
   the reception of a new valid frame.
   On success, GET_COMMAND_FRAME fills out the passed command_frame
   struct and return 0.
   In case of a failure it will return EATARISIO_COMMAND_TIMEOUT,
   EFAULT, or EINTR.

   Please note: the kernel driver checks for a valid checksum in the
   command frame and only return frames without checksum errors.
*/

#define ATARISIO_IOC_GET_COMMAND_FRAME	_IOR( ATARISIO_IOC_MAGIC, 5, SIO_command_frame *)

/*
   send ACK/NAK/... bytes as needed in the SIO protocol in SIOSERVER
   mode. These ioctls will wait for the corresponding minimum time
   (as specified in the SIO protocol) and then transmit the
   corresponding ACK/NAK/... character. In other words: you don't
   have to care about SIO timing in user space.

   On success, these IOCTLs return 0.
*/
#define ATARISIO_IOC_SEND_COMMAND_ACK	_IO(  ATARISIO_IOC_MAGIC, 6)
#define ATARISIO_IOC_SEND_COMMAND_NAK	_IO(  ATARISIO_IOC_MAGIC, 7)
#define ATARISIO_IOC_SEND_DATA_ACK	_IO(  ATARISIO_IOC_MAGIC, 8)
#define ATARISIO_IOC_SEND_DATA_NAK	_IO(  ATARISIO_IOC_MAGIC, 9)
#define ATARISIO_IOC_SEND_COMPLETE	_IO(  ATARISIO_IOC_MAGIC, 10)
#define ATARISIO_IOC_SEND_ERROR		_IO(  ATARISIO_IOC_MAGIC, 11)

/*
   send/receive a datablock over/from SIO.
   SEND_DATA_FRAME also calculates the checksum and transmits it together
   with the data block.
   RECEIVE_DATA_FRAME also receives and compares the checksum. In case
   of a checksum-error, it will return ATARISIO_CHECKSUM_ERROR.

   Both IOCTLs return 0 on success.
*/
#define ATARISIO_IOC_SEND_DATA_FRAME	_IOW( ATARISIO_IOC_MAGIC, 12, SIO_data_frame *)
#define ATARISIO_IOC_RECEIVE_DATA_FRAME	_IOR( ATARISIO_IOC_MAGIC, 13, SIO_data_frame *)

/*
   get current baudrate
*/
#define ATARISIO_IOC_GET_BAUDRATE	_IO( ATARISIO_IOC_MAGIC, 14)

/*
   add additional inter-byte pauses in high-speed modes and/or increase delays
*/
#define ATARISIO_HIGHSPEEDPAUSE_OFF 0
#define ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY 1
#define ATARISIO_HIGHSPEEDPAUSE_FRAME_DELAY 2
#define ATARISIO_HIGHSPEEDPAUSE_BOTH 3

#define ATARISIO_IOC_SET_HIGHSPEEDPAUSE _IOW( ATARISIO_IOC_MAGIC, 15, unsigned int)

/*
   printk debugging info
*/
#define ATARISIO_IOC_PRINT_STATUS	_IO( ATARISIO_IOC_MAGIC, 16)


/*
 * support for XF551 transfer modes:
 * these commands work exactly like the standard commands, but they set the
 * baudrate to 38400 (or back to 19200) just after they are finished
 */

#define ATARISIO_IOC_SEND_COMMAND_ACK_XF551	_IO(  ATARISIO_IOC_MAGIC, 17)
#define ATARISIO_IOC_SEND_COMPLETE_XF551	_IO(  ATARISIO_IOC_MAGIC, 18)
#define ATARISIO_IOC_SEND_DATA_FRAME_XF551	_IOW( ATARISIO_IOC_MAGIC, 19, SIO_data_frame *)

/*
   send/receive a raw over/from SIO.
   These ioctls work like send/receive data frame, but they don't
   calculate and/or check the checksum
*/
#define ATARISIO_IOC_SEND_RAW_FRAME	_IOW( ATARISIO_IOC_MAGIC, 20, SIO_data_frame *)
#define ATARISIO_IOC_RECEIVE_RAW_FRAME	_IOR( ATARISIO_IOC_MAGIC, 21, SIO_data_frame *)

/*
   timestamp structure for measuring system latency.
*/
typedef struct SIO_timestamp_struct {
	uint64_t system_entering; /* set inside the ioctl() */
	uint64_t transmission_start; /* only set on send operations */
	uint64_t transmission_send_irq; /* timestamp of first send-char interrupt */
	uint64_t transmission_end; /* set in interrupt, only on send operations */
	uint64_t transmission_wakeup; /* set in kernel driver after sleeping */
	uint64_t uart_finished; /* set when uart has finished transmitting all bits */
	uint64_t system_leaving; /* set before leaving the ioctl() */
} SIO_timestamps;

/*
   enable timestamp recording (0=off, 1=on)
*/
#define ATARISIO_IOC_ENABLE_TIMESTAMPS	_IOW( ATARISIO_IOC_MAGIC, 22, int)
/*
   get timestamps of last operation
*/
#define ATARISIO_IOC_GET_TIMESTAMPS	_IOR( ATARISIO_IOC_MAGIC, 23, SIO_timestamps *)

/*
   preliminary tape functions. Note: they are still under development
   and might change in the future.
*/

#define ATARISIO_IOC_SET_TAPE_BAUDRATE	_IOW( ATARISIO_IOC_MAGIC, 24, unsigned int)

/*
   send a tape block. The data block must also contain the header bytes and
   the checksum
*/

#define ATARISIO_IOC_SEND_TAPE_BLOCK	_IOW( ATARISIO_IOC_MAGIC, 25, SIO_data_frame *)

/*
   Set the highspeed baudrate (the default is 57600 bit/sec)
*/
#define ATARISIO_IOC_SET_HIGHSPEED_BAUDRATE	_IOW( ATARISIO_IOC_MAGIC, 26, unsigned int)

/*
   Talk to a connected Atari device (like $E459)
*/
#define ATARISIO_IOC_DO_EXT_SIO		_IOWR(ATARISIO_IOC_MAGIC, 27, Ext_SIO_parameters *)

/*
   get exact UART baudrate
*/
#define ATARISIO_IOC_GET_EXACT_BAUDRATE	_IO( ATARISIO_IOC_MAGIC, 28)

/*
   new tape block interface: begin with START_TAPE_MODE,
   then use SEND_RAW_DATA_NOWAIT (multiple times) to transmit the data
   and end with FLUSH_WRITE_BUFFER and END_TAPE_MODE
*/
#define ATARISIO_IOC_START_TAPE_MODE	_IO( ATARISIO_IOC_MAGIC, 29)
#define ATARISIO_IOC_END_TAPE_MODE	_IO( ATARISIO_IOC_MAGIC, 30)

/*
   send a raw frame, but don't wait for the FIFO/THRE to empty
*/
#define ATARISIO_IOC_SEND_RAW_DATA_NOWAIT	_IOW( ATARISIO_IOC_MAGIC, 31, SIO_data_frame *)

#define ATARISIO_IOC_FLUSH_WRITE_BUFFER	_IO( ATARISIO_IOC_MAGIC, 32)

/*
   FSK data struct used by SEND_FSK_DATA
   the array contains the length of the single bits to be transferred,
   in 100uS units.
*/
typedef struct FSK_data_struct {
	uint16_t* bit_time;
	unsigned int num_entries;
} FSK_data;

/*
   transmit FSK (raw bit) data, compatible with the liba8cas FSK
   extension.
   Before transmitting the data the ioctl waits for the FIFO (and THR)
   to empty.
   The code then transmits alternating 0 and 1 bits (starting with a 0 bit),
   the entries in bit_time array define the length of each bit (in units
   of 100uS)
*/

#define ATARISIO_IOC_SEND_FSK_DATA	_IOW( ATARISIO_IOC_MAGIC, 33, FSK_data *)

/*
   Set the standard baudrate (the default is 19200 bit/sec)
*/
#define ATARISIO_IOC_SET_STANDARD_BAUDRATE \
	_IOW( ATARISIO_IOC_MAGIC, 34, unsigned int)

#define ATARISIO_POKEY_DIVISOR_STANDARD		40
#define ATARISIO_POKEY_DIVISOR_2XSIO_XF551	16
#define ATARISIO_POKEY_DIVISOR_HAPPY		10
#define ATARISIO_POKEY_DIVISOR_SPEEDY		9
#define ATARISIO_POKEY_DIVISOR_3XSIO		8
#define ATARISIO_POKEY_DIVISOR_1050_TURBO	6

/*
   Get the baudrate that should be used for the given pokey divisor.
   0 means proper operation is not guaranteed.
*/
#define ATARISIO_IOC_GET_BAUDRATE_FOR_POKEY_DIVISOR \
	_IOR( ATARISIO_IOC_MAGIC, 35, unsigned int)

#define ATARISIO_IOC_MAXNR 35

/*
   errno codes for DO_SIO, mainly according to
   the standard error codes defined by the Atari 8bit
   operating system (plus 1000).
   If the ioctls return -1, look into errno for one of these errors
*/

#define ATARISIO_ERRORBASE 1000
#define EATARISIO_ERROR_BLOCK_TOO_LONG (ATARISIO_ERRORBASE + 137)
#define EATARISIO_COMMAND_NAK (ATARISIO_ERRORBASE + 146)
#define EATARISIO_COMMAND_TIMEOUT (ATARISIO_ERRORBASE + 138)
#define EATARISIO_CHECKSUM_ERROR (ATARISIO_ERRORBASE + 143)
#define EATARISIO_COMMAND_COMPLETE_ERROR (ATARISIO_ERRORBASE + 139)
#define EATARISIO_DATA_NAK (ATARISIO_ERRORBASE + 142)
#define EATARISIO_UNKNOWN_ERROR (ATARISIO_ERRORBASE + 140)

#endif
