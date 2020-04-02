
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_LENGTH 255

// Read inputs of arbitrary length
// Returns 0 on failed malloc or read
char* read_input()
{
  char* str = NULL;
  char buffer[READ_LENGTH] = "";
  size_t buf_len, input_len = 0;
  do {
    if (fgets(buffer, READ_LENGTH, stdin) < 0) {
      return 0;
    }
    buf_len = strlen(buffer) + 1;
    str = realloc(str, input_len + buf_len);
    if (str == NULL) {
      return 0;
    }
    strcpy(str+input_len, buffer);
    input_len += (buf_len - 1);
  } while (buf_len == READ_LENGTH &&
          buffer[buf_len-2] != '\n');

  if (str[strlen(str)-1] == '\n') {
    str[strlen(str)-1] = '\0';
  } 
  return str;
}

int int_from(char* c, int* i)
{
  char* endptr;
  int val = strtol(c, &endptr, 10);
  if (endptr == c || *endptr != '\0')
    return -1;
  
  *i = val;
  return 0;
}

//Does not support complex (nested) lists
struct list* list_insert(struct list* head, void* val)
{
  struct list *temp, *tail;
  temp = malloc(sizeof(*temp));
  temp->data = val;
  temp->next = NULL;

  struct list** indirect = &head;
  while (*indirect)
    indirect = &(*indirect)->next;
  tail = *indirect;
  *indirect = temp;
  temp->next = tail;
  return head;
}

int list_remove(struct list* head, void* val)
{
  struct list **indirect = &head;
  while (*indirect && (*indirect)->data != val)
    indirect = &(*indirect)->next;

  if (*indirect) {
    struct list *temp = *indirect;
    *indirect = temp->next;
    free(temp);
    return 1;
  }
  return 0;
}
void list_free(struct list* head)
{
  while (head) {
    struct list *temp = head;
    head = head->next;
    free(temp->data);
    free(temp);
  }
}

