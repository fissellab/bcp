#include "request_sample.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REQUEST_SERVER_ADDR "127.0.0.1"
#define REQUEST_SERVER_PORT "8080"

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("Usage: %s <metric_id> <type> - Request a single value\n",
               argv[0]);
        printf("    type: \"float\" or \"string\"\n");
        return 1;
    }

    char* test_name = argv[1];

    if(strcmp(test_name, "request") == 0) {
        if(argc < 4) {
            printf(
                "Error: request test requires metric_id and type arguments\n");
            return 1;
        }

        char* metric_id = argv[2];
        char* type = argv[3];

        Requester requester =
            make_requester(metric_id, REQUEST_SERVER_ADDR, REQUEST_SERVER_PORT);
        if(requester.socket_fd < 0) {
            printf("Socket creation failed.\n");
            return 1;
        }

        if(strcmp(type, "float") == 0) {
            RequestFloatResult result = request_float(&requester);
            if(result.err) {
                printf("Request failed.\n");
                return 1;
            }
            printf("Request successful. Result: %f.\n", result.value);
        } else if(strcmp(type, "string") == 0) {
            RequestStringResult result = request_string(&requester);
            if(result.err) {
                printf("Request failed.\n");
                return 1;
            }
            printf("Request successful. Result: %s.\n", result.value);
        } else {
            printf("Unknown type: %s (must be 'float' or 'string')\n", type);
            return 1;
        }
    } else {
        printf("Unknown test name: %s\n", test_name);
        return 1;
    }

    return 0;
}