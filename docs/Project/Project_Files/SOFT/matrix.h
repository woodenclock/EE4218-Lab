#pragma once

// If main.c already defines these, we reuse them.
// If not, we provide defaults so matrix.c still compiles.
#ifndef A_ROWS
#define A_ROWS 64
#endif

#ifndef A_COLS
#define A_COLS 8
#endif

#ifndef B_ROWS
#define B_ROWS A_COLS
#endif

#ifndef B_COLS
#define B_COLS 2
#endif

#ifndef RES_ROWS
#define RES_ROWS 64
#endif

#ifndef RES_COLS
#define RES_COLS 1
#endif

#define IN_ROWS 64
#define IN_COLS 3
#define W_COLS 1

void MatMulDiv256(const int A[A_ROWS][A_COLS],
                  const int B_T[B_COLS][A_COLS],
                  int RES[A_ROWS][B_COLS]);

void PrintResCsv(const int RES[RES_ROWS][RES_COLS]);

void sigmoid(int N[A_ROWS][B_COLS], int N_sigmoid[A_ROWS][B_COLS + 1]);

void output_matmul(const int IN[IN_ROWS][IN_COLS],
                  const int W_T[IN_COLS][W_COLS],
                  int RES[RES_ROWS][RES_COLS]);

void convertRES(int RES[RES_ROWS][RES_COLS]);

