/**
 * math_utils.hpp - 数学工具函数
 */

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <random>

namespace tensyoki {

// 线性插值
float lerp(float a, float b, float t);

// 限制值在范围内
float clamp(float value, float min, float max);

// 平滑步进
float smoothstep(float edge0, float edge1, float x);
float smootherstep(float edge0, float edge1, float x);

// 角度/弧度转换
float deg_to_rad(float degrees);
float rad_to_deg(float radians);

// 向量旋转
glm::vec2 rotate(const glm::vec2& v, float angle);
glm::vec2 reflect(const glm::vec2& v, const glm::vec2& n);

// 矩阵创建
glm::mat4 ortho(float left, float right, float bottom, float top, float near, float far);
glm::mat4 perspective(float fovy, float aspect, float near, float far);
glm::mat4 look_at(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

// 噪声生成
std::vector<float> generate_noise_2d(int width, int height, float scale, int octaves, float persistence, float lacunarity, int seed);

// 随机数
float random_float(float min, float max);
int random_int(int min, int max);

} // namespace tensyoki