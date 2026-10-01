#include "math.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace math;
void expect(bool b, const char *message) {
    if (!b)
        throw std::runtime_error(message);
}
bool close(float a, float b) {
    return std::abs(a - b) < .0001f;
}
int main() {
    try {
        auto v = parallelepiped();
        auto a = v[1].position - v[0].position, b = v[2].position - v[0].position,
             c = v[4].position - v[0].position;
        expect(close(dot(a, cross(b, c)), 2.6f * 1.8f * 1.6f), "parallelepiped volume");
        for (int i = 0; i < 8; ++i) {
            auto p = v[0].position + a * float(bool(i & 1)) + b * float(bool(i & 2)) +
                     c * float(bool(i & 4));
            expect(dot(p - v[i].position, p - v[i].position) < 1e-8, "parallel edge geometry");
            expect(v[i].color.x >= 0 && v[i].color.x <= 1 && v[i].color.y >= 0 &&
                       v[i].color.y <= 1 && v[i].color.z >= 0 && v[i].color.z <= 1,
                   "color range");
        }
        float volume = 0;
        std::map<std::pair<int, int>, int> uses;
        for (size_t i = 0; i < triangles.size(); i += 3) {
            int x = triangles[i], y = triangles[i + 1], z = triangles[i + 2];
            auto p = v[x].position, q = v[y].position, r = v[z].position;
            expect(dot(cross(q - p, r - p), (p + q + r) * (1.f / 3)) > 0, "outward winding");
            volume += dot(p, cross(q, r)) / 6;
            for (auto e : {std::pair{x, y}, std::pair{y, z}, std::pair{z, x}}) {
                if (e.first > e.second)
                    std::swap(e.first, e.second);
                uses[e]++;
            }
        }
        expect(close(volume, 2.6f * 1.8f * 1.6f), "closed mesh signed volume");
        for (auto [e, n] : uses)
            expect(n == 2, "closed mesh edges");
        auto m = model({1, 2, 3}, {0, 0, 90}, {2, 3, 4});
        auto p = transform(m, {1, 0, 0, 1});
        expect(close(p[0], 1) && close(p[1], 4) && close(p[2], 3), "S then R then T");
        auto view = lookAt({0, 0, 5}, {0, 0, 0});
        p = transform(view, {0, 0, 5, 1});
        expect(close(p[0], 0) && close(p[1], 0) && close(p[2], 0), "camera maps to origin");
        for (auto projection :
             {perspective(radians(45), 1.6f, .1f, 100), ortho(3, 1.6f, .1f, 100)}) {
            auto n = transform(projection, {0, 0, -.1f, 1}),
                 f = transform(projection, {0, 0, -100, 1});
            expect(close(n[2] / n[3], 0) && close(f[2] / f[3], 1), "Vulkan depth endpoints");
        }
        auto pr = perspective(radians(45), 1, .1f, 100);
        auto p1 = transform(pr, {1, 0, -2, 1}), p2 = transform(pr, {1, 0, -4, 1});
        expect(close(p1[0] / p1[3], 2 * p2[0] / p2[3]), "perspective distance");
        auto ort = ortho(3, 1, .1f, 100);
        p1 = transform(ort, {1, 0, -2, 1});
        p2 = transform(ort, {1, 0, -4, 1});
        expect(close(p1[0], p2[0]), "orthographic distance invariance");
        Animation anim;
        anim.playing = true;
        anim.advance(.5);
        expect(anim.phase == .5, "animation time");
        anim.playing = false;
        anim.advance(20);
        expect(anim.phase == .5, "pause");
        anim.playing = true;
        anim.speed = 2;
        anim.advance(.25);
        expect(anim.phase == 1, "resume and speed");
        Animation slow, fast;
        slow.playing = fast.playing = true;
        for (int i = 0; i < 30; i++)
            slow.advance(1. / 30);
        for (int i = 0; i < 120; i++)
            fast.advance(1. / 120);
        expect(std::abs(slow.phase - fast.phase) < 1e-12, "frame independence");
        std::cout << "PASS: geometry, manifold, winding, volume, colors, transforms, projections, "
                     "animation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
