#ifndef UTILS_H
#define UTILS_H


/* Realloc's ptr 
 * If ptr is null, will malloc */
char* read_input();

struct list {
  void* data;
  struct list* next;
}; 
struct list* list_insert(struct list*, void*);
int list_remove(struct list*, void*);
void list_free(struct list*);

int int_from(char*, int*);
#endif 
