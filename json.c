#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>

// 因为数据格式错误，导致解析错误，退出指针所指的位置
static const char *ep;

const char *JSON_GetErrorPtr(void) {return ep;}

typedef struct {
  char *buffer;
  int length;
  int offset;
} printbuffer;

// unicode to UTF-8
static const unsigned char firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

static void *(*JSON_malloc)(size_t sz) = malloc; // 指针函数

static void (*JSON_free)(void *ptr) = free;

/**
 * 解析函数以及打印函数声明
 * 解析函数返回值为指针指向的下个字符的位置
 * 打印函数为格式化后的字符指针
 */
static const char *parse_value(JSON *item, const char *value);
static const char *parse_array(JSON *item, const char *value);
static const char *parse_object(JSON *item, const char *value);
static char *print_value(JSON *item,int depth,int fmt,printbuffer *p);
static char *print_array(JSON *item,int depth,int fmt,printbuffer *p);
static char *print_object(JSON *item,int depth,int fmt,printbuffer *p);

static char *JSON_strdup(const char *str) {
  size_t len;
  char *copy;
  len = strlen(str) + 1;
  if (!(copy = (char*)JSON_malloc(len))) return 0;
  memcpy(copy, str, len);
  return copy;
}

static JSON *JSON_New_Item(void) {
  JSON *node = (JSON*)JSON_malloc(sizeof(JSON));
  if (node)
    memset(node, 0, sizeof(JSON));
  return node;
}

// 过滤空格 \r \n
static const char *skip(const char *in) {
  while (in && *in && (unsigned char)*in <= 32)
    in++;
  return in;
}

static int update(printbuffer *p) {
  char *str;
  if (!p || !p->buffer)
    return 0;
  str = p->buffer + p->offset;
  return p->offset + strlen(str);
}

static int pow2gt(int x) {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

static char *ensure(printbuffer *p, int needed) {
  char *newbuffer;
  int newsize;
  if (!p || !p->buffer)
    return 0;
  needed += p->offset;
  if (needed <= p->length)
    return p->buffer + p->offset;

  newsize = pow2gt(needed);
  newbuffer = (char*)JSON_malloc(newsize);
  if (!newbuffer) {
    JSON_free(p->buffer);
    p->length = 0;
    p->buffer = 0;
    return 0;
  }
  if (newbuffer)
    memcpy(newbuffer, p->buffer, p->length);
  JSON_free(p->buffer);
  p->length = newsize;
  p->buffer = newbuffer;
  return newbuffer + p->offset;
}

// 16进制转10进制
static unsigned parse_hex4(const char *str) {
  unsigned h = 0;
  
  if (*str >= '0' && *str <= '9') h += (*str) - '0';
  else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
  else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
  else return 0;
  h = h << 4; str++;
  
  if (*str >= '0' && *str <= '9') h += (*str) - '0';
  else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
  else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
  else return 0;
  h = h << 4; str++;

  if (*str >= '0' && *str <= '9') h += (*str) - '0';
  else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
  else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
  else return 0;
  h = h << 4; str++;

  if (*str >= '0' && *str <= '9') h += (*str) - '0';
  else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
  else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
  else return 0;
  
  return h;
}

// 解析字符串
static const char *parse_string(JSON *item, const char *str) {
  const char *ptr = str+1;
  char *ptr2;
  char *out;
  int len = 0;
  unsigned uc, uc2;
  if (*str != '\"') { // 不是字符串
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
  while (*ptr != '\"' && *ptr) {
    if (*ptr != '\\') *ptr2++ = *ptr++;
    else {
      ptr++;
      switch (*ptr) {
      case 'b': *ptr2++ = '\b'; break;
      case 'f': *ptr2++ = '\f'; break;
      case 'n': *ptr2++ = '\n'; break;
      case 'r': *ptr2++ = '\r'; break;
      case 't': *ptr2++ = '\t'; break;
      case 'u': // UTF-16 to UTF-8
        uc = parse_hex4(ptr + 1);
        ptr += 4;
        // UTF-16 surrogate pair
        if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break; //Low-surrogate, 不对应任何字符
        if ((uc >= 0xD800 && uc <= 0xDBFF)) { //High-surrogate
          if (ptr[1]!='\\' || ptr[2]!='u') break; // missing second-half of surrogate
          uc2 = parse_hex4(ptr+3); ptr+=6;
          if (uc2 < 0xDC00 || uc2 > 0xDFFF) break; // invalid second-half of surrogate
          // UTF-16（4字节）的编码（二进制）就是：110110yyyyyyyyyy 110111xxxxxxxxxx
          uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
        }
        len = 4; // UTF-8长度
        if (uc < 0x80) len = 1;
        else if (uc < 0x800) len = 2;
        else if (uc < 0x100000) len = 3;
        ptr2 += len;

        switch (len) {
        case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
        case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
        case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
        case 1: *--ptr2 = (uc | firstByteMark[len]);
        }
        ptr2+=len; break;
      default:
        *ptr2++ = *ptr; break;
      }
      ptr++;
    }
  }
  *ptr2 = 0;
  if (*ptr == '\"') ptr++;
  item->valuestring = out;
  item->type = JSON_String;
  return ptr;
}

// 打印字符串
static char *print_string_ptr(const char *str, printbuffer *p) {
  const char *ptr;
  char *ptr2, *out;
  int len = 0, flag = 0;
  unsigned char token;

  for (ptr=str; *ptr; ptr++)
    flag |= ((*ptr > 0 && *ptr < 32) || (*ptr == '\"') || (*ptr == '\\')) ? 1 : 0;
  if (!flag) {
    len = ptr - str;
    // '\"' '\"' '\0'
    if (p) out = ensure(p, len + 3);
    else out = (char*)JSON_malloc(len + 3);
    if (!out) return 0;
    ptr2 = out;
    *ptr2++ = '\"';
    strcpy(ptr2, str);
    ptr2[len] = '\"';
    ptr2[len+1] = 0;
    return out;
  }

  // 字符串为空
  if (!str) {
    if (p) out = ensure(p, 3);
    else out = (char*)JSON_malloc(3);
    if (!out) return 0;
    strcpy(out, "\"\"");
    return out;
  }
  
  ptr = str;
  while ((token = *ptr) && ++len) {
    if (strchr("\"\\\b\f\n\r\t", token))
      len++;
    else if (token < 32) //'\u'
      len += 5;
    ptr++;
  }
  if (p) out = ensure(p, len + 3);
  else out = (char*)JSON_malloc(len + 3);
  if (!out) return 0;

  ptr2 = out;
  ptr = str;
  *ptr2++ = '\"';
  while (*ptr) {
    if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
      *ptr2++ = *ptr++;
    else {
      *ptr2++ = '\\';
      switch (token = *ptr++) {
      case '\\': *ptr2++ = '\\'; break;
      case '\"': *ptr2++ = '\"'; break;
      case '\b': *ptr2++ = 'b'; break;
      case '\f': *ptr2++ = 'f'; break;
      case '\n': *ptr2++ = 'n'; break;
      case '\r': *ptr2++ = 'r'; break;
      case '\t': *ptr2++ = 't'; break;
      default: sprintf(ptr2, "u%04x", token); ptr2 += 5; break;
      }
    }
  }
  *ptr2++ = '\"';
  *ptr2++ = 0;
  return out;
}

static char *print_string(JSON *item, printbuffer *p) {
  return print_string_ptr(item->valuestring, p);
}

// 解析数字
static const char *parse_number(JSON *item, const char *num) {
  double n = 0, sign = 1, scale = 0;
  int subscale = 0, signsubscale = 1;

  if (*num == '-') sign = -1, num++;
  if (*num == '0') num++;
  if (*num >= '1' && *num <= '9') {
    do
      n = (n * 10.0) + (*num++ - '0');
    while (*num >= '0' && *num <= '9');
  }
  if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
    num++;
    do
      n = (n * 10.0) + (*num++ - '0'), scale--;
    while (*num >= '0' && *num <= '9');
  }
  if (*num == 'e' || *num == 'E') {
    num++;
    if (*num == '+') num++;
    else if (*num == '-')
      signsubscale = -1, num++;
    while (*num >= '0' && *num <= '9')
      subscale = (subscale * 10) + (*num++ - '0');
  }

  n = sign*n*pow(10.0, (scale + subscale*signsubscale));
  item->valuedouble = n;
  item->valueint = (int)n;
  item->type = JSON_Number;
  return num;
}

// 打印数字
static char *print_number(JSON *item, printbuffer *p) {
  char *str = 0;
  double d = item->valuedouble;
  if (d == 0) {
    if (p) str = ensure(p, 2);
    else str = (char*)JSON_malloc(2);
    if (str) strcpy(str, "0");
  } else if (fabs(((double)item->valueint)-d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
    if (p) str = ensure(p, 21);
    else str = (char*)JSON_malloc(21);
    if (str) sprintf(str, "%d", item->valueint);
  } else {
    if (p) str = ensure(p, 64);
    else str = (char*)JSON_malloc(64);
    if (str) {
      if (fabs(floor(d)-d) <= DBL_EPSILON && fabs(d) < 1.0e60)
        sprintf(str, "%.0f", d);
      else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
        sprintf(str, "%e", d);
      else
        sprintf(str, "%f", d);
    }
  }
  return str;
}

// 解析JSON数组
static const char *parse_array(JSON *item, const char *value) {
  JSON *child;
  if (*value != '[') {
    ep = value;
    return 0;
  }

  item->type = JSON_Array;
  value = skip(value + 1);
  if (*value == ']') return value + 1;

  item->child = child = JSON_New_Item();
  if (!item->child) return 0;
  value = skip(parse_value(child, skip(value)));
  if (!value)
    return 0;

  while (*value == ',') {
    JSON *new_item;
    if (!(new_item = JSON_New_Item())) return 0;
    child->next = new_item;
    new_item->prev = child;
    child = new_item;
    value = skip(parse_value(child, skip(value+1)));
    if (!value) return 0;
  }
  
  if (*value == ']') return value + 1;
  ep = value;
  return 0;
}


// 打印JSON数组
static char *print_array(JSON *item, int depth, int fmt, printbuffer *p) {
  char **entries;
  char *out = 0, *ptr, *ret;
  int len = 5;
  JSON *child = item->child;
  int numentries = 0, i = 0, fail = 0;
  size_t tmplen = 0;

  while (child) numentries++, child = child->next;
  if (!numentries) {
    if (p) out = ensure(p, 3);
    else out = (char*)JSON_malloc(3);
    if (out) strcpy(out, "[]");
    return out;
  }

  if (p) {
    i = p->offset;
    ptr = ensure(p, 1);
    if (!ptr) return 0;
    *ptr = '[';
    p->offset++;
    child = item->child;
    while (child && !fail) {
      print_value(child, depth+1, fmt, p);
      // 更新偏移位, 未更新前offset位于上次添加的字符串的'\0',即结束处
      p->offset = update(p);
      if (child->next) {
        // fmt代表','之后是否有' '
        len = fmt ? 2 : 1;
        ptr = ensure(p, len+1);
        if (!ptr)
          return 0;
        *ptr++ = ',';
        if (fmt)
          *ptr++ = ' ';
        *ptr = 0;
        p->offset+=len;
      }
      child = child->next;
    }
    ptr = ensure(p, 2);
    if (!ptr) return 0;
    *ptr++ = ']';
    *ptr = 0;
    out = (p->buffer) + i;
  } else {
    entries = (char**)JSON_malloc(numentries*sizeof(char*));
    if (!entries) return 0;
    memset(entries, 0, numentries*sizeof(char*));
    child=item->child;
    while(child && !fail) {
      ret = print_value(child, depth+1, fmt, 0);
      entries[i++] = ret;
      if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
      else fail = 1;
      child = child->next;
    }

    if (!fail) out = (char*)JSON_malloc(len);
    if (!out) fail = 1;
    if (fail) {
      for (i = 0; i < numentries; i++)
        if (entries[i])
          JSON_free(entries[i]);
      JSON_free(entries);
      return 0;
    }
    *out = '[';
    ptr = out + 1;
    *ptr = 0;
    for (i = 0; i < numentries; i++) {
      tmplen = strlen(entries[i]);
      memcpy(ptr, entries[i], tmplen);
      ptr += tmplen;
      if (i != numentries - 1) {
        *ptr++ = ',';
        if (fmt)
          *ptr++ = ' ';
        *ptr = 0;
      }
      JSON_free(entries[i]);
    }
    JSON_free(entries);
    *ptr++ = ']';
    *ptr++ = 0;
  }
  return out;
}

//解析JSON对象
static const char *parse_object(JSON *item, const char *value) {
  JSON *child;
  if (*value != '{') {
    ep = value;
    return 0;
  }

  item->type = JSON_Object;
  value = skip(value+1);
  if (*value == '}') return value+1;
  item->child = child = JSON_New_Item();
  if (!item->child) return 0;
  value = skip(parse_string(child, skip(value)));
  if (!value) return 0;
  child->string = child->valuestring;
  child->valuestring = 0;
  if (*value != ':') {
    ep = value;
    return 0;
  }
  value=skip(parse_value(child, skip(value+1)));
  if (!value) return 0;

  while (*value == ',') {
    JSON *new_item;
    if (!(new_item = JSON_New_Item())) return 0;
    child->next = new_item;
    new_item->prev = child;
    child = new_item;
    value = skip(parse_string(child, skip(value+1)));
    if (!value) return 0;
    child->string = child->valuestring;
    child->valuestring = 0;
    if (*value != ':') {
      ep = value;
      return 0;
    }
    value = skip(parse_value(child, skip(value+1)));
    if (!value) return 0;
  }

  if (*value == '}') return value+1;
  
  ep = value;
  return 0;
}

// 解析JSON字符串
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
  if (*value == '\"') {
    return parse_string(item, value);
  }
  if (*value == '-' || (*value >= '0' && *value <= '9')) {
    return parse_number(item, value);
  }
  if (*value == '[') {
    return parse_array(item, value);
  }
  if (*value == '{') {
    return parse_object(item, value);
  }
  ep = value;
  return 0;
}

static char *print_object(JSON *item, int depth, int fmt, printbuffer *p) {
  char **entries = 0, **names = 0;
  char *out = 0, *ptr, *ret, *str;
  int len = 7, i = 0, j;
  JSON *child = item->child;
  int numentries = 0, fail = 0;
  size_t tmplen = 0;
  while (child) numentries++, child = child->next;
  if (!numentries) {
    if (p) out = ensure(p, fmt ? depth+4 : 3);
    else out = (char*)JSON_malloc(fmt ? depth+4 : 3);
    if (!out) return 0;
    ptr = out; *ptr++ = '{';
    // 代表'{'之后是否有'\n'及'\t','\t'个数为depth
    if (fmt) {
      *ptr++ = '\n';
      for (i = 0; i < depth - 1; i++)
        *ptr++ = '\t';
    }
    *ptr++ = '}'; *ptr++ = 0;
    return out;
  }
  if (p) {
    i = p->offset;
    len = fmt ? 2 : 1;
    ptr = ensure(p, len+1);
    if (!ptr) return 0;
    *ptr++ = '{';
    if (fmt) *ptr++ = '\n';
    *ptr=0;
    p->offset += len;
    child = item->child;
    depth++;
    while (child) {
      if (fmt) {
        ptr = ensure(p, depth);
        if (!ptr) return 0;
        for (j = 0; j < depth; j++)
          *ptr++ = '\t';
        p->offset += depth;
      }
      print_string_ptr(child->string, p);
      p->offset = update(p);

      len = fmt ? 2 : 1;
      ptr = ensure(p, len);
      if (!ptr) return 0;
      *ptr++ = ':';
      if (fmt) *ptr++ = '\t';
      p->offset+=len;

      print_value(child, depth, fmt, p);
      p->offset = update(p);

      len = (fmt ? 1 : 0) + (child->next ? 1 : 0);
      ptr = ensure(p, len+1);
      if (!ptr) return 0;
      if (child->next) *ptr++ = ',';
      if (fmt) *ptr++ = '\n';
      *ptr = 0;
      p->offset+=len;
      child=child->next;
    }
    ptr = ensure(p, fmt ? (depth+1) : 2);
    if (fmt)
      for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
    *ptr++ = '}'; *ptr = 0;
    out = (p->buffer) + i;
  } else {
    entries = (char**)JSON_malloc(numentries*sizeof(char*));
    if (!entries) return 0;
    names = (char**)JSON_malloc(numentries*sizeof(char*));
    if (!names) {
      JSON_free(entries);
      return 0;
    }
    memset(entries, 0, sizeof(char*)*numentries);
    memset(entries, 0, sizeof(char*)*numentries);

    child = item->child;
    depth++;
    if (fmt) len += depth;
    while (child) {
      names[i] = str = print_string_ptr(child->string, 0);
      entries[i++] = ret = print_value(child, depth, fmt, 0);
      if (str && ret)
        len += strlen(ret) + strlen(str) + 2 + (fmt ? 2+depth : 0);
      else
        fail = 1;
      child=child->next;
    }

    if (!fail) out = (char*)JSON_malloc(len);
    if (!out) fail = 1;
    if (fail) {
      for (i = 0; i < numentries; i++) {
        if (names[i])
          JSON_free(names[i]);
        if (entries[i])
          JSON_free(entries[i]);
        return 0;
      }
    }

    *out = '{';
    ptr = out + 1;
    if (fmt) *ptr++ = '\n';
    *ptr = 0;
    for (i = 0; i < numentries ; i++) {
      if (fmt) for (j = 0; j < depth; j++) *ptr++ = '\t';
      tmplen = strlen(names[i]);
      memcpy(ptr, names[i], tmplen);
      ptr += tmplen;
      *ptr++ = ':';
      if (fmt) *ptr++ = '\t';
      strcpy(ptr, entries[i]);
      ptr += strlen(entries[i]);
      if (i != numentries - 1) *ptr++ = ',';
      if (fmt) *ptr++ = '\n';
      *ptr = 0;
      JSON_free(names[i]);
      JSON_free(entries[i]);
    }
    JSON_free(names);
    JSON_free(entries);
    if (fmt)
      for (i = 0; i < depth -1; i++)
        *ptr++ = '\t';
    *ptr++ = '}';
    *ptr++ = 0;
  }
  return out;
}

static char *print_value(JSON *item,int depth,int fmt,printbuffer *p) {
  char *out = 0;
  if (!item) return 0;
  if (p) {
    switch ((item->type) & 255) {
    case JSON_NULL: {
      out = ensure(p, 5);
      if (out) strcpy(out, "null");
      break;
    }
    case JSON_False: {
      out = ensure(p, 6);
      if (out) strcpy(out, "false");
      break;
    }
    case JSON_True: {
      out = ensure(p, 5);
      if (out) strcpy(out, "true");
      break;
    }
    case JSON_Number: {
      out = print_number(item, p);
      break;
    }
    case JSON_String: {
      out = print_string(item, p);
      break;
    }
    case JSON_Array: {
      out = print_array(item, depth, fmt, p);
      break;
    }
    case JSON_Object: {
      out = print_object(item, depth, fmt, p);
      break;
    }
    }
  } else {
    switch ((item->type) & 255) {
    case JSON_NULL: out = JSON_strdup("null"); break;
    case JSON_False: out = JSON_strdup("false"); break;
    case JSON_True: out = JSON_strdup("true"); break;
    case JSON_Number: out = print_number(item, 0); break;
    case JSON_String: out = print_string(item, 0); break;
    case JSON_Array: out = print_array(item, depth, fmt, 0); break;
    case JSON_Object: out = print_object(item, depth, fmt, 0); break;
    }
  }
  return out;
}

JSON *JSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated) {
  const char *end = 0;
  JSON *c = JSON_New_Item();
  ep = 0;
  if (!c) return 0;
  end = parse_value(c, skip(value));
  if (!end) {
    JSON_Delete(c);
    return 0;
  }
  if (require_null_terminated) {
    end = skip(end);
    if (*end) {
      JSON_Delete(c);
      ep = end;
      return 0;
    }
  }
  if (return_parse_end)
    *return_parse_end = end;
  return c;
}

JSON *JSON_Parse(const char *value) {
  return JSON_ParseWithOpts(value, 0, 0);
}

char *JSON_Print(JSON *item) {
  return print_value(item, 0, 1 ,0);
}

void JSON_Delete(JSON *c) {
  JSON *next;
  while (c) {
    next = c->next;
    if (!(c->type & JSON_IsReference) && c->child) JSON_Delete(c->child);
    if (!(c->type & JSON_IsReference) && c->valuestring) JSON_free(c->valuestring);
    if (!(c->type & JSON_StringIsConst) && c->string) JSON_free(c->string);
    JSON_free(c);
    c=next;
  }
}
