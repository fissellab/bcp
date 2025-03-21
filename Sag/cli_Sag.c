#include "cli_Sag.h"
#include "file_io_Sag.h"
#include "gps.h"
#include "command_server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

int exiting = 0;
int spec_running = 0;
pid_t python_pid;

void run_python_script(const char* logpath, const char* hostname,
                       const char* mode, int data_save_interval,
                       const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", "rfsoc_spec.py", hostname, logpath, mode, "-i",
           interval_str, "-p", data_save_path, (char*) NULL);
    perror("execlp failed");
    exit(1);
}

void print_command_response(const char* response, const command_server_t* command_server) {
    command_server_send(command_server, response);
    printf("%s\n", response);
}

void exec_command(char* input, FILE* cmdlog, const char* logpath,
                  const char* hostname, const char* mode,
                  int data_save_interval, const char* data_save_path, const command_server_t* command_server) {
    char* arg = (char*) malloc(strlen(input) * sizeof(char));
    char* cmd = (char*) malloc(strlen(input) * sizeof(char));
    int scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            print_command_response("print is missing argument usage is print <string>", command_server);
        } else {
            print_command_response(arg, command_server);
        }
    } else if (strcmp(cmd, "exit") == 0) {
        printf("Exiting BCP\n");
        if (spec_running) {
            spec_running = 0;
            kill(python_pid, SIGTERM);
            waitpid(python_pid, NULL, 0);
            print_command_response("Stopped spec script\n", command_server);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                         "Stopped spec script");
        }
        if (gps_is_logging()) {
            gps_stop_logging();
            print_command_response("Stopped GPS logging\n", command_server);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                         "Stopped GPS logging");
        }
        exiting = 1;
    } else if (strcmp(cmd, "start") == 0 && strcmp(arg, "spec") == 0) {
        if (!spec_running) {
            spec_running = 1;
            python_pid = fork();
            if (python_pid == 0) {
                // Child process
                run_python_script(logpath, hostname, mode, data_save_interval,
                                  data_save_path);
                exit(0); // Should never reach here
            } else if (python_pid < 0) {
                perror("fork failed");
                spec_running = 0;
            } else {
                // Parent process
                print_command_response("Started spec script\n", command_server);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                             "Started spec script");
            }
        } else {
            print_command_response("Spec script is already running\n", command_server);
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
                    print_command_response("Stopped spec script\n", command_server);
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                                 "Stopped spec script");
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
                print_command_response("Forcefully stopped spec script\n", command_server);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                             "Forcefully stopped spec script");
            }
        } else {
            print_command_response("Spec script is not running\n", command_server);
        }
    } else if (strcmp(cmd, "start") == 0 && strcmp(arg, "gps") == 0) {
        if (!gps_is_logging()) {
            if (gps_start_logging()) {
                print_command_response("Started GPS logging\n", command_server);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                             "Started GPS logging");
            } else {
                print_command_response("Failed to start GPS logging\n", command_server);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                             "Failed to start GPS logging");
            }
        } else {
            print_command_response("GPS logging is already active\n", command_server);
            write_to_log(
                cmdlog, "cli_Sag.c", "exec_command",
                "Attempted to start GPS logging, but it was already active");
        }
    } else if (strcmp(cmd, "stop") == 0 && strcmp(arg, "gps") == 0) {
        if (gps_is_logging()) {
            gps_stop_logging();
            print_command_response("Stopped GPS logging\n", command_server);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command",
                         "Stopped GPS logging");
        } else {
            print_command_response("GPS logging is not active\n", command_server);
            write_to_log(
                cmdlog, "cli_Sag.c", "exec_command",
                "Attempted to stop GPS logging, but it was not active");
        }
    } else {
        print_command_response("Unknown command\n", command_server);
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

void cmdprompt(FILE* cmdlog, const char* logpath, const char* hostname,
               const char* mode, int data_save_interval,
               const char* data_save_path) {
    int count = 1;
    char* input;
    struct pollfd fds[2];
    char buffer[1024];
    ssize_t bytes;

    // Initialize command server
    command_server_t* command_server = command_server_create(8000, 10); // Port 8000, max 10 connections
    if (!command_server) {
        printf("Failed to create command server\n");
        return;
    }

    if (command_server_listen(command_server) != 0) {
        printf("Failed to start command server\n");
        command_server_destroy(command_server);
        return;
    }

    // Setup poll fds
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = command_server->commands_read_fd;
    fds[1].events = POLLIN;

    while (exiting != 1) {
        char prompt[100];
        sprintf(prompt, "[BCP@Saggitarius]<%d>$ ", count);
        printf("%s", prompt);
        fflush(stdout);

        // Poll for input from both sources
        int ret = poll(fds, 2, -1); // Wait indefinitely
        if (ret < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, continue polling
            }
            printf("Poll error: %s\n", strerror(errno));
            break;
        }

        // Check stdin
        if (fds[0].revents & POLLIN) {
            input = get_input();
            if (strlen(input) != 0) {
                // broadcast prompt to command clients
                char* prompt = (char*) malloc(strlen("[BCP@Saggitarius]<%d>$ ") + strlen(input) + 1);
                sprintf(prompt, "[BCP@Saggitarius]<%d>$ %s", count, input);
                command_server_broadcast(command_server, prompt);
                
                write_to_log(cmdlog, "cli_Sag.c", "cmdprompt", input);

                exec_command(input, cmdlog, logpath, hostname, mode,
                           data_save_interval, data_save_path, command_server);
            }
            free(input);
            count++;
        }

        // Check command server
        if (fds[1].revents & POLLIN) {
            input = command_server_recv(command_server);

            if (input) {
                // print prompt to stdout
                printf("%s\n", input);

                write_to_log(cmdlog, "cli_Sag.c", "cmdprompt", input);
                exec_command(input, cmdlog, logpath, hostname, mode,
                           data_save_interval, data_save_path, command_server);
                free(input);
                count++;
            }
        }
    }

    // Cleanup
    command_server_destroy(command_server);
}