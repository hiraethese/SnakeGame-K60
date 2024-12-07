/* Header file with all the essential definitions for a given type of MCU */
// #include "MK60DZ10.h"
#include "../Includes/MK60DZ10.h"

/* Macros for bit-level registers manipulation */
#define GPIO_PIN_MASK	0x1Fu
#define GPIO_PIN(x)		(((1)<<(x & GPIO_PIN_MASK)))

/* Define button masks */
#define BUTTON_RIGHT_MASK 0x400     // PTE10
#define BUTTON_STOP_MASK 0x800      // PTE11
#define BUTTON_DOWN_MASK 0x1000     // PTE12
#define BUTTON_UP_MASK 0x4000000    // PTE26
#define BUTTON_LEFT_MASK 0x8000000  // PTE27

/* Constants specifying delay loop duration */
#define	tdelay1			10000
#define tdelay2 		20

/* Define the LED matrix properties */
#define ROWS 8
#define COLS 16

/* Define the snake properties */
#define SNAKE_LENGTH 5

/* Define the direction of movement */
typedef enum {
    UP,
    LEFT,
    DOWN,
    RIGHT
} Direction;

/* Define the snake structure */
typedef struct {
	int body[SNAKE_LENGTH][2];  /* Array of coordinates [x, y] */
	int length;                 /* Predefined length */
	Direction direction;        /* Current direction of movement */
} Snake;

/* Global variable for the Snake structure */
Snake snake;

/* Array of pin numbers to use */
unsigned int column_pins[4] = {8, 10, 6, 11};  // A0-A3
unsigned int row_pins[8] = {26, 24, 9, 25, 28, 7, 27, 29};  // R0-R7
unsigned int button_pins[5] = {10, 11, 12, 26, 27};  // RIGHT, STOP, DOWN, UP, LEFT

/* Configuration of the necessary MCU peripherals */
void SystemConfig() {
	/* Turn on all port clocks */
	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;

	/* Set corresponding PTA pins (column activators of 74HC154) for GPIO functionality */
	for (int i = 0; i < 4; i++) {
		PORTA->PCR[column_pins[i]] = ( 0|PORT_PCR_MUX(0x01) );
	}

	/* Set corresponding PTA pins (rows selectors of 74HC154) for GPIO functionality */
	for (int i = 0; i < 8; i++) {
		PORTA->PCR[row_pins[i]] = ( 0|PORT_PCR_MUX(0x01) );
	}

	/* Set corresponding PTE pins (output enable of 74HC154) for GPIO functionality */
	PORTE->PCR[28] = ( 0|PORT_PCR_MUX(0x01) ); // #EN

	/* Set corresponding PTE pins (buttons) for GPIO functionality */
	for (int i = 0; i < 5; i++) {
		PORTE->PCR[button_pins[i]] &= ~PORT_PCR_ISF_MASK;   /* Clear ISF */
		PORTE->PCR[button_pins[i]] |= PORT_PCR_IRQC(0x0A);  /* Interrupt on falling edge */
		PORTE->PCR[button_pins[i]] |= PORT_PCR_MUX(0x01);   /* Pin Mux Control: GPIO */
		PORTE->PCR[button_pins[i]] |= PORT_PCR_PE_MASK;     /* Enable pull resistor */
		PORTE->PCR[button_pins[i]] |= PORT_PCR_PS_MASK;     /* Select pull-up resistor */
	}

	/* Change corresponding PTA port pins as outputs */
	PTA->PDDR = GPIO_PDDR_PDD(0x3F000FC0);

	/* Change corresponding PTE port pins as inputs */
	PTE->PDDR &= ~( GPIO_PIN(10)|GPIO_PIN(11)|GPIO_PIN(12)|GPIO_PIN(26)|GPIO_PIN(27) );

	/* Change corresponding PTE port pins as outputs */
	PTE->PDDR |= GPIO_PIN(28);

	/* Clear any pending interrupts on Port E */
	NVIC_ClearPendingIRQ(PORTE_IRQn);

	/* Enable interrupts for Port E */
	NVIC_EnableIRQ(PORTE_IRQn);
}

/* Variable delay loop */
void delay(int t1, int t2)
{
	int i, j;

	for(i=0; i<t1; i++) {
		for(j=0; j<t2; j++);
	}
}

/* Conversion of requested column number into the 4-to-16 decoder control.  */
void column_select(unsigned int col_num)
{
	unsigned i, result, col_sel[4];

	for (i =0; i<4; i++) {
		result = col_num / 2;	  // Whole-number division of the input number
		col_sel[i] = col_num % 2;
		col_num = result;

		switch(i) {

			// Selection signal A0
		    case 0:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(8))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(8)));
				break;

			// Selection signal A1
			case 1:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(10))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(10)));
				break;

			// Selection signal A2
			case 2:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(6))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(6)));
				break;

			// Selection signal A3
			case 3:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(11))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(11)));
				break;

			// Otherwise nothing to do...
			default:
				break;
		}
	}
}

/* Selection of the row signal */
void row_select(unsigned int row_num) {
	PTA->PDOR = (PTA->PDOR & ~0x3F000FC0) | GPIO_PDOR_PDO(GPIO_PIN(row_pins[row_num]));
}

/* Initialize the snake */
void init_snake() {
	snake.length = SNAKE_LENGTH;
	snake.direction = RIGHT;

	for (int i = 0; i < snake.length; i++) {
        snake.body[i][0] = 0;
        snake.body[i][1] = (snake.length - 1) - i;
    }
}

/* Update the snake position */
void update_snake() {
	int new_head_x = snake.body[0][0];
    int new_head_y = snake.body[0][1];

    /* Calculate the new head position based on direction */
    switch (snake.direction) {
        case UP:
            new_head_x--;
            break;
        case LEFT:
            new_head_y--;
            break;
        case DOWN:
            new_head_x++;
            break;
        case RIGHT:
            new_head_y++;
            break;
        default:
            break;
    }

    /* Teleport the snake if out of bounds */
    if (new_head_x < 0) new_head_x = ROWS - 1;
    if (new_head_x >= ROWS) new_head_x = 0;
    if (new_head_y < 0) new_head_y = COLS - 1;
    if (new_head_y >= COLS) new_head_y = 0;

    /* Shift body positions */
    for (int i = snake.length - 1; i > 0; i--) {
        snake.body[i][0] = snake.body[i - 1][0];
        snake.body[i][1] = snake.body[i - 1][1];
    }

    /* Update the new head position */
    snake.body[0][0] = new_head_x;
    snake.body[0][1] = new_head_y;
}

/* Display the snake */
void display_snake() {
	for(int i = 0; i < snake.length; i++) {
		column_select(snake.body[i][0]);
		row_select(snake.body[i][1]);
		PTE->PDOR &= ~GPIO_PDOR_PDO(GPIO_PIN(28));
		delay(tdelay1, tdelay2);
		PTE->PDOR |= GPIO_PDOR_PDO(GPIO_PIN(28));
	}
}

void PORTE_IRQHandler() {
	/* Check if RIGHT button caused the interrupt */
	if (PORTE->ISFR & BUTTON_RIGHT_MASK) {
		if (snake.direction != LEFT) {
			snake.direction = RIGHT;
		}
	}

	/* Check if STOP button caused the interrupt */
	if (PORTE->ISFR & BUTTON_STOP_MASK) {
		/* TODO: Implement the STOP button handling later */
	}

	/* Check if DOWN button caused the interrupt */
	if (PORTE->ISFR & BUTTON_DOWN_MASK) {
		if (snake.direction != UP) {
			snake.direction = DOWN;
		}
	}

	/* Check if UP button caused the interrupt */
	if (PORTE->ISFR & BUTTON_UP_MASK) {
		if (snake.direction != DOWN) {
			snake.direction = UP;
		}
	}

	/* Check if LEFT button caused the interrupt */
	if (PORTE->ISFR & BUTTON_LEFT_MASK) {
		if (snake.direction != RIGHT) {
			snake.direction = LEFT;
		}
	}

	/* Clear all interrupt flags */
	PORTE->ISFR = ( BUTTON_RIGHT_MASK|BUTTON_STOP_MASK|BUTTON_DOWN_MASK|BUTTON_UP_MASK|BUTTON_LEFT_MASK );
}

/* Main function */
int main(void)
{
	/* Configure the hardware */
	SystemConfig();

	/* Initialize the snake */
	init_snake();
	display_snake();

	/* Main loop */
    while(1) {
		update_snake();
    	display_snake();
    }

    /* Never leave main */
    return 0;
}
