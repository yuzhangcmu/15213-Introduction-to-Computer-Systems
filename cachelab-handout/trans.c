/* 
 * trans.c - Matrix transpose B = A^T
 * Name: Bin Feng
 * Andrew ID: bfeng
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    int i, j, k, l;
    int x0, x1, x2, x3, x4, x5, x6, x7;


    switch(N){
        case 32:
            for (j = 0; j < M; j += 8) {
                for (i = 0; i < N; i += 8) {
                    x0 = 0;
                    for (k = i; k < i + 8; k++) {
                        for (l = j; l < j + 8; l++) {
                            // Diagonal element saved without accessing B[][]
                            if(k == l){
                                x0 = A[k][l];
                            }
                            else{
                                B[l][k] = A[k][l];
                            }
                        }
                        if(i == j){
                            B[k][k] = x0;
                        }
                    }
                }
            }
            break;

        case 67:
            for (j = 0; j < M; j += 20) {
                for (i = 0; i < N; i += 20) {
                    for (k = i; k < i+20; k++) {
                        for (l = j; l < j+20; l++) {
                            if(k<N && l<M){
                                B[l][k] = A[k][l];
                            }
                        }
                    }
                }
            }
            break;

        case 64:
            for (j = 0; j < M; j += 8)
                for (i = 0; i < N; i += 8){
                    for (k = 0; k < 8; k++)
                    {
                        // Beginning of i+k row
                        x0 = A[i+k][j];
                        x1 = A[i+k][j+1];
                        x2 = A[i+k][j+2];
                        x3 = A[i+k][j+3];
                        
                        if (k == 0){
                            x4 = A[i+k][j+4];
                            x5 = A[i+k][j+5];
                            x6 = A[i+k][j+6];
                            x7 = A[i+k][j+7];
                        }

                        // Beginning of i+k col
                        B[j][i+k] = x0;
                        B[j][i+k+64] = x1;
                        B[j][i+k+64*2] = x2;
                        B[j][i+k+64*3] = x3;
                    }


                    for (k = 7; k > 0; k--)
                    {
                        // Beginning of i+k row (k from 7)
                        x0 = A[i+k][j+4];
                        x1 = A[i+k][j+5];
                        x2 = A[i+k][j+6];
                        x3 = A[i+k][j+7];

                        B[j+4][i+k] = x0;
                        B[j+4][i+k+64] = x1;
                        B[j+4][i+k+64*2] = x2;
                        B[j+4][i+k+64*3] = x3;
                    }
                    
              
                    B[j+4][i] = x4;
                    B[j+4][i+64] = x5;
                    B[j+4][i+64*2] = x6;
                    B[j+4][i+64*3] = x7;

                    // for (k = 0; k < 8; k++){
                    //     x0 = A[i+k][j];
                    //     x1 = A[i+k][j+1];
                    //     x2 = A[i+k][j+2];
                    //     x3 = A[i+k][j+3];
                    //     x4 = A[i+k][j+4];
                    //     x5 = A[i+k][j+5];
                    //     x6 = A[i+k][j+6];
                    //     x7 = A[i+k][j+7];
                    //     B[j][i+k] = x0;
                    //     B[j][i+k+64] = x1;
                    //     B[j][i+k+64*2] = x2;
                    //     B[j][i+k+64*3] = x3;
                    //     B[j][i+k+64*4] = x4;
                    //     B[j][i+k+64*5] = x5;
                    //     B[j][i+k+64*6] = x6;
                    //     B[j][i+k+64*7] = x7;
                    // }
                }
            break;

    }

    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

