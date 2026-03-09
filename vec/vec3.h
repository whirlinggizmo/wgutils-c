#ifndef __V3_H__
#define __V3_H__

#include <raylib.h>
#include <math.h>

// In-place vector operations (modifies first argument)
static inline void vec3_set(Vector3* v, float x, float y, float z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void vec3_add(Vector3* v, const Vector3* v2) {
    v->x += v2->x;
    v->y += v2->y;
    v->z += v2->z;
}

static inline void vec3_sub(Vector3* v, const Vector3* v2) {
    v->x -= v2->x;
    v->y -= v2->y;
    v->z -= v2->z;
}

static inline void vec3_mul(Vector3* v, const Vector3* v2) {
    v->x *= v2->x;
    v->y *= v2->y;
    v->z *= v2->z;
}

static inline void vec3_div(Vector3* v, const Vector3* v2) {
    v->x /= v2->x;
    v->y /= v2->y;
    v->z /= v2->z;
}

static inline float vec3_dot(const Vector3* v, const Vector3* v2) {
    return v->x * v2->x + v->y * v2->y + v->z * v2->z;
}

static inline void vec3_cross(Vector3* v, const Vector3* v2) {
    Vector3 temp = {
        v->y * v2->z - v->z * v2->y,
        v->z * v2->x - v->x * v2->z,
        v->x * v2->y - v->y * v2->x
    };
    *v = temp;
}

static inline void vec3_scale(Vector3* v, float s) {
    v->x *= s;
    v->y *= s;
    v->z *= s;
}

static inline float vec3_length_squared(const Vector3* v) {
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

static inline float vec3_length(const Vector3* v) {
    return sqrtf(vec3_length_squared(v));
}

static inline void vec3_normalize(Vector3* v) {
    float len = vec3_length(v);
    if (len > 0) {
        float invLen = 1.0f / len;
        vec3_scale(v, invLen);
    }
}

// Note: For operations that create new vectors, consider using raymath.h functions:
// Vector3Zero() - Create zero vector
// Vector3One() - Create vector with all components 1.0f
// Vector3Add() - Add vectors (returns new vector)
// Vector3Subtract() - Subtract vectors (returns new vector)
// Vector3Scale() - Scale vector (returns new vector)
// Vector3Multiply() - Multiply vectors (returns new vector)
// Vector3CrossProduct() - Cross product (returns new vector)
// Vector3Normalize() - Get normalized vector (returns new vector)
// Vector3Lerp() - Linear interpolation (returns new vector)
// See raymath.h for the complete list

#endif // __V3_H__