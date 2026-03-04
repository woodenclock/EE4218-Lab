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
#include "xtmrctr.h"
#include "xstatus.h"

#include "xil_printf.h"
#include "xllfifo.h"
#include "xllfifo_hw.h"
#include "xil_io.h"

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
#define RES_ROWS                        A_ROWS
#define RES_COLS                        B_COLS

#define MAX_A_CSV                       (A_ROWS * A_COLS * 4)
#define MAX_B_CSV                       (B_ROWS * B_COLS * 4)
#define NUM_WORDS_TX                    (A_ROWS*A_COLS + B_ROWS*B_COLS)   // 64*8 + 8 = 520
#define NUM_WORDS_RX                    (RES_ROWS * RES_COLS)

#define ASCII_MASK                      0x30

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#else
#define	XUARTPS_BASEADDRESS		        XPAR_XUARTPS_0_BASEADDR
#endif

#ifndef SDT
#define TMRCTR_DEVICE_ID                XPAR_TMRCTR_0_DEVICE_ID
#else
#define XTMRCTR_BASEADDRESS             XPAR_XTMRCTR_0_BASEADDR
#endif

static XLlFifo Fifo;

static int A[A_ROWS][A_COLS];
static int B[B_ROWS][B_COLS];
static int RES[A_ROWS][B_COLS];

static u32 TxWords[NUM_WORDS_TX];
static u32 RxWords[NUM_WORDS_RX];

static u8 ARecvBuffer[MAX_A_CSV];
static u8 BRecvBuffer[MAX_B_CSV];

/************************** Function Prototypes ******************************/

#ifndef SDT
    int UartPsInitialise(u16 DeviceId);
#else
    int UartPsInitialise(UINTPTR BaseAddress);
#endif

static int TimerInit(void);
static int FifoInit(void);

void UartReceiveData(int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]);
void parseData(u8 ARecvBuffer[MAX_A_CSV], u8 BRecvBuffer[MAX_B_CSV], 
               u32 ABytesReceived, u32 BBytesReceived,
               int A[A_ROWS][A_COLS], int B[B_ROWS][B_COLS]);

// pack A and B matrix into a transmit buffer
static void PackAB(const int A[A_ROWS][A_COLS],const int B[B_ROWS][B_COLS], u32 tx[NUM_WORDS_TX]);

// unpack receive buffer from fifo into RES matrix
static void UnpackRES(const u32 rx[NUM_WORDS_RX], int RES[RES_ROWS][RES_COLS]);

static int FifoMatrixMultiply(const u32 *tx, u32 *rx, int txWords, int rxWords);
/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */
XTmrCtr TimerCounter;   /* The instance of the Tmrctr Device */

/*****************************************************************************/

static inline u32 ticks_down_elapsed(u32 start, u32 end)
{
    return (start >= end) ? (start - end)
                          : (start + (0xFFFFFFFFu - end) + 1u);
}


static inline u32 counts_to_us(u32 counts)
{
    return (u32)(((u64)counts * 1000000ULL) / (u64)XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
}


void PrintResCsv(const int RES[A_ROWS][B_COLS])
{
    // 1-column CSV: one value per line
    for (int i = 0; i < A_ROWS; i++) {
        xil_printf("%d\r\n", RES[i][0]);
    }
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
int main(void)
{
	int Status;
    int matrix_multiply_status;
    
    #ifndef SDT
        Status = UartPsInitialise(UART_DEVICE_ID);
    #else
        Status = UartPsInitialise(XUARTPS_BASEADDRESS);
    #endif
    if (Status == XST_FAILURE) {
        xil_printf("Unable to Initialise Uart\r\n");
        return XST_FAILURE;
    }
    
    if (TimerInit() != XST_SUCCESS) return XST_FAILURE;
    // xil_printf("Finished initializing timer\r\n");
    
    if (FifoInit() != XST_SUCCESS) {
        xil_printf("FIFO init failed\r\n");
        return XST_FAILURE;
    }

    UartReceiveData(A, B);
    PackAB(A, B, TxWords);

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);

    u32 v1 = XTmrCtr_GetValue(&TimerCounter, 0);    // get timer value before multiplication
    matrix_multiply_status = FifoMatrixMultiply(TxWords, RxWords, NUM_WORDS_TX, NUM_WORDS_RX);
    u32 v2 = XTmrCtr_GetValue(&TimerCounter, 0);    // get timer value after multiplication
    XTmrCtr_Stop(&TimerCounter, 0);

    if (matrix_multiply_status != XST_SUCCESS) {
        xil_printf("FIFO loopback failed\r\n");
        return XST_FAILURE;
    }

    u32 delta = ticks_down_elapsed(v1, v2);
    xil_printf("FIFO_MATMUL_US,%lu\r\n", counts_to_us(delta));

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);
    u32 m1 = XTmrCtr_GetValue(&TimerCounter, 0);    
    UnpackRES(RxWords, RES);
    u32 m2 = XTmrCtr_GetValue(&TimerCounter, 0);    
    XTmrCtr_Stop(&TimerCounter, 0);
    u32 mticks = ticks_down_elapsed(m1, m2);
    xil_printf("UNPACK_US,%lu\r\n", counts_to_us(mticks));

    /* Print RES clean */
    xil_printf("RES_BEGIN\r\n");
    PrintResCsv(RES);
    xil_printf("RES_END\r\n");

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


static int TimerInit(void)
{
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(XPAR_XTMRCTR_0_BASEADDR);

    if (!cfg) {
        xil_printf("TMR: LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    XTmrCtr_CfgInitialize(&TimerCounter, cfg, cfg->BaseAddress);
    
    XTmrCtr_SetOptions(&TimerCounter, 0, XTC_AUTO_RELOAD_OPTION);
    
    return XST_SUCCESS;
}


static int FifoInit(void)
{

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


static void PackAB(const int A[A_ROWS][A_COLS],const int B[B_ROWS][B_COLS], u32 tx[NUM_WORDS_TX])
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


static void UnpackRES(const u32 rx[NUM_WORDS_RX], int RES[RES_ROWS][RES_COLS])
{
    int idx = 0;

    for (int i = 0; i < RES_ROWS; i++) {
        for (int k = 0; k < RES_COLS; k++) {
            RES[i][k] = (int)rx[idx++];
        }
    }
}


static int FifoMatrixMultiply(const u32 *tx, u32 *rx, int txWords, int rxWords)
{
    for (int i = 0; i < txWords; i++) {
        while (XLlFifo_iTxVacancy(&Fifo) == 0) { }
        XLlFifo_TxPutWord(&Fifo, tx[i]);
    }

    XLlFifo_iTxSetLen(&Fifo, (u32)(txWords * 4));

    // read back exactly nwords as they arrive
    int got = 0;
    while (got < rxWords) {
        u32 occ = XLlFifo_iRxOccupancy(&Fifo);
        while (occ-- && got < rxWords) {
            rx[got++] = XLlFifo_RxGetWord(&Fifo);
        }
    }
    return XST_SUCCESS;
}