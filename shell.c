#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include "utils.h"
#include "builtin.h"
#include "shell.h"

// Builds path-list from PATH
struct list* build_path()
{
  char* p = getenv("PATH");
  char* path = malloc(strlen(p)+1);
  if (path == NULL)
    return NULL;
  strcpy(path,p);
  
  struct list* list = NULL;
  p = strtok(path, ":");
  do {
    // Can't free strtok tokens
    // Malloc each one to make freeing easier later
    char* temp = malloc(strlen(p)+1);
    strcpy(temp,p);
    list = list_insert(list, temp); 
  } while ((p = strtok(NULL, ":")));
 
  free(path);
  return list;
}

/*
 * Setters for the shell_context struct
 * */
int update_prompt(struct shell_context* context, char* c)
{
  free(context->prompt);
  context->prompt = c;
  return 0;
}

int update_path(struct shell_context* context)
{
  list_free(context->path);
  context->path = build_path();
  return 0;
}

int update_current_dir(struct shell_context* context, char* c)
{
  free(context->previous_dir);
  context->previous_dir = context->current_dir;
  context->current_dir = c;
  setenv("PWD", c, 1);
  return 0;
}

void shell_context_free(struct shell_context* context)
{
  free(context->prompt);
  free(context->previous_dir);
  free(context->current_dir);
  list_free(context->path);
  free(context);
}

struct shell_context* init_shell()
{
  struct shell_context* context = malloc(sizeof(*context));
  context->prompt = malloc(1);
  context->prompt[0] = '\0';
  context->current_dir = getcwd(NULL,0);
  context->previous_dir = getcwd(NULL,0);
  context->path = build_path();
  context->loop = 1;
  return context;
}

//Checks if a command is valid
//(in PATH, is a relative/absolute path, permissions, etc)
int find_command(char* command, struct list* path)
{
  
  if (is_in_path(command, path) != NULL)
    return 0;

  char* fullpath;
  int f = 1;
  //is a path
  if (strstr(command, "/") != NULL) {
    //get the full path
    if ((fullpath = realpath(command,NULL)) != NULL) { 
      struct stat stats;
      //isn't a directory
      if (stat(fullpath, &stats) == 0 && !(S_ISDIR(stats.st_mode)))
        if (access(fullpath, X_OK) == 0)
          f = 0;
    }
    free(fullpath);
  }
  return f;
}

/*
 * Signal handlers for shell timeout
 * */
// Im not a fan of global variables
// But signals are basically the most global thing you can imagine ._.
int CHILD_PID = -1;

void kill_child(int sig)
{
  if (CHILD_PID < 1) {
    return;
  }
  else {
    printf("Terminating [%d] by timeout\n", CHILD_PID);
    kill(CHILD_PID, SIGTERM);
  
  }
}

int alarm_child(int pid, int timer)
{
  if (timer < 1) {
    return 0;
  }
  CHILD_PID = pid;
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = kill_child;
  

  sigaction(SIGALRM, &act, 0);
  alarm(timer);
  return 0;
}

int sh(int argc, char** argv, char** envp, int timer)
{
  struct shell_context* context = init_shell();
  
  while(context->loop) {
    char* commandline = NULL;
    wordexp_t s_wordexp;
    int status, process = 0;

    /*
     * Prompt
     * */
    printf("%s [%s] ", context->prompt, context->current_dir);
    
    /*
     * Parse
     * */
    if ((commandline = read_input()) == NULL) {
      printf("\n");
      continue;
    }

    if (wordexp(commandline, &s_wordexp, 0) != 0) {
      fprintf(stderr, "Could not parse commandline");
      free(commandline);
      continue;
    }
    char** args = s_wordexp.we_wordv;
   
    // Empty string and EOF
    if (args[0] == NULL) {
      free(commandline);
      wordfree(&s_wordexp);
      continue;
    }
    
    /*
     * Check for Builtin commands
     * */
    int (*builtin_command)(int,char**,struct shell_context*);
    if ((builtin_command = find_builtin(args[0])) != NULL) {
      builtin_command(s_wordexp.we_wordc, args, context); 
    }

    /*
     * Check and run other programs
     * */
    else if (find_command(args[0], context->path) == 0) {
      if ((process = fork()) < 0) {
        perror("Fork Failed");
        return 1;
      }
      else if ( process == 0) {
        
        if (execvp(args[0], args) == -1)
          perror("Exec Failed");
        return 100;
      }
      else {
        alarm_child(process, timer);

        int pid = 0;
        do {
          pid = waitpid(process, &status, 0);
        } while (pid == -1 && errno == EINTR);
        
        if (pid == -1) {
          perror("waitpid");
          return 1;
        }
        else {
          printf("Processes [%d] exited with [%d] \n", process, status);
          alarm(0); // cancel alarm if the process exits first
        }
      }
    }
    else {
      printf("%s: command not found\n", args[0]);
    }

    wordfree(&s_wordexp);
    free(commandline);
  }
  shell_context_free(context);
  return 0;
}

//Ignoring signals
void IntHandle(int sig) {
}

int main(int argc, char** argv)
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = IntHandle;
  sigaction(SIGINT, &act, 0);
  sigaction(SIGTSTP, &act, 0);
  
  int timer = 0;
  if (argc == 2) {
    if (int_from(argv[1], &timer) != 0) {
      printf("Bad arg \n");
      return 0 ;
    }
  }
  sh(argc, argv, NULL, timer);
}
