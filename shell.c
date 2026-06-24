#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(){

    char command[512]; char user[128];

    printf("\nShell\\enter a username>");
    fgets(user, sizeof(user), stdin);
    user[strcspn(user, "\n")] = '\0';

    do{
        printf("\nShell\\%s>", user);
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if(strcmp(command, "exit") == 0){
            break;
        }
            
        int size = 0;
        char *token = strtok(command, " ");
        char **cmds = NULL;
        char **temp = NULL;

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
        cmds[size] = NULL; 
    
        pid_t pid = fork();

        if(pid<0){free(cmds); exit(1);}
        if(pid==0){
            execvp(cmds[0], cmds);
            perror("execvp failed");
            exit(1);
        }
        else{
            wait(NULL);
            free(cmds);
        }

    }while(1);

    return 0;
}

