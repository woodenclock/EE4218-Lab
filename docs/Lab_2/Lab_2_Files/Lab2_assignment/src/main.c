/****************************************************************************/
/**
* Assignment 2:
* - receive matrices A and B into a local array/arrays (either a single array for A and B together or separate arrays);
* - pass it through the AXI Stream FIFO configured in loopback mode - no processing done in hardware/PL, for now;
* - compute the result matrix, RES = A*B/256;
* - send RES back from the board to the PC (RealTerm).
*****************************************************************************/

/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xuartps.h"
#include "xil_printf.h"
#include <stdbool.h>

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define A_ROWS                          64
#define A_COLS                          8
#define B_ROWS                          A_COLS
#define B_COLS                          1

#define MAX_A_CSV                       (A_ROWS * A_COLS * 4)
#define MAX_B_CSV                       (B_ROWS * B_COLS * 4)

#define ASCII_MASK                      0x30

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#else
#define	XUARTPS_BASEADDRESS		XPAR_XUARTPS_0_BASEADDR
#endif

/************************** Function Prototypes ******************************/

#ifndef SDT
int UartPsInitialise(u16 DeviceId);
#else
int UartPsInitialise(UINTPTR BaseAddress);
#endif
void UartReceiveData(int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]);

/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */

/*****************************************************************************/
int main(void)
{
	int Status;
    int A[A_ROWS][A_COLS];
    int B[B_ROWS][B_COLS];
    // int RES[A_ROWS][B_COLS];

#ifndef SDT
	Status = UartPsInitialise(UART_DEVICE_ID);
#else
	Status = UartPsInitialise(XUARTPS_BASEADDRESS);
#endif
	if (Status == XST_FAILURE) {
		xil_printf("Unable to Initialise Uart\r\n");
		return XST_FAILURE;
	}

    xil_printf("Start Receiving Data\r\n");
    UartReceiveData(A, B);
}

/*****************************************************************************/
#ifndef SDT
int UartPsInitialise(u16 DeviceId)
#else
int UartPsInitialise(UINTPTR BaseAddress)
#endif
{
    int Status;
	XUartPs_Config *Config;
#ifndef SDT
	Config = XUartPs_LookupConfig(DeviceId);
#else
	Config = XUartPs_LookupConfig(BaseAddress);
#endif
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XUartPs_SetBaudRate(&Uart_Ps, 115200);
    return Status;
}

void UartReceiveData(int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]) {
    bool matrixAReady = false;
    bool matrixBReady = false;
    u8 ARecvBuffer[MAX_A_CSV];
    u8 BRecvBuffer[MAX_B_CSV];
    u32 bytesReceived = 0;
    u8 numRowsReceived = 0;
    int received = 0;
    xil_printf("Ready to receive A.csv\r\n");
    while (!matrixAReady) {
        while ((XUartPs_Recv(&Uart_Ps, &ARecvBuffer[bytesReceived], 1)) <= 0) {
        }
        if(bytesReceived >= A_COLS &&
            ARecvBuffer[bytesReceived] == '\n') {
            numRowsReceived++;
            if(numRowsReceived >= A_ROWS) {
                matrixAReady = true;
            }
        }
        bytesReceived++;
    }
    xil_printf("Received %d bytes for A.csv\r\n", bytesReceived);
    xil_printf("Ready to receive B.csv\r\n");
    bytesReceived = 0;
    numRowsReceived = 0;
    while (!matrixBReady) {
        while ((XUartPs_Recv(&Uart_Ps, &BRecvBuffer[bytesReceived], 1)) <= 0) {
        }
        if(bytesReceived >= B_COLS &&
            BRecvBuffer[bytesReceived] == '\n') {
            numRowsReceived++;
            if(numRowsReceived >= B_ROWS) {
                matrixBReady = true;
            }
        }
        bytesReceived++;
    }
    xil_printf("Received %d bytes for B.csv\r\n", bytesReceived);
}