#include "io430.h"
 
int v_tr1, v_tr0, v_adc;
unsigned char RX[3], TX[4];
 
int i=0, j=0;
 
int main( void )
{
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;
 
    __bis_SR_register(SCG0);                    // desabilita FLL
    CSCTL3 |= SELREF__REFOCLK;                  // Seta REFO como fonte de referencia do FLL
    CSCTL0 = 0;                                 // limpa registradores DCO e MOD
    CSCTL1 &= ~(DCORSEL_7);                     // limpa bits de seleção da frequencia
    CSCTL1 |= DCORSEL_3;                        // Seta DCO = 8MHz
    CSCTL2 = FLLD_0 + 243;                      // DCODIV = 8MHz
    __delay_cycles(3);
    __bic_SR_register(SCG0);                    // habilita FLL
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1));  // repete enquanto FLL esta travado
 
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;  // seta REFO(~32768Hz) como fonte ACLK, ACLK = 32768Hz
                                                // DCODIV como fonte do MCLK e SMCLK
 
    // Configura pinos UART
    P1SEL0 |= BIT0 | BIT1;                      // seta 2-UART pinos na função secundaria
 
    // Configura UART
    UCA0CTLW0 |= UCSWRST;
    UCA0CTLW0 |= UCSSEL__SMCLK;
 
    // calculo do Baud Rate
    // 8000000/(16*9600) = 52.083
    // parte fracional = 0.083
    // tabela User's Guide 14-4: UCBRSx = 0x49
    // UCBRFx = int ( (52.083-52)*16) = 1
    UCA0BR0 = 52;                               // 8000000/16/9600
    UCA0BR1 = 0x00;
    UCA0MCTLW = 0x4900 | UCOS16 | UCBRF_1;
 
    UCA0CTLW0 &= ~UCSWRST;                      // Inicializa eUSCI
    UCA0IE |= UCRXIE;                           // habilita interrupção USCI_A0 RX
 
 
    // Configura ADC
    ADCCTL0 |= ADCON | ADCMSC;                  // ADCON
    ADCCTL1 |= ADCSHS_2 | ADCCONSEQ_2;          // repete canal simple; TA1.1 como disparo de amotra
    ADCCTL2 = ADCRES_0;
    ADCMCTL0 |= ADCINCH_8;                      // seleciona pino A8 como ADC
    ADCIE |= ADCIE0;                            // habilita interrupção do ADC quando a conversão estiver completa
    ADCCTL0 |= ADCENC | ADCSC;                  // começa amostragem simples e conversão
 
    // TimerA1.1 - disparo da conversão do sinal de ADC
    TA1CTL = TASSEL__ACLK | MC__UP | TACLR;     // ACLK, modo up
    TA1CCTL1 = OUTMOD_4;                        // TA1CCR0 no modo toggle
    TA1CCR0 = 1024;                          // TA1.1 ADC trigger
 
    // disparo do sinal do Driver do LED
    TA0CTL = TASSEL__SMCLK | MC__UP | TACLR;
    TA0CCR0 = 1024; //32768/200;
    TA0CCR1 = 100;
    TA0CCTL1 = OUTMOD_7;                      // CCR1 reset/set
//    TA0CCTL0 |= CCIE;
    P1DIR |= BIT7;                     //  P1.7 output
    P1SEL0 |=  BIT7;                    // P1.7 options select
 
     while(1)
         __bis_SR_register(LPM3_bits | GIE);    // entra em LPM3 com interrupções
 
return 0;
}
 
// serviço de interrupção de rotina do UART
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
  switch(__even_in_range(UCA0IV,USCI_UART_UCTXCPTIFG))
  {
    case USCI_UART_UCRXIFG:                     // caso receba dado
    {
        RX[i]=UCA0RXBUF;                        // armazena dados no vetor RX
        switch(i)                               // testa valor e vai cadenciando comunicação
        {
        case 0:
            if (RX[i] == 0x05)
            i++;
            else i=0;
            break;
        case 1:
            if (RX[i] == 0x64)
                i++;
            else i=0;
            break;
        case 2:
            if (RX[i] == 0xAB){                 //processa frame
                i=0;
                TX[0] = 0xFA;
                TX[1] = 0x9AB;
                TX[2] = (33*v_adc)/1024;     // calcula tensao no pot
                TX[3] = (v_adc/4);        // calcula ciclo de trabalho
                while(!(UCA0IFG&UCTXIFG));      // aguarda ultima transmissao se houver
                UCA0IE |= UCTXIE;               // habilita interrupções de transmissao
                }
            else i=0;
        break;
        }
    }
    case USCI_UART_UCTXIFG:                     // caso receba dados
        UCA0TXBUF = TX[j++];                    // recebe proximo dado
        //if (j == sizeof TX)                   // transmissao acabou?
        if (j == 4)
        {
            UCA0IE &= ~UCTXIE;                  // desabilita interrupções de USCI_A0 TX
            j=0;
        }
  }
}
 
// serviço de interrupção de rotina do ADC
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void){
    v_tr0=TA0CCR0;                              // calcula e seta ciclo de trabalho
    v_adc=ADCMEM0;
    v_tr1=((v_tr0)*(v_adc/256));
    TA0CCR1 = v_adc;
 //   TA0CCTL1 |= CCIE;
}
