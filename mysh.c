#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#define MAX_LENGHT 255
#define ARG_MAX 1024

sig_atomic_t status;
char *cwd, *username, *hostname, myshPath[PATH_MAX];
int argc, root = 0;

char* getHostName();
char* getWorkingDirectory();
char* getUserName();
void signalHandler(int);
void formatPath();
void typePrompt(char**);
int readCommand(char***, char*);
int checkCdDestination(char**);
int cdCommand(char*);
int isReserved(char**);
int executeProcess(char**);


char* getHostName() {
    char *hostname = (char*) calloc(MAX_LENGHT, sizeof(char));
    
    if (gethostname(hostname, MAX_LENGHT) != -1) {
        return hostname;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}

char* getWorkingDirectory() {
    char *cwd = (char*) calloc(PATH_MAX, sizeof(char));
    
    if (getcwd(cwd, PATH_MAX) != NULL) {
        return cwd;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}

char* getUserName() {
    char *username = (char*) calloc(MAX_LENGHT, sizeof(char));
    
    if ((username = getenv("USER")) != NULL) {
        return username;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}

void signalHandler(int signal) {
    int childExitStatus;

    switch (signal) {
        case SIGCHLD:
            wait(&childExitStatus);
            status = childExitStatus;
            break;
        case SIGINT: // Trata o sinal de CTRL C
            printf("\n");
            break;
        case SIGTSTP: // Trata o sinal de CTRL Z
            printf("\n");
            break;
    }
}

void formatPath() {
    cwd = getWorkingDirectory();
    hostname = getHostName();
    username = getUserName();
    char *cwdAux;

    if (!root) {
        cwdAux = (char*) calloc(PATH_MAX, sizeof(char));

        strcpy(cwdAux, cwd);
        char *str = strstr(cwdAux, username);
        strcpy(cwd, &str[strlen(username)]);
        sprintf(myshPath, "\033[1;31m[MySh] \033[1;32m%s@%s\033[0m:\033[1;34m~%s\033[0m$ ", username, hostname, cwd);
        
        free(cwdAux);
    } else {
        sprintf(myshPath, "\033[1;31m[MySh] \033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", username, hostname, cwd);
    }
}

void typePrompt(char **cmd) {
    printf("%s", myshPath);
    char *input = (char*) calloc(ARG_MAX, sizeof(char));

    fgets(input, ARG_MAX, stdin);
    input[strlen(input)-1] = '\0';
    fflush(stdin);

    *cmd = input;
}

int readCommand(char ***argv, char *cmd) {
    int i = 0;
    char *token, delim[2] = " ";

    char **argList = (char**) calloc(1, sizeof(char*));
    token = strtok(cmd, delim);

    while (token != NULL) {
        if (strcmp(token, delim)) {
            argList[i] = (char*) calloc(MAX_LENGHT, sizeof(char));
            strcpy(argList[i++], token);
            argList = (char**) realloc(argList, sizeof(char*) * (i + 1));
        }
        token = strtok(NULL, delim);
    }

    argc = i;
    argList[i] = NULL;
    *argv = argList;
    free(token);

    return 0;
}

int checkCdDestination(char **argv) {
    int code;

    if (argv[1] == NULL || !strcmp(argv[1], "~")) {
        root = 0;
        code = cdCommand(getenv("HOME"));
    } else if (!strcmp(argv[1], "/")) {
        root = 1;
        code = cdCommand(argv[1]);
    } else {
        code = cdCommand(argv[1]);
    }

    return code;
}

int cdCommand(char *dest) {
    if (chdir(dest) == 0) {
        formatPath();
        return 0;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }
}

int isReserved(char **argv) {
    if (!strcmp(argv[0], "exit")) {
       return 1;
    } else if (!strcmp(argv[0], "cd")) {
        checkCdDestination(argv);
        return 2;
    } else {
        return 0;
    }
}

int executeProcess(char **argv) {
    int code;

    if (argv[0] == NULL || (code = isReserved(argv)) == 2) {
        return 0;
    } else if (code == 1) {
        return 1;
    }

    pid_t child_pid;

    if((child_pid = fork()) < 0) {
        printf("Unable to fork");
        return 1;
    }

    if(child_pid != 0) {
        wait(&status);
    } else {
        if(execvp(argv[0], argv) < 0) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            exit(1);
        }
        exit(0);
    }
    
    return 0;
}

int main() {
    int i, exit;
    struct sigaction sig;
    
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = &signalHandler;
    sigaction(SIGCHLD, &sig, NULL);
    sigaction(SIGINT, &sig, NULL);
    sigaction(SIGTSTP, &sig, NULL);

    formatPath();

    do {
        char **argv, *cmd;
        argc = 0;
        typePrompt(&cmd);
        
        if (feof(stdin)) { // Trata a entrada de um CTRL D
            exit = 1;
            printf("\n");
        } else {
            readCommand(&argv, cmd);
            exit = executeProcess(argv);
            for (i = 0; i < argc; i++) {
                free(argv[i]);
            }
            free(argv);
        }

        free(cmd);
    } while (!exit);

    free(cwd);
    free(hostname);

    return 0;
}