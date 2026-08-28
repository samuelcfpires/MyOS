/**
 * A driver for Video Graphics Array
 * Taken partially from https://littleosbook.github.io/ and https://wiki.osdev.org/Text_UI
 * 
 * Refer to:
 * https://en.wikipedia.org/wiki/VGA_text_mode
 * https://wiki.osdev.org/Text_UI
 * http://www.osdever.net/FreeVGA/home.htm
 * 
 * @author Samuel Pires
*/

#include <kernel/mm/mm.h>

#include <kernel/arch/i386/drivers/vga.h>
#include <kernel/arch/i386/io.h>

#include <stdint.h>
#include <stddef.h>

/* Framebuffer memory-mapped I/O location */
#define FB_MMIO_LOCATION	P2V(0xB8000)

static char* fb = (char*) FB_MMIO_LOCATION;  	// fb[i*2]:       Code Point
												// fb[i*2 + 1]:
												//		bit  7   - Blink Bit
												//      bits 6:4 - Background Color
												//      bits 3:0 - Foreground Color

/* I/O ports */
#define FB_COMMAND_PORT         0x3D4
#define FB_DATA_PORT            0x3D5

/* I/O port commands */
#define FB_HIGH_BYTE_COMMAND    0xE
#define FB_LOW_BYTE_COMMAND     0xF

/* Registers */
#define CURSOR_START_REGISTER   0xA		// bit  5   - Cursor Disable
										// bits 4:0 - Cursor Scan Line Start

#define CURSOR_END_REGISTER     0xB     // bits 6:5 - Cursor Skew
										// bits 4:0 - Cursor Scan Line End

/* Size of the framebuffer in cells */
#define FB_WIDTH            	80
#define FB_HEIGHT            	25
#define FB_CELL_COUNT			FB_WIDTH * FB_HEIGHT

/* Color codes */
#define BLACK                   0
#define WHITE                   15


static uint16_t cursor_pos;

/* Cursor Functions */

void cursor_enable(void)
{
	outb(FB_COMMAND_PORT, CURSOR_START_REGISTER);
	outb(FB_DATA_PORT, (inb(FB_DATA_PORT) & 0xC0) | 0);

	outb(FB_COMMAND_PORT, CURSOR_END_REGISTER);
	outb(FB_DATA_PORT, (inb(FB_DATA_PORT) & 0xE0) | (FB_HEIGHT-1));
}

void cursor_disable(void)
{
	outb(FB_COMMAND_PORT, CURSOR_START_REGISTER);
	outb(FB_DATA_PORT, 0x20);
}

/**
 *  Moves the cursor of the framebuffer to the given position
 *
 *  @param pos new position of the cursor
*/
static void cursor_move(uint16_t pos)
{
	outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
	outb(FB_DATA_PORT, (pos >> 8) & 0xFF);
	outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
	outb(FB_DATA_PORT, pos & 0xFF);
}

/**
 *  Configures and enables the cursor.
*/
static void cursor_init(void)
{
	cursor_enable();
	cursor_move(0);
	cursor_pos = 0;
}


void vga_init(void)
{
	fb_clear();
	cursor_init();
}

/* Framebuffer Functions */

/**
 *  Writes a character with the given foreground and background to character position
 *  in the framebuffer.
 *
 *  @param pos character position in the framebuffer
 *  @param c   character to be written
 *  @param bg  background color
 *  @param fg  foreground color
*/
static void fb_write_cell(unsigned int pos, char c, uint8_t bg, uint8_t fg)
{
	/*  Since each character takes up two bytes of space in memory and the index is given in
	 *  single steps, pos must be multiplied by 2 to get the correct position to write the character
	*/
	fb[pos*2] = c;
	fb[pos*2 + 1] = ((bg & 0x0F) << 4) | (fg & 0x0F);
}

/**
 * 	Reads and returns the data at the indicated position in the framebuffer
 * 
 * 	@return	the data read at the indicated position from the framebuffer
*/
static uint16_t fb_read_cell(unsigned int pos)
{
	uint16_t data = 0;

	data |= fb[pos*2 + 1];
	data |= (fb[pos*2] << 8);

	return data;
}

/**
 * 	Simulates a scroll up by copying every line's data to the line above
 * 	and clearing the last line.
*/
static void fb_scroll_up()
{
	// move every line up starting from the second
	for (unsigned int line = 1; line < FB_HEIGHT; line++)
		for (unsigned int col = 0; col < FB_WIDTH; col++) {
			unsigned int old_pos = line*FB_WIDTH + col;
			uint16_t cell_data = fb_read_cell(old_pos);
			fb_write_cell(old_pos - FB_WIDTH, ((cell_data >> 8) & 0xFF), ((cell_data >> 4) & 0xF), (cell_data & 0xF));
		}

	// clear the last line
	for (unsigned int i = FB_WIDTH; i > 0; i--)
		fb_write_cell((FB_CELL_COUNT-i), 0, BLACK, WHITE);
}

void fb_clear(void)
{
	for (unsigned int i = 0; i < FB_CELL_COUNT; i++)
		fb_write_cell(i, 0, BLACK, WHITE);

	cursor_move(0);
	cursor_pos = 0;
}

void fb_write(const char* data, size_t size)
{
	for (unsigned int i = 0; i < size; i++)
		fb_writechar(data[i++]);

	cursor_move(cursor_pos);
}

void fb_writestring(const char* data)
{
	unsigned int i = 0;
	
	while (data[i])
		fb_writechar(data[i++]);

	cursor_move(cursor_pos);
}

void fb_writechar(char c)
{
	switch (c) {
	
	case '\n':
		/* jump to the framebuffer's next line */
		cursor_pos = (cursor_pos/FB_WIDTH + 1) * FB_WIDTH;
		break;

	case '\b':
		/* clean and move to previous cell (if not it the first one) */
		if (cursor_pos)
			fb_write_cell(--cursor_pos, 0, BLACK, WHITE);
		break;

	/* any character to be actually written (in ASCII) */
	default:
		fb_write_cell(cursor_pos++, c, BLACK, WHITE);
		break;
	}	

	/* if the framebuffer is full, simulate a scroll up */
	if (cursor_pos >= FB_CELL_COUNT) {
		fb_scroll_up();
		cursor_pos -= FB_WIDTH;
	}

	cursor_move(cursor_pos);
}
