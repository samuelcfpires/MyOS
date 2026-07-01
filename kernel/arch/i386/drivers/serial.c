/**
 * A driver for Serial Ports
 * Taken partially from https://littleosbook.github.io/ and https://wiki.osdev.org/Serial_ports
 * 
 * Refer to:
 * https://wiki.osdev.org/Serial_ports
 * https://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming#FIFO_Control_Register
 * 
 * @author Samuel Pires
*/

#include <kernel/arch/i386/drivers/serial.h>
#include <kernel/arch/i386/io.h>

#include <stdint.h>

/* The I/O ports */

/* All the I/O ports are calculated relative to the data port. This is because
 * all serial ports (COM1, COM2, COM3, COM4) have their ports in the same
 * order, but they start at different values.
*/

#define SERIAL_COM1_BASE                	0x3F8      /* COM1 base port */

#define SERIAL_DATA_PORT(base)              (base)

/* This register allows you to control when and how the UART is going to trigger an interrupt event with the hardware interrupt associated with the serial COM port. */
#define SERIAL_INTERRUPT_ENABLE_PORT(base)  (base + 1)      // bits 7:6 - Reserved
															// bit 5    - Enable Low Power Mode
															// bit 4    - Enable Sleep Mode
															// bit 3    - Enable Modem Status Interrupt
															// bit 2    - Enable Receiver Line Status Interrupt
															// bit 1    - Enable Transmitter Holding Register Empty Interrupt
															// bit 0    - Enable Received Data Available Interrupt

/* This register is used to control how the First In/First Out (FIFO) buffers will behave on the chip */
#define SERIAL_FIFO_COMMAND_PORT(base)      (base + 2)      // bits 7:6 - number of bytes to store in the FIFOs (1/1, 4/16, 8/32, 14/56)
															// bit 5    - Enable 64 Byte FIFO
															// bit 4    - Reserved
															// bit 3    - DMA Mode Select
															// bit 2    - Clear Transmit FIFO
															// bit 1    - Clear Receive FIFO
															// bit 0    - Enable FIFOs

/* This register has two major purposes:
 *  - Setting the Divisor Latch Access Bit (DLAB), allowing you to set the values of the Divisor Latch Bytes.
 *  - Setting the bit patterns that will be used for both receiving and transmitting the serial data.
*/
#define SERIAL_LINE_COMMAND_PORT(base)      (base + 3)      // bit 7    - Enable DLAB
															// bit 6    - Enable break control
															// bits 5:3 - Parity Select                  (no parity, odd parity, even parity, mark, space)
															// bit 2    - Stop Bits                      (1, 1.5 or 2)
															// bits 1:0 - Data Word Length               (5, 6, 7, 8)

/* This register allows you to do "hardware" flow control, under software control. */
#define SERIAL_MODEM_COMMAND_PORT(base)     (base + 4)      // bits 6:7 - Reserved
															// bit 5    - Enable Autoflow Control
															// bit 4    - Loopback Mode
															// bit 3    - Auxiliary Output 2 (used for receiving interrupts)
															// bit 2    - Auxiliary Output 1
															// bit 1    - Request To Send
															// bit 0    - Data Terminal Ready

/* This register is used primarily to give you information on possible error conditions that may exist within the UART, based on the data that has been received. */
#define SERIAL_LINE_STATUS_PORT(base)       (base + 5)      // bit 7 - Error in Received FIFO
															// bit 6 - Empty Data Holding Registers
															// bit 5 - Empty Transmitter Holding Registers
															// bit 4 - Break Interrupt
															// bit 3 - Framing Error
															// bit 2 - Parity Error
															// bit 1 - Overrun Error
															// bit 0 - Data Ready

/* The I/O port commands */

/*
 * Tells the serial port to expect first the highest 8 bits on the data port,
 * then the lowest 8 bits will follow
*/
#define SERIAL_LINE_ENABLE_DLAB         0x80

/**
 *  Sets the speed of the data being sent. The default speed of a serial
 *  port is 115200 bits/s. The argument is a divisor of that number, hence
 *  the resulting speed becomes (115200 / divisor) bits/s.
 *
 *  @param com      The COM port to configure
 *  @param divisor  The divisor
*/
static void serial_configure_baud_rate(uint16_t com, uint16_t divisor)
{
	outb(SERIAL_LINE_COMMAND_PORT(com), SERIAL_LINE_ENABLE_DLAB);
	outb(SERIAL_DATA_PORT(com), (divisor >> 8) & 0x00FF);
	outb(SERIAL_DATA_PORT(com), divisor & 0x00FF);
}

#define BAUD_RATE_DIVISOR 0x0003

/* The serial ports are only going to be used for debugging, therefore,
 * since it won't be handling any received data, interrupts will be disabled.
*/
int serial_init()
{
	uint16_t com = SERIAL_COM1_BASE;

	outb(SERIAL_INTERRUPT_ENABLE_PORT(com), 0x00);     // Disable interrupts
	serial_configure_baud_rate(com, BAUD_RATE_DIVISOR);
	outb(SERIAL_LINE_COMMAND_PORT(com), 0x03);         // 8 bits, no parity, one stop bit
	outb(SERIAL_FIFO_COMMAND_PORT(com), 0xC7);         // Enable FIFO, clear them, with 14-byte threshold
	outb(SERIAL_MODEM_COMMAND_PORT(com), 0x1E);        // Set in loopback mode, test the serial chip
	outb(SERIAL_DATA_PORT(com), 0xAE);                 // Test serial chip (send byte 0xAE and check if serial returns same byte)

	// Check if serial is faulty (i.e: not same byte as sent)
	if (inb(SERIAL_DATA_PORT(com)) != 0xAE)
	   return -1;

	// If serial is not faulty, disable looback mode
	outb(SERIAL_MODEM_COMMAND_PORT(com), 0x03);
	return 0;
}

/**
 *  Checks whether the transmit FIFO queue is empty or not for the given COM
 *  port.
 *
 *  @param  com The COM port
 *  @return 0 if the transmit FIFO queue is not empty
 *          1 if the transmit FIFO queue is empty
 */
static int serial_transmit_fifo_empty(unsigned int com)
{
	return inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

void serial_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		serial_writechar(data[i]);
}


void serial_writestring(const char* str)
{
	for (size_t i = 0; str[i] != '\0'; i++)
		serial_writechar(str[i]);
}


void serial_writechar(char c)
{
	uint16_t com = SERIAL_COM1_BASE;

	while (!serial_transmit_fifo_empty(com)) {}
	outb(com, c);
}
