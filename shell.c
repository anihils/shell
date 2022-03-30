/*
 * Command line interpreter on top of Unix.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

FILE *input;  // batch file pointer
int redirect = 1;  // 0: redirect, 1: no redirect, else: error
char* fname = NULL;  // output file name for redirection
char *cmdline = NULL;  // to free command line from execcmd if it fails
char *sep_args[3];

void printerror(int errornum, char* arg) {
    char error[100];  // Error message cannot exceed 100 characters
    int len = 0;  // length of error message
    int arglen = 0;
    if (arg) {
        arglen = strlen(arg);
    }
    switch (errornum) {
        case 1 :  // Batch file couldn't be opened
            strcat(strcpy(error, "Error: Cannot open file "), arg);
            strcat(error, ".\n");
            len = arglen + 26;
            break;
        case 2 :  // Incorrect number of args for mysh
            strcat(error, "Usage: mysh [batch-file]\n");
            len = 25;
            break;
        case 3 :  // Invalid command/inccorect path to command
            strcat(strcpy(error, arg), ": Command not found.\n");
            len = arglen + 21;
            break;
        case 4 :  // Incorrect format for redirection
            strcpy(error, "Redirection misformatted.\n");
            len = 26;
            break;
        case 5 :  // Output file for redirection couldn't be used
            strcat(strcpy(error, "Cannot write to file "), arg);
            strcat(error, ".\n");
            len = arglen + 23;
            break;
        default :  // No error
            return;
    }
    *(error + len) = '\0';  // Null terminate error message
    write(STDERR_FILENO, error, (size_t) strlen(error));
}

void execcmd(char* args[]) {
    int child = fork();  // Create child process
    if (child == 0) {
        if (!redirect) {  // fd: output file
            int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                printerror(5, fname);
                return;
            }
            dup2(fd, 1);
            close(fd);
       }
       int status = execv(args[0], args);
        if (status == -1) {  // Executing command
            if (errno == 2) {  // command not found
                printerror(3, args[0]);
            }
            free(cmdline);
            fclose(input);
            _exit(1);  // Execution failed: exiting child process
        }
    } else if (child != -1) {  // Parent waits for child process to finish
        wait(NULL);
    }
}

void sep_arg(char *arg) {
    int i = 0;
    void *addr1 = arg;
    void *addr2 = strchr(arg, '>');
    if (addr1 != addr2) {  // '>' is the first character
        sep_args[i] = strtok(arg, ">");
        i++;
    }
    sep_args[i] = ">";
    sep_args[i + 1] = strtok(NULL, ">");
}

int main(int argc, char *argv[]) {
    char *prompt = "anihils-shell> ";  // prompt for interactive mode
    char delims[] = " \t\r\v\f\n";  // white space delimiters

    // 1) Setting mode: interactive or batch
    char mode = -1;
    if (argc == 1) {  // Interactive
        mode = 'i';
    } else if (argc == 2) {  // Batch
        mode = 'b';
    }

    // 2) Setting input stream according to mode
    switch (mode) {
        case 'b' :
            input = fopen(argv[1], "r");  // Opening input file to read
            if (!input) {  // File could not be opened: exiting process
                printerror(1, argv[1]);
                exit(1);
            }
            break;
        case 'i' :
            input = stdin;
            write(1, prompt, (size_t) strlen(prompt));
            break;
        default :  // Invalid number of arguments: exiting process
            printerror(2, NULL);
            exit(1);
    }

    char line[512];
    size_t len = 512;
    // 3) Looping to read from input stream
    while (fgets(line, len, input) != NULL && (!feof(input))) {
        int exe = 1;
        char* args_t[99];
        char *save_ptr = NULL;

        cmdline = strndup(line, (size_t) strlen(line));
        if (mode == 'b') {
            write(1, cmdline, strlen(cmdline));  // Echoing command from file
        }

        // Check for empty command
        char *cmd = strtok(line, delims);  // First arg: command
        if (cmd) {
            // 4) Parse commands and count number of args
            int argnum = 1;
            args_t[0] = strtok_r(cmdline, delims, &save_ptr);
            if (!strcmp(args_t[0], "exit")) {  // command is exit
                free(cmdline);
                break;
            }

            while ((args_t[argnum] = strtok_r(NULL, delims, &save_ptr))) {
                char *r_ptr = strchr(args_t[argnum], '>');
                if (r_ptr && (strlen(args_t[argnum]) > 1)) {
                    sep_arg(args_t[argnum]);
                    for (int i = 0; i < 3; i++) {
                        if (sep_args[i]) {
                            args_t[argnum] = sep_args[i];
                            argnum++;
                        }
                    }
                } else {
                    argnum++;
                }
            }

            // 5) Construct final null-terminated args[] array
            char* args[argnum + 1];
            for (int i = 0; i < argnum; i++) {  // remaining args
                args[i] = args_t[i];
                if (!strcmp(args[i], ">")) {
                    redirect--;  // more than two > make it invalid
                    if (i != (argnum - 2)) {
                        redirect = -1;  // if > is not second last arg
                    }
                }
            }
            args[argnum] = NULL;

            // Conditions to make redirection correctly formatted
            if (!strcmp(args[0], ">") || !strcmp(args[argnum - 1], ">")
                || redirect < 0) {
                printerror(4, NULL);
                exe = 0;
            } else if (!redirect) {  // redirection to file
                fname = args[argnum - 1];
                args[argnum - 2] = NULL;  // >  not to be added to output
            }

            // 6) Execute command
            if (exe) {
                execcmd(args);
            }
        }
        redirect = 1;

        // Check if EOF has been reached
        if (feof(input)) {
            break;
        }

        free(cmdline);
        // 7) Re-starting loop, prompt for interactive mode
        if (mode == 'i') {
            write(STDOUT_FILENO, prompt, (size_t) strlen(prompt));
        }
    }  // End of while loop
    fclose(input);
    return 0;
}
