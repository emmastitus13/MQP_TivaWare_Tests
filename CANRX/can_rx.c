//*****************************************************************************
//
// can.c - Simple CAN example.
//
// Copyright (c) 2013-2016 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.3.156 of the DK-TM4C123G Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_can.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>CAN Example (can)</h1>
//!
//! This example application utilizes CAN to send characters back and forth
//! between two boards. It uses the UART to read / write the characters to
//! the UART terminal. It also uses the graphical display on the board to show
//! the last character transmited / received. Error handling is also included.
//!
//! CAN HARDWARE SETUP:
//!
//! To use this example you will need to hook up two DK-TM4C123G boards
//! together in a CAN network. This involves hooking the CANH screw terminals
//! together and the CANL terminals together. In addition 120ohm termination
//! resistors will need to be added to the edges of the network between CANH
//! and CANL.  In the two board setup this means hooking a 120 ohm resistor
//! between CANH and CANL on both boards.
//!
//! See diagram below for visual. '---' represents wire.
//!
//! \verbatim
//!       CANH--+--------------------------+--CANH
//!             |                          |
//!            .-.                        .-.
//!            | |120ohm                  | |120ohm
//!            | |                        | |
//!            '-'                        '-'
//!             |                          |
//!       CANL--+--------------------------+--CANL
//! \endverbatim
//!
//! SOFTWARE SETUP:
//!
//! Once the hardware connections are setup connect both boards to the computer
//! via the In-Circuit Debug Interface USB port next to the graphical display.
//! Attach a UART terminal to each board configured 115,200 baud, 8-n-1 mode.
//!
//! Anything you type into one terminal will show up in the other terminal and
//! vice versa. The last character sent / received will also be displayed on
//! the graphical display on the board.
//
//*****************************************************************************

//*****************************************************************************
//
// A counter that keeps track of the number of times the TX & RX interrupt has
// occurred, which should match the number of messages that were transmitted /
// received.
//
//*****************************************************************************
volatile uint32_t g_ui32RXMsgCount = 0;
volatile uint32_t g_ui32TXMsgCount = 0;

//*****************************************************************************
//
// A flag for the interrupt handler to indicate that a message was received.
//
//*****************************************************************************
volatile bool g_bRXFlag = 0;

//*****************************************************************************
//
// A global to keep track of the error flags that have been thrown so they may
// be processed. This is necessary because reading the error register clears
// the flags, so it is necessary to save them somewhere for processing.
//
//*****************************************************************************
volatile uint32_t g_ui32ErrFlag = 0;

//*****************************************************************************
//
// CAN message Objects for data being sent / received
//
//*****************************************************************************
tCANMsgObject g_sCAN0RxMessage;
//*****************************************************************************
//
// Message Identifiers and Objects
// RXID is set to 0 so all messages are received
//
//*****************************************************************************
#define CAN0RXID                0
#define RXOBJECT                1

//*****************************************************************************
//
// Variables to hold character being received
//
//*****************************************************************************
uint8_t g_ui8RXMsgData;
//uint8_t g_ui8RXMsgData[8];

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// CAN 0 Interrupt Handler. It checks for the cause of the interrupt, and
// maintains a count of all messages that have been transmitted / received
//
//*****************************************************************************
void
CAN0IntHandler(void)
{
    uint32_t ui32Status;

    // Read the CAN interrupt status to find the cause of the interrupt
    // 0           = No Interrupt
    // 0x01 - 0x20 = Message object that caused the interrupt
    // 0x8000      = Status interrupt :(
    ui32Status = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);

    // If this was a status interrupt
    if(ui32Status == CAN_INT_INTID_STATUS)
    {
        // Get Status register. This clears the interrupt
        ui32Status = CANStatusGet(CAN0_BASE, CAN_STS_CONTROL);

        // Set the error flags to the status register
        g_ui32ErrFlag |= ui32Status;
    }

    // If RXOBJECT received a message
    else if(ui32Status == RXOBJECT)
    {
        // Clear the interrupt for RXOBJECT
        CANIntClear(CAN0_BASE, RXOBJECT);

        // Increment received message count
        g_ui32RXMsgCount++;

        // Set flag that received a message
        g_bRXFlag = true;

        // Clear error flags
        g_ui32ErrFlag = 0;
    }

    // Shouldn't happen
    else
    {
        //
        // Spurious interrupt handling can go here.
        //
    }
}

void writeLEDs(uint8_t leds) {
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, leds&0x0E);
}


void InitCAN0(void) {
    // Enable port F
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // Enable pins F0, F1, F2, F3
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    // Enable Port E
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Set pins to be used for CAN
    GPIOPinConfigure(GPIO_PE4_CAN0RX);
    GPIOPinConfigure(GPIO_PE5_CAN0TX);

    // Enable CAN functionality for pins E4, E5
    GPIOPinTypeCAN(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // Enable CAN0 peripheral
    SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);

    // Initialize CAN0 controller
    CANInit(CAN0_BASE);

    // Set CAN0 to run at 1Mbps
    CANBitRateSet(CAN0_BASE, SysCtlClockGet(), 1000000);

    // Register interrupt in vector table
    CANIntRegister(CAN0_BASE, CAN0IntHandler);

    // Enable interrupts, error interrupts, and status interrupts
    CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);

    // Enable CAN0 interrupt in interrupt controller
    IntEnable(INT_CAN0);

    // Enable CAN0
    CANEnable(CAN0_BASE);

    // Set up receive message object
    g_sCAN0RxMessage.ui32MsgID = 2;     // Accept any ID
    g_sCAN0RxMessage.ui32MsgIDMask = 0; // Accept any ID
    // Interrupt enable and use ID filter
    g_sCAN0RxMessage.ui32Flags = MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER;
    // Message length = 1 byte (g_ui8RXMsgData is 8 bits long)
    g_sCAN0RxMessage.ui32MsgLen = sizeof(g_ui8RXMsgData);
//    g_sCAN0RxMessage.ui32MsgLen = 1;
//    g_sCAN0RxMessage.pui8MsgData = (uint8_t *)&g_ui8RXMsgData;

    // Load message 1 with g_sCAN0RxMessage settings
    CANMessageSet(CAN0_BASE, RXOBJECT, &g_sCAN0RxMessage, MSG_OBJ_TYPE_RX);
}

void CANErrorHandler(void) {
    // If the bus is off (Too many errors happened)
    if(g_ui32ErrFlag & CAN_STATUS_BUS_OFF)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_BUS_OFF);

    }

    // If there have been many of errors (more than 96)
    if(g_ui32ErrFlag & CAN_STATUS_EWARN)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_EWARN);
    }

    // If there have been a lot of errors (more than 127)
    if(g_ui32ErrFlag & CAN_STATUS_EPASS)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_EPASS);
    }

    // Received message successfully
    if(g_ui32ErrFlag & CAN_STATUS_RXOK)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_RXOK);
    }

    // Transmit message successfully
    if(g_ui32ErrFlag & CAN_STATUS_TXOK)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_TXOK);
    }

    // Check the LEC
    if(g_ui32ErrFlag & CAN_STATUS_LEC_MSK)
    {
        // Clear the flag
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_MSK);
    }

    //
    // A bit stuffing error has occurred.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_STUFF)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_STUFF Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_STUFF);
    }

    //
    // A formatting error has occurred.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_FORM)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_FORM Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_FORM);
    }

    //
    // An acknowledge error has occurred.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_ACK)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_ACK Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_ACK);
    }

    //
    // The bus remained a bit level of 1 for longer than is allowed.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_BIT1)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_BIT1 Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_BIT1);
    }

    //
    // The bus remained a bit level of 0 for longer than is allowed.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_BIT0)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_BIT0 Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_BIT0);
    }

    //
    // A CRC error has occurred.
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_CRC)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_CRC Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_CRC);
    }

    //
    // This is the mask for the CAN Last Error Code (LEC).
    //
    if(g_ui32ErrFlag & CAN_STATUS_LEC_MASK)
    {
        //
        // Handle Error Condition here
        //

        //
        // Clear CAN_STATUS_LEC_MASK Flag
        //
        g_ui32ErrFlag &= ~(CAN_STATUS_LEC_MASK);
    }

    //
    // If there are any bits still set in g_ui32ErrFlag then something unhandled
    // has happened. Print the value of g_ui32ErrFlag.
    //
    if(g_ui32ErrFlag !=0)
    {

    }
}

tCANBitClkParms clkBits;

//*****************************************************************************
//
// Set up the system, initialize the UART, Graphics, and CAN. Then poll the
// UART for data. If there is any data send it, if there is any thing received
// print it out to the UART. If there are errors call the error handling
// function.
//
//*****************************************************************************
int
main(void)
{
    IntMasterDisable();
    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    ROM_FPULazyStackingEnable();

    //
    // Set the clocking to run directly from the crystal.
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);
    //
    // Initialize CAN0
    //
    InitCAN0();
    CANBitTimingGet(CAN0_BASE, &clkBits);
    IntMasterEnable();

    while(1) {
        //
        // If the flag is set, that means that the RX interrupt occurred and
        // there is a message ready to be read from the CAN
        //
        if(g_bRXFlag)
        {
            //
            // Reuse the same message object that was used earlier to configure
            // the CAN for receiving messages.  A buffer for storing the
            // received data must also be provided, so set the buffer pointer
            // within the message object.
            //
            g_sCAN0RxMessage.pui8MsgData = &g_ui8RXMsgData;
//            g_sCAN0RxMessage.ui32MsgLen = sizeof(g_ui8RXMsgData);

            //
            // Read the message from the CAN.  Message object RXOBJECT is used
            // (which is not the same thing as CAN ID).  The interrupt clearing
            // flag is not set because this interrupt was already cleared in
            // the interrupt handler.
            //
            CANMessageGet(CAN0_BASE, RXOBJECT, &g_sCAN0RxMessage, 0);

            // Set leds
//            writeLEDs((*g_sCAN0RxMessage.pui8MsgData)&0x0F);
            writeLEDs((g_ui8RXMsgData)&0x0F);

            //
            // Clear the pending message flag so that the interrupt handler can
            // set it again when the next message arrives.
            //
            g_bRXFlag = 0;

            //
            // Check to see if there is an indication that some messages were
            // lost.
            //
            if(g_sCAN0RxMessage.ui32Flags & MSG_OBJ_DATA_LOST)
            {

            }

        }
        else
        {
            //
            // Error Handling
            //
            if(g_ui32ErrFlag != 0)
            {
                CANErrorHandler();
            }
        }
    }
}
