#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

typedef struct{
    char** argv;
    char* infile;
    char* outfile;
    int valid;
}stage;

char** tokenize(char* command){
    int size = 0;
    char *token = strtok(command, " ");
    char **cmds = NULL; char **temp = NULL;

    while(token != NULL){
        if(size==0){
            cmds = malloc(sizeof(char*));
            if(!cmds){free(cmds); exit(1);}
        }else{
            temp = realloc(cmds, (size+1) * sizeof(char*));
            if(!temp){free(cmds); exit(1);}
            cmds = temp;  
        }
        cmds[size] = token;
        token = strtok(NULL, " ");
        size++;
    }
    temp = realloc(cmds, (size+1) * sizeof(char*));
    if(!temp){free(cmds); exit(1);}
    cmds = temp;
    cmds[size]=NULL;

    return cmds;
}

int validate_syntaxe(char** cmds){
    if(!cmds || cmds[0]==NULL){
        return 0;
    }
    if(strcmp(cmds[0], "|")==0){
        fprintf(stderr, "Syntax error: unexpected token '|' at start of command\n");
        return 0;
    }
    int i=0;
    while(cmds[i]!=NULL){
        if(strcmp(cmds[i], "|")==0){
            if(cmds[i+1]==NULL || strcmp(cmds[i+1], "|")==0){
                fprintf(stderr,"Syntax error: unexpected token '|'\n");
                return 0;
            }
        }
        i++;
    }
    return 1;
}

char*** commandsplit(char** cmds){
    if (!cmds) return NULL;

    char*** commands = NULL;
    int i=0; int j=0;
    while(cmds[j]!=NULL){
        char** cmds2 = NULL;
        int l = 0;
        while(cmds[j]!=NULL && strcmp(cmds[j], "|") != 0){
            if(cmds2 == NULL){
                cmds2 = malloc(sizeof(char*)*(l+1));
                if(!cmds2){free(cmds2);exit(1);}
            }else{
                char** temp2 = realloc(cmds2, sizeof(char*)*(l+1));
                if(!temp2){free(cmds2);exit(1);}
                cmds2 = temp2;
            }
            cmds2[l] = cmds[j];
            j++; l++;
        }
        char** temp2 = realloc(cmds2, sizeof(char*)*(l+1));
        if(!temp2){free(cmds2);exit(1);} 
        cmds2 = temp2;
        cmds2[l] = NULL;

        if(commands == NULL){
            commands = malloc(sizeof(char**)*(i+1));
            if(!commands){free(commands);exit(1);}
        }else{
            char*** temp1 = realloc(commands, sizeof(char**)*(i+1));
            if(!temp1){free(commands);exit(1);}
            commands = temp1;
        }
        commands[i] = cmds2;
        i++;
        if (cmds[j] != NULL && strcmp(cmds[j], "|") == 0) {
            j++;
        }
    }

    char*** temp1 = realloc(commands, (i + 1) * sizeof(char**));
    if (!temp1) { free(commands); exit(1); }
    commands = temp1;
    commands[i] = NULL;

    return commands;
}

stage stage_token(char** token){
    stage argument = {NULL, NULL, NULL, 1};
    int i=0; int j=0;
    char**temp3 = NULL;
    while(token[i] != NULL){
        if(strcmp(token[i], "<")!=0 && strcmp(token[i], ">")!=0){
            temp3 = realloc(argument.argv, sizeof(char*)*(j+1));
            if(!temp3){free(argument.argv);exit(1);}
            argument.argv = temp3;
            argument.argv[j]=token[i];
            j++;
        }
        if(strcmp(token[i], ">") == 0  ||  strcmp(token[i], "<") == 0){
            if(strcmp(token[i], "<")==0){
                if(token[i+1] != NULL){
                    argument.infile = token[i+1];
                }
            }
            if(strcmp(token[i], ">")==0){
                if(token[i+1] != NULL){
                    argument.outfile = token[i+1];
                }
            }
            if(token[i+1] == NULL){
                fprintf(stderr, "Syntax error: unexpected token at end of command\n");
                free(argument.argv);
                argument.argv=NULL;
                argument.valid = 0;
                return argument;
            }
            i++;
        }
        i++;
    }
    temp3 = realloc(argument.argv, sizeof(char*)*(j+1));
    if(!temp3){free(argument.argv);exit(1);}
    argument.argv = temp3;
    argument.argv[j]=NULL;

    return argument;
}

void cleanup_commands(char*** commands) {
    if (!commands) return;
    for (int i = 0; commands[i] != NULL; i++) {
        free(commands[i]);
    }
    free(commands);
}

int main() {
    char command[512]; 
    char user[128];

    printf("\nShell\\enter a username>");
    if (!fgets(user, sizeof(user), stdin)) return 0;
    user[strcspn(user, "\n")] = '\0';

    do {
        do {
            printf("\nShell\\%s>", user);
            if (!fgets(command, sizeof(command), stdin)) break;
            command[strcspn(command, "\n")] = '\0';
        } while (command[0] == '\0');

        if (strcmp(command, "exit") == 0) {
            break;
        }

        char** cmds = tokenize(command);
        if (validate_syntaxe(cmds) == 0) {
            free(cmds);
            continue;
        }

        char*** commands = commandsplit(cmds);
        if (cmds[0] == NULL) {
            free(cmds);
            cleanup_commands(commands);
            continue;
        }

        int num_stages = 0;
        while (commands[num_stages] != NULL) {
            num_stages++;
        }

        int prev_pipe_read = -1;

        for (int i = 0; i < num_stages; i++) {
            stage argument = stage_token(commands[i]);
            if (!argument.valid || argument.argv[0] == NULL) {
                free(argument.argv);
                continue;
            }
            if (strcmp(argument.argv[0], "cd") == 0) {
                if (argument.argv[1] != NULL) {
                    if (chdir(argument.argv[1]) < 0) perror("cd");
                } else {
                    char* home = getenv("HOME");
                    if (home != NULL && chdir(home) < 0) perror("cd");
                }
                free(argument.argv);
                continue;
            }

            int pipefd[2];
            
            if (i < num_stages - 1) {
                if (pipe(pipefd) < 0) {
                    perror("pipe");
                    free(argument.argv);
                    break;
                }
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                free(argument.argv);
                break;
            }

            if (pid == 0) { 
                if (i > 0) {
                    dup2(prev_pipe_read, STDIN_FILENO);
                    close(prev_pipe_read);
                }
                if (i < num_stages - 1) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }
                if (argument.infile != NULL) {
                    int fd = open(argument.infile, O_RDONLY);
                    if (fd == -1) { perror("open(infile)"); exit(1); }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (argument.outfile != NULL) {
                    int fd = open(argument.outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) { perror("open(outfile)"); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                execvp(argument.argv[0], argument.argv);
                perror("execvp");
                exit(1);
            }

            if (i > 0) {
                close(prev_pipe_read);
            }
            if (i < num_stages - 1) {
                close(pipefd[1]); 
                prev_pipe_read = pipefd[0];
            }

            free(argument.argv);
        }
        while (wait(NULL) > 0);

        free(cmds);
        cleanup_commands(commands);

    } while (1);

    return 0;
}