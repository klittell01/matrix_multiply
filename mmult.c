/*
 * mmult.c
 * Author: Kevin Littell
 * Date: 12-12-2018
 * COSC 3750, program 11
 * multiply two matrices
 */

#include "stdbool.h"
#include "unistd.h"
#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "fcntl.h"
#include "pthread.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

struct RowCol {
    int numRowsA, numRowsB, numColsB, r, c;
    double* A[2048];
    double* B[2048];
};

struct ResultMatrix{
    double result[2048][2048];
};

/*
Calculate: function to calculate the values of the resulting matrix
using one extra thread
*/
void* Calculate(void* param){
    struct RowCol* myArg;
    myArg = calloc(1, sizeof(struct RowCol));
    myArg = (struct RowCol*) param;

    int numRowsA, numRowsB, numColsB;

    numRowsA = myArg->numRowsA;
    numRowsB = myArg->numRowsB;
    numColsB = myArg->numColsB;
    printf("function numRowsA: %d, numColsB: %d\n", numRowsA, numColsB);
    double result[numRowsA][numColsB];
    for(int i = 0; i < numRowsA; i++){
        for(int j = 0; j < numColsB; j++){
            //printf("j = %d\n", j);
            result[i][j] = 0;
            for (int k = 0; k < numRowsB; k++){
                //printf("k = %d\n", k);
                //printf("A[%d][%d] = %f\n", i, k, myArg->A[i][k]);
                //printf("B[%d][%d] = %f\n", k, j, myArg->B[k][j]);
                result[i][j] += myArg->A[i][k] * myArg->B[k][j];

            }
        }
    }

    return NULL;
}

/*
Calc: function to calculate the values of the resulting matrix
without using any extra threads
*/
void Calc(int r, int c, int numRowsA, int numRowsB,
     int numColsB, double* A[], double* B[], FILE * fdOut){

    double result[numRowsA][numColsB];
    for(int i = 0; i < numRowsA; i++){
        for(int j = 0; j < numColsB; j++){
            result[i][j] = 0;
            for (int k = 0; k < numRowsB; k++){
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    /*
    convert the results to a single dimension array and cast to a char
    array so we can write them out easily using fwrite
    */
    double* outStore;
    char* out;
    int rowIndexOut, colIndexOut;
    outStore = malloc(sizeof(double) * numRowsA * numColsB);
    for(int i = 0; i < (numRowsA * numColsB); i++){
            rowIndexOut = i / numColsB;
            colIndexOut = i % numColsB;
            outStore[i] = result[rowIndexOut][colIndexOut];
            out = (char*) outStore;
            printf("row index: %d, col index: %d, result = %f\n", rowIndexOut,
                colIndexOut, result[rowIndexOut][colIndexOut]);
            fwrite(out, 1, sizeof(double), fdOut);
    }
}

struct ResultMatrix res;

int main (int argc, char * argv[]){
    FILE* fd1;
    FILE* fd2;
    FILE* fd3;
    int rows1, cols1, rows2, cols2;
    int outR, outC;
    double* A;
    double* B;
    int readCount, outfile, infile1, infile2, numThreads;
    pthread_cond_t writeCV = PTHREAD_COND_INITIALIZER;
    bool useThreads = false;
    char* p;
    char* buff1;
    char* buff2;
    char* aStore;
    char* bStore;

    pthread_mutex_t mutexRC;
    pthread_mutex_t mutexOut;

    // zero out buffer //
    buff1 = (char*) calloc(8, sizeof(char));
    buff2 = (char*) calloc(8, sizeof(char));

    // setup arguments depending on thread usage or no
    if(argc == 4){ // no threads will be used
        infile1 = 1;
        infile2 = 2;
        outfile = 3;
        numThreads = 0;
        useThreads = false;
    } else if (argc == 5){ // use number of threads in argv[1]
        infile1 = 2;
        infile2 = 3;
        outfile = 4;
        long conv = strtol(argv[1], &p, 10); // convert argv[1] to an int
        numThreads = conv;
        if(numThreads < 0){
            printf("Error: Cannot use zero or less threads.\n");
            return -1;
        }
        useThreads = true;
    }

    // open matrix files //
    fd1 = fopen(argv[infile1], "r");
    if(fd1 < 0){
        perror(argv[1]);
        return -1;
    }
    fd2 = fopen(argv[infile2], "r");
    if(fd2 < 0){
        perror(argv[2]);
        return -1;
    }

    // read in information about the first matrix //
    readCount = fread(buff1, sizeof(double), 1, fd1);
    if(readCount <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile1]);
    }
    // assign row/col info //
    rows1 = (int)buff1[0];
    cols1 = (int)buff1[4];

    // read in information about the second matrix //
    readCount = fread(buff2, sizeof(double), 1, fd2);
    if(readCount <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile2]);
        return -1;
    }
    // assign row/col info //
    rows2 = (int)buff2[0];
    cols2 = (int)buff2[4];
    if(rows1 != cols2){
        printf("Error: Cannot multiply matrices of these sizes.\n");
        return -1;
    }
    if(rows1 < 0 || rows2 < 0 || cols1 < 0 || cols2 < 0){
        printf("Error: Size of matrices must be positive.\n");
        return -1;
    }

    // open file for writing output //
    fd3 = fopen(argv[outfile], "w");
    if(fd3 < 0){
        perror(argv[outfile]);
        return -1;
    }

    // read complete files into memory //
    int itemsRead;
    aStore = malloc(sizeof(double) * rows1 * cols1);
    itemsRead = fread(aStore, sizeof(double), rows1 * cols1, fd1);
    if(itemsRead <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile1]);
        return -1;
    }
    A = (double*) aStore;

    bStore = malloc(sizeof(double) * rows2 * cols2);
    itemsRead = fread(bStore, sizeof(double), rows2 * cols2, fd2);
    if(itemsRead <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile2]);
        return -1;
    }
    B = (double*) bStore;

    // convert to easily usable arrays //
    // convert rows of A //
    double* rA[rows1];
    int rowIndexA, colIndexA;
    for(int i = 0; i < rows1; i++){
        rA[i] = malloc(sizeof(double) * cols1);
    }
    for(int i = 0; i < (rows1 * cols1); i++){
        rowIndexA = i / cols1;
        colIndexA = i % cols1;
        rA[rowIndexA][colIndexA] = A[i];
    }

    // convert rows of B //
    double* rB[rows2];
    int rowIndexB, colIndexB;
    for(int i = 0; i < rows2; i++){
        rB[i] = malloc(sizeof(double) * cols2);
    }
    for(int i = 0; i < (rows2 * cols2); i++){
        rowIndexB = i / cols2;
        colIndexB = i % cols2;
        rB[rowIndexB][colIndexB] = B[i];
    }

    // convert columns of B //
    /* when i uncomment this it does something wierd with the memory
       and puts the wrong numbers in the first couple columns and rows
       of the rB array.
    double* cB[cols2];
    for(int i = 0; i < cols2; i++){
        for(int j = 0; j < rows2; j++){
            cB[j] = malloc(sizeof(double) * rows2);
            for(int k = 0; k < rows2; k++){
                cB[j][k] = A[(k * cols2) + j];
            }
        }
    }
    */
/*
    NOTE: to access file A data use A[i]. for columns use cA[i][j] where i is
    the column number and j is the index of the item in the column. for
    rows use rA[i][j] where i is the row number and j is the index of
    the item in that row.
*/

/*
    as this is it works for single thread multipilcation.
    double result[rows1][cols2];
    for(int i = 0; i < rows1; i++){
        for(int j = 0; j < cols2; j++){
            result[i][j] = 0;
            for (int k = 0; k < rows2; k++){
                result[i][j] += rA[i][k] * rB[k][j];
            }
        }
    }
*/

    // start sending our info to all threads and stuff //
    printf("rows1 = %d, cols1 = %d\n", rows1, cols1);
    if(useThreads == false){
        Calc(0, 0, rows1, rows2, cols2, rA, rB, fd3);
    } else {
        struct RowCol* myArg = (struct RowCol *) calloc(1, sizeof(struct RowCol));
        myArg->numRowsA = rows1;
        myArg->numRowsB = rows2;
        myArg->numColsB = cols2;
        memcpy(myArg->A, rA, 2048);
        memcpy(myArg->B, rB, 2048);
        pthread_t threadId;
        pthread_create(&threadId, NULL, Calculate, (void*)myArg);
        pthread_join(threadId, NULL);
    }


    /*
    TODO: i need to get thread return values working. as well as thread
    concurrency and locking
    */

/*
    pthread_t t1;
    struct RowCol* myArg = (struct RowCol*) calloc(1, sizeof(struct RowCol));
    myArg->numRowsA = rows1;
    myArg->numRowsB = rows2;
    myArg->numColsB = cols2;

    //memcpy(myArg->A, rA, 2048);
    //memcpy(myArg->col, colVals, 2048);
    printf("create p thread\n");
    int rtn1 = pthread_create(&t1, NULL, Calculate, &myArg);
    printf("rtn1 = %d\n", rtn1);
    printf("pthread done\n");
*/
    return 0;
}
