#include "matrix.h"
#include "xil_printf.h"
#include <stdint.h>

void MatMulDiv256(const int A[A_ROWS][A_COLS],
                  const int B[B_ROWS][B_COLS],
                  int RES[A_ROWS][B_COLS])
{
    for (int i = 0; i < A_ROWS; i++) {
        int32_t acc = 0; // safe accumulator
        for (int k = 0; k < A_COLS; k++) {
            acc += (int32_t)A[i][k] * (int32_t)B[k][0];
        }
        // divide by 256 (integer-only)
        RES[i][0] = (int)(acc >> 8);
    }
}

void PrintResCsv(const int RES[A_ROWS][B_COLS])
{
    // 1-column CSV: one value per line
    for (int i = 0; i < A_ROWS; i++) {
        xil_printf("%d\r\n", RES[i][0]);
    }
}
