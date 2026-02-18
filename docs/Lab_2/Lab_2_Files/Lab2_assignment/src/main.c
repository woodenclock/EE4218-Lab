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
#include "xllfifo.h"

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

#define A_SIZE                          (A_ROWS * A_COLS)
#define B_SIZE                          (B_ROWS * B_COLS)

// UART constants
#define MAX_A_CSV                       (A_ROWS * A_COLS * 4)
#define MAX_B_CSV                       (B_ROWS * B_COLS * 4)

#define ASCII_MASK                      0x30

// AXI stream constants
#define WORD_SIZE 4			/* Size of words in bytes */
#define MAX_PACKET_LEN 4
#define NO_OF_PACKETS 64
#define MAX_DATA_BUFFER_SIZE NO_OF_PACKETS*MAX_PACKET_LEN

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#define FIFO_DEV_ID	   	                XPAR_AXI_FIFO_0_DEVICE_ID
#else
#define	XUARTPS_BASEADDRESS		XPAR_XUARTPS_0_BASEADDR
#endif

/************************** Function Prototypes ******************************/

#ifndef SDT
int UartPsInitialise(u16 DeviceId);
int XLlFifoInitialise(XLlFifo *InstancePtr, u16 DeviceId);
#else
int UartPsInitialise(UINTPTR BaseAddress);
int XLlFifoInitialise(XLlFifo *InstancePtr, UINTPTR BaseAddress);
#endif

void UartReceiveData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], u32* ABytesReceived, u32* BBytesReceived);
void parseData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], u32 ABytesReceived, u32 BBytesReceived, u32 SourceBuffer[A_SIZE + B_SIZE]);

// AXI stream functions
int AXISTxSend(XLlFifo *InstancePtr, u32 *SourceAddr, int transmitSize);
int AXISRxReceive(XLlFifo *InstancePtr, u32 *DestinationAddr);

void getMatrices(u32 DestinationBuffer[A_SIZE + B_SIZE], u32 A[A_ROWS][A_COLS], u32 B[B_ROWS][B_COLS]);
/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */
XLlFifo FifoInstance;

/*****************************************************************************/
int main(void)
{
	int Status;
    u8 ARecvBuffer[MAX_A_CSV];
    u8 BRecvBuffer[MAX_B_CSV];
    u32 ABytesReceived = 0;
    u32 BBytesReceived = 0;

    u32 SourceBuffer[A_SIZE + B_SIZE];
    u32 DestinationBuffer[A_SIZE + B_SIZE];
    
    u32 A[A_ROWS][A_COLS]; 
    u32 B[B_ROWS][B_COLS];

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
    UartReceiveData(ARecvBuffer, BRecvBuffer, &ABytesReceived, &BBytesReceived);
    
    parseData(ARecvBuffer, BRecvBuffer, ABytesReceived, BBytesReceived, SourceBuffer);
    AXISTxSend(&FifoInstance, SourceBuffer, (A_SIZE + B_SIZE));
    AXISRxReceive(&FifoInstance, DestinationBuffer);
    getMatrices(DestinationBuffer, A, B);
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

#ifndef SDT
int XLlFifoInitialise(XLlFifo *InstancePtr, u16 DeviceId)
#else
int XLlFifoInitialise(XLlFifo *InstancePtr, UINTPTR BaseAddress)
#endif
{
    XLlFifo_Config *Config;
	int Status;
	Status = XST_SUCCESS;

#ifndef SDT
	Config = XLlFfio_LookupConfig(DeviceId);
#else
	Config = XLlFfio_LookupConfig(BaseAddress);
#endif
	if (!Config) {
#ifndef SDT
		xil_printf("No config found for %d\r\n", DeviceId);
#endif
		return XST_FAILURE;
	}

    Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed\n\r");
		return Status;
	}

    Status = XLlFifo_Status(InstancePtr);
	XLlFifo_IntClear(InstancePtr,0xffffffff);
	Status = XLlFifo_Status(InstancePtr);
	if(Status != 0x0) {
		xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
			    "Expected : 0x0\n\r",
			    XLlFifo_Status(InstancePtr));
		return XST_FAILURE;
	}
    return Status;
}

void UartReceiveData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], u32* ABytesReceived, u32* BBytesReceived) {
    bool matrixAReady = false;
    bool matrixBReady = false;
    u8 numRowsReceived = 0;
    u32 bytesReceived = 0;
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
    *ABytesReceived = bytesReceived;

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
    *BBytesReceived = bytesReceived;
}

void parseData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], u32 ABytesReceived, u32 BBytesReceived, u32 SourceBuffer[A_SIZE + B_SIZE]) {
    int val = 0;
    for(u32 i = 0; i < ABytesReceived; i ++) {
        if(ARecvBuffer[i] == '\r') {
            continue;
        } else if(ARecvBuffer[i] == '\n' || ARecvBuffer[i] == ',') {
            SourceBuffer[i] = val;
            val = 0;
        } else {
            int num = (int)(ARecvBuffer[i] & ~ASCII_MASK);
            val = val * 10 + num;
        }
    }

    val = 0;
    for(u32 j = 0; j < BBytesReceived; j ++) {
        if(BRecvBuffer[j] == '\r') {
            continue;
        } else if(BRecvBuffer[j] == '\n' || BRecvBuffer[j] == ',') {
            SourceBuffer[ABytesReceived + j] = val;
            val = 0;
        } else {
            int num = (int)(BRecvBuffer[j] & ~ASCII_MASK);
            val = val * 10 + num;
        }
    }
}

int AXISTxSend(XLlFifo *InstancePtr, u32 *SourceAddr, int transmitSize)
{
	xil_printf("Transmitting Data ... \r\n");

	for(int i=0 ; i < transmitSize ; i++){
		/* Writing into the FIFO Transmit Port Buffer */
        if( XLlFifo_iTxVacancy(InstancePtr) ){
            XLlFifo_TxPutWord(InstancePtr, SourceAddr[i]);
        }
	}

	/* Start Transmission by writing transmission length into the TLR */
	XLlFifo_iTxSetLen(InstancePtr, transmitSize);

	/* Check for Transmission completion */
	while( !(XLlFifo_IsTxDone(InstancePtr)) ){
	}
    xil_printf("Transmission Complete \r\n");

	return XST_SUCCESS;
}

int AXISRxReceive (XLlFifo *InstancePtr, u32* DestinationAddr)
{
	int Status;
	u32 RxWord;
	static u32 ReceiveLength;

	xil_printf(" Receiving data ....\n\r");

	while(XLlFifo_iRxOccupancy(InstancePtr)) {
		/* Read Receive Length */
		ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/WORD_SIZE;
		for (u32 i=0; i < ReceiveLength; i++) {
			RxWord = XLlFifo_RxGetWord(InstancePtr);
			DestinationAddr[i] = RxWord;
		}
	}

	Status = XLlFifo_IsRxDone(InstancePtr);
	if(Status != TRUE){
		xil_printf("Failing in receive complete ... \r\n");
		return XST_FAILURE;
	}

    xil_printf("Receive Complete \r\n");

	return XST_SUCCESS;
}

void getMatrices(u32 DestinationBuffer[A_SIZE + B_SIZE], u32 A[A_ROWS][A_COLS], u32 B[B_ROWS][B_COLS]) {
    
    for(int i = 0; i < A_ROWS; i++) {
        for(int j = 0; j < A_COLS; j++) {
            A[i][j] = DestinationBuffer[i * A_COLS + j];
        }
    }

    for(int i = 0; i < B_ROWS; i++) {
        for(int j = 0; j < B_COLS; j++) {
            B[i][j] = DestinationBuffer[i * B_COLS + j];
        }
    }
}