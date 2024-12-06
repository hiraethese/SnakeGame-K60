/* Header file with all the essential definitions for a given type of MCU */
#include "MK60DZ10.h"

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
	int body[SNAKE_LENGTH][2];  // Array of coordinates [x, y]
	int length;                 // Predefined length
	Direction direction;        // Current direction of movement
} Snake;

/* Global variable for Snake structure */
Snake snake;

/* Define the pin for each row */
unsigned int row_pins[8] = {
	GPIO_PIN(26),  // R0
	GPIO_PIN(24),  // R1
	GPIO_PIN(9),   // R2
	GPIO_PIN(25),  // R3
	GPIO_PIN(28),  // R4
	GPIO_PIN(7),   // R5
	GPIO_PIN(27),  // R6
	GPIO_PIN(29)   // R7
};

/* Configuration of the necessary MCU peripherals */
void SystemConfig() {
	/* Turn on all port clocks */
	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;

	/* Set corresponding PTA pins (column activators of 74HC154) for GPIO functionality */
	PORTA->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );  // A0
	PORTA->PCR[10] = ( 0|PORT_PCR_MUX(0x01) ); // A1
	PORTA->PCR[6] = ( 0|PORT_PCR_MUX(0x01) );  // A2
	PORTA->PCR[11] = ( 0|PORT_PCR_MUX(0x01) ); // A3

	/* Set corresponding PTA pins (rows selectors of 74HC154) for GPIO functionality */
	PORTA->PCR[26] = ( 0|PORT_PCR_MUX(0x01) );  // R0
	PORTA->PCR[24] = ( 0|PORT_PCR_MUX(0x01) );  // R1
	PORTA->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );   // R2
	PORTA->PCR[25] = ( 0|PORT_PCR_MUX(0x01) );  // R3
	PORTA->PCR[28] = ( 0|PORT_PCR_MUX(0x01) );  // R4
	PORTA->PCR[7] = ( 0|PORT_PCR_MUX(0x01) );   // R5
	PORTA->PCR[27] = ( 0|PORT_PCR_MUX(0x01) );  // R6
	PORTA->PCR[29] = ( 0|PORT_PCR_MUX(0x01) );  // R7

	/* Set corresponding PTE pins (buttons) for GPIO functionality */
	PORTE->PCR[10] = ( PORT_PCR_ISF(0x01)|PORT_PCR_IRQC(0x0A)|PORT_PCR_MUX(0x01)|PORT_PCR_PE(0x01)|PORT_PCR_PS(0x01) );  // RIGHT
	PORTE->PCR[11] = ( PORT_PCR_ISF(0x01)|PORT_PCR_IRQC(0x0A)|PORT_PCR_MUX(0x01)|PORT_PCR_PE(0x01)|PORT_PCR_PS(0x01) );  // STOP
	PORTE->PCR[12] = ( PORT_PCR_ISF(0x01)|PORT_PCR_IRQC(0x0A)|PORT_PCR_MUX(0x01)|PORT_PCR_PE(0x01)|PORT_PCR_PS(0x01) );  // DOWN
	PORTE->PCR[26] = ( PORT_PCR_ISF(0x01)|PORT_PCR_IRQC(0x0A)|PORT_PCR_MUX(0x01)|PORT_PCR_PE(0x01)|PORT_PCR_PS(0x01) );  // UP
	PORTE->PCR[27] = ( PORT_PCR_ISF(0x01)|PORT_PCR_IRQC(0x0A)|PORT_PCR_MUX(0x01)|PORT_PCR_PE(0x01)|PORT_PCR_PS(0x01) );  // LEFT

	/* Set corresponding PTE pins (output enable of 74HC154) for GPIO functionality */
	PORTE->PCR[28] = ( 0|PORT_PCR_MUX(0x01) ); // #EN

	/* Change corresponding PTA port pins as outputs */
	PTA->PDDR = GPIO_PDDR_PDD(0x3F000FC0);

	/* Change corresponding PTE port pins as inputs */
	PTE->PDDR &= ~( GPIO_PIN(10)|GPIO_PIN(11)|GPIO_PIN(12)|GPIO_PIN(26)|GPIO_PIN(27) );

	/* Change corresponding PTE port pins as outputs */
	PTE->PDDR = GPIO_PDDR_PDD( GPIO_PIN(28) );

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
	/* Clear all rows */
	PTA->PDOR &= ~GPIO_PDOR_PDO(0xFFFF);

	/* Set the selected row */
	PTA->PDOR |= GPIO_PDOR_PDO(row_pins[row_num]);
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
    if (new_head_x < 0) {
        new_head_x = 7;
    }
    if (new_head_x >= 8) {
        new_head_x = 0;
    }
    if (new_head_y < 0) {
        new_head_y = 15;
    }
    if (new_head_y >= 16) {
        new_head_y = 0;
    }

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
		delay(400, 50);
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

	// PTA->PDOR |= GPIO_PDOR_PDO(0x3F000280); // turning the pixels of a particular row ON
	// PTE->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(28));
	// delay(tdelay1, tdelay2);

	/* Initialize the snake */
	init_snake();

	/* Main loop */
    while(1) {
    	display_snake();
		update_snake();
		delay(tdelay1, tdelay2);
    }

    /* Never leave main */
    return 0;
}
