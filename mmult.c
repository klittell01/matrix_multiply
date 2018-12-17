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
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

struct RowCol {
    int numRowsA, numRowsB, numColsB, r, c;
    double** A;
    double** B;
};

double ijResult = 0;
pthread_cond_t writeCV;

/*
Calculate: function to calculate the values of the resulting matrix
using as many threads as specified
*/
void* MultiCalculate(void* param){
    struct RowCol* myArg;
    myArg = calloc(1, sizeof(struct RowCol));
    myArg = (struct RowCol*) param;

    int numRowsA, numRowsB, numColsB, r, c;

    numRowsA = myArg->numRowsA;
    numRowsB = myArg->numRowsB;
    numColsB = myArg->numColsB;
    r = myArg->r;
    c = myArg->c;
    double myNum = 0.0;
    for(int i = 0; i < numColsB; i++){
        myNum += myArg->A[r][i] * myArg->B[i][c];
    }
    ijResult = myNum;
    pthread_cond_broadcast(&writeCV);
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
            fwrite(out, 1, sizeof(double), fdOut);
    }
}

int main (int argc, char * argv[]){
    FILE* fd1;
    FILE* fd2;
    FILE* fd3;
    int numRowsA, numColsA, numRowsB, numColsB;
    int myRow = 0;
    int myCol = 0;
    double* A;
    double* B;
    int readCount, outfile, infile1, infile2, numThreads;
    bool useThreads = false;
    char* p;
    char* buff1;
    char* buff2;
    char* aStore;
    char* bStore;

    pthread_mutex_t mutexRC;
    pthread_mutex_t mutexOut;

    // init mutex //
    pthread_mutex_init(&mutexRC, NULL);
    pthread_mutex_init(&mutexOut, NULL);

    // init CV //
    pthread_cond_init(&writeCV, NULL);

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
    numRowsA = (int)buff1[0];
    numColsA = (int)buff1[4];

    // read in information about the second matrix //
    readCount = fread(buff2, sizeof(double), 1, fd2);
    if(readCount <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile2]);
        return -1;
    }
    // assign row/col info //
    numRowsB = (int)buff2[0];
    numColsB = (int)buff2[4];
    if(numRowsA != numColsB){
        printf("Error: Cannot multiply matrices of these sizes.\n");
        return -1;
    }
    if(numRowsA < 0 || numRowsB < 0 || numColsA < 0 || numColsB < 0){
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
    aStore = malloc(sizeof(double) * numRowsA * numColsA);
    itemsRead = fread(aStore, sizeof(double), numRowsA * numColsA, fd1);
    if(itemsRead <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile1]);
        return -1;
    }
    A = (double*) aStore;

    bStore = malloc(sizeof(double) * numRowsB * numColsB);
    itemsRead = fread(bStore, sizeof(double), numRowsB * numColsB, fd2);
    if(itemsRead <= 0){
        printf("Error: Unable to read file %s.\n", argv[infile2]);
        return -1;
    }
    B = (double*) bStore;

    // convert to easily usable arrays //
    // convert rows of A //
    double* rA[numRowsA];
    int rowIndexA, colIndexA;
    for(int i = 0; i < numRowsA; i++){
        rA[i] = malloc(sizeof(double) * numColsA);
    }
    for(int i = 0; i < (numRowsA * numColsA); i++){
        rowIndexA = i / numColsA;
        colIndexA = i % numColsA;
        rA[rowIndexA][colIndexA] = A[i];
    }

    // convert rows of B //
    double* rB[numRowsB];
    int rowIndexB, colIndexB;
    for(int i = 0; i < numRowsB; i++){
        rB[i] = malloc(sizeof(double) * numColsB);
    }
    for(int i = 0; i < (numRowsB * numColsB); i++){
        rowIndexB = i / numColsB;
        colIndexB = i % numColsB;
        rB[rowIndexB][colIndexB] = B[i];
    }

/*
    NOTE: to access file A data use A[i]. for rows use rA[i][j] where i
    is the row number and j is the index of the item in that row.
*/

    // start sending our info to all threads and stuff //
    // begin by timing the process //
    int startT;
    int endT;
    pthread_t threads[numThreads];

    startT = time(NULL);

    int* outSize;
    char* sizeBuff;
    outSize = malloc(2 * sizeof(int));
    outSize[0] = numRowsA;
    outSize[1] = numColsB;
    sizeBuff = (char*) outSize;
    fwrite(sizeBuff, 1, sizeof(double), fd3);

    if(useThreads == false){
        Calc(0, 0, numRowsA, numRowsB, numColsB, rA, rB, fd3);
    } else {
        int total = numRowsA * numColsB;
        int count = 0;
        struct RowCol* myArg = (struct RowCol *) calloc(1, sizeof(struct RowCol));
        myArg->A = calloc(numRowsA * numColsB, sizeof(double));
        myArg->B = calloc(numRowsA * numColsB, sizeof(double));
        myArg->numRowsA = numRowsA;
        myArg->numRowsB = numRowsB;
        myArg->numColsB = numColsB;
        memcpy(myArg->A, rA, (sizeof(double) * numRowsA * numColsA));
        memcpy(myArg->B, rB, (sizeof(double) * numRowsA * numColsA));

        while(count < total){
            pthread_mutex_lock(&mutexRC);
                myArg->r = myRow;
                myArg->c = myCol;

                if(myRow < (numRowsA - 1)){
                    if(myCol < (numColsB - 1)){
                        myCol++;
                    } else {
                        myRow++;
                        myCol = 0;
                    }
                } else {
                    if(myCol < (numColsB - 1)){
                        myCol++;
                    }
                }

                pthread_create(&threads[count % numThreads], NULL, MultiCalculate, (void*)myArg);

                pthread_cond_wait(&writeCV, &mutexRC);

                double* outDouble;
                char* out;
                outDouble = malloc(sizeof(double));
                outDouble[0] = ijResult;
                out = (char*) outDouble;
                fwrite(out, 1, sizeof(double), fd3);

            pthread_mutex_unlock(&mutexRC);

            count++;
            }
        }

    endT = time(NULL);
    double totalTime;
    totalTime = (double)(endT - startT);

    printf("\nIt took %.1f seconds to multiply \n", totalTime);
    printf("matrix A: rows %d, cols %d\n", numRowsA, numColsA);
    printf("matrix B: rows %d, cols %d\n", numRowsB, numColsB);
    printf("Using %d worker threads.\n\n", numThreads);

    fclose(fd1);
    fclose(fd2);
    fclose(fd3);
    if(useThreads == true){
        for(int i = 0; i < numThreads; i++){
           pthread_join(threads[i], NULL);
        }
    }

    return 0;
}
