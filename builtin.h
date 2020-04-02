#ifndef BUILTIN_H
#define BUILTIN_H

struct shell_context;
struct builtin_command {
  char* name;
  int (*command)(int,char**,struct shell_context*);
};

int (*find_builtin(char*))(int,char**,struct shell_context*);
char* is_in_path(char*, struct list*);
#endif

