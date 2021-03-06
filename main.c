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
 
/*
 * La rutina del ADC lee el valor de entrada de la LDR y actualiza el valor del DAC según corresponda.
 * Si se lee un valor alto (930) de luz ambiental, se le pasa al DAC el valor más bajo para generar
 * una luz de baja intensidad. Si el valor leído es medio (entre 465 y 930), se le pasa un valor medio.
 * Si el valor es bajo (menor a 465), se prende la luz a la máxima intensidad.
 *
 * Además, si es la primera vez que se entró en el estado dado, se envía un msj mediante el módulo UART
 * para indicar el cambio.
 */
void ADC_IRQHandler() {
  uint32_t ADC_value;
  static uint32_t last_sent = 1;
  LPC_ADC->ADCR &= START_ON_MAT1_0;

  // El resultado se encuentra en los bits [15:4]
  // Para obtenerlo, se corre el registro 4 lugares y se enmascara con 0x3ff
  // para descartar el valor de los demás bits
  // TODO: revisar por qué ignora los 2 bits menos significativos del valor
  ADC_value = ((LPC_ADC->ADDR0) >> 6) & 0x3ff;

  // TODO: Agregar cálculo de donde sale este 930, y agregar macro con el valor
  // 930 = 3v
  // 465 = 1.5v
  if (ADC_value < MEDIUM) {
    DAC_UpdateValue(LPC_DAC, HIGH);
    if (last_sent != LOW) {
      UART_Send(LPC_UART0, MSG_LOW_LIGHT, sizeof(MSG_LOW_LIGHT), NONE_BLOCKING);
      last_sent = LOW;
    }
  }
  else if (ADC_value >= MEDIUM && ADC_value < HIGH) {
    DAC_UpdateValue(LPC_DAC, MEIDUM);
    if (last_sent != MEDIUM) {
      UART_Send(LPC_UART0, MSG_MEDIUM_LIGHT, sizeof(MSG_MEDIUM_LIGHT), NONE_BLOCKING);
      last_sent = MEDIUM;
    }
  }
  else if (ADC_value >= HIGH) {
    DAC_UpdateValue(LPC_DAC, LOW);
    if (last_sent != HIGH) {
      UART_Send(LPC_UART0, MSG_HIGH_LIGHT, sizeof(MSG_HIGH_LIGHT), NONE_BLOCKING);
      last_sent = HIGH;
    }
  }
}

void UART_IntReceive() {
  //TODO: por qué desactiva las interrupciones por EINT3 y cdo las vuelve a activar?
  NVIC_DisableIRQ(EINT3_IRQn);

  UART_Receive(LPC_UART0, info, sizeof(info), NONE_BLOCKING);
}

/*
 * Esta rutina genera una señal PWM de período 20 ms y ciclo de trabajo variable según el valor
 * cargado en los registros de match correspondientes, para modificar el sentido de giro de los
 * motores.
 */
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

/*
 * Esta rutina genera una señal PWM de período 20 ms y ciclo de trabajo variable según el valor
 * cargado en los registros de match correspondientes, para modificar el sentido de giro de los
 * motores.
 */
void TIMER1_IRQHandler() {
  if (LPC_TIM1->IR & 1) 
    LPC_GPIO0->FIOCLR = (1 << 9);
  else if (LPC_TIM1->IR & (1 << 1)) 
    LPC_GPIO0->FIOSET = (1 << 9);

  // Limpio banderas de interrupción
  LPC_TIM1->IR |= 3;
}

/*
 * Esta rutina genera una señal que conmuta cada medio segundo para generar la intermitencia
 * de los guiñes que correspondan.
 */
void TIMER2_IRQHandler() {
  // El timer2 tiene configurado un solo match, por lo que no hace falta preguntar
  // cuál interrumpió

  // Prender o apagar los guiñes según corresponda
  set_blinkers();

  // Limpiar bandera de interrupción
  LPC_TIM2->IR = 1;
}

/*
 * Esta función lee la tecla del teclado matricial presionada, y cambia el sentido de giro del
 * motor que corresponda, modificando los valores de los registros de match según lo calculado.
 * Antes de hacer esto, notifica por UART el cambio de estado del sistema, notificando la tecla
 * leída.
 */
void EINT3_IRQHandler() {
  uint8_t key;

  NVIC_DisableIRQ(EINT3_IRQn);
  debouncing();
  key = getKey();

  UART_Send(LPC_UART0, key, sizeof(key), NONE_BLOCKING);

  change_motion(key);

  LPC_GPIOINT->IO0IntClr| = INTERRUPT_PINS;
  NVIC_EnableIRQ(EINT3_IRQn);
}

/*
 * Funciones extra
 */
uint8_t get_key() {
  // TODO: implementar funcion
  return 2;
}

void change_motion(enum Movement mov) {
  movement = mov;
  
  switch (mov) {
    case STRAIGHT:
      LPC_TIM1->MR0 = 150000;
      break;
    case RIGHT:
      LPC_TIM1->MR0 = 120000;
      NVIC_EnableIRQ(TIMER2_IRQn);
      break;
    case LEFT:
      LPC_TIM1->MR0 = 180000;
      NVIC_EnableIRQ(TIMER2_IRQn);
      break;
    case STOP:
      LPC_TIM0->MR0 = 150000;
      break;
    case REVERSE:
      LPC_TIM0->MR0 = 100000;
      break;
  }
}

void set_blinkers() {
  switch(movement) {
    case LEFT:
      // Si estoy girando a la izquierda, apago el guiñe derecho y le hago un toggle al izquierdo
      LPC_GPIO2->FIOCLR |= BLINK_RIGHT;
      toggle(BLINK_LEFT);
      break;
    case RIGHT:
      // Si estoy girando a la izquierda, apago el guiñe izquierdo y le hago un toggle al derecho
      LPC_GPIO2->FIOCLR |= BLINK_LEFT;
      toggle(BLINK_RIGHT);
      break;
    case STRAIGHT:
    case STOP:
    case REVERSE:
      // Si no estoy girando, apago los guiñes
      LPC_GIPO2->FIOCLR |= BLINK_RIGHT | BLINK_LEFT;
      NVIC_DisableIRQ(TIMER2_IRQn);
      break;
  }
}

void toggle(uint32_t pin) {
  if (LPC_GPIO2->FIOPIN & pin) 
      LPC_GPIO2->FIOCLR |= pin;
    else
      LPC_GPIO2->FIOSET |= pin;
}

void retardo() {
  for(int i = 0; i < 500000; i++);
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
