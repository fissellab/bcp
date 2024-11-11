#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"

int exiting = 0;
int spec_running = 0;
pid_t python_pid;

void run_python_script(const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", "rfsoc_spec.py", hostname, logpath, mode, "-i", interval_str, "-p", data_save_path, (char*)NULL);
    perror("execlp failed");
    exit(1);
}

void exec_command(char* input, FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char* arg = (char*) malloc(strlen(input) * sizeof(char));
    char* cmd = (char*) malloc(strlen(input) * sizeof(char));
    int scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            printf("print is missing argument usage is print <string>\n");
        } else {
            printf("%s\n", arg);
        }
    } else if (strcmp(cmd, "exit") == 0) {
        printf("Exiting BCP\n");
        if (spec_running) {
            spec_running = 0;
            kill(python_pid, SIGTERM);
            waitpid(python_pid, NULL, 0);
            printf("Stopped spec script\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
        }
        if (gps_is_logging()) {
            gps_stop_logging();
            printf("Stopped GPS logging\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
        }
        exiting = 1;
    } else if (strcmp(cmd, "start") == 0 && strcmp(arg, "spec") == 0) {
        if (!spec_running) {
            spec_running = 1;
            python_pid = fork();
            if (python_pid == 0) {
                // Child process
                run_python_script(logpath, hostname, mode, data_save_interval, data_save_path);
                exit(0);  // Should never reach here
            } else if (python_pid < 0) {
                perror("fork failed");
                spec_running = 0;
            } else {
                // Parent process
                printf("Started spec script\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started spec script");
            }
        } else {
            printf("Spec script is already running\n");
        }
    } else if (strcmp(cmd, "stop") == 0 && strcmp(arg, "spec") == 0) {
        if (spec_running) {
            spec_running = 0;
            
            // Send SIGTERM first
            kill(python_pid, SIGTERM);
            
            // Wait for up to 5 seconds for the process to terminate
            int status;
            int timeout = 5;
            while (timeout > 0) {
                pid_t result = waitpid(python_pid, &status, WNOHANG);
                if (result == python_pid) {
                    printf("Stopped spec script\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
                    return;
                } else if (result == -1) {
                    perror("waitpid failed");
                    break;
                }
                sleep(1);
                timeout--;
            }
            
            // If the process hasn't terminated, use SIGKILL
            if (timeout == 0) {
                kill(python_pid, SIGKILL);
                waitpid(python_pid, NULL, 0);
                printf("Forcefully stopped spec script\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped spec script");
            }
        } else {
            printf("Spec script is not running\n");
        }
    } else if (strcmp(cmd, "start") == 0 && strcmp(arg, "gps") == 0) {
        if (!gps_is_logging()) {
            if (gps_start_logging()) {
                printf("Started GPS logging\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started GPS logging");
            } else {
                printf("Failed to start GPS logging\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start GPS logging");
            }
        } else {
            printf("GPS logging is already active\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to start GPS logging, but it was already active");
        }
    } else if (strcmp(cmd, "stop") == 0 && strcmp(arg, "gps") == 0) {
        if (gps_is_logging()) {
            gps_stop_logging();
            printf("Stopped GPS logging\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
        } else {
            printf("GPS logging is not active\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to stop GPS logging, but it was not active");
        }
    } else {
        printf("%s: Unknown command\n", cmd);
    }

    free(arg);
    free(cmd);
}

char* get_input() {
    char* input = (char*) malloc(1 * sizeof(char));
    char c;
    int i = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        input[i++] = c;
        input = (char*) realloc(input, i + 1);
    }
    input[i] = '\0';
    return input;
}

void cmdprompt(FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    int count = 1;
    char* input;
    while (exiting != 1) {
        printf("[BCP@Saggitarius]<%d>$ ", count);
        input = get_input();
        if (strlen(input) != 0) {
            write_to_log(cmdlog, "cli_Sag.c", "cmdprompt", input);
            exec_command(input, cmdlog, logpath, hostname, mode, data_save_interval, data_save_path);
        }
        free(input);
        count++;
    }
}