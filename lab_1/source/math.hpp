#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace math {
constexpr float pi = 3.14159265358979323846f;
constexpr float radians(float d) {
    return d * pi / 180.f;
}
struct Vec3 {
    float x{}, y{}, z{};
};
inline Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalized(Vec3 v) {
    return v * (1.f / std::sqrt(dot(v, v)));
}
// Column-major storage; vectors are columns. Multiplication A*B applies B first.
struct alignas(16) Mat4 {
    std::array<float, 16> v{};
    float &at(int row, int col) {
        return v[col * 4 + row];
    }
    float at(int row, int col) const {
        return v[col * 4 + row];
    }
    static Mat4 identity() {
        Mat4 m;
        for (int i = 0; i < 4; ++i)
            m.at(i, i) = 1;
        return m;
    }
};
inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i < 4; ++i)
            for (int k = 0; k < 4; ++k)
                r.at(i, c) += a.at(i, k) * b.at(k, c);
    return r;
}
inline std::array<float, 4> transform(const Mat4 &a, std::array<float, 4> p) {
    std::array<float, 4> r{};
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k)
            r[i] += a.at(i, k) * p[k];
    return r;
}
inline Mat4 translation(Vec3 p) {
    auto m = Mat4::identity();
    m.at(0, 3) = p.x;
    m.at(1, 3) = p.y;
    m.at(2, 3) = p.z;
    return m;
}
inline Mat4 scale(Vec3 s) {
    auto m = Mat4::identity();
    m.at(0, 0) = s.x;
    m.at(1, 1) = s.y;
    m.at(2, 2) = s.z;
    return m;
}
inline Mat4 rotation(int axis, float angle) {
    auto m = Mat4::identity();
    int i = (axis + 1) % 3, j = (axis + 2) % 3;
    float c = std::cos(angle), s = std::sin(angle);
    m.at(i, i) = c;
    m.at(j, j) = c;
    m.at(i, j) = -s;
    m.at(j, i) = s;
    return m;
}
inline Mat4 model(Vec3 p, Vec3 r, Vec3 s) {
    return translation(p) * rotation(2, radians(r.z)) * rotation(1, radians(r.y)) *
           rotation(0, radians(r.x)) * scale(s);
}
inline Mat4 lookAt(Vec3 eye, Vec3 target) {
    Vec3 f = normalized(target - eye), s = normalized(cross(f, {0, 1, 0})), u = cross(s, f);
    auto m = Mat4::identity();
    m.at(0, 0) = s.x;
    m.at(0, 1) = s.y;
    m.at(0, 2) = s.z;
    m.at(0, 3) = -dot(s, eye);
    m.at(1, 0) = u.x;
    m.at(1, 1) = u.y;
    m.at(1, 2) = u.z;
    m.at(1, 3) = -dot(u, eye);
    m.at(2, 0) = -f.x;
    m.at(2, 1) = -f.y;
    m.at(2, 2) = -f.z;
    m.at(2, 3) = dot(f, eye);
    return m;
}
// Right-handed camera, Vulkan depth [0,1], flipped Y for a positive viewport height.
inline Mat4 perspective(float fov, float aspect, float near, float far) {
    Mat4 m;
    float f = 1 / std::tan(fov / 2);
    m.at(0, 0) = f / aspect;
    m.at(1, 1) = -f;
    m.at(2, 2) = far / (near - far);
    m.at(2, 3) = far * near / (near - far);
    m.at(3, 2) = -1;
    return m;
}
inline Mat4 ortho(float halfHeight, float aspect, float near, float far) {
    auto m = Mat4::identity();
    m.at(0, 0) = 1 / (halfHeight * aspect);
    m.at(1, 1) = -1 / halfHeight;
    m.at(2, 2) = 1 / (near - far);
    m.at(2, 3) = near / (near - far);
    return m;
}
struct Animation {
    double phase = 0;
    float speed = 1, radius = 1.5f, height = .7f, frequency = 2;
    bool playing = false;
    void advance(double dt) {
        if (playing)
            phase += std::max(0., dt) * speed;
    }
    Vec3 offset() const {
        return {radius * float(std::cos(phase)), height * float(std::sin(frequency * phase)),
                radius * .6f * float(std::sin(phase))};
    }
    Vec3 angles() const {
        return {float(std::fmod(phase * 23, 360.)), float(std::fmod(phase * 45, 360.)),
                float(std::fmod(phase * 11, 360.))};
    }
};
struct Vertex {
    Vec3 position;
    Vec3 color;
};
inline std::array<Vertex, 8> parallelepiped() {
    std::array<Vertex, 8> result{};
    for (int i = 0; i < 8; ++i) {
        float a = (i & 1) ? .5f : -.5f, b = (i & 2) ? .5f : -.5f, c = (i & 4) ? .5f : -.5f;
        Vec3 p = {2.6f * a + .7f * c, 1.8f * b + .35f * c, 1.6f * c};
        result[i] = {p, {(p.x + 1.65f) / 3.3f, (p.y + 1.075f) / 2.15f, (p.z + .8f) / 1.6f}};
    }
    return result;
}
inline constexpr std::array<uint16_t, 36> triangles = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6,
                                                       0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5,
                                                       0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3};
inline constexpr std::array<uint16_t, 24> edges = {0, 1, 2, 3, 4, 5, 6, 7, 0, 2, 1, 3,
                                                   4, 6, 5, 7, 0, 4, 1, 5, 2, 6, 3, 7};
} // namespace math
