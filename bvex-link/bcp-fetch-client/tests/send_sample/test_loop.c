#include "test_loop.h"
#include "common.h"
#include "send_sample.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void run_telemetry_loop(int socket_fd, bool measure_timing)
{
    const int TIMING_WINDOW = 1000;
    double timing_buffer[TIMING_WINDOW];
    int timing_index = 0;
    struct timespec start_time, end_time;

    while(1) {
        if(measure_timing) {
            clock_gettime(CLOCK_MONOTONIC, &start_time);
        }

        // Get current time and generate simulated data
        time_t t;
        time(&t);
        float timestamp = (float)t;

        // Generate and send simulated sensor data
        int temp = (int)generate_sinusoid(20.0, 1.0 / 60.0, 0.0, t) + 20;
        long altitude =
            (long)generate_sinusoid(1000.0, 1.0 / 120.0, 0.0, t) + 10000;
        float roll = generate_sinusoid(1.0, 1.0 / 60.0, 0.0, t);

        send_sample_int32(socket_fd, "temperature", timestamp, temp);
        send_sample_int64(socket_fd, "altitude", timestamp, altitude);
        send_sample_float(socket_fd, "roll", timestamp, roll);

        if(measure_timing) {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                                (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

            timing_buffer[timing_index++] = time_taken;

            if(timing_index == TIMING_WINDOW) {
                double sum = 0.0;
                for(int i = 0; i < TIMING_WINDOW; i++) {
                    sum += timing_buffer[i];
                }
                double avg = sum / TIMING_WINDOW;
                printf("Avg send time: %.3fms\n", avg * 1000.0);
                timing_index = 0;
            }
        }

        usleep(1000); // 1ms delay
    }
}