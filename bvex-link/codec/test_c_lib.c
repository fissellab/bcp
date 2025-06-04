#include "primitive.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Usage: %s <metric_id> <timestamp> <primitive-type> <value>\n", argv[0]);
        return 1;
    }
    char *metric_id = argv[1];
    double timestamp = strtod(argv[2], NULL);
    char *sample;
    if (strcmp(argv[3], "int") == 0) {
        int value = atoi(argv[4]);
        sample = encode_int_sample(metric_id, timestamp, value);
    } else if (strcmp(argv[3], "float") == 0) {
        float value = atof(argv[4]);
        sample = encode_float_sample(metric_id, timestamp, value);
    } else if (strcmp(argv[3], "bool") == 0) {
        int value = atoi(argv[4]);
        sample = encode_bool_sample(metric_id, timestamp, value);
    } else {
        printf("Invalid primitive type: %s\n", argv[3]);
        return 1;
    }
    printf("%s", sample);
    free(sample);
    return 0;
}