#ifndef json_h
#define json_h

#ifdef __cplusplus
extern "C"
{
#endif

  #define JSON_False 0
  #define JSON_True 1
  #define JSON_NULL 2
  #define JSON_Number 3
  #define JSON_String 4
  #define JSON_Array 5
  #define JSON_Object 6

  #define JSON_IsReference 256
  #define JSON_StringIsConst 512

  typedef struct JSON {
    struct JSON *next, *prev;
    struct JSON *child;

    int type;
  
    char *valuestring; // type == JSON_String
    int valueint; // type == JSON_Number
    double valuedouble; // type == JSON_Number

    char *string; // name
  } JSON;

  extern const char *JSON_GetErrorPtr(void);

  extern JSON *JSON_Parse(const char *value);
  extern char *JSON_Print(JSON *item);
  extern void JSON_Delete(JSON *c);

  extern JSON *JSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);

#ifdef __cplusplus
}
#endif

#endif
