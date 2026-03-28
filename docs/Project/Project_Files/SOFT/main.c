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
#include "matrix.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define X_ROWS                          64
#define X_COLS                          8
#define NUM_HID_NEURONS                 2
#define W_HID_ROWS                      X_COLS //64
#define W_HID_COLS                      NUM_HID_NEURONS //2
#define W_OUT_ROWS                      (W_HID_COLS + 1) //3
#define W_OUT_COLS                      1
#define RES_ROWS                        X_ROWS
#define RES_COLS                        W_OUT_COLS


#define MAX_X_CSV                       (X_ROWS * (X_COLS - 1) * 4)
#define NUM_WORDS                       (X_ROWS * X_COLS)

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

static int X[X_ROWS][X_COLS];
static int W_HID[W_HID_COLS][W_HID_ROWS] = {{26, 25, 31, 29, 22, 1, 11, 26}, {6, 18, 6, 26, 1, 28, 9, 45}};
static int W_OUT[W_OUT_COLS][W_OUT_ROWS] = {{80, 50, 200}};
static int N[X_ROWS][W_HID_COLS];
static int N_sigmoid[X_ROWS][W_HID_COLS + 1];
static int RES[RES_ROWS][RES_COLS];

static u32 TxWords[NUM_WORDS];
static u32 RxWords[NUM_WORDS];

static u8 XRecvBuffer[MAX_X_CSV];

/************************** Function Prototypes ******************************/

#ifndef SDT
    int UartPsInitialise(u16 DeviceId);
#else
    int UartPsInitialise(UINTPTR BaseAddress);
#endif

void UartReceiveData(int X[X_ROWS][X_COLS]);
void parseData(u8 XRecvBuffer[MAX_X_CSV],
               u32 XBytesReceived,
               int X[X_ROWS][X_COLS]);

/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */
XTmrCtr TimerCounter;   /* The instance of the Tmrctr Device */

/*****************************************************************************/

static int TimerInit(void)
{
#ifndef SDT
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(TMRCTR_DEVICE_ID);
#else
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(XTMRCTR_BASEADDRESS);
#endif

    if (!cfg) {
        xil_printf("TMR: LookupConfig failed\r\n");
        return XST_FAILURE;
    }


    XTmrCtr_CfgInitialize(&TimerCounter, cfg, cfg->BaseAddress);

    XTmrCtr_SetOptions(&TimerCounter, 0, XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);
    XTmrCtr_SetResetValue(&TimerCounter, 0, 0xFFFFFFFF);

    return XST_SUCCESS;
}


static inline u32 ticks_down_elapsed(u32 start, u32 end)
{
    return (start >= end) ? (start - end)
                          : (start + (0xFFFFFFFFu - end) + 1u);
}


static inline u32 counts_to_us(u32 counts)
{
    return (u32)(((u64)counts * 1000000ULL) / (u64)XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
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
    int loopback_status;
    
    #ifndef SDT
        Status = UartPsInitialise(UART_DEVICE_ID);
    #else
        Status = UartPsInitialise(XUARTPS_BASEADDRESS);
    #endif
    if (Status == XST_FAILURE) {
        xil_printf("Unable to Initialise Uart\r\n");
        return XST_FAILURE;
    }
    
    // Initialize Timer
    if (TimerInit() != XST_SUCCESS) return XST_FAILURE;
    // xil_printf("Finished initializing timer\r\n");

    UartReceiveData(X);
    
    /* FIFO timing */
    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);

    // Overwrite A,B with what came back from FIFO
    u32 m1 = XTmrCtr_GetValue(&TimerCounter, 0);
    MatMulDiv256(X, W_HID, N);
    sigmoid(N, N_sigmoid);
    output_matmul(N_sigmoid, W_OUT, RES);
    convertRES(RES);
    u32 m2 = XTmrCtr_GetValue(&TimerCounter, 0);

    XTmrCtr_Stop(&TimerCounter, 0);

    u32 mticks = ticks_down_elapsed(m1, m2);
    xil_printf("NN_US,%lu\r\n", counts_to_us(mticks));


    // /* Print timing (separate from RES) */
    // xil_printf("TIME_BEGIN\r\n");
    // xil_printf("FIFO_US,%lu\r\n", delta);
    // xil_printf("MATMUL_US,%lu\r\n", mticks);
    // xil_printf("TIME_END\r\n");

    /* Print RES clean */
    xil_printf("RES_BEGIN\r\n");
    PrintResCsv(RES);
    xil_printf("RES_END\r\n");
    xil_printf("NN_US,%lu\r\n", counts_to_us(mticks));

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


void UartReceiveData(int X[X_ROWS][X_COLS]) 
{
    bool matrixXReady = false;
    u32 XBytesReceived = 0;
    u8 numRowsReceived = 0;
    xil_printf("Ready to receive X.csv\r\n");
    while (!matrixXReady) {
        while ((XUartPs_Recv(&Uart_Ps, &XRecvBuffer[XBytesReceived], 1)) <= 0) {
        }
        if(XBytesReceived >= X_COLS &&
            XRecvBuffer[XBytesReceived] == '\n') {
            numRowsReceived++;
            if(numRowsReceived >= X_ROWS) {
                matrixXReady = true;
            }
        }
        XBytesReceived++;
    }
    xil_printf("Received %d bytes for X.csv\r\n", XBytesReceived);
    parseData(XRecvBuffer, XBytesReceived, X);
}


void parseData(u8 XRecvBuffer[MAX_X_CSV], 
               u32 XBytesReceived,
               int X[X_ROWS][X_COLS])
{
    int r = 0, c = 0;
    int val = 0;
    bool in_num = false;

    for (u32 i = 0; i < XBytesReceived; i++) {
        u8 ch = XRecvBuffer[i];
        if (ch == '\r') continue;

        if (ch >= '0' && ch <= '9') {
            val = val * 10 + (ch - '0');
            in_num = true;
            continue;
        }

        if (ch == ',' || ch == '\n') {
            if (in_num) {
                if (r < X_ROWS && c < X_COLS) {
                    if (c == 0) {
                        X[r][c] = 256; // add bias term for new row
                        c++;
                    }
                    X[r][c] = val;
                }
                c++;
                if (ch == '\n') { r++; c = 0; }
            }
            val = 0;
            in_num = false;
            continue;
        }
    }

    // Flush
    if (in_num && r < X_ROWS && c < X_COLS) {
        X[r][c] = val;
    }
}