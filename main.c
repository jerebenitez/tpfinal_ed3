#include "LPC17xx.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpio.h"

#include "tp3.h"

void inline configUART();
void inline configPins();
void inline configDAC();
void inline configGPIO();
void inline configIntGPIO();
void inline configADC();
void inline configTimers();

int main() {
  // Configuración de periféricos.
  configUART();
  UART_Send(LPC_UART0, MSG_CFG_OK_UART, sizeof(MSG_CFG_OK_UART), NONE_BLOCKING);
  configPins();
  UART_Send(LPC_UART0, MSG_CFG_OK_PINS, sizeof(MSG_CFG_OK_PINS), NONE_BLOCKING);
  configDAC();
  UART_Send(LPC_UART0, MSG_CFG_OK_DAC, sizeof(MSG_CFG_OK_DAC), NONE_BLOCKING);
  configGPIO();
  UART_Send(LPC_UART0, MSG_CFG_OK_GPIO, sizeof(MSG_CFG_OK_GPIO), NONE_BLOCKING);
  configIntGPIO();
  UART_Send(LPC_UART0, MSG_CFG_OK_INT_GPIO, sizeof(MSG_CFG_OK_INT_GPIO), NONE_BLOCKING);
  configADC();
  UART_Send(LPC_UART0, MSG_CFG_OK_ADC, sizeof(MSG_CFG_OK_ADC), NONE_BLOCKING);
  configTimers();
  UART_Send(LPC_UART0, MSG_CFG_OK_TIMERS, sizeof(MSG_CFG_OK_TIMERS), NONE_BLOCKING);

  while(1);
}

/*
 * Handlers de interrupciones
 */
void ADC_IRQHandler() {
  uint32_t ADC_value;
  LPC_ADC->ADCR &= START_ON_MAT1_0;

  // El resultado se encuentra en los bits [15:4]
  // Para obtenerlo, se corre el registro 4 lugares y se enmascara con 0x3ff
  // para descartar el valor de los demás bits
  // TODO: revisar por qué ignora los 2 bits menos significativos del valor
  ADC_value = ((LPC_ADC->ADDR0) >> 6) & 0x3ff;

  // TODO: Agregar cálculo de donde sale este 930, y agregar macro con el valor
  if (ADC_value >= 930) {
    if (control == 0)
      UART_Send(LPC_UART0, MSG_HIGH_BEAM, sizeof(MSG_HIGH_BEAM), NONE_BLOCKING);

    control++;
  } else {
    control = 0;
  }

  DAC_UpdateValue(LPC_ADC, ADC_value);
}

void UART_IntReceive() {
  //TODO: por qué desactiva las interrupciones por EINT3 y cdo las vuelve a activar?
  NVIC_DisableIRQ(EINT3_IRQn);

  UART_Receive(LPC_UART0, info, sizeof(info), NONE_BLOCKING);
}

void TIMER0_IRQHandler() {
  if (LPC_TIM0->IR & 1) {
    LPC_GPIO0->FIOCLR = (1 << 8);
  }
  else if (LPC_TIM0-> IR & (1 << 1)) {
    //TODO: por qué este timer prende el adc, y qué se supone que está activando?
    LPC_ADC->ADCR |= START_NOW;
    LPC_GPIO0->FIOSET = (1 << 8);
  }

  // Limpiar banderas de interrupción
  LPC_TIM0->IR |= 3;
}

void TIMER1_IRQHandler() {
  if (LPC_TIM1->IR & 1) 
    LPC_GPIO0->FIOCLR = (1 << 9);
  else if (LPC_TIM1->IR & (1 << 1)) 
    LPC_GPIO0->FIOSET = (1 << 9);

  // Limpio banderas de interrupción
  LPC_TIM1->IR |= 3;
}

/*
 * Configuración de periféricos
 */
void configUART() {
  UART_CFG_Type uart;
  UART_FIFO_CFG_Type fifo;

  // Inicializar UART0 y usar configuraciones por defecto
  UART_ConfigStructInit(&uart);
  UART_Init(LPC_UART0, &uart);

  // Inicializar FIFO y usar configuraciones por defecto
  UART_FIFOConfigStructInit(&fifo);
  UART_FIFOConfig(LPC_UART0, &config);

  // Habilitar interrupciones por Rx
  UART_IntConfig(LPC_UART0, UART_INTCFG_RBR, ENABLE);
  // Habilitar interrupción por estado de línea
  UART_IntConfig(LPC_UART0, UART_INTCFG_RLS, ENABLE);

  // Habilitar transmisión por UART
  UART_TxCmd(LPC_UART0, ENABLE);

  NVIC_EnableIRQ(UART0_IRQn);
}

void configDAC() {
  DAC_Init(LPC_DAC);
  DAC_SetBias(LPC_DAC, DAC_BIAS_700uA);
}

void configADC() {
  LPC_SC->PCONP |= ADC_POWER;
  LPC_ADC->ADCR |= ADC_ENABLE;
  LPC_PCLKSEL0 |= ADC_CCLK_8;
  LPC_ADC-> ADCR &= ~(ADC_CLKDIV_1 | NO_BURST);
  LPC_PINCON->PINMODE1 |= NEITHER;
  LPC_PINCON->PINSEL1 |= AD0_0;
  // Habilita las interrupciones por el canal 0
  LPC_ADC->ADINTEN = 1;
  NVIC_EnableIRQ(ADC_IRQn);
}

void configGPIO() {
  LPC_GPIO0->FIODIR |= P0_OUTPUTS;
  LPC_GPIO0->FIODIR &= INTERRUPT_PINS;

  LPC_GPIO2->FIODIR |= 3;
}

void configIntGPIO() {
  // Habilitar interrupciones por flanco de bajada
  LPC_GPIOINT->IO0IntEnF |= INTERRUPT_PINS;
  // Limpiar flags de interrupciones
  LPC_GPIOINT->IO0IntClr |= INTERRUPT_PINS; 

  // Habilitar las interrupciones externas, que comparten entrada en el vector
  // con EINT3
  NVIC_EnableIRQ(EINT3_IRQn);

  //TODO: Revisar qué está haciendo acá. Nunca configuró esos pines
  LPC_GPIO0->FIOCLR |= (1 << 21) | (1 << 28);
  LPC_GPIO2->FIOCLR |= (1 << 13);
}

void configPins() {
  PINSEL_CFG_Type pin;

  // Configurar pin del DAC
  pin.Portnum = 0;
  pin.Pinnum = 26;
  pin.Funcnum = 2;
  pin.Pinmode = 0;
  pin.OpenDrain = 0;
  PINSEL_ConfigPin(&pin);

  // Configurar pin de Tx (TX0)
  pin.Funcnum = 1;
  pin.Pinnum = 2;
  PINSEL_ConfigPin(&pin);

  // Configurar pin de Rx (RX0)
  pin.Pinnum = 3;
  PINSEL_ConfigPin(&pin);

  // TODO: revisar si esto queda, en teoría no es necesario para el tp (según los comentarios)
  // Configurar pin 2.12 como GPIO para prender led
  pin.Funcnum = 0;
  pin.Portnum = 2;
  pin.Pinnum = 12;
  PINSEL_ConfigPin(&pin);

  // Configurar pin 2.13 como GPIO para prender led
  pin.Pinnum = 13;
  PINSEL_ConfigPin(&pin);
}

void configTimers() {
  configTimer0();
  configTimer1();
  configTimer2();
}

void configTimer0() {
  LPC_SC->PCLKSEL0 |= T0_CCLK;
  LPC_TIM0->MR0 = 150000;
  LPC_TIM0->MR1 = 2000000;
  LPC_TIM0->TCR |= TIM_ENABLE | TIM_RESET;
  LPC_TIM0->TCR &= ~TIM_RESET;
  LPC_TIM0->MCR |= INT_M0 | INT_M1 | RESET_M1;
  NVIC_Enable(TIMER0_IRQn);
}

void configTimer1() {
  LPC_SC->PCLKSEL0 |= T1_CCLK;
  LPC_TIM1->MR0 = 150000;
  LPC_TIM1->MR1 = 2000000;
  LPC_TIM1->TCR |= TIM_ENABLE | TIM_RESET;
  LPC_TIM1->TCR &= ~TIM_RESET;
  LPC_TIM1->MCR |= INT_M0 | INT_M1 | RESET_M1;
  NVIC_Enable(TIMER1_IRQn);
}

void inline configTimer2() {
  LPC_SC->PCONP |= T2_POWER;
  LPC_SC->PCLKSEL1 |= T2_CCLK;
  LPC_TIM2->MR0 = 50000000;
  LPC_TIM2->TCR |= TIM_ENABLE | TIM_RESET;
  LPC_TIM2->TCR &= ~TIM_RESET;
  LPC_TIM2->MCR |= INT_M0 | RESET_M0;
}
