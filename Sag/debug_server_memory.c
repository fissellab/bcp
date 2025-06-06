#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    volatile int ready;
    volatile int active_type;
    volatile double timestamp;
    volatile int data_size;
    volatile double data[16384];
} shared_spectrum_t;

int main() {
    const char *SHM_NAME = "/bcp_spectrometer_data";
    
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        return 1;
    }
    
    // Map shared memory
    shared_spectrum_t *shared_memory = mmap(NULL, sizeof(shared_spectrum_t), 
                                           PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        return 1;
    }
    
    printf("C Server Shared Memory Debug\n");
    printf("============================\n");
    
    for (int i = 0; i < 10; i++) {
        printf("Reading #%d:\n", i + 1);
        printf("  ready: %d\n", shared_memory->ready);
        printf("  active_type: %d\n", shared_memory->active_type);
        printf("  timestamp: %.6f\n", shared_memory->timestamp);
        printf("  data_size: %d\n", shared_memory->data_size);
        printf("  data_points: %d\n", shared_memory->data_size / 8);
        
        // Check first 10 data points
        printf("  First 10 values: ");
        for (int j = 0; j < 10 && j < (shared_memory->data_size / 8); j++) {
            printf("%.1f ", shared_memory->data[j]);
        }
        printf("\n");
        
        // Count non-zero values
        int non_zero = 0;
        int total_points = shared_memory->data_size / 8;
        for (int j = 0; j < total_points; j++) {
            if (shared_memory->data[j] != 0.0) {
                non_zero++;
            }
        }
        printf("  Non-zero values: %d/%d\n", non_zero, total_points);
        printf("\n");
        
        sleep(2);
    }
    
    // Cleanup
    munmap(shared_memory, sizeof(shared_spectrum_t));
    close(shm_fd);
    
    return 0;
} 