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


  extern void JSON_Delete(JSON *c);
  extern JSON *JSON_Parse(const char *value);

  extern JSON *JSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);

  // JSON创建函数
  extern JSON *JSON_CreateNull(void);
  extern JSON *JSON_CreateTrue(void);
  extern JSON *JSON_CreateFalse(void);
  extern JSON *JSON_CreateBool(int b);
  extern JSON *JSON_CreateNumber(double num);
  extern JSON *JSON_CreateString(const char *string);
  extern JSON *JSON_CreateArray(void);
  extern JSON *JSON_CreateObject(void);

  extern JSON *JSON_CreateIntArray(const int *numbers, int count);
  extern JSON *JSON_CreateFloatArray(const float *numbers, int count);
  extern JSON *JSON_CreateDoubleArray(const double *numbers, int count);
  extern JSON *JSON_CreateStringArray(const char **strings, int count);

  // 添加JSON数据项
  extern void JSON_AddItemToArray(JSON *array, JSON *item);
  extern void JSON_AddItemToObject(JSON *object, const char *string, JSON *item);
  extern void JSON_AddItemToObjectCS(JSON *object, const char *string, JSON *item);
  extern void JSON_AddItemReferenceToArray(JSON *array, JSON *item);
  extern void JSON_AddItemReferenceToObject(JSON *object, const char *string, JSON *item);
  
  
#ifdef __cplusplus
}
#endif

#endif
