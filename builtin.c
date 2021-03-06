#define _POSIX_SOURCE 
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>

#include "utils.h"
#include "shell.h"
#include "builtin.h"

int builtin_exit(int argc, char** argv, struct shell_context* context)
{
  context->loop = 0;
  return 0;
}

int builtin_prompt(int argc, char** argv, struct shell_context* context)
{
  char* prev = NULL;
  if (argc > 2) {
    printf("Usage: prompt <new prompt> \n");
    return 0;
  }
  else if (argc == 2) {
    size_t len = strlen(argv[1]) + 1;
    prev = realloc(prev, len);
    strncpy(prev, argv[1], len);
  }
  else {
    printf("Input a new prompt prefix: ");
    prev = read_input();
  }

  return update_prompt(context, prev);
}


// Check if file is in a given directory
int is_in_dir(const char* dir, const char* file)
{
  struct dirent *s_dirent;
  DIR *dir_handle;
  if((dir_handle = opendir(dir)) == NULL) { 
    perror("Could not open directory");
    return -1;
  }
   
  int file_fd = dirfd(dir_handle);
  while ((s_dirent = readdir(dir_handle)) != NULL) {
    if (strcmp(file, s_dirent->d_name) == 0 
        && faccessat(file_fd,file, X_OK,0) == 0) {
      closedir(dir_handle);
      return 0;
    }
  }

  closedir(dir_handle);
  return 1;
}

// Check if a command is in the PATH
char* is_in_path(char* command, struct list* path)
{
  do {
    char* p = (char*)path->data;
    if (is_in_dir(p, command) == 0) {
      return p;
    }
  } while ((path = path->next));
  return NULL;
}

int builtin_which(int argc, char** argv, struct shell_context* context)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: which <command> \n");
    return 0;
  }
  int count = 1;
  while (count < argc) {
    struct list* current = context->path;
    char* dir;
    if ((dir = is_in_path(argv[count], current)) != NULL)
      printf("%s/%s\n", dir, argv[count]);
    else
      printf("%s: Command not found\n", argv[count]);

    ++count;
  }
    return 0;
}

int builtin_where(int argc, char** argv, struct shell_context* context)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: where <command> \n");
    return 0;
  }
  int count = 1;
  while (count < argc) {
  struct list* current = context->path;
    do {
      char* p = (char*)current->data;
      if (is_in_dir(p, argv[count]) == 0) 
        printf("%s/%s \n", p, argv[count]);
    } while ((current = current->next));
    ++count;
  }
  return 0;
}

int builtin_cd(int argc, char** argv, struct shell_context* context)
{
  char* path = NULL;
  if (argc == 1) {
    if ((path = getenv("HOME")) == NULL) {
      fprintf(stderr, "%s: Could not find home directory\n", argv[0]);
      return 0;
    }
  }
  else if (argc == 2) {
    if (strcmp(argv[1], "-") == 0)
      path = context->previous_dir;
    else
      path = argv[1];
  }
  else {
    fprintf(stderr, "Too many args for '%s' command\n", argv[0]);
    return 0;
  }
  char* temp;
  if ((temp = realpath(path, NULL)) == NULL) {
    perror("cd");
    return 0;
  }
  struct stat stats;
  if (stat(temp, &stats) == 0 && !(S_ISDIR(stats.st_mode))) {
    printf("cd: '%s' is not a directory\n", argv[1]);
    return 0;
  }

  update_current_dir(context, temp);
  return chdir(temp);
}

int builtin_pwd(int argc, char** argv, struct shell_context* context)
{
  if (argc != 1) {
    fprintf(stderr, "Too many args for '%s' command\n", argv[0]);
    return 0;
  }
  char* cwd = getcwd(NULL, 0);
  if (cwd == NULL) {
    perror("pwd");
    return 1;
  }
  printf("%s \n", cwd);
  free (cwd);
  return 0;
}

// Lists everything in a given directory
int list_dir(char* dir) 
{
  int count = 0;
  struct dirent *s_dirent;
  DIR *dir_handle;
  if((dir_handle = opendir(dir)) == NULL) {
    perror("Could not open directory");
    return -1;
  } 
  
  printf("%s: \n", dir);
  while ((s_dirent = readdir(dir_handle)) != NULL) {
    printf("%s \n", s_dirent->d_name); 
    ++count;
  }

  printf("\n");
  closedir(dir_handle);
  return count;
}

int builtin_list(int argc, char** argv, struct shell_context* context)
{
  if (argc == 1) {
    char* cwd = getcwd(NULL, 0);
    list_dir(cwd);
    free(cwd);
  }
  else {
    int count = 1;
    char* dir;
    while ((dir = argv[count++])) {
      list_dir(dir);
    }
  }
  return 0;
}

int builtin_pid(int argc, char** argv, struct shell_context* context)
{
  if (argc != 1) {
    fprintf(stderr, "Too many args for '%s' command\n", argv[0]);
    return 0;
  }
  int pid = getpid();
  printf("Process id: %d \n", pid);
  return 0;
}

int builtin_kill(int argc, char** argv, struct shell_context* context) {
  int sig = 0;
  pid_t pid = 0;
  if (argc == 2) {
    if (int_from(argv[1], &pid) != 0) {
      printf("Invalid PID\n"); 
      return 0;
    }
    sig = SIGTERM;
  }
  else if (argc == 3) {
    if (int_from(argv[2], &pid) != 0 || int_from(argv[1] + 1, &sig) != 0) {
      printf("Usage: kill -<signal> <pid>\n");
      return 0;
    }
  }
  else {
    printf("Invalid args\n");
    return 0;
  }
  return kill(pid,sig);
}

int builtin_printenv(int argc, char** argv, struct shell_context* context) {
  if (argc == 1) {
    extern char** environ;
    int i = 0;
    while (environ[i]) {
      printf("%s \n",environ[i++]);
    }
  }
  else if (argc == 2) {
    char* env = getenv(argv[1]);
    // tcsh behavior:
    // no newline on null
    // newline on empty
    if (env == NULL)
      return 0;
    printf("%s \n", getenv(argv[1]));
  }
  else {
    fprintf(stderr, "Too many args\n");  
  }
  return 0;
}

int builtin_setenv(int argc, char** argv, struct shell_context* context) {
  char* key, *value = "";
  if (argc == 1) {
    builtin_printenv(argc, argv, context);
    return 0;
  }
  else if (argc > 3) {
    fprintf(stderr, "Too many args\n");
    return 0;
  }
  key = argv[1];
  if (argc == 3) {
    value = argv[2];
  }
  
  if (strcmp(key,"PATH") == 0)
    update_path(context);

  return setenv(key, value, 1);
}

// Map command to function
const struct builtin_command builtin_data[] = {
  {"prompt", &builtin_prompt},
  {"which", &builtin_which},
  {"where", &builtin_where},
  {"cd", &builtin_cd},
  {"pwd", &builtin_pwd},
  {"list", &builtin_list},
  {"pid", &builtin_pid},
  {"kill", &builtin_kill},
  {"printenv", &builtin_printenv},
  {"setenv", &builtin_setenv},
  {"exit", &builtin_exit}
};

const int builtin_data_length = sizeof(builtin_data) / sizeof(*builtin_data);

int (*find_builtin(char* command))(int,char**,struct shell_context*) {
  for (int i = 0; i < builtin_data_length; ++i) {
    if (strcmp(command, builtin_data[i].name) == 0) {
      return builtin_data[i].command;
    } 
  }
  return NULL;
}
