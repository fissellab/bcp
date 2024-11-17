/* Funcrions swap(), pivot(), forwardElimination(), backSubstitution(), and 
** gaussianElimination() were adapted from Sedgewick and Wayne's 
** GaussianElimination.java package:
** https://algs4.cs.princeton.edu/code/edu/princeton/cs/algs4/GaussianElimination.java.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "matrix.h"

#define EPSILON 1e-8

/* Helper function to find minimum of two input numbers.
** Input: The two numbers to be compared.
** Output: The smaller number.
*/
int min(int num1, int num2) {
    return (num1 > num2) ? num2 : num1;
}

/* Function to print matrices row-wise.
** Input: The matrix to be printed to the terminal.
** Output: None (void). Prints the matrix to the terminal for verification.
*/
void printMatrix(double matrix[M][N]) {
    for (int row = 0; row < M; row++) {
        printf("[");
        for (int col = 0; col < N; col++) {
            printf("%17lf\t", matrix[row][col]);
        }
        printf("]\n");
    }
}

/* Function to swap the rows of a matrix.
** Input: The rows to be swapped (row1 and row2) and the matrix itself.
** Output: None (void).
*/
void swap(int row1, int row2, double matrix[M][N]) {
    for (int col = 0; col < N; col++) {
        double temp = matrix[row1][col];
        matrix[row1][col] = matrix[row2][col];
        matrix[row2][col] = temp;
    }
}

/* Function to pivot on matrix[p][p].
** Input: the pivot index (p) and the matrix itself.
** Output: None (void).
*/
void pivot(int p, double matrix[M][N]) {
    for (int i = p + 1; i < M; i++) {
        double multiplier = matrix[i][p]/matrix[p][p];
        for (int j = p; j <= N; j++) {
            matrix[i][j] -= multiplier*matrix[p][j];
        }
    }
}

/* Function to reduce input matrix.
** Input: The matrix.
** Output: None.
*/
void forwardElimination(double matrix[M][N]) {
    for (int p = 0; p < M; p++) {
        // find pivot row via partial pivoting
        int max = p;
        for (int i = p + 1; i < M; i++) {
            if (fabs(matrix[i][p]) > fabs(matrix[max][p])) {
                max = i;
            }
        }

        // if absolute value of diagonal element is smaller than any terms below
        //  it, interchange the rows
        swap(p, max, matrix);

        // if after partial pivoting diagonal element is ~0, matrix might be 
        // singular
        if (fabs(matrix[p][p]) <= EPSILON) {
            continue;
        }

        // pivot
        pivot(p, matrix);   
    }
}

/* Function to perform back substitution on a matrix and thereby solve system of
** linear equations.
** Input: The matrix.
** Output: A ptr to solution vector (x) or NULL if a solution does not exist.
*/
double * backSubstitution(double matrix[M][N]) {
    double * x = calloc(M, sizeof(double));
    if (x == NULL) {
        fprintf(stderr, "Back substitution solution allocation error: %s.\n", 
                strerror(errno));
    }

    for (int i = min(N - 1, M - 1); i >= 0; i--) {
        double sum = 0.0;
        for (int j = i + 1; j < N - 1; j++) {
            sum += matrix[i][j]*x[j];
        }

        if (fabs(matrix[i][i]) > EPSILON) {
            x[i] = (matrix[i][N - 1] - sum)/matrix[i][i];
        } else if (fabs(matrix[i][N - 1] - sum) > EPSILON) {
            if (x != NULL) {
                free(x);
            }

            return NULL;
        }
    }

    // redundant rows
    for (int i = N; i < M; i++) {
        double sum = 0.0;
        for (int j = 0; j < N; j++) {
            sum += matrix[i][j]*x[j];
        }

        if (fabs(matrix[i][N] - sum) > EPSILON) {
            if (x != NULL) {
                free(x);
            }
            
            return NULL;
        }
    } 

    return x;
}

/* Function to perform Gaussian elimination on input augmented matrix and solve 
** system of equations for a, b, c (elements of the input solution vector, x).
** Input: the augmented matrix and a vector to hold the solution (x).
** Output: None (void). Populates the solution vector. 
*/
int gaussianElimination(double A[M][N], double x[M]) {
    double * sol = calloc(3, sizeof(double));
    if (sol == NULL) {
        fprintf(stderr, "Error allocating memory for solution in "
                        "gaussianElimination(): %s.\n", 
                strerror(errno));
        return -1;
    }

    forwardElimination(A);

    sol = backSubstitution(A);
    if (sol == NULL) {
        printf("System is infeasible.\n");
        return -1;
    } 

    for (int i = 0; i < M; i++) {
        x[i] = sol[i];
    }

    if (sol != NULL) {
        free(sol);
    }

    return 1;    
}