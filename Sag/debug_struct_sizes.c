#include <stdio.h>
#include <stdint.h>
#include <signal.h>

typedef enum {
    SPEC_TYPE_NONE = 0,
    SPEC_TYPE_STANDARD = 1,
    SPEC_TYPE_120KHZ = 2
} spec_type_t;

typedef struct {
    volatile sig_atomic_t ready;
    volatile spec_type_t active_type;
    volatile double timestamp;
    volatile int data_size;
    volatile double data[16384];
} shared_spectrum_t;

int main() {
    shared_spectrum_t test_struct;
    
    printf("=== C Structure Layout Analysis ===\n");
    printf("sizeof(sig_atomic_t): %zu bytes\n", sizeof(sig_atomic_t));
    printf("sizeof(spec_type_t): %zu bytes\n", sizeof(spec_type_t));
    printf("sizeof(double): %zu bytes\n", sizeof(double));
    printf("sizeof(int): %zu bytes\n", sizeof(int));
    printf("sizeof(shared_spectrum_t): %zu bytes\n", sizeof(shared_spectrum_t));
    
    printf("\n=== Field Offsets ===\n");
    printf("ready offset: %zu\n", (char*)&test_struct.ready - (char*)&test_struct);
    printf("active_type offset: %zu\n", (char*)&test_struct.active_type - (char*)&test_struct);
    printf("timestamp offset: %zu\n", (char*)&test_struct.timestamp - (char*)&test_struct);
    printf("data_size offset: %zu\n", (char*)&test_struct.data_size - (char*)&test_struct);
    printf("data offset: %zu\n", (char*)&test_struct.data - (char*)&test_struct);
    
    printf("\n=== Python Expectation ===\n");
    printf("Python expects ready at: 0\n");
    printf("Python expects active_type at: 4\n");
    printf("Python expects timestamp at: 8\n");
    printf("Python expects data_size at: 16\n");
    printf("Python expects data at: 20\n");
    
    return 0;
} 