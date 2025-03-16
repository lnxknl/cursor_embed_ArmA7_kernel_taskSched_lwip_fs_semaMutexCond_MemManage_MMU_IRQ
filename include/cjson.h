#ifndef CJSON_H
#define CJSON_H

#include <stddef.h>

/* cJSON Types */
#define cJSON_Invalid   (0)
#define cJSON_False     (1 << 0)
#define cJSON_True      (1 << 1)
#define cJSON_NULL      (1 << 2)
#define cJSON_Number    (1 << 3)
#define cJSON_String    (1 << 4)
#define cJSON_Array     (1 << 5)
#define cJSON_Object    (1 << 6)
#define cJSON_Raw       (1 << 7)

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

/* The cJSON structure */
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

typedef struct {
    const unsigned char *content;
    size_t position;
    size_t length;
    int depth;
} parse_buffer;

/* Supply a block of JSON, and this returns a cJSON object */
cJSON *cJSON_Parse(const char *value);
cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length);
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);

/* Render a cJSON entity to text for transfer/storage */
char *cJSON_Print(const cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, int fmt);

/* Delete a cJSON entity and all subentities */
void cJSON_Delete(cJSON *c);

/* Returns the number of items in an array (or object) */
int cJSON_GetArraySize(const cJSON *array);

/* Retrieve item number "index" from array "array" */
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
int cJSON_HasObjectItem(const cJSON *object, const char *string);

/* Add item to array/object */
void cJSON_AddItemToArray(cJSON *array, cJSON *item);
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);

/* Remove/Detach items from Arrays/Objects */
cJSON *cJSON_DetachItemFromArray(cJSON *array, int which);
void cJSON_DeleteItemFromArray(cJSON *array, int which);
cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string);
void cJSON_DeleteItemFromObject(cJSON *object, const char *string);

/* Create basic types */
cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);
cJSON *cJSON_CreateBool(int boolean);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateRaw(const char *raw);

/* Create Arrays */
cJSON *cJSON_CreateIntArray(const int *numbers, int count);
cJSON *cJSON_CreateFloatArray(const float *numbers, int count);
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count);
cJSON *cJSON_CreateStringArray(const char *const *strings, int count);

/* Duplicate */
cJSON *cJSON_Duplicate(const cJSON *item, int recurse);

/* Minify a strings, remove blank characters(such as ' ', '\t', '\r', '\n') from strings */
void cJSON_Minify(char *json);

/* Helper functions for creating and adding items to an object at the same time */
void cJSON_AddNullToObject(cJSON * const object, const char * const name);
void cJSON_AddTrueToObject(cJSON * const object, const char * const name);
void cJSON_AddFalseToObject(cJSON * const object, const char * const name);
void cJSON_AddBoolToObject(cJSON * const object, const char * const name, const int boolean);
void cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
void cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);
void cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw);
void cJSON_AddObjectToObject(cJSON * const object, const char * const name);
void cJSON_AddArrayToObject(cJSON * const object, const char * const name);

#endif 