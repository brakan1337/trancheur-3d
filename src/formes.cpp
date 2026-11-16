#include "trancheur/formes.hpp"

#include <cmath>
#include <numbers>

namespace trancheur {

Maillage revolution(const Profil& profil, int segments) {
    std::vector<Triangle> triangles;
    const auto m = profil.size();
    triangles.reserve(static_cast<std::size_t>(segments) * m * 2);

    auto sommet = [&](int i, std::size_t j) {
        const double angle = 2.0 * std::numbers::pi * (i % segments) / segments;
        const auto [r, z] = profil[j % m];
        return Vec3{static_cast<float>(r * std::cos(angle)), static_cast<float>(r * std::sin(angle)), static_cast<float>(z)};
    };

    for (int i = 0; i < segments; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            const Vec3 a = sommet(i, j), b = sommet(i + 1, j), c = sommet(i + 1, j + 1), d = sommet(i, j + 1);
            triangles.push_back({a, b, c});
            triangles.push_back({a, c, d});
        }
    }
    return depuisTriangles(triangles);
}

Maillage bagueEpaulee(int segments) {
    return revolution({{6, 0}, {20, 0}, {20, 10}, {12, 10}, {12, 30}, {6, 30}}, segments);
}

Maillage sphere(double rayon, int segments, int anneaux) {
    Profil demiCercle;
    for (int k = 0; k <= anneaux; ++k) {
        const double phi = std::numbers::pi * k / anneaux;  // du pôle sud au pôle nord
        demiCercle.emplace_back(rayon * std::sin(phi), rayon - rayon * std::cos(phi));
    }
    return revolution(demiCercle, segments);
}

Maillage cube(double c) {
    const auto f = static_cast<float>(c);
    const Vec3 p[8] = {{0, 0, 0}, {f, 0, 0}, {f, f, 0}, {0, f, 0}, {0, 0, f}, {f, 0, f}, {f, f, f}, {0, f, f}};
    const int faces[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                              {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    std::vector<Triangle> t;
    for (const auto& face : faces) {
        t.push_back({p[face[0]], p[face[1]], p[face[2]]});
    }
    return depuisTriangles(t);
}

}  // namespace trancheur
