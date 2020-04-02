#define _XOPEN_SOURCE 500
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

#include "utils.h"
#include "builtin.h"
#include "shell.h"

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

int find_command(char* command, struct list* path)
{
  if (is_in_path(command, path) != NULL)
    return 0;

  char* fullpath;
  int f = 1;
  if (strstr(command, "/") != NULL) {
    if ((fullpath = realpath(command,NULL)) != NULL) {
      if (access(fullpath, X_OK) == 0)
        f = 0;
    }
    free(fullpath);
  }
  return f;
}

int sh(int argc, char** argv, char** envp)
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
    commandline = read_input();
    if (wordexp(commandline, &s_wordexp, 0) != 0) {
      fprintf(stderr, "Could not parse commandline");
      return -1;
    }
    char** args = s_wordexp.we_wordv;
    
    if (args[0] == NULL) {
      printf("\n");
      continue;
    }
    
    /*
     * Check for Builtin commands
     * */
    int (*builtin_command)(int,char**,struct shell_context*);
    if ((builtin_command = find_builtin(args[0])) != NULL) {
      builtin_command(s_wordexp.we_wordc, args, context); 
    }
    else if (find_command(args[0], context->path) == 0) {
      // check if path / inpath to prevent a bad fork
      if ((process = fork()) < 0) {
        fprintf(stderr, "Fork error\n");
        return 1;
      }
      else if ( process == 0) {
        execvp(args[0], args);
        printf("This shouldn't happen (exec) \n");
        return 100;
      }
      else {
        int pid = 0;
        do {
          pid = waitpid(process, &status, 0);
        } while (pid == -1 && errno == EINTR);
        /*
        if ((process = waitpid(process, &status, 0)) < 0) {
          perror("waitpid");
          return 1;
        }
        */
        if (pid == -1) {
          perror("waitpid");
          return 1;
        }
        else 
          printf("Processes [%d] exited with [%d] \n", process, status);
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

void IntHandle(int sig) {
}

int main(int argc, char** argv)
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = IntHandle;

  sigaction(SIGINT, &act, 0);
  sigaction(SIGTSTP, &act, 0);
  sh(argc, argv, NULL);
  return 0;
}
