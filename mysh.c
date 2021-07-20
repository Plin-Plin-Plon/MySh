/* Sistemas Operacionais II - Projeto 1 - Integral */
/* Implementando um processador de comandos do Linux em C */

/********************************************************************************/
/*                                                                              */
/*   O Programa têm como objetivo simular um processador de comandos (Shell)    */ 
/*    do Linux em C, de forma que respeite as exigências dadas no documento     */
/*    disponibilizado na plataforma Google Classroom pelo docente.              */
/*                                                                              */
/********************************************************************************/

/* Grupo:
/*  Christian Camilo */
/*  Matheus Missio */
/*  Paulo César */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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
int argc;

struct pipeCmd {
    char **argv;
};

char* getHostName();
char* getWorkingDirectory();
char* getUserName();
void signalHandler(int);
void formatPath();
void typePrompt(char**);
int readCommand(char***, char*);
char* parseQuotedCmd(char*, int*, int, char);
int checkCdDestination(char**);
int cdCommand(char*);
int isReserved(char**);
int executeProcess(char**);
int countPipes(char**);
struct pipeCmd* createPipeArgs(char**, int);
int executePipeline(char**, int);
void freePipeCmd(struct pipeCmd*, int);


/*  Função que retorna o hostName da máquina em questão

    Caso a função não consiga obter o hostName, é retornado uma mensagem de erro
    e o programa é abortado com código 1.
    A ideia é que a shell só consiga de fato executar se houver a obtenção do hostName corretamente
*/
char* getHostName() {
    char *hostname = (char*) calloc(MAX_LENGHT, sizeof(char));
    
    if (gethostname(hostname, MAX_LENGHT) != -1) {
        return hostname;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}


/*  Função que retorna o diretório atual do espaço de trabalho 

    Caso a função não consiga obter o cwd, é retornado uma mensagem de erro
    e o programa é abortado com código 1.
    A ideia é que a shell só consiga de fato executar se houver a obtenção do cwd corretamente
*/
char* getWorkingDirectory() {
    char *cwd = (char*) calloc(PATH_MAX, sizeof(char));
    
    if (getcwd(cwd, PATH_MAX) != NULL) {
        return cwd;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}


/*  Função que retorna o nome do usuário 

    Caso a função não consiga obter o nome do 
    usuário, é exibida uma mensagem de erro e o programa
    aborta com o código 1.
*/
char* getUserName() {
    char *username = (char*) calloc(MAX_LENGHT, sizeof(char));
    
    if ((username = getenv("USER")) != NULL) {
        return username;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
}


/*  Função que trata o sinal de ctrl+C, ctrl+Z e o sinal de controle dos processos filhos 

    Caso receba o sinal de um processo filho 
    espera o término do processo e sai da função.
    Caso receba um sinal do comando ctrl+C ou ctrl+Z
    ignora a linha de comando.
*/
void signalHandler(int signal) {
    int childExitStatus;

    switch(signal) {
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


/*  Formata o caminho da shell

    Formata o caminho levando em consideração
    se está na pasta root do sistema ou pastas pessoais.
*/
void formatPath() {
    cwd = getWorkingDirectory();
    hostname = getHostName();
    username = getUserName();
    char *cwdAux = (char*) calloc(PATH_MAX, sizeof(char));
    char *personalDir = (char*) calloc(PATH_MAX, sizeof(char));

    strcpy(cwdAux, cwd);
    strcpy(personalDir, "/home/");
    strcat(personalDir, username);

    char *str = strstr(cwdAux, personalDir);
    
    if (str != NULL) {
        strcpy(cwd, &str[strlen(personalDir)]);
        sprintf(myshPath, "\033[1;31m[MySh] \033[1;32m%s@%s\033[0m:\033[1;34m~%s\033[0m$ ", username, hostname, cwd);
    } else {
        sprintf(myshPath, "\033[1;31m[MySh] \033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", username, hostname, cwd);
    }
    
    free(cwdAux);
}


/*  Printa o path atual e lê a entrada do usuário   */
void typePrompt(char **cmd) {
    printf("%s", myshPath);
    char *input = (char*) calloc(ARG_MAX, sizeof(char));

    fgets(input, ARG_MAX, stdin);
    input[strlen(input)-1] = '\0';
    fflush(stdin);

    *cmd = input;
}


/*  Função que cria a lista de argumentos

    Lê a entrada do usuário e cria a lista 
    de argumentos correspondente baseado do delimitador " ".
*/
int readCommand(char ***argv, char *cmd) {
    int i = 0, pos, tam = strlen(cmd);
    char *token, delim[6] = " \'\"", singleQuote[2] = "\'", doubleQuote[2] = "\"";
    char *cmdAux = (char*) calloc(ARG_MAX, sizeof(char));
    ptrdiff_t diff;

    char **argList = (char**) calloc(1, sizeof(char*));
    strcpy(cmdAux, cmd);
    token = strtok(cmd, delim);

    while (token != NULL) {
        if (strcmp(token, delim)) {
            argList[i] = (char*) calloc(MAX_LENGHT, sizeof(char));

            diff = token - cmd; // Calcula o índice no comando do usuário pelo token do strtok
            pos = diff;

            // Analisa a posição de trás, pois o strtok coloca o token após as possíveis aspas
            if (cmd[pos-1] == singleQuote[0]) { 
                char* quotedCmd = parseQuotedCmd(cmdAux, &pos, tam, singleQuote[0]);
                
                strcpy(argList[i++], quotedCmd);
                strcpy(cmd, &cmdAux[pos]);
                tam = strlen(cmd);
                strtok(cmd, delim); // Recomeça o processo de strtok
            } else if (cmd[pos-1] == doubleQuote[0]) {
                char* quotedCmd = parseQuotedCmd(cmdAux, &pos, tam, doubleQuote[0]);

                strcpy(argList[i++], quotedCmd);
                strcpy(cmd, &cmdAux[pos]);
                tam = strlen(cmd);
                strtok(cmd, delim); // Recomeça o processo de strtok
            } else {
                strcpy(argList[i++], token);
            }
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

/*  Função que pega e retorna todo o conteúdo do comando 
    que está entre as aspas duplas ou simples

    Pega caracter por caracter e salva em uma string resultante
*/
char* parseQuotedCmd(char* cmd, int *pos, int len, char typeQuote) {
    char *quotedCmd = (char*) calloc(ARG_MAX, sizeof(char));
    char ch;
    int k = 0;

    while ((ch = cmd[*pos]) != typeQuote && *pos < len) {
        quotedCmd[k] = cmd[*pos];
        k++;
        *pos = *pos + 1;
    }

    quotedCmd[k] = '\0';

    return quotedCmd;
}

/*  Função que trata os diferentes destinos para o comando cd 

    Caso seja "cd" ou "cd ~" o programa é redirecionado para
    a home da pasta pessoal.
    Caso seja "cd /" o programa é redirecionado para o root.
    E para outros casos o comando cd segue sua execução normal.
*/
int checkCdDestination(char **argv) {
    int code;

    if (argv[1] == NULL || !strcmp(argv[1], "~")) {
        code = cdCommand(getenv("HOME"));
    } else if (!strcmp(argv[1], "/")) {
        code = cdCommand(argv[1]);
    } else {
        code = cdCommand(argv[1]);
    }

    return code;
}


/*  Função que executa o comando "cd" e formata o path da shell novamente   */
int cdCommand(char *dest) {
    if (chdir(dest) == 0) {
        formatPath();
        return 0;
    } else {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }
}


/*  Função que verifica se o comando inserido pelo usuário é uma palavra reservada

    Caso seja lido "exit" a função retorna 1 para que a função
    executeProcess retorne 1 e consequentemente finalize o programa.
    Caso seja lido "cd" a função chama a checkCdDestination e retorna 2.
*/
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


/*  Função que executa os comandos inseridos

    Caso o comando inserido não seja um palavra
    reservada, é criado um processo filho e o
    comando é executado.
*/
int executeProcess(char **argv) {
    int code;

    if ((code = isReserved(argv)) == 2) {
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


/*  Função que conta os pipes

    Retorna a quantidade de pipes na 
    linha de comando.
    Caso depois de um pipe não exista um 
    argumento é retornado o valor -1 e sinalizado
    como erro.
*/
int countPipes(char **argv) {
    int i, qtdPipes = 0;

    for(i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "|")) {
            qtdPipes++;
            if (argv[i+1] == NULL) {
                fprintf(stderr, "Error: Empty pipe arguments\n");
                return -1;
            }
        }
    }

    return qtdPipes;
}


/*  Função que cria um array de structs de pipes

    Cada struct armazena uma argList diferente referente
    ao pipe que pertence.
*/
struct pipeCmd* createPipeArgs(char **argv, int qtdPipes) {
    int i = 0, j = 0, k;
    struct pipeCmd *pCmd = (struct pipeCmd*) calloc(qtdPipes + 1, sizeof(struct pipeCmd));
    
    for (i = 0; i < qtdPipes + 1; i++) {
        pCmd[i].argv = (char**) calloc(1, sizeof(char*));
    }

    for (i = 0; i < qtdPipes + 1; i++) {
        k = 0;
        while (argv[j] != NULL && strcmp(argv[j], "|")) {
            pCmd[i].argv[k] = (char*) calloc(MAX_LENGHT, sizeof(char));
            strcpy(pCmd[i].argv[k], argv[j]);
            k++;
            pCmd[i].argv = (char**) realloc(pCmd[i].argv, sizeof(char*) * (k + 1));
            j++;
        }
        pCmd[i].argv[k] = NULL;
        j++;
    }

    return pCmd;
}


/*  Função que cria e executa os comandos com multiplos pipes

    Cada comando vai ser executado e seu resultado
    repassado para o próximo pipe. O último pipe
    ´~e o responsavel por escrever a saída na shell.
*/
int executePipeline(char **argv, int qtdPipes) {
    int i, j, k, pipeFds[qtdPipes][2];
    pid_t child_pid;
    struct pipeCmd *pCmd = createPipeArgs(argv, qtdPipes);

    // Criando todos os pipes
    for (i = 0; i < qtdPipes; i++) {
        if (pipe(pipeFds[i])) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            exit(1);
        }
    }

    for (i = 0; i < qtdPipes + 1; i++) {
        if((child_pid = fork()) < 0) {
            printf("Unable to fork");
            return 1;
        }

        if (child_pid == 0) {

            // Abre a saída de todos os pipes, menos o último
            if (i < qtdPipes) {
                if (dup2(pipeFds[i][1], STDOUT_FILENO) < 0) {
                    perror("dup21");
                    exit(1);
                }
            }
            
            // Abre a entrada de todos os pipes menos o primeiro
            if (i != 0) {
                if (dup2(pipeFds[i-1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }

            // Fecha todos os pipes do processo filho em questão
            for (j = 0; j < qtdPipes; j++) {
                for (k = 0; k < 2; k++) {
                    close(pipeFds[j][k]);
                }
            }

            // Executa o comando
            if(execvp(pCmd[i].argv[0], pCmd[i].argv) < 0) {
                fprintf(stderr, "Error: %s\n", strerror(errno));
                exit(1);
            }
        }
    }

    // Fechando todos os pipes do processo pai
    for (i = 0; i < qtdPipes; i++) {
        for (j = 0; j < 2; j++) {
            close(pipeFds[i][j]);
        }
    }
  
    // Espera pelos filhos
    for (i = 0; i < qtdPipes + 1; i++) {
        wait(&status);
    }

    freePipeCmd(pCmd, qtdPipes);

    return 0;
}


/*  Função que libera a memória do array de pipes e a lista de argumentos correspondente    */
void freePipeCmd(struct pipeCmd* pCmd, int qtdPipes) {
    int i, j;

    for (i = 0; i < qtdPipes; i++) {
        j = 0;
        while (pCmd[i].argv[j] != NULL) {
            free(pCmd[i].argv[j]);
            j++;
        }
        free(pCmd[i].argv);
    }

    free(pCmd);
}


int main() {
    int i, exit = 0;
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
            int qtdPipes;
            readCommand(&argv, cmd);

            if (argv[0] != NULL) {
                if ((qtdPipes = countPipes(argv)) == 0) {
                    exit = executeProcess(argv);
                } else if (qtdPipes < 0) {
                    exit = 0;
                } else {
                    exit = executePipeline(argv, qtdPipes);
                }
            }

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
