#ifndef SHELL_H
#define SHELL_H

int sh(int, char**, char**, int);

struct list;
struct list* build_path();

struct shell_context {
  struct list* path;
  char* prompt;
  char* current_dir;
  char* previous_dir;
  int loop;
};

int update_prompt(struct shell_context*, char*);
int update_path(struct shell_context*);
int update_current_dir(struct shell_context*, char*);
void shell_context_free(struct shell_context*);
#endif
