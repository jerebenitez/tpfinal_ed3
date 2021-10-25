/*
 * Macros y constantes
 */
// Peripheral Clock
#define CCLK 1
#define CCLK_8 3
// Pin modes
#define PULL_UP 0
#define REPEATER 1
#define NEITHER 2
#define PULL DOWN 3

// Pines
#define P0_23 14
#define INTERRUPT_PINS ((1 << 1) | (1 << 6) | (1 << 18))
#define P0_OUTPUTS ((1 << 8) | (1 << 9) | (1 << 15) | (1 << 17) | (1 << 22))

// DAC
#define DAC_BIAS_700uA 0

// ADC
#define ADC_POWER (1 << 12)
#define ADC_ENABLE (1 << 21)
#define ADC_CCLK_8 (CCLK_8 << 24)
#define ADC_CLKDIV_1 (255 << 8)
#define NO_BURST (1 << 16)
#define ADC_PINMODE (NEITHER << P0_23)
#define AD0_0 (1 << 14)

// Timers
#define T2_POWER (1 << 22)
#define T0_CCLK (CCLK << 2)
#define T1_CCLK (CCLK << 4)
#define T2_CCLK (CCLK << 12)
#define TIM_ENABLE (1)
#define TIM_RESET (1 << 1)
#define INT_M0 (1)
#define INT_M1 (1 << 3)
#define RESET_M0 (1 << 1)
#define RESET_M1 (1 << 4)

// Messages
#define MSG_CFG_OK_UART "UART cfg OK"
#define MSG_CFG_OK_PINS "Pins cfg OK"
#define MSG_CFG_OK_DAC  "DAC cfg OK"
#define MSG_CFG_OK_GPIO "GPIO cfg OK"
#define MSG_CFG_OK_INT_GPIO "GPIO Int cfg OK"
#define MSG_CFG_OK_ADC  "ADC cfg OK"
#define MSG_CFG_OK_TIMERS "TIMERS cfg OK"
