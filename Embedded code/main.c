#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <avr/interrupt.h>

// ================= PINS =================
#define CS_PIN      PB4
#define DC_PIN      PD6
#define RST_PIN     PD7
#define BUSY_PIN    PD5

#define CS_PORT     PORTB
#define DC_PORT     PORTD
#define RST_PORT    PORTD
#define BUSY_PORT   PIND

#define CS_DDR      DDRB
#define DC_DDR      DDRD
#define RST_DDR     DDRD
#define BUSY_DDR    DDRD

#define BTN_DOWN_PIN   PD2   // INT0
#define BTN_UP_PIN     PD3   // INT1

volatile uint16_t int0_press_ms = 0;
volatile uint16_t int1_press_ms = 0;
volatile uint8_t  int0_pressed  = 0;
volatile uint8_t  int1_pressed  = 0;

// ================= DISPLAY SPECS =================
#define EPD_WIDTH    250
#define EPD_HEIGHT   122

// ================= MACROS =================
#define CS_LOW()    (CS_PORT &= ~(1 << CS_PIN))
#define CS_HIGH()   (CS_PORT |=  (1 << CS_PIN))
#define DC_LOW()    (DC_PORT &= ~(1 << DC_PIN))
#define DC_HIGH()   (DC_PORT |=  (1 << DC_PIN))
#define RST_LOW()   (RST_PORT &= ~(1 << RST_PIN))
#define RST_HIGH()  (RST_PORT |=  (1 << RST_PIN))
#define IS_BUSY()   (BUSY_PORT & (1 << BUSY_PIN))

// ================= COMMANDS =================
#define CMD_DRIVER_OUTPUT_CONTROL    0x01
#define CMD_DATA_ENTRY_MODE          0x11
#define CMD_SW_RESET                 0x12
#define CMD_DISPLAY_UPDATE_CONTROL_2 0x22
#define CMD_WRITE_RAM_BW             0x24
#define CMD_SET_RAM_X_ADDRESS        0x44
#define CMD_SET_RAM_Y_ADDRESS        0x45
#define CMD_SET_RAM_X_COUNTER        0x4E
#define CMD_SET_RAM_Y_COUNTER        0x4F
#define CMD_MASTER_ACTIVATION        0x20

// ================= FONT =================
const uint8_t font_5x7[][5] PROGMEM = {
	{ 0x3E, 0x51, 0x49, 0x45, 0x3E }, { 0x00, 0x42, 0x7F, 0x40, 0x00 },
	{ 0x42, 0x61, 0x51, 0x49, 0x46 }, { 0x21, 0x41, 0x45, 0x4B, 0x31 },
	{ 0x18, 0x14, 0x12, 0x7F, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 },
	{ 0x3C, 0x4A, 0x49, 0x49, 0x30 }, { 0x01, 0x71, 0x09, 0x05, 0x03 },
	{ 0x36, 0x49, 0x49, 0x49, 0x36 }, { 0x06, 0x49, 0x49, 0x29, 0x1E },
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x5F, 0x00, 0x00, 0x00 },
	{ 0x07, 0x00, 0x07, 0x00, 0x00 }, { 0x14, 0x7F, 0x14, 0x7F, 0x14 },
	{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, { 0x23, 0x13, 0x08, 0x64, 0x62 },
	{ 0x36, 0x49, 0x55, 0x22, 0x50 }, { 0x00, 0x05, 0x03, 0x00, 0x00 },
	{ 0x00, 0x1C, 0x22, 0x41, 0x00 }, { 0x00, 0x41, 0x22, 0x1C, 0x00 },
	{ 0x14, 0x08, 0x3E, 0x08, 0x14 }, { 0x08, 0x08, 0x3E, 0x08, 0x08 },
	{ 0x00, 0x50, 0x30, 0x00, 0x00 }, { 0x08, 0x08, 0x08, 0x08, 0x08 },
	{ 0x00, 0x60, 0x60, 0x00, 0x00 }, { 0x20, 0x10, 0x08, 0x04, 0x02 },
	{ 0x00, 0x36, 0x36, 0x00, 0x00 }, { 0x00, 0x56, 0x36, 0x00, 0x00 },
	{ 0x08, 0x14, 0x22, 0x41, 0x00 }, { 0x14, 0x14, 0x14, 0x14, 0x14 },
	{ 0x00, 0x41, 0x22, 0x14, 0x08 }, { 0x02, 0x01, 0x51, 0x09, 0x06 },
	{ 0x7E, 0x11, 0x11, 0x11, 0x7E }, { 0x7F, 0x49, 0x49, 0x49, 0x36 },
	{ 0x3E, 0x41, 0x41, 0x41, 0x22 }, { 0x7F, 0x41, 0x41, 0x22, 0x1C },
	{ 0x7F, 0x49, 0x49, 0x49, 0x41 }, { 0x7F, 0x09, 0x09, 0x09, 0x01 },
	{ 0x3E, 0x41, 0x49, 0x49, 0x7A }, { 0x7F, 0x08, 0x08, 0x08, 0x7F },
	{ 0x00, 0x41, 0x7F, 0x41, 0x00 }, { 0x20, 0x40, 0x41, 0x3F, 0x01 },
	{ 0x7F, 0x08, 0x14, 0x22, 0x41 }, { 0x7F, 0x40, 0x40, 0x40, 0x40 },
	{ 0x7F, 0x02, 0x04, 0x02, 0x7F }, { 0x7F, 0x04, 0x08, 0x10, 0x7F },
	{ 0x3E, 0x41, 0x41, 0x41, 0x3E }, { 0x7F, 0x09, 0x09, 0x09, 0x06 },
	{ 0x3E, 0x41, 0x51, 0x21, 0x5E }, { 0x7F, 0x09, 0x19, 0x29, 0x46 },
	{ 0x46, 0x49, 0x49, 0x49, 0x31 }, { 0x01, 0x01, 0x7F, 0x01, 0x01 },
	{ 0x3F, 0x40, 0x40, 0x40, 0x3F }, { 0x1F, 0x20, 0x40, 0x20, 0x1F },
	{ 0x3F, 0x40, 0x38, 0x40, 0x3F }, { 0x63, 0x14, 0x08, 0x14, 0x63 },
	{ 0x07, 0x08, 0x70, 0x08, 0x07 }, { 0x61, 0x51, 0x49, 0x45, 0x43 },
	{ 0x20, 0x54, 0x54, 0x54, 0x78 }, { 0x7F, 0x48, 0x44, 0x44, 0x38 },
	{ 0x38, 0x44, 0x44, 0x44, 0x20 }, { 0x38, 0x44, 0x44, 0x48, 0x7F },
	{ 0x38, 0x54, 0x54, 0x54, 0x18 }, { 0x08, 0x7E, 0x09, 0x01, 0x02 },
	{ 0x08, 0x54, 0x54, 0x54, 0x3C }, { 0x7F, 0x08, 0x04, 0x04, 0x78 },
	{ 0x00, 0x44, 0x7D, 0x40, 0x00 }, { 0x20, 0x40, 0x44, 0x3D, 0x00 },
	{ 0x7F, 0x10, 0x28, 0x44, 0x00 }, { 0x00, 0x41, 0x7F, 0x40, 0x00 },
	{ 0x7C, 0x04, 0x18, 0x04, 0x78 }, { 0x7C, 0x08, 0x04, 0x04, 0x78 },
	{ 0x38, 0x44, 0x44, 0x44, 0x38 }, { 0x7C, 0x14, 0x14, 0x14, 0x08 },
	{ 0x08, 0x14, 0x14, 0x18, 0x7C }, { 0x7C, 0x08, 0x04, 0x04, 0x08 },
	{ 0x48, 0x54, 0x54, 0x54, 0x20 }, { 0x04, 0x3F, 0x44, 0x40, 0x20 },
	{ 0x3C, 0x40, 0x40, 0x40, 0x7C }, { 0x1C, 0x20, 0x40, 0x20, 0x1C },
	{ 0x3C, 0x40, 0x20, 0x40, 0x3C }, { 0x44, 0x28, 0x10, 0x28, 0x44 },
	{ 0x0C, 0x50, 0x50, 0x50, 0x3C }, { 0x44, 0x64, 0x54, 0x4C, 0x44 },
};

int8_t GetFontIndex(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c == ' ')  return 10;
	if (c == '!')  return 11; if (c == '\"') return 12; if (c == '#')  return 13;
	if (c == '$')  return 14; if (c == '%')  return 15; if (c == '&')  return 16;
	if (c == '\'') return 17; if (c == '(')  return 18; if (c == ')')  return 19;
	if (c == '*')  return 20; if (c == '+')  return 21; if (c == ',')  return 22;
	if (c == '-')  return 23; if (c == '.')  return 24; if (c == '/')  return 25;
	if (c == ':')  return 26; if (c == ';')  return 27; if (c == '<')  return 28;
	if (c == '=')  return 29; if (c == '>')  return 30; if (c == '?')  return 31;
	if (c >= 'A' && c <= 'Z') return (c - 'A') + 32;
	if (c >= 'a' && c <= 'z') return (c - 'a') + 58;
	return -1;
}

// ================= DATA STRUCTURES =================
typedef struct {
	char    date[9];   // "19 Apr\0" = 7 chars, +2 safety
	char    day[12];   // "Wednesday\0" = 10 chars, +2 safety
	uint8_t battery_pct;
} HeaderData;

typedef struct {
	char    tasks[10][24];
	char    task_times[10][9];
	uint8_t selected;
	uint8_t count;
} BodyData;

typedef struct {
	char    exam_name[24];
	char    exam_date[9];   // "25 May\0" = 7 chars, +2 safety
	char    habit_name[24];
	uint8_t habit_total;
	uint8_t habit_filled;
} TailData;

HeaderData header = {
	.date        = "19 Apr",
	.day         = "Wednesday",
	.battery_pct = 75
};

BodyData body = {
	.tasks      = { "- Microprocessor", "- Analog CMOS", "- Digital CMOS", "- Networks" },
	.task_times = { "1:00 PM", "2:00 PM", "3:00 PM", "4:00 PM" },
	.selected   = 0,
	.count      = 4
};

TailData tail = {
	.exam_name    = "- Digital CMOS Exam",
	.exam_date    = "20 Mar",
	.habit_name   = "- Habit",
	.habit_total  = 5,
	.habit_filled = 3
};

// ================= SPI =================
void SPI_Init(void) {
	DDRB |= (1 << PB5) | (1 << PB7) | (1 << CS_PIN);
	DDRB &= ~(1 << PB6);
	SPCR  = (1 << SPE) | (1 << MSTR);
	SPSR  = (1 << SPI2X);
	CS_HIGH();
}

static inline void SPI_Transfer_Fast(uint8_t data) {
	SPDR = data;
	while (!(SPSR & (1 << SPIF)));
}

void EPD_SendCommand(uint8_t cmd) {
	DC_LOW();  CS_LOW(); SPI_Transfer_Fast(cmd);  CS_HIGH();
}

void EPD_SendData(uint8_t data) {
	DC_HIGH(); CS_LOW(); SPI_Transfer_Fast(data); CS_HIGH();
}

void EPD_WaitBusy(void) {
	while (IS_BUSY()) _delay_ms(10);
}

void EPD_Reset(void) {
	RST_HIGH(); _delay_ms(200);
	RST_LOW();  _delay_ms(50);
	RST_HIGH(); _delay_ms(200);
}

void EPD_Init(void) {
	DC_DDR   |=  (1 << DC_PIN);
	RST_DDR  |=  (1 << RST_PIN);
	BUSY_DDR &= ~(1 << BUSY_PIN);

	SPI_Init();
	EPD_Reset();

	EPD_SendCommand(CMD_SW_RESET);
	EPD_WaitBusy();

	EPD_SendCommand(CMD_DRIVER_OUTPUT_CONTROL);
	EPD_SendData(0xF9); EPD_SendData(0x00); EPD_SendData(0x00);

	EPD_SendCommand(CMD_DATA_ENTRY_MODE);
	EPD_SendData(0x03);

	EPD_SendCommand(CMD_SET_RAM_X_ADDRESS);
	EPD_SendData(0x00); EPD_SendData(0x0F);

	EPD_SendCommand(CMD_SET_RAM_Y_ADDRESS);
	EPD_SendData(0x00); EPD_SendData(0x00);
	EPD_SendData(0xF9); EPD_SendData(0x00);

	EPD_SendCommand(CMD_SET_RAM_X_COUNTER);
	EPD_SendData(0x00);

	EPD_SendCommand(CMD_SET_RAM_Y_COUNTER);
	EPD_SendData(0x00); EPD_SendData(0x00);
}

// ================= ROW BUFFER + RENDERING =================
static uint8_t row_buf[16];

static void StampText(uint16_t land_x, uint16_t x_off,
const char *str, uint8_t str_len,
uint8_t y_start)
{
	uint16_t rel = land_x - x_off;
	uint8_t  ci  = rel / 6;
	uint8_t  col = rel % 6;

	if (ci >= str_len) return;
	if (col >= 5)      return;

	int8_t idx = GetFontIndex(str[ci]);
	if (idx < 0) return;

	uint8_t glyph = pgm_read_byte(&(font_5x7[idx][col]));
	if (!glyph)  return;

	for (uint8_t row = 0; row < 7; row++) {
		if (glyph & (1 << row)) {
			uint8_t ly = y_start + row;
			if (ly >= EPD_HEIGHT) break;   // FIX: guard against out-of-bounds
			row_buf[ly >> 3] &= ~(1 << (7 - (ly & 7)));
		}
	}
}

#define SET_BLACK(ly) row_buf[(ly) >> 3] &= ~(1 << (7 - ((ly) & 7)))

static void BuildRow(uint16_t land_x) {
	// All white
	for (uint8_t i = 0; i < 16; i++) row_buf[i] = 0xFF;

	// ?? HEADER ??????????????????????????????????????????????????????????????

	SET_BLACK(21); // separator

	{ // Date (x=4, y=5)
		uint8_t len = (uint8_t)strlen(header.date);
		if (land_x >= 4 && land_x < (uint16_t)(4 + (uint16_t)len * 6))
		StampText(land_x, 4, header.date, len, 5);
	}

	{ // Day (x=109, y=5)
		uint8_t len = (uint8_t)strlen(header.day);
		if (land_x >= 109 && land_x < (uint16_t)(109 + (uint16_t)len * 6))
		StampText(land_x, 109, header.day, len, 5);
	}

	// Battery icon (x=220..239, y=5..14)
	if (land_x >= 220 && land_x < 240) {
		uint8_t bx   = (uint8_t)(land_x - 220);
		uint8_t fill = (header.battery_pct * 16) / 100;

		if (bx < 18) {
			for (uint8_t by = 0; by < 10; by++) {
				uint8_t ly    = 5 + by;
				uint8_t black = (bx == 0 || bx == 17 || by == 0 || by == 9)
				|| (bx > 0 && bx <= fill);
				if (black) SET_BLACK(ly);
			}
			} else {
			// Tip: bx=18..19, land_y=8..11
			SET_BLACK(8); SET_BLACK(9); SET_BLACK(10); SET_BLACK(11);
		}
	}

	// ?? BODY ????????????????????????????????????????????????????????????????
	{
		uint8_t start = body.selected * 4;

		for (uint8_t s = 0; s < 4; s++) {
			uint8_t ti = start + s;
			if (ti >= body.count) break;

			uint8_t line_y = 30 + s * 15;

			{ // Task label (x=4..149)
				uint8_t  len   = (uint8_t)strlen(body.tasks[ti]);
				uint16_t x_end = (uint16_t)(4 + (uint16_t)len * 6);
				if (x_end > 150) x_end = 150;
				if (land_x >= 4 && land_x < x_end)
				StampText(land_x, 4, body.tasks[ti], len, line_y);
			}

			{ // Time label (x=195..239)
				uint8_t  len   = (uint8_t)strlen(body.task_times[ti]);
				uint16_t x_end = (uint16_t)(195 + (uint16_t)len * 6);
				if (x_end > 240) x_end = 240;
				if (land_x >= 195 && land_x < x_end)
				StampText(land_x, 195, body.task_times[ti], len, line_y);
			}
		}
	}

	// ?? TAIL ????????????????????????????????????????????????????????????????

	SET_BLACK(91); // separator

	{ // Exam name (x=4..179, y=100)
		uint8_t  len   = (uint8_t)strlen(tail.exam_name);
		uint16_t x_end = (uint16_t)(4 + (uint16_t)len * 6);
		if (x_end > 180) x_end = 180;
		if (land_x >= 4 && land_x < x_end)
		StampText(land_x, 4, tail.exam_name, len, 100);
	}

	{ // Exam date (x=195..239, y=100)
		uint8_t len = (uint8_t)strlen(tail.exam_date);
		if (land_x >= 195 && land_x < (uint16_t)(195 + (uint16_t)len * 6))
		StampText(land_x, 195, tail.exam_date, len, 100);
	}

	{ // Habit name (x=4..139, y=115)
		uint8_t  len   = (uint8_t)strlen(tail.habit_name);
		uint16_t x_end = (uint16_t)(4 + (uint16_t)len * 6);
		if (x_end > 140) x_end = 140;
		if (land_x >= 4 && land_x < x_end)
		StampText(land_x, 4, tail.habit_name, len, 115);
	}

	// Habit dots (x=195..244, y=115..121, centres at y=118)
	if (land_x >= 195 && land_x < 245) {
		for (uint8_t i = 0; i < tail.habit_total; i++) {
			int16_t cx  = 198 + (int16_t)((uint16_t)i * 10);
			int16_t dx  = (int16_t)land_x - cx;
			if (dx < -3 || dx > 3) continue;
			int16_t dx2 = dx * dx;
			for (uint8_t ly = 115; ly <= 121; ly++) {
				int16_t dy = (int16_t)ly - 118;
				int16_t d2 = dx2 + dy * dy;
				if (d2 <= 9 && (i < tail.habit_filled || d2 >= 4))
				SET_BLACK(ly);
			}
		}
	}
}

// ================= CORE RENDER =================
static void EPD_WriteFullRAM(void) {
	EPD_SendCommand(CMD_SET_RAM_X_ADDRESS);
	EPD_SendData(0x00); EPD_SendData(0x0F);

	EPD_SendCommand(CMD_SET_RAM_Y_ADDRESS);
	EPD_SendData(0x00); EPD_SendData(0x00);
	EPD_SendData(0xF9); EPD_SendData(0x00);

	EPD_SendCommand(CMD_SET_RAM_X_COUNTER);
	EPD_SendData(0x00);

	EPD_SendCommand(CMD_SET_RAM_Y_COUNTER);
	EPD_SendData(0x00); EPD_SendData(0x00);

	EPD_SendCommand(CMD_WRITE_RAM_BW);
	DC_HIGH();
	CS_LOW();

	for (uint16_t phys_y = 0; phys_y < 250; phys_y++) {
		BuildRow(249 - phys_y);
		for (uint8_t i = 0; i < 16; i++) {
			SPDR = row_buf[i];
			while (!(SPSR & (1 << SPIF)));
		}
	}

	CS_HIGH();
}

static void EPD_TriggerRefresh(void) {
	EPD_SendCommand(CMD_DISPLAY_UPDATE_CONTROL_2);
	EPD_SendData(0xF7);
	EPD_SendCommand(CMD_MASTER_ACTIVATION);
	EPD_WaitBusy();
}

void UpdateFullScreen(void) { EPD_WriteFullRAM(); EPD_TriggerRefresh(); }
void UpdateHeader(void)     { EPD_WriteFullRAM(); EPD_TriggerRefresh(); }
void UpdateBody(void)       { EPD_WriteFullRAM(); EPD_TriggerRefresh(); }
void UpdateTail(void)       { EPD_WriteFullRAM(); EPD_TriggerRefresh(); }

// ================= NAVIGATION =================
void go_down(BodyData *data) {
	uint8_t max_page = (data->count + 3) / 4;
	if (data->selected + 1 < max_page) data->selected++;
}

void go_up(BodyData *data) {
	if (data->selected > 0) data->selected--;
}

// ================= BATTERY =================
uint16_t read_vcc_mv(void) {
	ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
	_delay_ms(2);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return (uint16_t)((3000UL * 1023UL) / ADC);
}

uint16_t read_vcc_avg(void) {
	uint32_t sum = 0;
	for (uint8_t i = 0; i < 5; i++) { sum += read_vcc_mv(); _delay_ms(80); }
	return (uint16_t)(sum / 5);
}

uint8_t battery_percentage(uint16_t vcc) {
	if (vcc >= 3000) return 100;
	if (vcc >= 2950) return 90;
	if (vcc >= 2900) return 80;
	if (vcc >= 2850) return 70;
	if (vcc >= 2800) return 60;
	if (vcc >= 2750) return 50;
	if (vcc >= 2700) return 40;
	if (vcc >= 2650) return 30;
	if (vcc >= 2600) return 20;
	if (vcc >= 2550) return 10;
	return 0;
}

// ================= TIMER0 — 1ms tick =================
void Timer0_Init(void) {
	TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);   // CTC, prescaler 64
	OCR0  = (uint8_t)((F_CPU / (64UL * 1000UL)) - 1);    // = 61 ? 1ms @ 4MHz
	TIMSK |= (1 << OCIE0);
}

ISR(TIMER0_COMP_vect) {
	if (int0_pressed) int0_press_ms++;
	if (int1_pressed) int1_press_ms++;
}

// ================= BUTTONS =================
// FIX 1: NO _delay_ms inside ISR.
// Debounce is handled in HandleButtons() by checking pin state on release.
// The ISR only records the press time — fast and safe.
void Buttons_Init(void) {
	DDRD  &= ~((1 << BTN_DOWN_PIN) | (1 << BTN_UP_PIN));
	PORTD |=  (1 << BTN_DOWN_PIN)  | (1 << BTN_UP_PIN);   // pull-up

	MCUCR |= (1 << ISC01);   // INT0 falling edge
	MCUCR |= (1 << ISC11);   // INT1 falling edge

	GICR  |= (1 << INT0) | (1 << INT1);
}

ISR(INT0_vect) {    // DOWN — no delay, just latch
	int0_pressed  = 1;
	int0_press_ms = 0;
}

ISR(INT1_vect) {    // UP — no delay, just latch
	int1_pressed  = 1;
	int1_press_ms = 0;
}

// FIX 1 (cont): debounce on release — pin must still be released AND
// a minimum of 20ms must have passed since the falling edge.
void HandleButtons(void) {
	// INT0 released
	if (int0_pressed && (PIND & (1 << BTN_DOWN_PIN))) {
		if (int0_press_ms < 20) {          // bounce: ignore very short pulses
			int0_pressed = 0;
			return;
		}
		int0_pressed = 0;
		if (int0_press_ms >= 2000) {
			if (tail.habit_filled < tail.habit_total) tail.habit_filled++;
			UpdateTail();
			} else {
			go_down(&body);
			UpdateBody();
		}
	}

	// INT1 released
	if (int1_pressed && (PIND & (1 << BTN_UP_PIN))) {
		if (int1_press_ms < 20) {
			int1_pressed = 0;
			return;
		}
		int1_pressed = 0;
		if (int1_press_ms >= 2000) {
			// long-press UP: reserved for future use
			} else {
			go_up(&body);
			UpdateBody();
		}
	}
}

// ================= UART / HC-06 =================
// ATmega32 @ 4MHz, HC-06 at 9600 baud
// UBRR = (4,000,000 / (16 * 9600)) - 1 = 25
#define BAUD      9600
#define UBRR_VAL  ((F_CPU / (16UL * BAUD)) - 1)   // = 25

// FIX 2: RX buffer uses uint8_t indices which wrap at 256.
// This works correctly ONLY because RX_BUF_SIZE == 256.
// Indices overflow from 255?0 naturally, matching the modulo.
#define RX_BUF_SIZE   256
#define MAX_FIELDS     20
#define MAX_FIELD_LEN  24

static char     fields[MAX_FIELDS][MAX_FIELD_LEN];
static uint8_t  field_count = 0;

// FIX 3: rx_buf is uint8_t (was char) to avoid signed-char issues
// with bytes >= 0x80 on platforms where char is signed.
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t line_ready = 0;

void UART_Init(void) {
	UBRRH = (uint8_t)(UBRR_VAL >> 8);
	UBRRL = (uint8_t)(UBRR_VAL);
	UCSRB = (1 << RXEN) | (1 << RXCIE);
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);   // 8N1, URSEL=1 for ATmega32
}

ISR(USART_RXC_vect) {
	uint8_t c    = UDR;
	uint8_t next = (uint8_t)(rx_head + 1);   // wraps at 256 naturally

	if (next != rx_tail) {
		rx_buf[rx_head] = c;
		rx_head = next;
		if (c == '\n') line_ready = 1;
	}
	// if buffer full, byte is silently dropped
}

static uint8_t Receive_Line(char *buf, uint8_t max) {
	if (!line_ready) return 0;

	// FIX 2 (cont): disable RX interrupt while reading to prevent
	// race condition between ISR writing and main loop reading.
	UCSRB &= ~(1 << RXCIE);

	uint8_t i = 0;
	while (rx_head != rx_tail && i < (uint8_t)(max - 1)) {
		uint8_t c = rx_buf[rx_tail];
		rx_tail = (uint8_t)(rx_tail + 1);
		if (c == '\n' || c == '\r') break;
		buf[i++] = (char)c;
	}
	buf[i] = '\0';

	// Flush trailing \r\n
	while (rx_head != rx_tail &&
	(rx_buf[rx_tail] == '\r' || rx_buf[rx_tail] == '\n')) {
		rx_tail = (uint8_t)(rx_tail + 1);
	}

	line_ready = 0;

	// Re-enable RX interrupt
	UCSRB |= (1 << RXCIE);

	return (i > 0) ? 1 : 0;
}

static void Parse_CSV(char *line) {
	field_count = 0;
	char *token = strtok(line, ",");
	while (token != NULL && field_count < MAX_FIELDS) {
		strncpy(fields[field_count], token, MAX_FIELD_LEN - 1);
		fields[field_count][MAX_FIELD_LEN - 1] = '\0';
		field_count++;
		token = strtok(NULL, ",");
	}
}

static void Extract_And_Update(void) {
	// FIX 4: minimum valid payload = Date, Day, ExamName, ExamDate, Habit = 5 fields
	// (was incorrectly '< 3', which allowed partial overwrites of header)
	if (field_count < 5) return;

	// Header
	strncpy(header.date, fields[0], sizeof(header.date) - 1);
	header.date[sizeof(header.date) - 1] = '\0';
	strncpy(header.day,  fields[1], sizeof(header.day)  - 1);
	header.day[sizeof(header.day)   - 1] = '\0';

	// Tail: last 3 fields
	uint8_t exam_idx = field_count - 3;
	strncpy(tail.exam_name,  fields[exam_idx],     sizeof(tail.exam_name)  - 1);
	tail.exam_name[sizeof(tail.exam_name)   - 1] = '\0';
	strncpy(tail.exam_date,  fields[exam_idx + 1], sizeof(tail.exam_date)  - 1);
	tail.exam_date[sizeof(tail.exam_date)   - 1] = '\0';
	strncpy(tail.habit_name, fields[exam_idx + 2], sizeof(tail.habit_name) - 1);
	tail.habit_name[sizeof(tail.habit_name) - 1] = '\0';

	// Body tasks: fields[2..exam_idx-1] in pairs
	uint8_t num_tasks = (exam_idx >= 2) ? (exam_idx - 2) / 2 : 0;
	if (num_tasks > 10) num_tasks = 10;

	for (uint8_t i = 0; i < num_tasks; i++) {
		strncpy(body.tasks[i],      fields[2 + i * 2],     sizeof(body.tasks[i])      - 1);
		strncpy(body.task_times[i], fields[2 + i * 2 + 1], sizeof(body.task_times[i]) - 1);
		body.tasks[i][sizeof(body.tasks[i])           - 1] = '\0';
		body.task_times[i][sizeof(body.task_times[i]) - 1] = '\0';
	}
	body.count    = num_tasks;
	body.selected = 0;

	UpdateFullScreen();
}

// ================= MAIN =================
int main(void) {
	EPD_Init();
	Timer0_Init();
	Buttons_Init();
	UART_Init();
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	sei();

	header.battery_pct = battery_percentage(read_vcc_avg());
	UpdateFullScreen();

	char line[RX_BUF_SIZE];

	while (1) {
		HandleButtons();

		if (Receive_Line(line, sizeof(line))) {
			Parse_CSV(line);
			Extract_And_Update();
		}
	}
}