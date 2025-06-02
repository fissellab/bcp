#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"

int exiting = 0;
int spec_running = 0;
int spec_479khz = 0; // Flag to track if 479kHz spectrometer is running
int spec_120khz = 0; // Flag to track if 120kHz spectrometer is running
pid_t python_pid;

void run_python_script(const char* script_name, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", script_name, hostname, logpath, mode, "-i", interval_str, "-p", data_save_path, (char*)NULL);
    perror("execlp failed");
    exit(1);
}

void exec_command(char* input, FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char* arg = (char*) malloc(strlen(input) * sizeof(char));
    char* cmd = (char*) malloc(strlen(input) * sizeof(char));
    
    // Additional argument to capture optional "479khz" or "120khz" parameter
    char* sub_arg = (char*) malloc(strlen(input) * sizeof(char));
    sub_arg[0] = '\0'; // Initialize to empty string
    
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
    } else if (strcmp(cmd, "start") == 0) {
        // Check for "start spec" or "start spec 479khz" or "start spec 120khz"
        sscanf(arg, "%s %s", sub_arg, arg + strlen(sub_arg) + 1);
        
        if (strcmp(sub_arg, "spec") == 0) {
            if (!spec_running) {
                spec_running = 1;
                
                // Check if we're starting the 120khz version
                if (strlen(arg) > strlen(sub_arg) && strstr(arg, "120khz") != NULL) {
                    spec_120khz = 1;
                    spec_479khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process for 120khz spectrometer
                        run_python_script("rfsoc_spec_120khz.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        perror("fork failed");
                        spec_running = 0;
                        spec_120khz = 0;
                    } else {
                        // Parent process
                        printf("Started 120kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started 120kHz spec script");
                    }
                }
                // Check if we're starting the 479khz version
                else if (strlen(arg) > strlen(sub_arg) && strstr(arg, "479khz") != NULL) {
                    spec_479khz = 1;
                    spec_120khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process for 479khz spectrometer
                        run_python_script("rfsoc_spec_479khz.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        perror("fork failed");
                        spec_running = 0;
                        spec_479khz = 0;
                    } else {
                        // Parent process
                        printf("Started 479kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started 479kHz spec script");
                    }
                } else {
                    // Standard spectrometer (960kHz)
                    spec_479khz = 0;
                    spec_120khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process
                        run_python_script("rfsoc_spec.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        perror("fork failed");
                        spec_running = 0;
                    } else {
                        // Parent process
                        printf("Started spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started spec script");
                    }
                }
            } else {
                printf("Spec script is already running\n");
            }
        } else if (strcmp(sub_arg, "gps") == 0) {
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
        } else {
            printf("Unknown start command: %s\n", sub_arg);
        }
    } else if (strcmp(cmd, "stop") == 0) {
        // Check for "stop spec" or "stop spec 479khz" or "stop spec 120khz"
        sscanf(arg, "%s %s", sub_arg, arg + strlen(sub_arg) + 1);
        
        if (strcmp(sub_arg, "spec") == 0) {
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
                        if (spec_479khz) {
                            printf("Stopped 479kHz spec script\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped 479kHz spec script");
                        } else if (spec_120khz) {
                            printf("Stopped 120kHz spec script\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped 120kHz spec script");
                        } else {
                            printf("Stopped spec script\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
                        }
                        spec_479khz = 0; // Reset the spectrometer type flags
                        spec_120khz = 0;
                        break;
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
                    if (spec_479khz) {
                        printf("Forcefully stopped 479kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped 479kHz spec script");
                    } else if (spec_120khz) {
                        printf("Forcefully stopped 120kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped 120kHz spec script");
                    } else {
                        printf("Forcefully stopped spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped spec script");
                    }
                    spec_479khz = 0; // Reset the spectrometer type flags
                    spec_120khz = 0;
                }
            } else {
                printf("Spec script is not running\n");
            }
        } else if (strcmp(sub_arg, "gps") == 0) {
            if (gps_is_logging()) {
                gps_stop_logging();
                printf("Stopped GPS logging\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
            } else {
                printf("GPS logging is not active\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to stop GPS logging, but it was not active");
            }
        } else {
            printf("Unknown stop command: %s\n", sub_arg);
        }
    } else if (strcmp(cmd, "gps") == 0 && scan > 1) {
        // Handle GPS sub-commands
        if (strcmp(arg, "status") == 0) {
            // Command for GPS status display
            if (!gps_is_logging()) {
                printf("GPS logging is not active. Start GPS logging first.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to show GPS status, but GPS logging was not active");
            } else if (gps_is_status_active()) {
                printf("GPS status display is already active.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to show GPS status, but it was already active");
            } else {
                printf("Starting GPS status display. Press 'q' to exit.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started GPS status display");
                
                // Save terminal settings
                struct termios old_term;
                tcgetattr(STDIN_FILENO, &old_term);
                struct termios new_term = old_term;
                new_term.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
                tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
                
                // Display GPS status (blocks until user exits)
                gps_display_status();
                
                // Restore terminal settings
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
                
                // Flush any remaining input to prevent interference with CLI
                fflush(stdin);
                
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS status display");
            }
        } else {
            printf("Unknown GPS command: %s\n", arg);
        }
    } else {
        printf("%s: Unknown command\n", cmd);
    }

    free(arg);
    free(cmd);
    free(sub_arg);
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
