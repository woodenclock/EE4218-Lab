/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xuartps.h"
#include "xtmrctr.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xaxidma.h"
#include "xil_cache.h"
#include "matrix.h"

#include <stdbool.h>
#include <stdint.h>

/************************** Constant Definitions *****************************/

/* Common data dimensions */
#define X_ROWS                          64
#define X_COLS_INPUT                    7
#define X_COLS_HLS                      8
#define WHID_ROWS                       8
#define WHID_COLS                       2
#define WOUT_ROWS                       3
#define WOUT_COLS                       1

#define LABELS_ROWS                     X_ROWS
#define LABELS_COLS                     WOUT_COLS

#define AXI_DATA_WIDTH                  32
#define DATA_WIDTH                      8
#define PACKET_SIZE                     (AXI_DATA_WIDTH / DATA_WIDTH)

/* UART receive buffers */
#define MAX_X_CSV                       (X_ROWS * X_COLS_INPUT * 4)
#define MAX_LABELS_CSV                  (LABELS_ROWS * LABELS_COLS * 4)

/* HDL format sizes */
#define HDL_X_IN_SIZE                   (X_ROWS * (X_COLS_INPUT + 1))
#define HDL_WHID_SIZE                   (WHID_ROWS * WHID_COLS)
#define HDL_WOUT_SIZE                   (WOUT_ROWS * WOUT_COLS)

#define HDL_TX_WORDS                    ((HDL_X_IN_SIZE + PACKET_SIZE - 1)/PACKET_SIZE + \
                                         (HDL_WHID_SIZE + PACKET_SIZE - 1)/PACKET_SIZE + \
                                         (HDL_WOUT_SIZE + PACKET_SIZE - 1)/PACKET_SIZE)

#define HDL_RX_WORDS                    ((RES_ROWS * RES_COLS + AXI_DATA_WIDTH - 1) / AXI_DATA_WIDTH)

/* HLS format sizes */
#define HLS_TX_WORDS                    531
#define HLS_RX_WORDS                    64

/************************** Peripheral IDs / Addresses ***********************/

#ifndef SDT
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID
#define DMA_HLS_DEVICE_ID               XPAR_AXIDMA_0_DEVICE_ID   /* DMA 0 = HLS */
#define DMA_HDL_DEVICE_ID               XPAR_AXIDMA_1_DEVICE_ID   /* DMA 1 = HDL */
#define TMRCTR_DEVICE_ID                XPAR_TMRCTR_0_DEVICE_ID
#else
#define XUARTPS_BASEADDRESS             XPAR_XUARTPS_0_BASEADDR
#define DMA_HLS_BASEADDR                XPAR_XAXIDMA_0_BASEADDR   /* DMA 0 = HLS */
#define DMA_HDL_BASEADDR                XPAR_XAXIDMA_1_BASEADDR   /* DMA 1 = HDL */
#define XTMRCTR_BASEADDRESS             XPAR_XTMRCTR_0_BASEADDR
#endif

/************************** Types ********************************************/

typedef enum {
    MODE_SOFT = 0,
    MODE_HLS  = 1,
    MODE_HDL  = 2
} PredictorMode;

/************************** Global Variables *********************************/

/* DMA instances */
static XAxiDma AxiDma0;   /* HLS */
static XAxiDma AxiDma1;   /* HDL */

/* UART / Timer */
static XUartPs Uart_Ps;
static XTmrCtr TimerCounter;

/* Input / output data */
static int X_input[X_ROWS][X_COLS_INPUT];
static int X_hls[X_ROWS][X_COLS_HLS];
static int X_soft[X_ROWS][X_COLS_INPUT + 1];
static int N[X_ROWS][WHID_COLS];
static int N_sigmoid[X_ROWS][WHID_COLS + 1];
static int LABELS[LABELS_ROWS][LABELS_COLS];
static int RES[RES_ROWS][RES_COLS];

/* Weights */
static int WHID[WHID_COLS][WHID_ROWS] = {
    {26, 25, 31, 29, 22,  1, 11, 26},
    { 6, 18,  6, 26,  1, 28,  9, 45}
};

/* 3 x 1, so it matches output_matmul() in software path */
static int WOUT[WOUT_ROWS][WOUT_COLS] = {
    {80},
    {50},
    {200}
};

/* HDL buffers */
static u32 HdlTxWords[HDL_TX_WORDS] __attribute__((aligned(64)));
static u32 HdlRxWords[HDL_RX_WORDS] __attribute__((aligned(64)));

/* HLS buffers */
static u32 HlsTxBuf[HLS_TX_WORDS] __attribute__((aligned(64)));
static u32 HlsRxBuf[HLS_RX_WORDS] __attribute__((aligned(64)));

/* UART raw receive */
static u8 XRecvBuffer[MAX_X_CSV];
static u8 LABELSRecvBuffer[MAX_LABELS_CSV];

/************************** Function Prototypes *******************************/

#ifndef SDT
int UartPsInitialise(u16 DeviceId);
#else
int UartPsInitialise(UINTPTR BaseAddress);
#endif

static int TimerInit(void);
static int DmaInit(void);

static PredictorMode GetMode(void);

void UartReceiveData(int X[X_ROWS][X_COLS_INPUT], int LABELS[LABELS_ROWS][LABELS_COLS]);
void parseData(u8 XRecvBuffer[MAX_X_CSV], u8 LABELSRecvBuffer[MAX_LABELS_CSV],
               u32 XBytesReceived, u32 LABELSBytesReceived,
               int X[X_ROWS][X_COLS_INPUT], int LABELS[LABELS_ROWS][LABELS_COLS]);

static void BuildSoftInputMatrix(void);
static void BuildHlsInputMatrix(void);

/* Software path */
static int RunSoft(u32 *delta);

/* HDL path */
static void PackHdl(int X[X_ROWS][X_COLS_INPUT],
                    int WHID[WHID_COLS][WHID_ROWS],
                    int WOUT[WOUT_ROWS][WOUT_COLS],
                    u32 tx[HDL_TX_WORDS]);
static void UnpackHdlRES(const u32 rx[HDL_RX_WORDS], int RES[RES_ROWS][RES_COLS]);
static int RunHdl(u32 *delta);

/* HLS path */
static void PackHls(void);
static void UnpackHlsRES(const u32 rx[HLS_RX_WORDS], int RES[RES_ROWS][RES_COLS]);
static int RunHls(u32 *delta);

/* Shared DMA helper */
static int DmaMatrixMultiply(XAxiDma *DmaPtr, u32 *tx, u32 *rx, int txWords, int rxWords);

static float calc_accuracy(int RES[RES_ROWS][RES_COLS], int LABELS[LABELS_ROWS][LABELS_COLS]);

static inline u32 ticks_down_elapsed(u32 start, u32 end)
{
    return (start >= end) ? (start - end) : (start + (0xFFFFFFFFu - end) + 1u);
}

static inline u32 counts_to_us(u32 counts)
{
    return (u32)(((u64)counts * 1000000ULL) / (u64)XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
}

/****************************** Main Function *********************************/

int main(void)
{
    int Status;
    int run_status = XST_SUCCESS;
    PredictorMode mode;
    u32 delta = 0;

#ifndef SDT
    Status = UartPsInitialise(UART_DEVICE_ID);
#else
    Status = UartPsInitialise(XUARTPS_BASEADDRESS);
#endif
    if (Status != XST_SUCCESS) {
        xil_printf("UART init failed\r\n");
        return XST_FAILURE;
    }

    if (TimerInit() != XST_SUCCESS) {
        xil_printf("Timer init failed\r\n");
        return XST_FAILURE;
    }

    if (DmaInit() != XST_SUCCESS) {
        xil_printf("DMA init failed\r\n");
        return XST_FAILURE;
    }

    mode = GetMode();

    UartReceiveData(X_input, LABELS);
    BuildSoftInputMatrix();
    BuildHlsInputMatrix();

    if (mode == MODE_SOFT) {
        run_status = RunSoft(&delta);
    } else if (mode == MODE_HLS) {
        run_status = RunHls(&delta);
    } else {
        run_status = RunHdl(&delta);
    }

    if (run_status != XST_SUCCESS) {
        xil_printf("Run failed: %d\r\n", run_status);
        return XST_FAILURE;
    }

    xil_printf("RES_BEGIN\r\n");
    PrintResCsv(RES);
    xil_printf("RES_END\r\n");
    xil_printf("NN_US,%lu\r\n", counts_to_us(delta));

    {
        float acc = calc_accuracy(RES, LABELS);
        int acc_int = (int)(acc * 100.0f);
        xil_printf("Accuracy: %d.%02d%%\r\n", acc_int / 100, acc_int % 100);
    }

    while (1);
}

/************************** Init Functions ***********************************/

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

    if (Config == NULL) {
        return XST_FAILURE;
    }

    Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XUartPs_SetBaudRate(&Uart_Ps, 115200);
    return XST_SUCCESS;
}

static int TimerInit(void)
{
#ifndef SDT
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(TMRCTR_DEVICE_ID);
#else
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(XTMRCTR_BASEADDRESS);
#endif

    if (!cfg) {
        xil_printf("Timer lookup failed\r\n");
        return XST_FAILURE;
    }

    XTmrCtr_CfgInitialize(&TimerCounter, cfg, cfg->BaseAddress);
    XTmrCtr_SetOptions(&TimerCounter, 0, XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);
    XTmrCtr_SetResetValue(&TimerCounter, 0, 0xFFFFFFFF);

    return XST_SUCCESS;
}

static int DmaInit(void)
{
    int Status;
    XAxiDma_Config *CfgHLS;
    XAxiDma_Config *CfgHDL;

#ifndef SDT
    CfgHLS = XAxiDma_LookupConfig(DMA_HLS_DEVICE_ID);
    CfgHDL = XAxiDma_LookupConfig(DMA_HDL_DEVICE_ID);
#else
    CfgHLS = XAxiDma_LookupConfig(DMA_HLS_BASEADDR);
    CfgHDL = XAxiDma_LookupConfig(DMA_HDL_BASEADDR);
#endif

    if (!CfgHLS || !CfgHDL) {
        xil_printf("DMA lookup failed\r\n");
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma0, CfgHLS);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA HLS init failed\r\n");
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma1, CfgHDL);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA HDL init failed\r\n");
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(&AxiDma0) || XAxiDma_HasSg(&AxiDma1)) {
        xil_printf("DMA is in SG mode; expected Simple mode\r\n");
        return XST_FAILURE;
    }

    XAxiDma_Reset(&AxiDma0);
    while (!XAxiDma_ResetIsDone(&AxiDma0)) {}

    XAxiDma_Reset(&AxiDma1);
    while (!XAxiDma_ResetIsDone(&AxiDma1)) {}

    XAxiDma_IntrDisable(&AxiDma0, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrDisable(&AxiDma0, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    return XST_SUCCESS;
}

/************************** Mode Select **************************************/

static PredictorMode GetMode(void)
{
    char ch;

    xil_printf("\r\nSelect predictor:\r\n");
    xil_printf("0 = Software\r\n");
    xil_printf("1 = HLS\r\n");
    xil_printf("2 = HDL\r\n");
    xil_printf("Choice: ");

    ch = inbyte();
    xil_printf("%c\r\n", ch);

    if (ch == '0') {
        xil_printf("Running software computation\r\n");
        return MODE_SOFT;
    }
    if (ch == '1') {
        xil_printf("Running HLS processor\r\n");
        return MODE_HLS;
    }
    if (ch == '2') {
        xil_printf("Running HDL processor\r\n");
        return MODE_HDL;
    }

    xil_printf("Invalid choice, defaulting to software\r\n");
    return MODE_SOFT;
}

/************************** UART Receive / Parse ******************************/

void UartReceiveData(int X[X_ROWS][X_COLS_INPUT], int LABELS[LABELS_ROWS][LABELS_COLS])
{
    bool matrixXReady = false;
    bool matrixLABELSReady = false;
    u32 XBytesReceived = 0;
    u32 LABELSBytesReceived = 0;
    u8 numRowsReceived = 0;

    xil_printf("Ready to receive X.csv\r\n");
    while (!matrixXReady) {
        while ((XUartPs_Recv(&Uart_Ps, &XRecvBuffer[XBytesReceived], 1)) <= 0) {}
        if (XBytesReceived >= X_COLS_INPUT && XRecvBuffer[XBytesReceived] == '\n') {
            numRowsReceived++;
            if (numRowsReceived >= X_ROWS) {
                matrixXReady = true;
            }
        }
        XBytesReceived++;
    }
    xil_printf("Received %d bytes for X.csv\r\n", XBytesReceived);

    xil_printf("Ready to receive labels.csv\r\n");
    numRowsReceived = 0;
    while (!matrixLABELSReady) {
        while ((XUartPs_Recv(&Uart_Ps, &LABELSRecvBuffer[LABELSBytesReceived], 1)) <= 0) {}
        if (LABELSBytesReceived >= LABELS_COLS && LABELSRecvBuffer[LABELSBytesReceived] == '\n') {
            numRowsReceived++;
            if (numRowsReceived >= LABELS_ROWS) {
                matrixLABELSReady = true;
            }
        }
        LABELSBytesReceived++;
    }
    xil_printf("Received %d bytes for labels.csv\r\n", LABELSBytesReceived);

    parseData(XRecvBuffer, LABELSRecvBuffer,
              XBytesReceived, LABELSBytesReceived,
              X, LABELS);
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
                if (ch == '\n') {
                    r++;
                    c = 0;
                }
            }
            val = 0;
            in_num = false;
            continue;
        }
    }

    if (in_num && r < rows && c < cols) {
        out[r][c] = val;
    }
}

void parseData(u8 XRecvBuffer[MAX_X_CSV], u8 LABELSRecvBuffer[MAX_LABELS_CSV],
               u32 XBytesReceived, u32 LABELSBytesReceived,
               int X[X_ROWS][X_COLS_INPUT], int LABELS[LABELS_ROWS][LABELS_COLS])
{
    parseOneCsvMatrix(XRecvBuffer, XBytesReceived, X_ROWS, X_COLS_INPUT, X);
    parseOneCsvMatrix(LABELSRecvBuffer, LABELSBytesReceived, LABELS_ROWS, LABELS_COLS, LABELS);
}

/************************** Input Builders ***********************************/

static void BuildSoftInputMatrix(void)
{
    int r, c;
    for (r = 0; r < X_ROWS; r++) {
        X_soft[r][0] = 256;
        for (c = 0; c < X_COLS_INPUT; c++) {
            X_soft[r][c + 1] = X_input[r][c];
        }
    }
}

static void BuildHlsInputMatrix(void)
{
    int r, c;
    for (r = 0; r < X_ROWS; r++) {
        X_hls[r][0] = 0;
        for (c = 0; c < X_COLS_INPUT; c++) {
            X_hls[r][c + 1] = X_input[r][c];
        }
    }
}

/************************** Software Path ************************************/

static int RunSoft(u32 *delta)
{
    u32 t1, t2;

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);

    t1 = XTmrCtr_GetValue(&TimerCounter, 0);
    MatMulDiv256(X_soft, WHID, N);
    sigmoid(N, N_sigmoid);
    output_matmul(N_sigmoid, WOUT, RES);
    convertRES(RES);
    t2 = XTmrCtr_GetValue(&TimerCounter, 0);

    XTmrCtr_Stop(&TimerCounter, 0);
    *delta = ticks_down_elapsed(t1, t2);

    return XST_SUCCESS;
}

/*****************************************************************************/
/************************** HDL Path *****************************************/
/*****************************************************************************/

static void PackHdl(int X[X_ROWS][X_COLS_INPUT],
                    int WHID[WHID_COLS][WHID_ROWS],
                    int WOUT[WOUT_ROWS][WOUT_COLS],
                    u32 tx[HDL_TX_WORDS])
{
    int tx_idx = 0;
    int i, neuron;

    for (i = 0; i < X_ROWS; i++) {
        u32 word1 = 0;
        u32 word2 = 0;

        word1 |= ((u32)X[i][2] << 24);
        word1 |= ((u32)X[i][1] << 16);
        word1 |= ((u32)X[i][0] << 8);
        word1 |= ((u32)0xFF);
        tx[tx_idx++] = word1;

        word2 |= ((u32)X[i][6] << 24);
        word2 |= ((u32)X[i][5] << 16);
        word2 |= ((u32)X[i][4] << 8);
        word2 |= ((u32)X[i][3]);
        tx[tx_idx++] = word2;
    }

    for (neuron = 0; neuron < WHID_COLS; neuron++) {
        u32 word1 = 0;
        u32 word2 = 0;

        word1 |= ((u32)WHID[neuron][3] << 24);
        word1 |= ((u32)WHID[neuron][2] << 16);
        word1 |= ((u32)WHID[neuron][1] << 8);
        word1 |= ((u32)WHID[neuron][0]);
        tx[tx_idx++] = word1;

        word2 |= ((u32)WHID[neuron][7] << 24);
        word2 |= ((u32)WHID[neuron][6] << 16);
        word2 |= ((u32)WHID[neuron][5] << 8);
        word2 |= ((u32)WHID[neuron][4]);
        tx[tx_idx++] = word2;
    }

    {
        u32 wout_word = 0;
        wout_word |= ((u32)WOUT[2][0] << 16);
        wout_word |= ((u32)WOUT[1][0] << 8);
        wout_word |= ((u32)WOUT[0][0]);
        tx[tx_idx++] = wout_word;
    }
}

static void UnpackHdlRES(const u32 rx[HDL_RX_WORDS], int RES[RES_ROWS][RES_COLS])
{
    int i, j;
    for (i = 0; i < HDL_RX_WORDS; i++) {
        u32 recv = rx[i];
        u32 bit_mask = 0x01;

        for (j = 0; j < AXI_DATA_WIDTH; j++) {
            int idx = i * AXI_DATA_WIDTH + j;
            int row = idx / RES_COLS;
            int col = idx % RES_COLS;

            if (row < RES_ROWS) {
                RES[row][col] = (recv & bit_mask) ? 1 : 0;
            }
            bit_mask <<= 1;
        }
    }
}

static int RunHdl(u32 *delta)
{
    int Status;
    u32 t1, t2;

    PackHdl(X_input, WHID, WOUT, HdlTxWords);

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);

    t1 = XTmrCtr_GetValue(&TimerCounter, 0);
    Status = DmaMatrixMultiply(&AxiDma1, HdlTxWords, HdlRxWords, HDL_TX_WORDS, HDL_RX_WORDS);
    t2 = XTmrCtr_GetValue(&TimerCounter, 0);

    XTmrCtr_Stop(&TimerCounter, 0);
    *delta = ticks_down_elapsed(t1, t2);

    if (Status != XST_SUCCESS) {
        xil_printf("HDL DMA failed: %d\r\n", Status);
        return Status;
    }

    UnpackHdlRES(HdlRxWords, RES);
    return XST_SUCCESS;
}

/*****************************************************************************/
/************************** HLS Path *****************************************/
/*****************************************************************************/

static void PackHls(void)
{
    int idx = 0;
    int r, c, n, k;

    for (r = 0; r < X_ROWS; r++) {
        for (c = 0; c < X_COLS_HLS; c++) {
            HlsTxBuf[idx++] = (u32)(X_hls[r][c] & 0xFF);
        }
    }

    for (n = 0; n < WHID_COLS; n++) {
        for (k = 0; k < WHID_ROWS; k++) {
            HlsTxBuf[idx++] = (u32)(WHID[n][k] & 0xFF);
        }
    }

    for (k = 0; k < WOUT_ROWS; k++) {
        HlsTxBuf[idx++] = (u32)(WOUT[k][0] & 0xFF);
    }
}

static void UnpackHlsRES(const u32 rx[HLS_RX_WORDS], int RES[RES_ROWS][RES_COLS])
{
    int i;
    for (i = 0; i < HLS_RX_WORDS; i++) {
        RES[i][0] = (int)(rx[i] & 0x1);
    }
}

static int RunHls(u32 *delta)
{
    int Status;
    u32 t1, t2;
    PackHls();

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);

    t1 = XTmrCtr_GetValue(&TimerCounter, 0);
    Status = DmaMatrixMultiply(&AxiDma0, HlsTxBuf, HlsRxBuf, HLS_TX_WORDS, HLS_RX_WORDS);
    t2 = XTmrCtr_GetValue(&TimerCounter, 0);

    XTmrCtr_Stop(&TimerCounter, 0);
    *delta = ticks_down_elapsed(t1, t2);

    if (Status != XST_SUCCESS) {
        xil_printf("HLS DMA failed: %d\r\n", Status);
        return Status;
    }

    UnpackHlsRES(HlsRxBuf, RES);
    return XST_SUCCESS;
}

/*****************************************************************************/
/************************** Shared DMA Helper ********************************/
/*****************************************************************************/

static int DmaMatrixMultiply(XAxiDma *DmaPtr, u32 *tx, u32 *rx, int txWords, int rxWords)
{
    int Status;
    const u32 TxBytes = (u32)(txWords * 4);
    const u32 RxBytes = (u32)(rxWords * 4);

    Xil_DCacheFlushRange((UINTPTR)tx, TxBytes);
    Xil_DCacheInvalidateRange((UINTPTR)rx, RxBytes);

    Status = XAxiDma_SimpleTransfer(DmaPtr, (UINTPTR)rx, RxBytes, XAXIDMA_DEVICE_TO_DMA);
    if (Status != XST_SUCCESS) return Status;

    Status = XAxiDma_SimpleTransfer(DmaPtr, (UINTPTR)tx, TxBytes, XAXIDMA_DMA_TO_DEVICE);
    if (Status != XST_SUCCESS) return Status;

    while (XAxiDma_Busy(DmaPtr, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(DmaPtr, XAXIDMA_DEVICE_TO_DMA)) {}

    Xil_DCacheInvalidateRange((UINTPTR)rx, RxBytes);

    return XST_SUCCESS;
}

/*****************************************************************************/
/************************** Output Accuracy **********************************/
/*****************************************************************************/

static float calc_accuracy(int RES[RES_ROWS][RES_COLS], int LABELS[LABELS_ROWS][LABELS_COLS])
{
    int i, j;
    int correct = 0;

    for (i = 0; i < RES_ROWS; i++) {
        for (j = 0; j < RES_COLS; j++) {
            if (RES[i][j] == LABELS[i][j]) {
                correct++;
            }
        }
    }

    return ((float)correct / (float)(RES_ROWS * RES_COLS) * 100.0f);
}