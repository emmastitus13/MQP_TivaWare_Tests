
//#include <xdc/std.h>
//#include <xdc/runtime/System.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"

#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/ssi.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"

int g_i32Value;          // Value from the ADC
uint32_t g_ui32PWMValue; // Value to PWM Generator
uint32_t g_ui32SPIData;  // Vale from SPI ADC
bool state = false;      // State of the ADC waveform pin

void setPins(void) {
    // Initialize PE0 as ADC input
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);

    // Initialize PB1 as GPIO output to toggle when the ADC is on/off
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_1 | GPIO_PIN_5);

    // Initialize PB6 as PWM output
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB6_M0PWM0);
    GPIOPinTypePWM(GPIO_PORTB_BASE, GPIO_PIN_6);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
}

void getADC(void) {
    // Clear ADC0SS0 interrupt
    ADCIntClear(ADC0_BASE, 0);
    // Reset P? to measure ADC frequency
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
    // Retrieve the reading
    ADCSequenceDataGet(ADC0_BASE, 0, &g_i32Value);

    g_i32Value -= 2048;
    if (g_i32Value < 0) {
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_5, GPIO_PIN_5);
        g_ui32PWMValue = g_i32Value*(-2.5);
        PWMPulseWidthSet(PWM0_BASE, PWM_GEN_0, g_ui32PWMValue);
    } else {
        g_ui32PWMValue = g_i32Value*2.5;
        PWMPulseWidthSet(PWM0_BASE, PWM_GEN_0, g_ui32PWMValue);
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_5, 0);
    }

    SSIDataPut(SSI0_BASE, 0xD000);
    while(SSIBusy(SSI0_BASE)) ;
    SSIDataGet(SSI0_BASE, &g_ui32SPIData);
    g_ui32SPIData &= 0x0FFF;
}

void setADC(void) {
    // Initialize ADC0 Module
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

//    // Wait for the module to be ready
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {
    }

    ADCSequenceDisable(ADC0_BASE, 0);
//    // ADC_PC_SR_125k, ADC_PC_SR_250k, ADC_PC_SR_500k, ADC_PC_SR_1M
//    ADC0_BASE + ADC_O_PC = (ADC_PC_SR_1M);

    // ADC clock
    // Source - internal PIOSC at 16MHz, clock rate full for now
    // Divider - 1 for now, could change later
    // Maybe use PLL if the frequency isn't high enough
//    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_FULL, 2);

    // Trigger when the processor tells it to (one shot)
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);

    // Take a sample and interrupt
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END);

    // Oversample at 16x (could go higher maybe? lower?)
    ADCHardwareOversampleConfigure(ADC0_BASE, 16);

    ADCSequenceEnable(ADC0_BASE, 0);

    ADCIntEnable(ADC0_BASE, 0);
    ADCIntRegister(ADC0_BASE, 0, &getADC);
}

void startADC(void) {
    // Set PB1 to measure ADC frequency
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_PIN_1);
    // Start the ADC in one-shot mode
    ADCProcessorTrigger(ADC0_BASE, 0);
    // Move to ISR
//    while(!ADCIntStatus(ADC0_BASE, 0, false));  // wait until the sample sequence has completed
//    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
//    ADCSequenceDataGet(ADC0_BASE, 0, &g_ui32Value);// retrieve joystick data
//    ADCIntClear(ADC0_BASE, 0);                  // clear ADC sequence interrupt flag
}

void timerISR(void) {
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    startADC();
}

void setTimer(void) {
    // Enable Timer1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
//    TimerDisable(TIMER1_BASE, TIMER_A);
    // Set clock source
    TimerClockSourceSet(TIMER1_BASE, TIMER_CLOCK_SYSTEM);
    // Set to periodic
    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC);
    // Set the prescaler to 1
    TimerPrescaleSet(TIMER1_BASE, TIMER_A, 0);
    // Set load value
    TimerLoadSet(TIMER1_BASE, TIMER_A, 0x0FFFF);
    // Set compare value
//    TimerMatchSet(TIMER1_BASE, TIMER_A, 0x07FFF);
    // Enable timeout interrupt
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    // Register ISR
    TimerIntRegister(TIMER1_BASE, TIMER_A, &timerISR);
    // Enable Timer1A
    TimerEnable(TIMER1_BASE, TIMER_A);
}

void setPWM(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    SysCtlPWMClockSet(SYSCTL_PWMDIV_32);
//    PWMClockSet(PWM_BASE, PWM_SYSCLK_DIV_64);

    PWMDeadBandDisable(PWM0_BASE, PWM_GEN_0);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);

    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, 5120);

    PWMPulseWidthSet(PWM0_BASE, PWM_GEN_0, 2560);

    PWMOutputState(PWM0_BASE, PWM_OUT_0_BIT, true);

    PWMOutputUpdateMode(PWM0_BASE, PWM_OUT_0_BIT, PWM_OUTPUT_MODE_NO_SYNC);

    PWMGenEnable(PWM0_BASE, PWM_GEN_0);
}

void setSSI(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);

    SSIClockSourceSet(SSI0_BASE, SSI_CLOCK_SYSTEM);

    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 500000, 16);
    SSIEnable(SSI0_BASE);

    // Initialize MCP3202
    SSIDataPut(SSI0_BASE, 0xD000);
    while(SSIBusy(SSI0_BASE)) ;
    SSIDataGet(SSI0_BASE, &g_ui32SPIData);
}


/**
 * main.c
 */
int main(void) {
    IntMasterDisable();

    // Set clock speed to 40MHz ?
    SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);

    setPins();
    setADC();
    setPWM();
    setTimer();
    setSSI();
    IntMasterEnable();
    while(1) {

    }
}
