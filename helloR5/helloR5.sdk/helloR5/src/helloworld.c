/******************************************************************************
 *
 * Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
 ******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xgpio.h"
#include "sleep.h"
#include "xscugic.h"

XGpio sw;
XGpio led;
XScuGic gic;

#define LED_CHANNEL 2
#define SW_INT XGPIO_IR_CH1_MASK
#define GPIO_0_INTERRUPT_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR

void SW_Irq_Handler(void *gpio)
{
    // Disable GPIO Interrupts
    XGpio_InterruptDisable(gpio, SW_INT);

    // Ignore addition button press
    if ((XGpio_InterruptGetStatus(gpio) & SW_INT) != SW_INT)
        return;

    int val = XGpio_DiscreteRead(gpio, 1);
    switch(val) {
        case 0x4:
            xil_printf("SW 4 pressed!\n\r");
            XGpio_DiscreteWrite(&led, LED_CHANNEL, 0x4); // D14: ON
            break;
        case 0x1:
            xil_printf("SW 3 pressed!\n\r");
            XGpio_DiscreteWrite(&led, LED_CHANNEL, 0x2); // D13: ON
            break;
        case 0x2:
            xil_printf("SW 2 pressed!\n\r");
            XGpio_DiscreteWrite(&led, LED_CHANNEL, 0x1); // D12: ON
            break;
    }

    // Clear the interrupt bit
    XGpio_InterruptClear(gpio, SW_INT);
    // Enabl GPIO interrupts
    XGpio_InterruptEnable(gpio, SW_INT);
}

int My_InterruptInitialize(u16 DeviceID)
{
    // interrpt controller initizlization
    XScuGic_Config *IntcConfig = XScuGic_LookupConfig(DeviceID);
    int ret = XScuGic_CfgInitialize(&gic, IntcConfig, IntcConfig->CpuBaseAddress);
    if (ret != XST_SUCCESS)
        return XST_FAILURE;

    // Register Interrupt handler
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler) XScuGic_InterruptHandler,
                                 &gic);
    Xil_ExceptionEnable();


    // Connect GPIO interrupt to handler
    ret = XScuGic_Connect(&gic, GPIO_0_INTERRUPT_ID,
                          (Xil_ExceptionHandler) SW_Irq_Handler,
                          (void *) &sw);

    if (ret !=  XST_SUCCESS)
        return XST_FAILURE;

    // Enable GPIO Interrupts
    XGpio_InterruptEnable(&sw, SW_INT);
    XGpio_InterruptGlobalEnable(&sw);

    // Enable GPIO interrupts in the controller
    XScuGic_Enable(&gic, GPIO_0_INTERRUPT_ID);

    return XST_SUCCESS;
}

int main()
{
    init_platform();

    // Initialize SW2 ~ SW4
    int ret = XGpio_Initialize(&sw, XPAR_GPIO_0_DEVICE_ID);
    if (ret != XST_SUCCESS)
        return XST_FAILURE;

    // setup gpio direction to IN
    XGpio_SetDataDirection(&sw, 1, 0xff);

    print("Hello Cortex-R5\n\r");

    // Initialize LEDs
    ret = XGpio_Initialize(&led, XPAR_GPIO_0_DEVICE_ID);
    if (ret != XST_SUCCESS)
        return XST_FAILURE;

    // setup gpio direction to OUT
    XGpio_SetDataDirection(&led, 2, 0xff);

    // Initialize Interrupt
    ret = My_InterruptInitialize(XPAR_SCUGIC_0_DEVICE_ID);
    if (ret != XST_SUCCESS)
        return XST_FAILURE;

    // wait for interrupt triggered
    while (1) ;
    cleanup_platform();
    return 0;
}