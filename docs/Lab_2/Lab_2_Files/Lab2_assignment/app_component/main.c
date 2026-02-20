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
#include "xllfifo_hw.h"

#include <stdbool.h>
#include "matrix.h"

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
#define NUM_WORDS                       (A_ROWS*A_COLS + B_ROWS*B_COLS)   // 64*8 + 8 = 520

#define ASCII_MASK                      0x30

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#else
#define	XUARTPS_BASEADDRESS		        XPAR_XUARTPS_0_BASEADDR
#endif

static XLlFifo Fifo;

static int A[A_ROWS][A_COLS];
static int B[B_ROWS][B_COLS];
static int RES[A_ROWS][B_COLS];

static u32 TxWords[NUM_WORDS];
static u32 RxWords[NUM_WORDS];

static u8 ARecvBuffer[MAX_A_CSV];
static u8 BRecvBuffer[MAX_B_CSV];

/************************** Function Prototypes ******************************/

#ifndef SDT
    int UartPsInitialise(u16 DeviceId);
#else
    int UartPsInitialise(UINTPTR BaseAddress);
#endif

void UartReceiveData(int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]);
void parseData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], 
               u32 ABytesReceived, u32 BBytesReceived,
               int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]);

/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */

/*****************************************************************************/

static int FifoInit(void)
{
    // xil_printf("Entering FifoInit\r\n");

#ifdef SDT
    XLlFifo_Config *Cfg = XLlFfio_LookupConfig(XPAR_XLLFIFO_0_BASEADDR);
#else
    XLlFifo_Config *Cfg = XLlFfio_LookupConfig(XPAR_XLLFIFO_0_DEVICE_ID);
#endif
    if (!Cfg) {
        xil_printf("XLlFifo lookup failed\r\n");
        return XST_FAILURE;
    }

    int Status = XLlFifo_CfgInitialize(&Fifo, Cfg, Cfg->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("XLlFifo_CfgInitialize failed: %d\r\n", Status);
        return XST_FAILURE;
    }

    XLlFifo_Reset(&Fifo);
    XLlFifo_IntClear(&Fifo, 0xFFFFFFFF);
    
    return XST_SUCCESS;
}


static int FifoSendRecvFrame(const u32 *tx, u32 *rx, int nwords)
{
    // write nwords
    for (int i = 0; i < nwords; i++) {
        while (XLlFifo_iTxVacancy(&Fifo) == 0) { }
        XLlFifo_TxPutWord(&Fifo, tx[i]);
    }

    XLlFifo_iTxSetLen(&Fifo, (u32)(nwords * 4));

    // read back exactly nwords as they arrive
    int got = 0;
    while (got < nwords) {
        u32 occ = XLlFifo_iRxOccupancy(&Fifo);
        while (occ-- && got < nwords) {
            rx[got++] = XLlFifo_RxGetWord(&Fifo);
        }
    }
    return XST_SUCCESS;
}


static int FifoLoopbackChunked(const u32 *tx, u32 *rx, int total_words)
{
    const int CHUNK = 256; // safe (<=512 even if RX depth is 512)
    int offset = 0;

    while (offset < total_words) {
        int n = total_words - offset;
        if (n > CHUNK) n = CHUNK;

        int st = FifoSendRecvFrame(tx + offset, rx + offset, n);
        if (st != XST_SUCCESS) return st;

        offset += n;
    }
    return XST_SUCCESS;
}


static void PackAB(const int A[A_ROWS][A_COLS],const int B[B_ROWS][B_COLS], u32 tx[NUM_WORDS])
{
    int idx = 0;

    for (int i = 0; i < A_ROWS; i++) {
        for (int k = 0; k < A_COLS; k++) {
            tx[idx++] = (u32)A[i][k];   // 1 value per 32-bit word (simple + safe)
        }
    }

    for (int k = 0; k < B_ROWS; k++) {
        tx[idx++] = (u32)B[k][0];
    }
}


static void UnpackAB(const u32 rx[NUM_WORDS], int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS])
{
    int idx = 0;

    for (int i = 0; i < A_ROWS; i++) {
        for (int k = 0; k < A_COLS; k++) {
            A[i][k] = (int)rx[idx++];
        }
    }

    for (int k = 0; k < B_ROWS; k++) {
        B[k][0] = (int)rx[idx++];
    }
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
int main(void)
{
	int Status;

#ifndef SDT
	Status = UartPsInitialise(UART_DEVICE_ID);
#else
	Status = UartPsInitialise(XUARTPS_BASEADDRESS);
#endif
	if (Status == XST_FAILURE) {
		xil_printf("Unable to Initialise Uart\r\n");
		return XST_FAILURE;
	}

    UartReceiveData(A, B);

    // --- FIFO loopback stage (Only for Lab 2) ---
    if (FifoInit() != XST_SUCCESS) {
        xil_printf("FIFO init failed\r\n");
        return XST_FAILURE;
    }

    PackAB(A, B, TxWords);

    if (FifoLoopbackChunked(TxWords, RxWords, NUM_WORDS) != XST_SUCCESS) {
        xil_printf("FIFO loopback failed\r\n");
        return XST_FAILURE;
    }

    // Overwrite A,B with what came back from FIFO
    UnpackAB(RxWords, A, B);

    // compute RES = A*B/256
    MatMulDiv256(A, B, RES);    

    // send RES back to PC as CSV (1 value per line)
    PrintResCsv(RES);

    while (1);
}
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////


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


void UartReceiveData(int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]) 
{
    bool matrixAReady = false;
    bool matrixBReady = false;
    u32 ABytesReceived = 0;
    u8 numRowsReceived = 0;
    xil_printf("Ready to receive A.csv\r\n");
    while (!matrixAReady) {
        while ((XUartPs_Recv(&Uart_Ps, &ARecvBuffer[ABytesReceived], 1)) <= 0) {
        }
        if(ABytesReceived >= A_COLS &&
            ARecvBuffer[ABytesReceived] == '\n') {
            numRowsReceived++;
            if(numRowsReceived >= A_ROWS) {
                matrixAReady = true;
            }
        }
        ABytesReceived++;
    }
    xil_printf("Received %d bytes for A.csv\r\n", ABytesReceived);
    xil_printf("Ready to receive B.csv\r\n");
    u32 BBytesReceived = 0;
    numRowsReceived = 0;
    while (!matrixBReady) {
        while ((XUartPs_Recv(&Uart_Ps, &BRecvBuffer[BBytesReceived], 1)) <= 0) {
        }
        if(BBytesReceived >= B_COLS &&
            BRecvBuffer[BBytesReceived] == '\n') {
            numRowsReceived++;
            if(numRowsReceived >= B_ROWS) {
                matrixBReady = true;
            }
        }
        BBytesReceived++;
    }
    xil_printf("Received %d bytes for B.csv\r\n", BBytesReceived);
    parseData(ARecvBuffer, BRecvBuffer, ABytesReceived, BBytesReceived, A, B);
}


static void parseOneCsvMatrix(const u8 *buf, u32 n, int rows, int cols, int out[rows][cols])
{
    int r = 0, c = 0;
    int val = 0;
    bool in_num = false;

    for (u32 i = 0; i < n; i++) {
        u8 ch = buf[i];
        if (ch == '\r') continue;

        if (ch >= '0' && ch <= '9') {
            val = val * 10 + (ch - '0');
            in_num = true;
            continue;
        }

        if (ch == ',' || ch == '\n') {
            if (in_num) {
                if (r < rows && c < cols) out[r][c] = val;
                c++;
                if (ch == '\n') { r++; c = 0; }
            }
            val = 0;
            in_num = false;
            continue;
        }
    }

    // Flush
    if (in_num && r < rows && c < cols) {
        out[r][c] = val;
    }
}


void parseData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], 
               u32 ABytesReceived, u32 BBytesReceived, 
               int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS])
{
    parseOneCsvMatrix(ARecvBuffer, ABytesReceived, A_ROWS, A_COLS, A);
    parseOneCsvMatrix(BRecvBuffer, BBytesReceived, B_ROWS, B_COLS, B);
}
