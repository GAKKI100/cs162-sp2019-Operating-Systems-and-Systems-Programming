#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);



/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "cdw", "print the current working directory to standard output"},
  {cmd_cd, "cd", "take one argument, a directory path, and changes \
the current working directory to that directory"}, 
  {cmd_wait, "wait", "waits until all background jobs have terminated before returning to the prompt"}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Print the current working directory*/
int cmd_pwd(unused struct tokens *tokens){
    char dir[PATH_MAX];
    if(getcwd(dir, PATH_MAX)){
        printf("cwd: %s\n", dir);
    }else{
        perror("cmd_pwd erreo!");
    }
    return 1;
}

/* Change working directory to given directory*/
int cmd_cd(unused struct tokens *tokens){
  if(tokens_get_length(tokens) == 1){
      return 0;
  }
  if(chdir(tokens_get_token(tokens, 1))){
     perror("cannot change dictionary\n");
     return 0; 
  }
  return 1;
}

int back_process = 0;

/*waits until all background jobs have terminated*/
int cmd_wait(unused struct tokens *tokens){
    for(int i = 0; i < back_process; i++){
        wait(NULL);
    }
    return 1;
}


/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);
  
  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);
   
    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));
    

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
        /* implement the BACKGROUND function*/
        int background = 0;
        size_t len = tokens_get_length(tokens);

        if(len > 1){
            if(!strcmp(tokens_get_token(tokens, len - 1), "&")){
                background = 1;
                back_process += 1;
                len -= 1;
            }
        }
        /* shell in the backgound should ignore the signal*/
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGKILL, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGCONT, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        pid_t pid = fork();
        if (pid < 0){   
            perror("error in fork!");  
        } else if(pid > 0){
            int status = -1;
            if(!background){
                wait(&status);
            }
        }else{
            /* process group*/
            setpgrp();

            if(!background){
                /*foreground subprocesses take control*/
                tcsetpgrp(shell_terminal, getpid());    
                /* foreground subprocesses should respond to the signal*/
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGKILL, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGCONT, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
            }
        
            int redi = 0;
            int fd;

            if(len > 2){
                /* io redirection*/
                if(!strcmp(tokens_get_token(tokens, len - 2), "<")){
                    redi = 1;
                    fd = open(tokens_get_token(tokens, len - 1), O_CREAT|O_TRUNC|O_RDONLY, 0644);
                    if (fd < 0) {
                        perror("wrong path");
                        exit(0);
                    }
                    if(dup2(fd, 0) == -1){
                        perror("error in dup2");
                        exit(0);
                    }
                    close(fd);
                }else if(!strcmp(tokens_get_token(tokens, len - 2), ">")){
                    redi = 1;
                    fd = open(tokens_get_token(tokens, len - 1),O_CREAT|O_TRUNC|O_WRONLY, 0644);
                    if (fd < 0) {
                        perror("wrong path");
                        exit(0);
                    }
                    if(dup2(fd, 1) == -1){
                        perror("error in dup2");
                    }
                    close(fd);
                }
            }            

            if(redi){
                len -= 2;
            }
            char *args[len + 1];
            for(size_t i = 0; i < len; i++){
                args[i] = tokens_get_token(tokens, i);
            }
            args[len] = NULL;
            
            char *env = getenv("PATH");
            char cat_env[1024];
            char *parse = env;
            char *end = env;
            while (parse) {
                strsep(&end, ":");
                strcpy(cat_env, parse);
	        strcat(cat_env, "/");
                strcat(cat_env, tokens_get_token(tokens, 0));
                execv(cat_env, args);
                memset(&cat_env, 0, sizeof(cat_env));
                parse = end;
            }

            if((execv(tokens_get_token(tokens, 0), args)) == -1){
                fprintf(stderr, "cannot found the command\n");
            }
            exit(1);
        }
    }
    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    
    /* shell in the foregound should respone to the signal*/
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    
    fflush(stdout);
    fflush(stderr);
    fflush(stdin);
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }
   
  return 0;
}
