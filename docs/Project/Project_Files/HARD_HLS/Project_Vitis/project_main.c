/****************************************************************************/
/**
* EE4218 Project – HARD_HDL (AXI DMA, SDT/Vitis unified build)
*
* TX stream to coprocessor (531 x 32-bit words, data in [7:0]):
*   [  0..511]  X[64][8]    – row-major; X[r][0]=0 (bias handled in HW)
*   [512..527]  W_HID[2][8] – neuron-0 first (k=0..7), then neuron-1
*   [528..530]  W_OUT[3]    – {bias_w=80, hid0_w=50, hid1_w=200}
*
* RX stream from coprocessor (64 x 32-bit words): predictions 0 or 1.
*****************************************************************************/

#include "xparameters.h"
#include "xaxidma.h"
#include "xuartps.h"
#include "xtmrctr.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Dimensions
 *--------------------------------------------------------------------------*/
#define X_ROWS      64
#define X_COLS      8
#define NUM_HID     2
#define NUM_WOUT    3

#define TX_WORDS    531
#define RX_WORDS    64
#define TX_BYTES    (TX_WORDS * 4)
#define RX_BYTES    (RX_WORDS * 4)

#define MAX_X_CSV   (X_ROWS * (X_COLS - 1) * 4)

/*---------------------------------------------------------------------------
 * Peripheral base addresses (from xparameters.h)
 *--------------------------------------------------------------------------*/
#define UART_BASE   XPAR_XUARTPS_0_BASEADDR
#define TMRCTR_BASE XPAR_XTMRCTR_0_BASEADDR
#define DMA_BASE    XPAR_XAXIDMA_0_BASEADDR

/*---------------------------------------------------------------------------
 * Hardcoded weights
 *--------------------------------------------------------------------------*/
static const u8 W_HID[NUM_HID][X_COLS] = {
    {26, 25, 31, 29, 22,  1, 11, 26},
    { 6, 18,  6, 26,  1, 28,  9, 45},
};
static const u8 W_OUT[NUM_WOUT] = {80, 50, 200};

/*---------------------------------------------------------------------------
 * Buffers – 64-byte aligned for cache operations
 *--------------------------------------------------------------------------*/
static u32 TxBuf[TX_WORDS] __attribute__((aligned(64)));
static u32 RxBuf[RX_WORDS] __attribute__((aligned(64)));
static int X[X_ROWS][X_COLS];
static u8  XRecvBuf[MAX_X_CSV];

/*---------------------------------------------------------------------------
 * Driver instances
 *--------------------------------------------------------------------------*/
static XAxiDma  Dma;
static XUartPs  Uart_Ps;
static XTmrCtr  TimerCounter;

/*---------------------------------------------------------------------------
 * Prototypes
 *--------------------------------------------------------------------------*/
static int  UartInit(UINTPTR BaseAddress);
static int  TimerInit(UINTPTR BaseAddress);
static int  DmaInit(UINTPTR BaseAddress);
static void UartReceiveX(void);
static void ParseData(u8 *buf, u32 len);
static void PackTxBuffer(void);
static int  DmaTransfer(void);
static inline u32 ticks_elapsed(u32 start, u32 end);
static inline u32 counts_to_us(u32 counts);

/*===========================================================================
 * main
 *=========================================================================*/
int main(void)
{
    if (UartInit(UART_BASE)   != XST_SUCCESS) { xil_printf("UART init failed\r\n");  return XST_FAILURE; }
    if (TimerInit(TMRCTR_BASE)!= XST_SUCCESS) { xil_printf("Timer init failed\r\n"); return XST_FAILURE; }
    if (DmaInit(DMA_BASE)     != XST_SUCCESS) { xil_printf("DMA init failed\r\n");   return XST_FAILURE; }

    UartReceiveX();
    PackTxBuffer();

    XTmrCtr_Reset(&TimerCounter, 0);
    XTmrCtr_Start(&TimerCounter, 0);
    u32 t_start = XTmrCtr_GetValue(&TimerCounter, 0);

    if (DmaTransfer() != XST_SUCCESS) {
        xil_printf("DMA transfer failed\r\n");
        return XST_FAILURE;
    }

    u32 t_end = XTmrCtr_GetValue(&TimerCounter, 0);
    XTmrCtr_Stop(&TimerCounter, 0);

    xil_printf("RES_BEGIN\r\n");
    for (int i = 0; i < RX_WORDS; i++) {
        xil_printf("%d\r\n", (int)(RxBuf[i] & 0xFF));
    }
    xil_printf("RES_END\r\n");
    xil_printf("NN_US,%lu\r\n", counts_to_us(ticks_elapsed(t_start, t_end)));

    while (1);
}

/*===========================================================================
 * PackTxBuffer
 *=========================================================================*/
static void PackTxBuffer(void)
{
    int idx = 0;
    for (int r = 0; r < X_ROWS; r++) {
        TxBuf[idx++] = 0;                           /* col 0: bias slot  */
        for (int c = 1; c < X_COLS; c++)
            TxBuf[idx++] = (u32)(X[r][c] & 0xFF);
    }
    for (int n = 0; n < NUM_HID; n++)
        for (int k = 0; k < X_COLS; k++)
            TxBuf[idx++] = (u32)W_HID[n][k];
    for (int i = 0; i < NUM_WOUT; i++)
        TxBuf[idx++] = (u32)W_OUT[i];
    /* idx == 531 */
}

/*===========================================================================
 * DmaTransfer
 *   S2MM (receive) must be armed BEFORE MM2S (send) starts.
 *=========================================================================*/
static int DmaTransfer(void)
{
    Xil_DCacheFlushRange((UINTPTR)TxBuf, TX_BYTES);
    Xil_DCacheInvalidateRange((UINTPTR)RxBuf, RX_BYTES);

    /* Arm receive channel first */
    if (XAxiDma_SimpleTransfer(&Dma, (UINTPTR)RxBuf, RX_BYTES,
                               XAXIDMA_DEVICE_TO_DMA) != XST_SUCCESS) {
        xil_printf("S2MM start failed\r\n");
        return XST_FAILURE;
    }

    /* Then start transmit */
    if (XAxiDma_SimpleTransfer(&Dma, (UINTPTR)TxBuf, TX_BYTES,
                               XAXIDMA_DMA_TO_DEVICE) != XST_SUCCESS) {
        xil_printf("MM2S start failed\r\n");
        return XST_FAILURE;
    }

    /* Poll until both channels finish */
    while (XAxiDma_Busy(&Dma, XAXIDMA_DMA_TO_DEVICE) ||
           XAxiDma_Busy(&Dma, XAXIDMA_DEVICE_TO_DMA)) { }

    return XST_SUCCESS;
}

/*===========================================================================
 * DMA init
 *=========================================================================*/
static int DmaInit(UINTPTR BaseAddress)
{
    XAxiDma_Config *cfg = XAxiDma_LookupConfig(BaseAddress);
    if (!cfg) { xil_printf("DMA lookup failed\r\n"); return XST_FAILURE; }

    if (XAxiDma_CfgInitialize(&Dma, cfg) != XST_SUCCESS) {
        xil_printf("DMA cfg init failed\r\n");
        return XST_FAILURE;
    }
    if (XAxiDma_HasSg(&Dma)) {
        xil_printf("DMA is SG mode, expected Simple\r\n");
        return XST_FAILURE;
    }
    XAxiDma_IntrDisable(&Dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrDisable(&Dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    return XST_SUCCESS;
}

/*===========================================================================
 * Timer
 *=========================================================================*/
static int TimerInit(UINTPTR BaseAddress)
{
    XTmrCtr_Config *cfg = XTmrCtr_LookupConfig(BaseAddress);
    if (!cfg) return XST_FAILURE;
    XTmrCtr_CfgInitialize(&TimerCounter, cfg, cfg->BaseAddress);
    XTmrCtr_SetOptions(&TimerCounter, 0,
                       XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);
    XTmrCtr_SetResetValue(&TimerCounter, 0, 0xFFFFFFFF);
    return XST_SUCCESS;
}

static inline u32 ticks_elapsed(u32 start, u32 end)
{
    return (start >= end) ? (start - end) : (start + (0xFFFFFFFFu - end) + 1u);
}

static inline u32 counts_to_us(u32 counts)
{
    return (u32)(((u64)counts * 1000000ULL) / (u64)XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
}

/*===========================================================================
 * UART
 *=========================================================================*/
static int UartInit(UINTPTR BaseAddress)
{
    XUartPs_Config *cfg = XUartPs_LookupConfig(BaseAddress);
    if (!cfg) return XST_FAILURE;
    if (XUartPs_CfgInitialize(&Uart_Ps, cfg, cfg->BaseAddress) != XST_SUCCESS)
        return XST_FAILURE;
    XUartPs_SetBaudRate(&Uart_Ps, 115200);
    return XST_SUCCESS;
}

static void UartReceiveX(void)
{
    bool ready = false;
    u32  bytes = 0;
    u8   rows  = 0;
    xil_printf("Ready to receive X.csv\r\n");
    while (!ready) {
        while (XUartPs_Recv(&Uart_Ps, &XRecvBuf[bytes], 1) <= 0) { }
        if (bytes >= X_COLS && XRecvBuf[bytes] == '\n')
            if (++rows >= X_ROWS) ready = true;
        bytes++;
    }
    xil_printf("Received %d bytes\r\n", bytes);
    ParseData(XRecvBuf, bytes);
}

static void ParseData(u8 *buf, u32 len)
{
    int  r = 0, c = 0, val = 0;
    bool in_num = false;
    for (u32 i = 0; i < len; i++) {
        u8 ch = buf[i];
        if (ch == '\r') continue;
        if (ch >= '0' && ch <= '9') { val = val * 10 + (ch - '0'); in_num = true; continue; }
        if (ch == ',' || ch == '\n') {
            if (in_num && r < X_ROWS && c < X_COLS) {
                if (c == 0) { X[r][0] = 256; c++; }
                X[r][c] = val; c++;
            }
            if (ch == '\n') { r++; c = 0; }
            val = 0; in_num = false;
        }
    }
    if (in_num && r < X_ROWS && c < X_COLS) X[r][c] = val;
}