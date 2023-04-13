#include <stdio.h>
#include <stdlib.h>
#include <mpi/mpi.h>

// mpicc main.c -o prueba
// pirun -n 4 --oversubscribe ./prueba 100 A

void inicializaCadena(char *cadena, int n) {
    int i;

    for(i=0; i < n/2; i++){
        cadena[i] = 'A';
    }

    for(i=n/2; i < 3*n/4; i++){
        cadena[i] = 'C';
    }

    for(i=3*n/4; i < 9*n/10; i++){
        cadena[i] = 'G';
    }

    for(i=9*n/10; i < n; i++){
        cadena[i] = 'T';
    }
}

int main(int argc, char *argv[]) {
    int i, n, count = 0;
    char *cadena;
    char letra;
    int size, rank;
    int c;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(rank == 0) {
        printf("Escribe una letra y un tamaño: \n"); scanf("%c %d", &letra, &n);

        cadena = (char *) malloc(n * sizeof(char));

        inicializaCadena(cadena, n);
        cadena[n] = '\0';
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if(rank != 0) {
        cadena = (char *) malloc(n * sizeof(char));
    }

    MPI_Bcast(&letra,1, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(cadena, n, MPI_CHAR,0, MPI_COMM_WORLD);

    for(i = rank; i < n; i += size) {
        if(cadena[i] == letra) {
            count++;
        }
    }

    MPI_Reduce(&count, &c, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if(rank == 0) {
        printf("El numero de apariciones de la letra %c es %d\n", letra, c);
    }

    free(cadena);
    MPI_Finalize();
    exit(0);
}