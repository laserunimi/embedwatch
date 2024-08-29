// https://github.com/forkachild/C-Simple-JSON-Parser

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef __cplusplus
typedef char*                                   string;
typedef unsigned char                           bool;
#define true                                    (1)
#define false                                   (0)
#define TRUE                                    true
#define FALSE                                   false
#endif

#define new(x)                                  (x *) malloc(sizeof(x))
#define newWithSize(x, y)                       (x *) malloc(y * sizeof(x))
#define renewWithSize(x, y, z)                  (y *) realloc(x, z * sizeof(y))
#define isWhitespace(x)                         x == '\r' || x == '\n' || x == '\t' || x == ' '
#define removeWhitespace(x)                     while(isWhitespace(*x)) x++
#define removeWhitespaceCalcOffset(x, y)        while(isWhitespace(*x)) { x++; y++; }

//Timing Stuff
#include <time.h>
#include <sys/time.h>
#define TIME_S(tv_s) {\
                gettimeofday(&tv_s,NULL);\
            }
    
#define TIME_F(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Elapsed: %.6f\n", elapsed);\
                }

typedef char character;

struct _jsonobject;
struct _jsonpair;
union _jsonvalue;

typedef enum {
    JSON_STRING = 0,
    JSON_OBJECT
} JSONValueType;

typedef struct _jsonobject {
    struct _jsonpair *pairs;
    int count;
} JSONObject;

typedef struct _jsonpair {
    string key;
    union _jsonvalue *value;
    JSONValueType type;
} JSONPair;

typedef union _jsonvalue {
    string stringValue;
    struct _jsonobject *jsonObject;
} JSONValue;

void freeJSONFromMemory(JSONObject *obj) {
    int i;
    
    if(obj == NULL)
        return;
    
    if(obj->pairs == NULL) {
        free(obj);
        return;
    }
    
    for(i = 0; i < obj->count; i++) {
        if(obj->pairs[i].key != NULL)
            free(obj->pairs[i].key);
        if(obj->pairs[i].value != NULL) {
            switch(obj->pairs[i].type) {
                case JSON_STRING:
                    free(obj->pairs[i].value->stringValue);
                    break;
                case JSON_OBJECT:
                    freeJSONFromMemory(obj->pairs[i].value->jsonObject);
            }
            free(obj->pairs[i].value);
        }
    }
    
}

static int strNextOccurence(string str, char ch) {
    int pos = 0;
    int res = -1;
    
    if(str == NULL){
        res = -1;
    }else{
        while(*str != ch && *str != '\0') {
            str++;
            pos++;
        }
        res = (*str == '\0') ? -1 : pos;
    }

    return res;
}

static JSONObject *_parseJSON(string str, int * offset) {
    
    int _offset = 0;
    
    JSONObject *obj = new(JSONObject);
    obj->count = 1;
    obj->pairs = newWithSize(JSONPair, 1);
    
    while(*str != '\0') {
        removeWhitespaceCalcOffset(str, _offset);
        if(*str == '{') {
            str++;
            _offset++;
        } else if(*str == '"') {
            
            int i = strNextOccurence(++str, '"');
            if(i <= 0) {
                freeJSONFromMemory(obj);
                return NULL;
            }
            
            JSONPair tempPtr = obj->pairs[obj->count - 1];
            
            tempPtr.key = newWithSize(character , i + 1);
            memcpy(tempPtr.key, str, i * sizeof(character));
            tempPtr.key[i] = '\0';
            
            str += i + 1;
            _offset += i + 2;
            
            i = strNextOccurence(str, ':');
            if(i == -1)
                return NULL;
            str += i + 1;
            _offset += i + 1;
            
            removeWhitespaceCalcOffset(str, _offset);
            
            if(*str == '{') {
                int _offsetBeforeParsingChildObject = _offset;
                int _sizeOfChildObject;

                tempPtr.value = new(JSONValue);
                tempPtr.type = JSON_OBJECT;
                tempPtr.value->jsonObject = _parseJSON(str, &_offset);
                if(tempPtr.value->jsonObject == NULL) {
                    freeJSONFromMemory(obj);
                    return NULL;
                }
                // Advance the string pointer by the size of the processed child object
                _sizeOfChildObject = _offset - _offsetBeforeParsingChildObject;
                str += _sizeOfChildObject;
            } else if(*str == '"') {
                i = strNextOccurence(++str, '"');
                if(i == -1) {
                    freeJSONFromMemory(obj);
                    return NULL;
                }
                tempPtr.value = new(JSONValue);
                tempPtr.type = JSON_STRING;
                tempPtr.value->stringValue = newWithSize(character, i + 1);
                memcpy(tempPtr.value->stringValue, str, i * sizeof(character));
                tempPtr.value->stringValue[i] = '\0';
                str += i + 1;
                _offset += i + 2;
            }
            obj->pairs[obj->count - 1] = tempPtr;
            
        } else if (*str == ',') {
            obj->count++;
            obj->pairs = renewWithSize(obj->pairs, JSONPair, obj->count);
            str++;
            _offset++;
        } else if (*str == '}') {
            (*offset) += _offset + 1;
            return obj;
        }
    }
    return obj;
}

JSONObject *parseJSON(string jsonString) {
    int offset = 0;
    JSONObject *tempObj = _parseJSON(jsonString, &offset);
    return tempObj;
}



int main(int argc, const char * argv[]) {
    struct  timeval t1,t2;
    TIME_S(t1)

    char * someJsonString = "{\"hello\":\"world\",\"key\":\"value\"}";

    JSONObject *json = parseJSON(someJsonString);
    printf("Count: %i\n", json->count);
    printf("Key: %s, Value: %s\n", json->pairs[0].key, json->pairs[0].value->stringValue);
    
    TIME_F(t1,t2)

    return 0;
}
