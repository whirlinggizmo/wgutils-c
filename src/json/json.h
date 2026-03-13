#ifndef WGUTILS_JSON_H
#define WGUTILS_JSON_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_value json_value_t;

json_value_t *json_parse_with_length(const char *value, size_t length);
void json_delete(json_value_t *value);

const json_value_t *json_object_get(const json_value_t *object, const char *key);
json_value_t *json_object_get_mut(json_value_t *object, const char *key);
bool json_object_get_string_value(const json_value_t *object, const char *key, const char **out_value);
bool json_object_get_number_value(const json_value_t *object, const char *key, double *out_value);
bool json_object_get_int_value(const json_value_t *object, const char *key, int *out_value);
bool json_object_get_bool_value(const json_value_t *object, const char *key, bool *out_value);

bool json_is_array(const json_value_t *value);
bool json_is_object(const json_value_t *value);
bool json_is_string(const json_value_t *value);
bool json_is_number(const json_value_t *value);
bool json_is_bool(const json_value_t *value);

const char *json_get_string(const json_value_t *value);
double json_get_number(const json_value_t *value);
int json_get_int(const json_value_t *value);
bool json_get_bool(const json_value_t *value);

int json_array_size(const json_value_t *array);
const json_value_t *json_array_get(const json_value_t *array, int index);

json_value_t *json_create_object(void);
json_value_t *json_create_array(void);
json_value_t *json_add_number_to_object(json_value_t *object, const char *name, double number);
json_value_t *json_add_bool_to_object(json_value_t *object, const char *name, bool boolean);
json_value_t *json_add_string_to_object(json_value_t *object, const char *name, const char *string);
int json_add_item_to_object(json_value_t *object, const char *name, json_value_t *item);
int json_add_item_to_array(json_value_t *array, json_value_t *item);

char *json_print_unformatted(const json_value_t *value);
void json_free_string(char *value);

#ifdef __cplusplus
}
#endif

#endif
