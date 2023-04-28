#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <mpi/mpi.h>

/* mpirun -n X ./mpi-sim > salida
 * ./similarity salida2
 * diff salida salida2*/

#define DEBUG 2

/* Translation of the DNA bases
   A -> 0
   C -> 1
   G -> 2
   T -> 3
   N -> 4*/

#define M  10 // Number of sequences
#define N  2 // Number of bases per sequence

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

      if((base2 == 2) && (base1 == 1)) {
            return 1;
      }

      return 2;
}

int main(int argc, char *argv[] ) {

    int i, j;
    int size;
    int *data1, *data2;
    int *result;
    struct timeval  tv1, tv2;
    int numprocs, rank;

    data1 = (int *) malloc(M * N * sizeof(int));
    data2 = (int *) malloc(M * N * sizeof(int));
    result = (int *) malloc(M * sizeof(int));


    /* Initialize Matrices */
    for(i = 0; i < M; i++) {
        for(j = 0; j < N; j++) {
              /* random with 20% gap proportion */
              data1[i * N + j] = fast_rand();
              data2[i * N + j] = fast_rand();
        }
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    size = redondearArriba(M, numprocs) * N;

    printf("size %d\n", size);

    // MPI_Scatter(A, block * N, MPI_INT, rank == root? MPI_IN_PLACE : A, block * N, MPI_INT, 0, MPI_COMM_WORLD);

    /* La función de arriba divide A entre los distintos procesos sin necesitar crear una nueva variable, ya que cuando el proceso root ejecute esta línea
     * los datos que le corresponde se quedarán donde están y si es otro proceso, se mandará al que sea.
     * Salió de la clase de teoría en el ejercicio de la multiplicación de matrices */

    // Dividimos data1, data2 y result entre el número de procesos
    MPI_Scatter(data1, size, MPI_INT, rank == 0? MPI_IN_PLACE : data1, size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(data2, size, MPI_INT, rank == 0? MPI_IN_PLACE : data2, size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(result, size, MPI_INT, rank == 0? MPI_IN_PLACE : result, size, MPI_INT, 0, MPI_COMM_WORLD);

    gettimeofday(&tv1, NULL);

    for(i = rank; i < M / numprocs; i += numprocs) {
        result[i] = 0;
        for(j = 0; j < N; j++) {
            result[i] += base_distance(data1[i * N + j], data2[i * N + j]);
        }
    }

    MPI_Gather(result, size, MPI_INT, result, size, MPI_INT, 0, MPI_COMM_WORLD);


    gettimeofday(&tv2, NULL);

    int microseconds = (tv2.tv_usec - tv1.tv_usec)+ 1000000 * (tv2.tv_sec - tv1.tv_sec);

    /* Display result */
    if(DEBUG == 1) {
        int checksum = 0;
        for(i = 0; i < M; i++) {
            checksum += result[i];
        }
        printf("Checksum: %d\n ", checksum);
    } else if(DEBUG == 2) {
        for(i = 0; i < M; i++) {
            printf("Result i %d: %d \n", i, result[i]);
        }
    } else {
        printf ("Time (seconds) = %lf\n", (double) microseconds/1E6);
    }

    free(data1); free(data2); free(result);
    MPI_Finalize();
    return 0;
}

