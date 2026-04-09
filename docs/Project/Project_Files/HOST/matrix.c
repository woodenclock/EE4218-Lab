#include "matrix.h"
#include "sigmoid.h"
#include "xil_printf.h"
#include <stdint.h>

void MatMulDiv256(const int A[A_ROWS][A_COLS],
                  const int B_T[B_COLS][A_COLS],
                  int RES[A_ROWS][B_COLS])
{
    for (int i = 0; i < A_ROWS; i++) {
        for (int j = 0; j < B_COLS; j++) {
            int32_t acc = 0;
            for (int k = 0; k < A_COLS; k++) {
                acc += (int32_t)A[i][k] * (int32_t)B_T[j][k]; // B_T[j][k] == B[k][j]
            }
            RES[i][j] = (int)(acc >> 8); // divide by 256
        }
    }
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

void sigmoid(int N[A_ROWS][B_COLS], int N_sigmoid[A_ROWS][B_COLS + 1]) 
{
    for(int i = 0; i < A_ROWS; i ++) {
        for(int j = 0; j < B_COLS; j++) {
            if(j == 0) {
                N_sigmoid[i][0] = 256;
            }
            N_sigmoid[i][j + 1] = sigmoid_LUT[(u8)N[i][j]];
        }
    }
}


void output_matmul(const int IN[IN_ROWS][IN_COLS],
                  const int W_T[IN_COLS][W_COLS],
                  int RES[RES_ROWS][RES_COLS])
{
    for (int i = 0; i < IN_ROWS; i++) {
        for (int j = 0; j < W_COLS; j++) {
            int32_t acc = 0;
            for (int k = 0; k < IN_COLS; k++) {
                acc += (int32_t)IN[i][k] * (int32_t)W_T[j][k]; // B_T[j][k] == B[k][j]
            }
            RES[i][j] = (int)(acc >> 8); // divide by 256
        }
    }
}


void convertRES(int RES[RES_ROWS][RES_COLS])
{
    for (int i = 0; i < RES_ROWS; i++) {
        for (int j = 0; j < RES_COLS; j++) {
            int val = (RES[i][j] > 128);
            RES[i][j] = val;
        }
    }
}