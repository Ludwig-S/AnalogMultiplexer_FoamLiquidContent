#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>

#include "stm32f4xx.h"

#define NUMBER_OF_OUTPUT_STATES 7


/*
Pin overview:

uC pin:		board pin name / function:


PA2			USART2_TX
PA3			USART2_RX

PA0			A0, trigger input

PA1			A1, output 1, S1L
PA4		    A2, output 2, S2L
PB0			A3, output 3, S3L
PC1			A4, output 4, S1R
PC0			A5, output 5, S2R

*/

// GLOBAL VARIABLES
uint8_t currentState;

uint8_t n_epp = 1; // number of emissions per profile
uint8_t n_epp_digit[2] = {255, 255}; // char array representing the 2 digits for desired n_epp
uint8_t n_trigger = 0; // number of received triggers

char transString[200];
char recChar;
char recChar_desiredState_int;
char recChar_instruction; // 0: toggle interrupt mask; 1: set n_epp

uint8_t lineStarted = 0;

// FUNCTIONS
// pin set/ reset functions:
static inline void set_S1L(void)
{
    GPIOA->BSRR |= GPIO_BSRR_BS_1;
}

static inline void reset_S1L(void)
{
    GPIOA->BSRR |= GPIO_BSRR_BR_1;
}

static inline void set_S2L(void)
{
    GPIOA->BSRR |= GPIO_BSRR_BS_4;
}

static inline void reset_S2L(void)
{
    GPIOA->BSRR |= GPIO_BSRR_BR_4;
}

static inline void set_S3L(void)
{
    GPIOB->BSRR |= GPIO_BSRR_BS_0;
}

static inline void reset_S3L(void)
{
    GPIOB->BSRR |= GPIO_BSRR_BR_0;
}

static inline void set_S1R(void)
{
    GPIOC->BSRR |= GPIO_BSRR_BS_1;
}

static inline void reset_S1R(void)
{
    GPIOC->BSRR |= GPIO_BSRR_BR_1;
}

static inline void set_S2R(void)
{
    GPIOC->BSRR |= GPIO_BSRR_BS_0;
}

static inline void reset_S2R(void)
{
    GPIOC->BSRR |= GPIO_BSRR_BR_0;
}

void setOutputs(uint8_t desiredState){

    desiredState -= 1; // convert state number to bitwise coded output state

    if (desiredState & 1<<0) // check bit 0
    {
        set_S1L();
    } 
    else
    {
        reset_S1L();
    }

    if (desiredState & 1<<1) // check bit 1
    {
        set_S2L();
        set_S1R();
    }
    else
    {
        reset_S2L();
        reset_S1R();
    }

    if (desiredState & 1<<2) // check bit 2
    {
        set_S3L();
        set_S2R();
    }
    else
    {
        reset_S3L();
        reset_S2R();
    }    
}

// USART2 functions:
// initialize usart2 RX and TX
void usart2_init(void)
{
	// USART2_RX uses port A2 and USART2_TX uses port A3 if each are set to alternate function 7 (p.59 datasheet)
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;	// enable clock to USART2
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // enable clock to Port A
	GPIOA->MODER |= (GPIO_MODER_MODE2_1|GPIO_MODER_MODE3_1); // set PA2 and PA3 to alternate function
	GPIOA->AFR[0] |= 
		(GPIO_AFRL_AFRL2_2 | GPIO_AFRL_AFRL2_1 | GPIO_AFRL_AFRL2_0 | GPIO_AFRL_AFRL3_2 
		| GPIO_AFRL_AFRL3_1 | GPIO_AFRL_AFRL3_0); // set PA2 to AF7 (USART2_TX) and PA3 to AF7 (USART2_RX)
	USART2->CR1 |= (USART_CR1_TE|USART_CR1_RE);// enable transmitter and receiver
	// Baud rate Infos (p. 808 reference manual)
	USART2->BRR = 0x0683;// set baud rate to 9600 (with 16 MHz clock)
	USART2->CR1 |= USART_CR1_UE;// enable USART2 module	
}


void usart2_transChar(char msg_char)
{
	while(!(USART2->SR & USART_SR_TXE)); // wait while transmit data register is full (register empty => TXE=1)
	USART2->DR = (uint8_t) msg_char; // write char in data register
}


void usart2_transString(char *msg_string)
{
	size_t msg_string_len = strlen(msg_string); // get size of string pointed to by *msg_string
	// print each character of input string
	for (uint32_t i = 0; i < msg_string_len; i++)
		usart2_transChar(msg_string[i]);
}



// returns received char of USART2
char usart2_recChar(void){
	if(USART2->SR & USART_SR_RXNE){ // if data is ready to be read
		return USART2->DR; // read received char and return it
	}
	else return '\0';
}

void usart2_transCurrentState(void)
{
    sprintf(transString, "state %d\n", currentState);
    newLineIfLineStarted();
    usart2_transString(transString);
}

void EXTI0_IRQHandler() // this function gets called in case of a trigger interrupt
{
    ++n_trigger;
    if (n_trigger == n_epp) // if n_epp was reached
    {
        n_epp = 0;
        ++currentState;
        if (currentState > NUMBER_OF_OUTPUT_STATES)
        {
            currentState = 1;
        }    
        setOutputs(currentState);
        usart2_transCurrentState();
    }

    // print current number of triggers:
    usart2_transChar('\b');
    usart2_transChar('\b');
    usart2_transChar('\b');
    sprintf(transString, "n%d\n", n_trigger);
    usart2_transString(transString);
    
    EXTI->PR |= EXTI_PR_PR0; // interrupt finished
}

void newLineIfLineStarted(void)
{
    if (lineStarted)
    {
        usart2_transChar('\n');
        lineStarted = 0;
    }
}

int main()
{
    // initialize GPIO pins:
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN); // clock to port A
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOBEN); // clock to port B
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOCEN); // clock to port C
    // set pins to output:
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER1, GPIO_MODER_MODE1_0);
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER4, GPIO_MODER_MODE4_0);
    MODIFY_REG(GPIOB->MODER, GPIO_MODER_MODER0, GPIO_MODER_MODE0_0);
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODER1, GPIO_MODER_MODE1_0);
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODER0, GPIO_MODER_MODE0_0);
    // set fastest speed to all outputs:
    MODIFY_REG(GPIOA->OSPEEDR, 0, GPIO_OSPEEDR_OSPEED1);
    MODIFY_REG(GPIOA->OSPEEDR, 0, GPIO_OSPEEDR_OSPEED4);
    MODIFY_REG(GPIOB->OSPEEDR, 0, GPIO_OSPEEDR_OSPEED0);
    MODIFY_REG(GPIOC->OSPEEDR, 0, GPIO_OSPEEDR_OSPEED1);
    MODIFY_REG(GPIOC->OSPEEDR, 0, GPIO_OSPEEDR_OSPEED0);
    // pin A0 is input by default
    // !!!PULL UP ONLY FOR TESTING ON BREADBOARD!!! 
    // !! comment out the following line for normal use case:
    // MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPDR0, GPIO_PUPDR_PUPDR0_0); // pull up to PA0
    // initalize interrupt for trigger input:
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // clock to SYSCFG
    // MODIFY_REG(SYSCFG->EXTICR[2], SYSCFG_EXTICR3_EXTI9, 0); // EXTI9 points to A by default
    //EXTI->FTSR |= EXTI_FTSR_TR0; // set interrupt for falling trigger
    EXTI->RTSR |= EXTI_RTSR_TR0; // set interrupt for rising trigger
    EXTI->IMR |= EXTI_IMR_IM0; // unmask EXTI0 interrupt line in EXTI
    NVIC_EnableIRQ(EXTI0_IRQn); // enable EXTI0 in NVIC
    
    usart2_init();
    usart2_transCurrentState();

    while (1)
    {
        // here usart2 communication:
        if(USART2->SR & USART_SR_RXNE) // if data is ready to be read
        {
            recChar = usart2_recChar(); // receive char

            // if valid number for desired state was received:      
            if (recChar>=48 + 1 && recChar <= 48 + NUMBER_OF_OUTPUT_STATES) // in ASCII: 0d48 = char '0' 
            {
                recChar_instruction = 255;
                recChar_desiredState_int = (uint8_t) recChar - 48;
                usart2_transChar('\b');
                usart2_transChar(recChar);
                lineStarted = 1;
            }

            // if valid instruction was received:
            else if (recChar == 'i' || recChar != 'n')
            {
                recChar_desiredState_int = 255;
                usart2_transChar('\b');
                usart2_transChar(recChar);

                if (recChar == 'i')
                {
                    recChar_instruction = 0;
                }

                if (recChar == 'n')
                {
                    recChar_instruction = 1;
                }
            }
            
            else if (recChar == '\r')
            {
                if (recChar_desiredState_int != 255)
                {
                    // set current state to desired state:
                    setOutputs(recChar_desiredState_int);
                    currentState = recChar_desiredState_int;
                    recChar_desiredState_int = 255; // reset desired state
                    newLineIfLineStarted();                
                    sprintf(transString, "state was set to %d\n", currentState);
                    usart2_transString(transString);
                }
                else if (recChar_instruction != 255)
                {
                    // follow instruction:
                    if (recChar_instruction == 0) // toggle interrupt mask
                    {
                        if (EXTI->IMR & EXTI_IMR_IM0) // if interrupt is unmasked
                        {
                            EXTI->IMR &= ~EXTI_IMR_IM0; // mask trigger interrupt
                            usart2_transString("I DON'T react to triggers!\n");
                        }
                        else
                        {
                            EXTI->IMR |= EXTI_IMR_IM0; // unmask trigger interrupt
                            usart2_transString("I DO react to triggers\n");
                        }
                        recChar_instruction = 255;
                    }

                    else if (recChar_instruction == 1) // set n_epp
                    {
                        EXTI->IMR &= ~EXTI_IMR_IM0; // mask trigger interrupt

                        usart2_transString("Please type desired n_epp: ");
                        while (n_epp_digit[1] > 9) // loop until 2nd digit of n_epp is received
                        {
                            if(USART2->SR & USART_SR_RXNE) // if data is ready to be read
                            {
                                recChar = usart2_recChar();
                                if (recChar >= 48 && recChar <= 57) // if a digit was received
                                {
                                    if (n_epp_digit[0] > 9) // n_epp_digit[0] is not yet written write first digit
                                    {
                                        n_epp_digit[0] = recChar - 48;
                                        usart2_transChar(recChar);
                                    }
                                    else // write second digit
                                    {
                                        n_epp_digit[1] = recChar - 48;
                                        usart2_transChar(recChar);
                                        usart2_transChar('\n');
                                    }                                                                        
                                }
                            }
                        }
                        n_epp = 10*n_epp_digit[0] + n_epp_digit[1];
                        sprintf(transString, "n_epp was set to %d\n", n_epp);
                        usart2_transString(transString);                        
                        recChar_instruction = 255;

                        EXTI->IMR |= EXTI_IMR_IM0; // unmask trigger interrupt
                    }
                }
                
            }
/*
            else // no valid instruction was received
            {
                recChar_desiredState_int = 255;
                usart2_transChar('\b');
            }
*/            
        }
    }
    
}

/*
register editing macros:
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)    ((REG) & (BIT))
#define CLEAR_REG(REG)        ((REG) = (0x0))
#define WRITE_REG(REG, VAL)   ((REG) = (VAL))
#define READ_REG(REG)         ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))
#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL))) 
*/
