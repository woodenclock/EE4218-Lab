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
#define B_COLS 1
#endif

void MatMulDiv256(const int A[A_ROWS][A_COLS],
                  const int B[B_ROWS][B_COLS],
                  int RES[A_ROWS][B_COLS]);

void PrintResCsv(const int RES[A_ROWS][B_COLS]);
