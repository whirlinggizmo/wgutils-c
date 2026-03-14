#include "json/json.h"

#include "../vendor/cjson/cJSON.h"
#include "../vendor/cjson/cJSON.c"

struct json_value
{
    cJSON impl;
};

static inline cJSON *json_impl(json_value_t *value)
{
    return (cJSON *)value;
}

static inline const cJSON *json_impl_const(const json_value_t *value)
{
    return (const cJSON *)value;
}

json_value_t *json_parse_with_length(const char *value, size_t length)
{
    return (json_value_t *)cJSON_ParseWithLength(value, length);
}

void json_delete(json_value_t *value)
{
    cJSON_Delete(json_impl(value));
}

const json_value_t *json_object_get(const json_value_t *object, const char *key)
{
    return (const json_value_t *)cJSON_GetObjectItemCaseSensitive(json_impl_const(object), key);
}

json_value_t *json_object_get_mut(json_value_t *object, const char *key)
{
    return (json_value_t *)cJSON_GetObjectItemCaseSensitive(json_impl(object), key);
}

bool json_object_get_string_value(const json_value_t *object, const char *key, const char **out_value)
{
    const json_value_t *value = json_object_get(object, key);
    const char *string_value = NULL;

    if (!json_is_string(value) || out_value == NULL) {
        return false;
    }

    string_value = json_get_string(value);
    if (string_value == NULL) {
        return false;
    }

    *out_value = string_value;
    return true;
}

bool json_object_get_number_value(const json_value_t *object, const char *key, double *out_value)
{
    const json_value_t *value = json_object_get(object, key);

    if (!json_is_number(value) || out_value == NULL) {
        return false;
    }

    *out_value = json_get_number(value);
    return true;
}

bool json_object_get_int_value(const json_value_t *object, const char *key, int *out_value)
{
    const json_value_t *value = json_object_get(object, key);

    if (!json_is_number(value) || out_value == NULL) {
        return false;
    }

    *out_value = json_get_int(value);
    return true;
}

bool json_object_get_bool_value(const json_value_t *object, const char *key, bool *out_value)
{
    const json_value_t *value = json_object_get(object, key);

    if (!json_is_bool(value) || out_value == NULL) {
        return false;
    }

    *out_value = json_get_bool(value);
    return true;
}

bool json_is_array(const json_value_t *value)
{
    return cJSON_IsArray(json_impl_const(value)) != 0;
}

bool json_is_object(const json_value_t *value)
{
    return cJSON_IsObject(json_impl_const(value)) != 0;
}

bool json_is_string(const json_value_t *value)
{
    return cJSON_IsString(json_impl_const(value)) != 0;
}

bool json_is_number(const json_value_t *value)
{
    return cJSON_IsNumber(json_impl_const(value)) != 0;
}

bool json_is_bool(const json_value_t *value)
{
    return cJSON_IsBool(json_impl_const(value)) != 0;
}

const char *json_get_string(const json_value_t *value)
{
    return cJSON_GetStringValue(json_impl_const(value));
}

double json_get_number(const json_value_t *value)
{
    return cJSON_GetNumberValue(json_impl_const(value));
}

int json_get_int(const json_value_t *value)
{
    const cJSON *impl = json_impl_const(value);
    return impl != NULL ? impl->valueint : 0;
}

bool json_get_bool(const json_value_t *value)
{
    return cJSON_IsTrue(json_impl_const(value)) != 0;
}

int json_array_size(const json_value_t *array)
{
    return cJSON_GetArraySize(json_impl_const(array));
}

const json_value_t *json_array_get(const json_value_t *array, int index)
{
    return (const json_value_t *)cJSON_GetArrayItem(json_impl_const(array), index);
}

json_value_t *json_create_object(void)
{
    return (json_value_t *)cJSON_CreateObject();
}

json_value_t *json_create_array(void)
{
    return (json_value_t *)cJSON_CreateArray();
}

json_value_t *json_create_number(double number)
{
    return (json_value_t *)cJSON_CreateNumber(number);
}

json_value_t *json_add_number_to_object(json_value_t *object, const char *name, double number)
{
    return (json_value_t *)cJSON_AddNumberToObject(json_impl(object), name, number);
}

json_value_t *json_add_bool_to_object(json_value_t *object, const char *name, bool boolean)
{
    return (json_value_t *)cJSON_AddBoolToObject(json_impl(object), name, boolean ? 1 : 0);
}

json_value_t *json_add_string_to_object(json_value_t *object, const char *name, const char *string)
{
    return (json_value_t *)cJSON_AddStringToObject(json_impl(object), name, string);
}

int json_add_item_to_object(json_value_t *object, const char *name, json_value_t *item)
{
    return cJSON_AddItemToObject(json_impl(object), name, json_impl(item)) ? 0 : -1;
}

int json_add_item_to_array(json_value_t *array, json_value_t *item)
{
    return cJSON_AddItemToArray(json_impl(array), json_impl(item)) ? 0 : -1;
}

char *json_print_unformatted(const json_value_t *value)
{
    return cJSON_PrintUnformatted(json_impl_const(value));
}

void json_free_string(char *value)
{
    cJSON_free(value);
}
