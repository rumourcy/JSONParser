#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ep;

static void *(*JSON_malloc)(size_t sz) = malloc; // 指针函数

static void (*JSON_free)(void *ptr) = free;

static JSON *JSON_New_Item(void) {
  JSON *node = (JSON*)JSON_malloc(sizeof(JSON));
  if (node)
    memset(node, 0, sizeof(JSON));
  return node;
}

// TODO
static const char *pasre_string(JSON *item, const char *str) {
  const char *ptr = str+1;
  char *ptr2;
  char *out;
  int len = 0;
  unsigned uc, uc2;
  if (*str != '\"') {
    ep = str;
    return 0;
  }

  // "终止,或者'\0'终止
  while (*ptr!='\"' && *ptr && ++len)
    if (*ptr++ == '\\') //跳过转义符
      ptr++;

  out = (char*)JSON_malloc(len+1);
  if (!out) return 0;
  ptr = str + 1;
  ptr2 = out;
  
}

// TODO
static const char *parse_value(JSON *item, const char *value) {
  if (!value) return 0;
  if (!strncmp(value, "null", 4)) {
    item->type = JSON_NULL;
    return value + 4;
  }
  if (!strncmp(value, "false", 5)) {
    item->type = JSON_False;
    return value + 5;
  }
  if (!strncmp(value, "true", 4)) {
    item->type = JSON_True;
    item->valueint = 1;
    return value + 4;
  }
  ep = value;
  return 0;
}

void JSON_Delete(JSON *c) {
  JSON *next;
  while (c) {
    next = c->next;
    if (!(c->type&JSON_IsReference) && c->child) JSON_Delete(c->child);
    if (!(c->type&JSON_IsReference) && c->valuestring) JSON_free(c->valuestring);
    if (!(c->type&JSON_StringIsConst) && c->string) JSON_free(c->string);
    JSON_free(c);
    c=next;
  }
}

// TODO
JSON *JSON_ParseWithOpts(const char *value, char **return_parse_end, int require_null_terminated) {
  const char *end = 0;
  JSON *c = JSON_New_Item();
  ep = 0;
  if (!c) return 0;
  
  return c;
}

JSON *JSON_Parse(const char *value) {
  return JSON_ParseWithOpts(value, 0, 0);
}
