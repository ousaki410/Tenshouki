/**
 * math_utils.hpp - 确定性数学工具库
 * 参考 OpenCE 的 musl-math 封装，保证跨平台浮点一致性
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace tensyoki {
namespace math {

// 编译时检查确定性数学支持
#if defined(TENSYOKI_PORTABLE)
    // 便携构建：使用 musl 数学函数
    extern "C" {
        float musl_sinf(float x);
        float musl_cosf(float x);
        float musl_tanf(float x);
        float musl_asinf(float x);
        float musl_acosf(float x);
        float musl_atanf(float x);
        float musl_atan2f(float y, float x);
        float musl_sqrtf(float x);
        float musl_expf(float x);
        float musl_logf(float x);
        float musl_powf(float x, float y);
        float musl_floorf(float x);
        float musl_ceilf(float x);
        float musl_fmodf(float x, float y);
    }
    
    #define MATH_SIN musl_sinf
    #define MATH_COS musl_cosf
    #define MATH_TAN musl_tanf
    #define MATH_ASIN musl_asinf
    #define MATH_ACOS musl_acosf
    #define MATH_ATAN musl_atanf
    #define MATH_ATAN2 musl_atan2f
    #define MATH_SQRT musl_sqrtf
    #define MATH_EXP musl_expf
    #define MATH_LOG musl_logf
    #define MATH_POW musl_powf
    #define MATH_FLOOR musl_floorf
    #define MATH_CEIL musl_ceilf
    #define MATH_FMOD musl_fmodf
#else
    // 标准构建：使用标准库 (但禁用 FMA)
    #define MATH_SIN sinf
    #define MATH_COS cosf
    #define MATH_TAN tanf
    #define MATH_ASIN asinf
    #define MATH_ACOS acosf
    #define MATH_ATAN atanf
    #define MATH_ATAN2 atan2f
    #define MATH_SQRT sqrtf
    #define MATH_EXP expf
    #define MATH_LOG logf
    #define MATH_POW powf
    #define MATH_FLOOR floorf
    #define MATH_CEIL ceilf
    #define MATH_FMOD fmodf
#endif

// 常量
constexpr float PI = 3.14159265358979323846f;
constexpr float PI_2 = 1.57079632679489661923f;
constexpr float PI_4 = 0.785398163397448309616f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;
constexpr float EPSILON = 1.192092896e-07f;  // FLT_EPSILON
constexpr float INV_EPSILON = 8388608.0f;

// 确定性浮点比较
inline bool float_eq(float a, float b, float epsilon = EPSILON) {
    return MATH_FMOD(a - b, 0.0f) < epsilon;  // 简化版，实际应用需更健壮
}

inline bool float_lt(float a, float b, float epsilon = EPSILON) {
    return a < b - epsilon;
}

inline bool float_le(float a, float b, float epsilon = EPSILON) {
    return a <= b + epsilon;
}

// 安全除法
inline float safe_div(float num, float denom, float fallback = 0.0f) {
    return (MATH_FMOD(denom, 0.0f) > EPSILON) ? num / denom : fallback;
}

// 角度归一化
inline float normalize_angle(float angle) {
    angle = MATH_FMOD(angle, 2.0f * PI);
    if (angle < 0) angle += 2.0f * PI;
    return angle;
}

inline float normalize_angle_180(float angle) {
    angle = MATH_FMOD(angle + PI, 2.0f * PI);
    if (angle < 0) angle += 2.0f * PI;
    return angle - PI;
}

// 线性插值 (确定性)
inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float inv_lerp(float a, float b, float v) {
    return safe_div(v - a, b - a);
}

// 球面线性插值 (四元数)
inline glm::quat slerp(const glm::quat& a, const glm::quat& b, float t) {
    return glm::slerp(a, b, t);
}

// 矩阵/向量操作 (使用 GLM，但确保确定性)
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

// 向量工具
inline Vec3 vec3_from_spherical(float radius, float theta, float phi) {
    float st = MATH_SIN(theta);
    return Vec3{
        radius * st * MATH_COS(phi),
        radius * MATH_COS(theta),
        radius * st * MATH_SIN(phi)
    };
}

inline float vec3_length(const Vec3& v) {
    return MATH_SQRT(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 vec3_normalize(const Vec3& v) {
    float len = vec3_length(v);
    return len > EPSILON ? v / len : Vec3(0, 1, 0);
}

inline float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec3_cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// 四元数工具
inline Quat quat_from_euler(float yaw, float pitch, float roll) {
    return glm::quat(Vec3(pitch, yaw, roll));
}

inline Quat quat_look_at(const Vec3& forward, const Vec3& up = Vec3(0, 1, 0)) {
    return glm::quatLookAt(glm::normalize(forward), glm::normalize(up));
}

inline Quat quat_from_axis_angle(const Vec3& axis, float angle) {
    return glm::angleAxis(angle, glm::normalize(axis));
}

// 矩阵工具
inline Mat4 mat4_identity() {
    return Mat4(1.0f);
}

inline Mat4 mat4_translate(const Vec3& v) {
    return glm::translate(Mat4(1.0f), v);
}

inline Mat4 mat4_rotate(const Quat& q) {
    return glm::mat4_cast(q);
}

inline Mat4 mat4_scale(const Vec3& s) {
    return glm::scale(Mat4(1.0f), s);
}

inline Mat4 mat4_trs(const Vec3& t, const Quat& r, const Vec3& s) {
    return glm::translate(Mat4(1.0f), t) * 
           glm::mat4_cast(r) * 
           glm::scale(Mat4(1.0f), s);
}

inline Mat4 mat4_look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
    return glm::lookAt(eye, center, up);
}

inline Mat4 mat4_perspective(float fovy, float aspect, float near, float far) {
    return glm::perspective(fovy, aspect, near, far);
}

inline Mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far) {
    return glm::ortho(left, right, bottom, top, near, far);
}

// 插值工具 (用于帧插值，参考 OpenCE render_interpolation.c)
struct InterpolatedTransform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

inline InterpolatedTransform interpolate_transform(
    const InterpolatedTransform& a,
    const InterpolatedTransform& b,
    float t)  // [0, 1]
{
    InterpolatedTransform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

// 六角地图坐标转换
struct HexCoord {
    int q, r;  // axial coordinates
    int s() const { return -q - r; }
};

inline Vec3 hex_to_world(const HexCoord& hex, float hex_size) {
    float x = hex_size * 1.5f * hex.q;
    float z = hex_size * MATH_SQRT(3.0f) * (hex.r + hex.q * 0.5f);
    return Vec3(x, 0, z);
}

inline HexCoord world_to_hex(const Vec3& world, float hex_size) {
    float q = world.x / (hex_size * 1.5f);
    float r = (world.z / MATH_SQRT(3.0f) - world.x * 0.5f) / hex_size;
    
    int iq = static_cast<int>(MATH_FLOOR(q + 0.5f));
    int ir = static_cast<int>(MATH_FLOOR(r + 0.5f));
    int is = static_cast<int>(MATH_FLOOR(-q - r + 0.5f));
    
    float dq = MATH_FMOD(q - iq, 0.0f) >= 0 ? q - iq : iq - q;
    float dr = MATH_FMOD(r - ir, 0.0f) >= 0 ? r - ir : ir - r;
    float ds = MATH_FMOD(-q - r - is, 0.0f) >= 0 ? -q - r - is : is + q + r;
    
    if (dq > dr && dq > ds) iq = -ir - is;
    else if (dr > ds) ir = -iq - is;
    
    return {iq, ir};
}

inline int hex_distance(const HexCoord& a, const HexCoord& b) {
    int dq = a.q - b.q;
    int dr = a.r - b.r;
    int ds = a.s() - b.s();
    return (abs(dq) + abs(dr) + abs(ds)) / 2;
}

// 固定时间步工具 (参考 OpenCE 30Hz 固定更新)
struct FixedTimestep {
    float accumulator = 0.0f;
    const float dt = 1.0f / 30.0f;  // 30 Hz
    const float max_dt = 0.25f;     // 最大帧时间
    
    template<typename Func>
    void update(float frame_time, Func&& fixed_update) {
        frame_time = std::min(frame_time, max_dt);
        accumulator += frame_time;
        
        while (accumulator >= dt) {
            fixed_update(dt);
            accumulator -= dt;
        }
    }
    
    float alpha() const { return accumulator / dt; }  // 插值因子 [0, 1]
};

// 随机数 (确定性, Xorshift32)
class DeterministicRNG {
    uint32_t state_;
public:
    explicit DeterministicRNG(uint32_t seed = 0x12345678) : state_(seed ? seed : 0x12345678) {}
    
    void seed(uint32_t s) { state_ = s ? s : 0x12345678; }
    
    uint32_t next_u32() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    
    float next_float() {
        return next_u32() * (1.0f / 4294967296.0f);
    }
    
    float next_float(float min, float max) {
        return min + next_float() * (max - min);
    }
    
    int next_int(int min, int max) {
        return min + int(next_u32() % uint32_t(max - min + 1));
    }
    
    bool next_bool(float probability = 0.5f) {
        return next_float() < probability;
    }
    
    Vec3 next_vec3(float radius = 1.0f) {
        float u = next_float();
        float v = next_float();
        float theta = 2.0f * PI * u;
        float phi = MATH_ACOS(2.0f * v - 1.0f);
        return vec3_from_spherical(radius, theta, phi);
    }
};

} // namespace math
} // namespace tensyoki