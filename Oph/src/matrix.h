#ifndef MATRIX_H
#define MATRIX_H

#define M 3
#define N 4

int min(int num1, int num2);
void printMatrix(double matrix[M][N]);
int gaussianElimination(double A[M][N], double x[M]);

#endif 