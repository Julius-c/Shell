#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAXN 100
#define FLAGS O_CREAT | O_RDWR | O_TRUNC
#define MODE S_IRWXU | S_IXGRP | S_IROTH | S_IXOTH
int I_BGD = 0; //Background Use
int NoHist = 1; //History Use
typedef struct node {
    int num;
    char hist[1024];
    struct node *next;
    struct node *prev;
} NODE;
NODE *head = NULL;
char buff[1024]; // Stdin Buff

//Built-in Commands
static int cmd_help(char **args);
static int cmd_exit(char **args);
static int cmd_cd(char **args);
static int cmd_history(char **args);
//Special
static int cmd_exec_former(char *buff);
static int cmd_find_and_exec(char *buff);

static struct {
    char *name;
    char *discription;
    int (*handler) (char **);
} cmd_table [] = {
    {"help", "Display information about all built-in commands", cmd_help},
    {"exit", "Free resources and Exit", cmd_exit},
    {"cd", "Change directory", cmd_cd},
    {"history", "Simplified bash history", cmd_history},
    {"!!", "Execute former command", NULL},
    {"![String]", "Execute former [String]-start command", NULL}
};
const int NR_CMD = (sizeof(cmd_table) / sizeof(cmd_table[0]));

static int cmd_help(char **args) {
    char *arg = args[1];
    int i;
    if(arg == NULL) {
        for(i = 0; i < NR_CMD; i ++)
            printf("\033[1;36m%s\033[0m - %s\n", cmd_table[i].name, cmd_table[i].discription);
        return 0;
    }else {
        for(i = 0; i < NR_CMD; i ++) {
            if(strcmp(arg, cmd_table[i].name) == 0) {
                printf("\033[1;36m%s\033[0m - %s\n", cmd_table[i].name, cmd_table[i].discription);
                return 0;
            }
        }
        printf("Not built-in command '%s'\n", arg);
        return 0;
    }
    return -1;
}

static int cmd_exit(char **args) {
    NODE *l = head;
    while(l->next){
        head = l->next;
        free(l);
        l = head;
    }
    free(head);
    exit(EXIT_SUCCESS);
}

static int cmd_cd(char **args) {
    char *arg = args[1];
    if(arg == NULL) {
        printf("\033[1mUsage: cd [dir]\033\[0m\n");
        return 0;
    }else {
        if(chdir(arg) == -1) {
            perror("chdir()");
            return -1;
        }else
            return 0;
    }
    return -1;
}

static int cmd_history(char **args) {
    char *arg = args[1];
    int number = 100;
    if(arg == NULL) {
        NODE *l = head;
        while(number -- > 1) {
            if(l->next->next) l = l->next;
            else break;
        }
        while(l) {
            printf("%d %s\n", l->num, l->hist);
            l = l->prev;
        }
       return 0;
    }else if(strcmp(arg, "-c") == 0) {
        NoHist = 1;
        NODE *l = head;
        while(l->next){
            head = l->next;
            free(l);
            l = head;
        }
        return 0;
    }else if(sscanf(arg, "%d", &number) == 1) {
        NODE *l = head;
        while(number -- > 1) {
            if(l->next->next) l = l->next;
            else break;
        }
        while(l) {
            printf("%d %s\n", l->num, l->hist);
            l = l->prev;
        }
        return 0;
    }else {
        printf("\033[1mUsage: history [-c] [number]\033\[0m\n");
        return 0;
    }
    return -1;
}
//!!
static int cmd_exec_former(char *buff) {
    char *pos;
    while( (pos = strstr(buff, "!!")) != NULL ) {
        NODE *temp = head;
        if(temp->next == NULL) {
            printf("No previous effective command.\n");
            return -1;
        }
        while(temp != NULL) {
            if(strcmp(temp->hist, "!!") != 0) {
                break;
            }else {
                temp = temp->next;
                if(temp == NULL) {
                    printf("No previous effective command.\n");
                    return -1;
                }
            }
        }
        char *new_buff = (char *)malloc(sizeof(char) * 1024);
        strncpy(new_buff, buff, pos - buff);
        if(pos + 3 - buff <= strlen(buff))
            sprintf(new_buff + (pos - buff), "%s %s", temp->hist, pos + 3);
        else
            sprintf(new_buff + (pos - buff), "%s", temp->hist);
        strcpy(buff, new_buff);
        free(new_buff);
    }
    return 0;
}
//!String
static int cmd_find_and_exec(char *buff) {
    char fstr[32];
    char fcmd[32];
    while(sscanf(buff, "%*[^!]!%[^ ]", fstr) >= 1
        || sscanf(buff, "!%s", fstr) >= 1) {
        NODE *l = head;
        if(l->next == NULL) {
            printf("No previous effective command.\n");
            return -1;
        }
        while(l){
            if(sscanf(l->hist, "%s", fcmd) == 1
                && strcmp(fcmd, fstr) == 0) break;
            else {
                l = l->next;
                if(l == NULL) {
                    printf("No previous effective command.\n");
                    return -1;
                }
            }
        }
        char *new_fstr = (char *)malloc(sizeof(char) * 64);
        sprintf(new_fstr, "!%s", fstr);
        char *pos = strstr(buff, new_fstr);

        char *new_buff = (char *)malloc(sizeof(char) * 1024);
        strncpy(new_buff, buff, pos - buff);
        if(pos + strlen(new_fstr) + 1 - buff <= strlen(buff))
           sprintf(new_buff + (pos - buff), "%s %s", l->hist, pos + strlen(new_fstr) + 1);
        else
            sprintf(new_buff + (pos - buff), "%s", l->hist);
        strcpy(buff, new_buff);

        free(new_buff);
        free(new_fstr);    
    }
    return 0;
}

static int redirect(char *options[][MAXN], int index) {
    int n = 0;
    while(options[index][n] != NULL) {
        if(strcmp(options[index][n], ">") == 0
            && options[index][n + 2] == NULL) {
            options[index][n] = NULL;
            return open(options[index][n + 1], FLAGS, MODE);
        }
        n ++;
    }
    return -1;
}

static int _if_built_in(char *token) {
    int i = 0;
    for(i = 0; i < NR_CMD; i ++) {
        if(strcmp(token, cmd_table[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static void execute(int index, int curp, int redfd, 
             int fd[][2], char *options[][MAXN], 
             pid_t *cpid) {
    if(index == 0) {
        if((redfd = redirect(options, index)) != -1) {
            close(1);
            dup2(redfd, 1);
        }
        if(curp > 0) {
            if(redfd == -1) {
                close(1);
                dup2(fd[index][1], 1);
            }
            for(int i = 0; i < curp; i ++)
                close(fd[i][0]), close(fd[i][1]);
        }
        int id = _if_built_in(options[index][0]);
        if(id != -1){
            assert(cmd_table[id].handler(options[index]) != -1);
        }else {
            execvp(options[index][0], options[index]);
            perror("exec()");
            exit(EXIT_FAILURE);
        }
    } else if(index >= 1 && index < curp) {
        close(0), close(1);
        if((redfd = redirect(options, index)) != -1) {
            dup2(redfd, 1);
        }else {
            dup2(fd[index][1], 1);
        }
        dup2(fd[index - 1][0], 0);
        for(int i = 0; i < curp; i ++)
            close(fd[i][1]), close(fd[i][0]);
        int id = _if_built_in(options[index][0]);
        if(id != -1){
            assert(cmd_table[id].handler(options[index]) != -1);
        }else {
            execvp(options[index][0], options[index]);
            perror("exec()");
            exit(EXIT_FAILURE);
        }
    } else if(index == curp && curp > 0) {
        if((redfd = redirect(options, index)) != -1) {
            close(1);
            dup2(redfd, 1);
        }
        close(0);
        dup2(fd[curp - 1][0], 0);
        for(int i = 0; i < curp; i ++)
            close(fd[i][0]), close(fd[i][1]);
        int id = _if_built_in(options[index][0]);
        if(id != -1){
            assert(cmd_table[id].handler(options[index]) != -1);
        }else {
            execvp(options[index][0], options[index]);
            perror("exec()");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[], char *envp[]) {
    if(argc > 1) {
        printf("\033[1;35mUsage: ./njush\033\[0m\n");
        exit(EXIT_SUCCESS);
    }
    printf("\033[1;35mWelcome to NJUSH 1.0.0! \
           \nType \"exit\" to exit.\033\[0m\n");

    head = (NODE *)malloc(sizeof(NODE));
    head->next = NULL;
    head->prev = NULL;
    while(1) {
        printf("\033[1;31mNJUSH\033\[0m:\033[1;33m~$ \033\[0m");

        assert(fgets(buff, sizeof(buff), stdin) != NULL);
        buff[strlen(buff) - 1] = '\0';
        if(strlen(buff) == 0) continue;

        if(strcmp(buff, "exit") == 0) break;
        //Replace !! and !String
        char tstr[32];
        if( strstr(buff, "!!") != NULL 
            || sscanf(buff, "%*[^!]!%[^ ]", tstr) >= 1
            || sscanf(buff, "!%s", tstr) >= 1) {
            if(cmd_exec_former(buff) == -1) continue;
            if(cmd_find_and_exec(buff) == -1) continue;
            printf("%s\n", buff);
        }
        //Background
        I_BGD = 0;
        if(buff[strlen(buff) - 1] == '&' && strlen(buff) > 1) {
            buff[strlen(buff) - 1] = '\0';
            I_BGD = 1;
        }
        //Store in History
        if(strcmp(head->hist, buff) != 0) {
            NODE *new = (NODE *)malloc(sizeof(NODE));
            strcpy(new->hist, buff);
            new->num = NoHist ++;
            new->next = head;
            new->prev = NULL;
            head->prev = new;
            head = new;
        }
        //Analyze Input Command
        char *options[MAXN][MAXN]; //Divide Commands
        int pip[MAXN][2]; //Pipe Array
        pid_t cpid[MAXN]; //Child Pid Array

        char *token = strtok(buff, " ");
        int curo = 0; // current option
        int curp = 0; // current pipe
        while(token) {
            if(strcmp(token, "|") == 0){
                options[curp][curo] = NULL;
                curo = 0;
                curp ++;
            } else {
                options[curp][curo] = (char *)malloc(sizeof(char) * 128);
                strcpy(options[curp][curo], token);
                curo ++;
                if(strcmp(token, "ls") == 0) {
                    options[curp][curo] = (char *)malloc(sizeof(char) * 128);
                    strcpy(options[curp][curo], "--color=auto");
                    curo ++;
                }
            }
            token = strtok(NULL, " ");
        }
        options[curp][curo] = NULL;

        int id;
        if(curp == 0 && (id = _if_built_in(options[0][0])) != -1) {
            assert(cmd_table[id].handler(options[0]) != -1);
            continue;
        }

        for(int i = 0; i < curp; i ++)
            assert(pipe(pip[i]) == 0);

        //Execute
        int index; //Process Index
        pid_t pid; //Fork Use
        int redfd = -1; //Fd After Redirection
 
        for(index = 0; index <= curp; index ++) {
            if(_if_built_in(options[index][0]) != -1) {
                pid = fork();
                if(pid == 0) {
                    execute(index, curp, redfd, pip, options, cpid);
                    exit(EXIT_SUCCESS);
                }else {
                    waitpid(pid, NULL, 0);
                    continue;
                }
            }
            pid = fork();
            if(pid > 0) cpid[index] = pid; //Store to wait
            else if(pid == 0) break; //Exec child process
            else if(pid < 0) {
                perror("fork()");
                exit(EXIT_FAILURE);
            }
        }
        execute(index, curp, redfd, pip, options, cpid);
        if(index > curp) {
            for(int i = 0; i < curp; i ++)
                close(pip[i][0]), close(pip[i][1]);
            if(!I_BGD)
                for(int i = 0; i <= curp; i ++)
                    waitpid(cpid[i], NULL, 0);
        }
    }
    return 0;
}
