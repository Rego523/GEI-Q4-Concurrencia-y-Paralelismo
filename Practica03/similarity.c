#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <mpi/mpi.h>

/* mpirun -n X ./mpi-sim > salida
 * ./similarity salida2
 * diff salida salida2*/

// mpicc similarity.c -o prueba -lm
// mpirun -n 3 --oversubscribe ./prueba

#define DEBUG 0

/* Translation of the DNA bases
   A -> 0
   C -> 1
   G -> 2
   T -> 3
   N -> 4*/

#define M  1000000 // Number of sequences
#define N  200 // Number of bases per sequence

int redondearArriba(int x, int y) {
    if( x % y == 0) {
        return (x / y);
    } else {
        return (x / y) + 1;
    }
}

unsigned int g_seed = 0;

int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16) % 5;
}

// The distance between two bases
int base_distance(int base1, int base2) {

      if((base1 == 4) || (base2 == 4)){
            return 3;
      }

      if(base1 == base2) {
            return 0;
      }

      if((base1 == 0) && (base2 == 3)) {
            return 1;
      }

      if((base2 == 0) && (base1 == 3)) {
            return 1;
      }

      if((base1 == 1) && (base2 == 2)) {
            return 1;
      }

      return 2;
}

int main(int argc, char *argv[]) {

    int i, j;
    int sendsize;
    int *data1, *data2, *dataR1, *dataR2;
    int *result, *resultR;
    struct timeval  tv1, tv2;
    int numprocs, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int filas = M / numprocs;

        //int filas = redondearArriba(M, numprocs);

        if(M % numprocs != 0) {
            filas++;
    }

    if(rank == 0) {
        data1 = (int *) malloc(filas * numprocs * N * sizeof(int));
        data2 = (int *) malloc(filas * numprocs * N * sizeof(int));
        result = (int *) malloc(filas * numprocs * sizeof(int));


        /* Initialize Matrices */
        for(i = 0; i < M; i++) {
            for (j = 0; j < N; j++) {
                /* random with 20% gap proportion */
                data1[i * N + j] = fast_rand();
                data2[i * N + j] = fast_rand();
            }
        }
    }

    dataR1 = (int *) malloc(filas* N * sizeof(int));
    dataR2 = (int *) malloc(filas * N * sizeof(int));
    resultR = (int *) malloc(filas * sizeof(int));

    gettimeofday(&tv1, NULL);

    MPI_Scatter(data1, filas * N, MPI_INT, dataR1, filas * N, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(data2, filas * N, MPI_INT, dataR2, filas * N, MPI_INT, 0, MPI_COMM_WORLD);


    for(i = 0; i < filas; i++) {
        resultR[i] = 0;
        for(j = 0; j < N; j++) {
            resultR[i] += base_distance(dataR1[i * N + j], dataR2[i * N + j]);
        }
    }


    MPI_Gather(resultR,  filas, MPI_INT, result,  filas, MPI_INT, 0, MPI_COMM_WORLD);


    gettimeofday(&tv2, NULL);

    int microseconds = (tv2.tv_usec - tv1.tv_usec) + 1000000 * (tv2.tv_sec - tv1.tv_sec);

    /* Display result */
    if(DEBUG == 1) {
        if(rank == 0 ){
            int checksum = 0;

            for(i = 0; i < M; i++) {
                checksum += result[i];
            }

            printf("Checksum: %d\n ", checksum);
        }

    } else if(DEBUG == 2) {
        for(i = 0; i < M; i++) {
            printf("Result i %d: %d \n", i, result[i]);
        }
    } else {
        printf ("Process %d, Time (seconds) = %lf\n", rank, (double) microseconds/1E6);
    }

    if(rank == 0) {
        free(data1);
        free(data2);
        free(result);
    }

    free(dataR1);
    free(dataR2);
    free(resultR);

    MPI_Finalize();
    return 0;
}

