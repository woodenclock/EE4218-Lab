/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xuartps.h"
#include "xtmrctr.h"
#include "xstatus.h"

#include "xil_printf.h"
// #include "xllfifo.h"
// #include "xllfifo_hw.h"
#include "xil_io.h"

#include "xaxidma.h"
#include "xil_cache.h"

#include <stdbool.h>

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define X_ROWS                          64
#define X_COLS                          7
#define WHID_ROWS                       8
#define WHID_COLS                       2
#define WOUT_ROWS                       3
#define WOUT_COLS                       1

#define X_IN_SIZE                       (X_ROWS * (X_COLS + 1))
#define WHID_SIZE                       (WHID_ROWS * WHID_COLS)
#define WOUT_SIZE                       (WOUT_ROWS * WOUT_COLS)

#define RES_ROWS                        X_ROWS
#define RES_COLS                        WOUT_COLS
#define LABELS_ROWS                     RES_ROWS
#define LABELS_COLS                     RES_COLS

#define AXI_DATA_WIDTH                  32
#define DATA_WIDTH                      8
#define PACKET_SIZE                     (AXI_DATA_WIDTH / DATA_WIDTH)

#define MAX_X_CSV                       (X_ROWS * X_COLS * 4)
#define MAX_LABELS_CSV                  (LABELS_ROWS * LABELS_COLS)

#define NUM_WORDS_TX                    ((X_IN_SIZE + PACKET_SIZE - 1)/PACKET_SIZE + (WHID_SIZE + PACKET_SIZE - 1)/PACKET_SIZE + (WOUT_SIZE + PACKET_SIZE - 1)/PACKET_SIZE) 
#define NUM_WORDS_RX                    ((RES_ROWS * RES_COLS + AXI_DATA_WIDTH - 1) / AXI_DATA_WIDTH)

#define ASCII_MASK                      0x30

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#else
#define	XUARTPS_BASEADDRESS		        XPAR_XUARTPS_0_BASEADDR
#endif

#ifndef SDT
#define DMA_DEVICE_ID                   XPAR_AXIDMA_0_DEVICE_ID
#else
#define DMA_BASEADDR                    XPAR_XAXIDMA_0_BASEADDR
#endif

#ifndef SDT
#define TMRCTR_DEVICE_ID                XPAR_TMRCTR_0_DEVICE_ID
#else
#define XTMRCTR_BASEADDRESS             XPAR_XTMRCTR_0_BASEADDR
#endif

// static XLlFifo Fifo;
static XAxiDma AxiDma;

static int X[X_ROWS][X_COLS];
static int LABELS[LABELS_ROWS][LABELS_COLS];
static int RES[RES_ROWS][RES_COLS];

static int WHID[WHID_COLS][WHID_ROWS] = {{26, 25, 31, 29, 22, 1, 11, 26}, {6, 18, 6, 26, 1, 28, 9, 45}};
static int WOUT[WOUT_COLS][WOUT_ROWS] = {{80, 50, 200}};

static u32 TxWords[NUM_WORDS_TX];
static u32 RxWords[NUM_WORDS_RX];

static u8 XRecvBuffer[MAX_X_CSV];
static u8 LABELSRecvBuffer[MAX_LABELS_CSV];

/************************** Function Prototypes ******************************/

#ifndef SDT
    int UartPsInitialise(u16 DeviceId);
#else
    int UartPsInitialise(UINTPTR BaseAddress);
#endif

// static int FifoInit(void);
static int DmaInit(void);
static int TimerInit(void);

void UartReceiveData(int X[X_ROWS][X_COLS], int LABELS[LABELS_ROWS][LABELS_COLS]);
void parseData(u8 XRecvBuffer[MAX_X_CSV], u8 LABELSRecvBuffer[MAX_LABELS_CSV], 
                u32 XBytesReceived, u32 LABELSBytesReceived, int X[X_ROWS][X_COLS], 
                int LABELS[LABELS_ROWS][LABELS_COLS]);

// pack X and B matrix into a transmit buffer
static void Pack(int X[X_ROWS][X_COLS], int WHID[WHID_COLS][WHID_ROWS], int WOUT[WOUT_COLS][WOUT_ROWS], u32 tx[NUM_WORDS_TX]);

// unpack receive buffer from fifo into RES matrix
static void UnpackRES(const u32 rx[NUM_WORDS_RX], int RES[RES_ROWS][RES_COLS]);

// static int FifoMatrixMultiply(const u32 *tx, u32 *rx, int txWords, int rxWords);
static int DmaMatrixMultiply(u32 *tx, u32 *rx, int txWords, int rxWords);

static inline u32 ticks_down_elapsed(u32 start, u32 end) { return (start >= end) ? (start - end) : (start + (0xFFFFFFFFu - end) + 1u); }

static inline u32 counts_to_us(u32 counts) { return (u32)(((u64)counts * 1000000ULL) / (u64)XPAR_XTMRCTR_0_CLOCK_FREQUENCY); }

void PrintResCsv(const int RES[RES_ROWS][RES_COLS]);

float calc_accuracy(int RES[RES_ROWS][RES_COLS], int LABELS[RES_ROWS][RES_COLS]);

/************************** Variable Definitions *****************************/

XUartPs Uart_Ps;		/* The instance of the UART Driver */
XTmrCtr TimerCounter;   /* The instance of the Tmrctr Device */

/*****************************************************************************/


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/****************************** Main Function *********************************/

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
        xil_printf("Uart init failed\r\n");
        return XST_FAILURE;
    }
    
    if (TimerInit() != XST_SUCCESS) {
        xil_printf("Uart init failed\r\n");
        return XST_FAILURE;
    }
    

    if (DmaInit() != XST_SUCCESS) {
        xil_printf("DMA init failed\r\n");
        return XST_FAILURE;
    }

    UartReceiveData(X, LABELS);
    Pack(X, WHID, WOUT, TxWords);

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);
    u32 v1 = XTmrCtr_GetValue(&TimerCounter, 0);    // get timer value before multiplication
    matrix_multiply_status = DmaMatrixMultiply(TxWords, RxWords, NUM_WORDS_TX, NUM_WORDS_RX);
    u32 v2 = XTmrCtr_GetValue(&TimerCounter, 0);    // get timer value after multiplication
    XTmrCtr_Stop(&TimerCounter, 0);

    if (matrix_multiply_status != XST_SUCCESS) {
        xil_printf("DMA matrix multiply failed: %d\r\n", matrix_multiply_status);
        return XST_FAILURE;
    }

    u32 delta = ticks_down_elapsed(v1, v2);

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);   
    u32 m1 = XTmrCtr_GetValue(&TimerCounter, 0); 
    UnpackRES(RxWords, RES);
    u32 m2 = XTmrCtr_GetValue(&TimerCounter, 0);    
    XTmrCtr_Stop(&TimerCounter, 0);
    u32 mticks = ticks_down_elapsed(m1, m2);

    /* Print RES clean */
    xil_printf("RES_BEGIN\r\n");
    PrintResCsv(RES);
    xil_printf("RES_END\r\n");
    xil_printf("NN_US,%lu\r\n", counts_to_us(delta));

    float acc = calc_accuracy(RES, LABELS);
    int acc_int = (int)(acc * 100);   // percentage

    xil_printf("Accuracy: %d.%02d%%\r\n", acc_int / 100, acc_int % 100);
    while (1);
}
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/************************** Function Definitions *****************************/

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


void UartReceiveData(int X[X_ROWS][X_COLS], int LABELS[LABELS_ROWS][LABELS_COLS]) 
{
    bool matrixXReady = false;
    bool matrixLABELSReady = false;
    u32 XBytesReceived = 0;
    u8 numRowsReceived = 0;
    xil_printf("Ready to receive X.csv\r\n");
    while (!matrixXReady) {
        while ((XUartPs_Recv(&Uart_Ps, &XRecvBuffer[XBytesReceived], 1)) <= 0) {
        }
        if (XBytesReceived >= X_COLS &&
            XRecvBuffer[XBytesReceived] == '\n') {
            numRowsReceived++;
            if (numRowsReceived >= X_ROWS) {
                matrixXReady = true;
            }
        }
        XBytesReceived++;
    }
    xil_printf("Received %d bytes for X.csv\r\n", XBytesReceived);
    xil_printf("Ready to receive labels.csv\r\n");
    u32 LABELSBytesReceived = 0;
    numRowsReceived = 0;
    while (!matrixLABELSReady) {
        while ((XUartPs_Recv(&Uart_Ps, &LABELSRecvBuffer[LABELSBytesReceived], 1)) <= 0) {
        }
        if (LABELSBytesReceived >= LABELS_COLS &&
            LABELSRecvBuffer[LABELSBytesReceived] == '\n') {
            numRowsReceived++;
            if (numRowsReceived >= LABELS_ROWS) {
                matrixLABELSReady = true;
            }
        }
        LABELSBytesReceived++;
    }
    xil_printf("Received %d bytes for labels.csv\r\n", LABELSBytesReceived);
    parseData(XRecvBuffer, LABELSRecvBuffer, XBytesReceived, LABELSBytesReceived, X, LABELS);
}


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

    xil_printf("Timer initialized. Clock: %lu Hz\r\n", 
               (unsigned long)XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
    
    return XST_SUCCESS;
}


static int DmaInit(void)
{
    int Status;

#ifndef SDT
    XAxiDma_Config *Cfg = XAxiDma_LookupConfig(DMA_DEVICE_ID);
#else
    XAxiDma_Config *Cfg = XAxiDma_LookupConfig(DMA_BASEADDR);
#endif
    if (!Cfg) {
        xil_printf("DMA lookup failed\r\n");
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma, Cfg);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA init failed: %d\r\n", Status);
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(&AxiDma)) {
        xil_printf("DMA is in SG mode; expected Simple mode\r\n");
        return XST_FAILURE;
    }

    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma)) {}

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


void parseData(u8 XRecvBuffer[MAX_X_CSV], u8 LABELSRecvBuffer[MAX_LABELS_CSV], 
               u32 XBytesReceived, u32 LABELSBytesReceived, 
               int X[X_ROWS][X_COLS], int LABELS[LABELS_ROWS][LABELS_COLS])
{
    parseOneCsvMatrix(XRecvBuffer, XBytesReceived, X_ROWS, X_COLS, X);
    parseOneCsvMatrix(LABELSRecvBuffer, LABELSBytesReceived, LABELS_ROWS, LABELS_COLS, LABELS);
}


static void Pack(int X[X_ROWS][X_COLS], int WHID[WHID_COLS][WHID_ROWS], int WOUT[WOUT_COLS][WOUT_ROWS], u32 tx[NUM_WORDS_TX])
{
    int tx_idx = 0;

    int row = 0;
    for (int i = 0; i < X_ROWS; i++) {
        u32 word1 = 0;
        word1 |= ((u32)X[i][2] << 24);
        word1 |= ((u32)X[i][1] << 16);
        word1 |= ((u32)X[i][0] << 8);
        word1 |= ((u32)0xFF);   // bias

        tx[tx_idx++] = word1;

        u32 word2 = 0;
        word2 |= ((u32)X[i][6] << 24);
        word2 |= ((u32)X[i][5] << 16);
        word2 |= ((u32)X[i][4] << 8);
        word2 |= ((u32)X[i][3]);

        tx[tx_idx++] = word2;
    }

    // WHID
    for (int neuron = 0; neuron < WHID_COLS; neuron++) {

        // First 4 weights
        u32 word1 = 0;
        word1 |= ((u32)WHID[neuron][3] << 24);
        word1 |= ((u32)WHID[neuron][2] << 16);
        word1 |= ((u32)WHID[neuron][1] << 8);
        word1 |= ((u32)WHID[neuron][0]);

        tx[tx_idx++] = word1;

        // Next 4 weights
        u32 word2 = 0;
        word2 |= ((u32)WHID[neuron][7] << 24);
        word2 |= ((u32)WHID[neuron][6] << 16);
        word2 |= ((u32)WHID[neuron][5] << 8);
        word2 |= ((u32)WHID[neuron][4]);

        tx[tx_idx++] = word2;
    }

    // WOUT
    u32 wout_word = 0;
    wout_word |= ((u32)WOUT[0][2] << 16);
    wout_word |= ((u32)WOUT[0][1] << 8);
    wout_word |= ((u32)WOUT[0][0]);
    tx[tx_idx++] = wout_word;

    // // checks - comment out ltr
    // for (int i = 0; i < tx_idx; i++) {
    //     xil_printf("%08X\r\n", tx[i]);
    // }
}


static void UnpackRES(const u32 rx[NUM_WORDS_RX], int RES[RES_ROWS][RES_COLS])
{
    for (int i = 0; i < NUM_WORDS_RX; i++) {
        u32 recv = rx[i];
        xil_printf("%08X\r\n", recv);
        u32 bit_mask = 0x01;
        for (int j =0; j < AXI_DATA_WIDTH; j++) {
            int idx = i * AXI_DATA_WIDTH + j;
            int row = idx / RES_COLS;
            int col = idx % RES_COLS;
            if (recv & bit_mask) {
                RES[row][col] = 1;
            } else {
                RES[row][col] = 0;
            }
            bit_mask = bit_mask << 1;
        }
        bit_mask = 0x01;
    }
}


static int DmaMatrixMultiply(u32 *tx, u32 *rx, int txWords, int rxWords)
{
    int Status;
    const u32 TxBytes = (u32)(txWords * 4);
    const u32 RxBytes = (u32)(rxWords * 4);

    // 1) Cache maintenance (CRITICAL)
    Xil_DCacheFlushRange((UINTPTR)tx, TxBytes);
    Xil_DCacheInvalidateRange((UINTPTR)rx, RxBytes);

    // 2) Start S2MM first (so DMA is ready to receive output)
    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx, RxBytes, XAXIDMA_DEVICE_TO_DMA);
    if (Status != XST_SUCCESS) return Status;

    // 3) Then start MM2S to send input
    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)tx, TxBytes, XAXIDMA_DMA_TO_DEVICE);
    if (Status != XST_SUCCESS) return Status;

    // 4) Poll until both complete
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) {}

    // 5) Invalidate rx so CPU reads fresh data from DDR
    Xil_DCacheInvalidateRange((UINTPTR)rx, RxBytes);

    return XST_SUCCESS;
}

void PrintResCsv(const int RES[RES_ROWS][RES_COLS])
{
    // 1-column CSV: one value per line
    for (int i = 0; i < RES_ROWS; i++) {
        for (int j = 0; j < RES_COLS; j++) {
            xil_printf("%d\r\n", RES[i][j]);
        }
    }
}

float calc_accuracy(int RES[RES_ROWS][RES_COLS], int LABELS[RES_ROWS][RES_COLS])
{
    int correct = 0;
    for (int i = 0; i < RES_ROWS; i++) {
        for (int j = 0; j < RES_COLS; j++) {
            if (RES[i][j] == LABELS [i][j]) {
                correct++;
            }
        }
    }
    return ((float) correct / (float) (RES_ROWS * RES_COLS) * 100);
}
