#pragma once
#include <cmath>

struct vec2 {
    float x, y;
    vec2(float x = 0, float y = 0) : x(x), y(y) {}
};

struct vec3 {
    float x, y, z;
    vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    vec3 operator+(const vec3& other) const { return vec3(x + other.x, y + other.y, z + other.z); }
    vec3 operator-(const vec3& other) const { return vec3(x - other.x, y - other.y, z - other.z); }
    vec3 operator*(float scalar) const { return vec3(x * scalar, y * scalar, z * scalar); }
};

struct vec4 {
    float x, y, z, w;
    vec4(float x = 0, float y = 0, float z = 0, float w = 0) : x(x), y(y), z(z), w(w) {}
};