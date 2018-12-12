/*
 * mmult.c
 * Author: Kevin Littell
 * Date: 12-12-2018
 * COSC 3750, program 11
 * multiply two matrices
 */
// possible 1270
// have      704
// availble  400
// (.80 + .93) / 2 = .86

// .86

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
    int numRows, numCols;
    double* row;
    double* col;
};

void* Calculate(void* param){
    struct RowCol* myArg;
    int rtnVal;
    myArg = (struct RowCol*) param;
    for(int i = 0; i < myArg->numRows; i++){
        rtnVal = rtnVal + myArg->row[i];
    }

    return NULL;
}

int main (int argc, char * argv[]){
    int fd1, fd2, fd3, rows1, cols1, rows2, cols2;
    int outR, outC;
    double d1, d2;
    int readCount, outfile, infile1, infile2, numThreads;
    bool writeCV = false;
    bool useThreads = false;
    char* buff1;
    char* buff2;
    char* p;

    pthread_t* threads;
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

        // allocate memory for threads //
        threads = (pthread_t*) calloc(numThreads, sizeof(pthread_t));
        useThreads = true;
    }

        // open matrix files //
        fd1 = open(argv[infile1], O_RDONLY);
        if(fd1 < 0){
            perror(argv[1]);
            return -1;
        }
        fd2 = open(argv[infile2], O_RDONLY);
        if(fd2 < 0){
            perror(argv[2]);
            return -1;
        }

        // read in information about the first matrix //
        readCount = read(fd1, buff1, 8);
        if(readCount < 0){
            printf("Error: Unable to read file.\n");
        }
        // assign row/col info //
        rows1 = (int)buff1[0];
        cols1 = (int)buff1[4];

        // read in information about the second matrix //
        readCount = read(fd2, buff2, 8);
        if(readCount < 0){
            printf("Error: Unable to read file.\n");
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
        fd3 = open(argv[outfile], O_WRONLY | O_TRUNC | O_CREAT, 00600);
        if(fd3 < 0){
            perror(argv[outfile]);
            return -1;
        }


        double buffer[16];
        int a = read(fd1,buffer,sizeof(double));
        int size = 1;
        int c=0;

        for(c=0;c<size;c++){
            char* temp;
            temp = (char*) calloc(16, sizeof(char));
            double test;
            memset(&test, 0, 8);
            int x = snprintf(temp,16,"%f", buffer[c]);
            printf("temp1: %.3f\n", temp[1]);
            test = (double)temp[5];
            printf("temp2: %.3f\n", test);
            write(fd3, temp, x);
        }

        //printf("first double value is: %d\n", d1);


        printf("rows1: %i\n", rows1);
        printf("cols1: %i\n", cols1);
        printf("rows2: %i\n", rows2);
        printf("cols2: %i\n", cols2);

        pthread_t t1;
        struct RowCol* myArg = (struct RowCol*) calloc(1, sizeof(struct RowCol));
        myArg->row = calloc(cols2, sizeof(double));
        myArg->col = calloc(rows1, sizeof(double));
        double rowVals[10] = {1,1};
        double colVals[10] = {1,1};
        printf("kevin\n");
        myArg->numRows = 2;
        myArg->numCols = 2;
        memcpy(myArg->row, rowVals, 32);
        memcpy(myArg->col, colVals, 32);

        pthread_create(&t1, NULL, Calculate, &myArg);


    return 0;
}
