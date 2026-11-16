#include "trancheur/maillage.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <istream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace trancheur {

Boite Maillage::boite() const {
    Boite b{sommets.empty() ? Vec3{} : sommets.front(), sommets.empty() ? Vec3{} : sommets.front()};
    for (const Vec3& s : sommets) {
        b.min = {std::min(b.min.x, s.x), std::min(b.min.y, s.y), std::min(b.min.z, s.z)};
        b.max = {std::max(b.max.x, s.x), std::max(b.max.y, s.y), std::max(b.max.z, s.z)};
    }
    return b;
}

namespace {

// Un STL répète chaque sommet dans chaque triangle, avec exactement les mêmes octets :
// on fusionne sur la représentation binaire, sans tolérance qui pourrait coller deux points distincts.
struct CleSommet {
    std::uint32_t x, y, z;
    bool operator==(const CleSommet&) const = default;
};

struct HachageSommet {
    std::size_t operator()(const CleSommet& c) const noexcept {
        std::uint64_t h = c.x;
        h = h * 0x9E3779B97F4A7C15ULL ^ c.y;
        h = h * 0x9E3779B97F4A7C15ULL ^ c.z;
        return static_cast<std::size_t>(h ^ (h >> 31));
    }
};

CleSommet cle(const Vec3& v) {
    // -0.0 et +0.0 désignent le même point.
    auto bits = [](float f) { return std::bit_cast<std::uint32_t>(f == 0.0f ? 0.0f : f); };
    return {bits(v.x), bits(v.y), bits(v.z)};
}

template <typename T>
T lireBrut(const char* donnees) {
    T valeur;
    std::memcpy(&valeur, donnees, sizeof(T));
    return valeur;
}

std::vector<Triangle> lireBinaire(const std::string& contenu) {
    const auto nombre = lireBrut<std::uint32_t>(contenu.data() + 80);
    std::vector<Triangle> triangles;
    triangles.reserve(nombre);
    const char* p = contenu.data() + 84;
    for (std::uint32_t i = 0; i < nombre; ++i, p += 50) {
        Triangle t;
        for (std::size_t s = 0; s < 3; ++s) {
            const char* v = p + 12 + 12 * s;  // les 12 premiers octets sont la normale, recalculable
            t[s] = {lireBrut<float>(v), lireBrut<float>(v + 4), lireBrut<float>(v + 8)};
        }
        triangles.push_back(t);
    }
    return triangles;
}

std::vector<Triangle> lireTexte(const std::string& contenu) {
    std::istringstream flux(contenu);
    std::vector<Triangle> triangles;
    Triangle courant;
    std::size_t rang = 0;
    std::string mot;
    while (flux >> mot) {
        if (mot != "vertex") {
            continue;
        }
        Vec3 v;
        if (!(flux >> v.x >> v.y >> v.z)) {
            throw ErreurSTL("STL texte : sommet illisible");
        }
        courant[rang++] = v;
        if (rang == 3) {
            triangles.push_back(courant);
            rang = 0;
        }
    }
    if (rang != 0) {
        throw ErreurSTL("STL texte : le dernier triangle est incomplet");
    }
    return triangles;
}

}  // namespace

Maillage depuisTriangles(const std::vector<Triangle>& triangles) {
    Maillage m;
    std::unordered_map<CleSommet, std::uint32_t, HachageSommet> index;
    index.reserve(triangles.size() / 2 + 16);
    m.faces.reserve(triangles.size());

    auto indice = [&](const Vec3& v) {
        auto [it, nouveau] = index.try_emplace(cle(v), static_cast<std::uint32_t>(m.sommets.size()));
        if (nouveau) {
            m.sommets.push_back(v);
        }
        return it->second;
    };

    for (const Triangle& t : triangles) {
        const std::array<std::uint32_t, 3> f{indice(t[0]), indice(t[1]), indice(t[2])};
        if (f[0] != f[1] && f[1] != f[2] && f[0] != f[2]) {
            m.faces.push_back(f);
        }
    }
    return m;
}

Maillage lireSTL(std::istream& flux) {
    const std::string contenu{std::istreambuf_iterator<char>(flux), std::istreambuf_iterator<char>()};
    if (contenu.size() >= 84) {
        const auto nombre = lireBrut<std::uint32_t>(contenu.data() + 80);
        // Un STL binaire a une taille exacte ; certains commencent pourtant par « solid ».
        if (contenu.size() == 84 + std::size_t{nombre} * 50) {
            return depuisTriangles(lireBinaire(contenu));
        }
    }
    if (contenu.starts_with("solid")) {
        return depuisTriangles(lireTexte(contenu));
    }
    throw ErreurSTL("ce fichier n'est ni un STL binaire, ni un STL texte");
}

Maillage lireSTL(const std::filesystem::path& chemin) {
    std::ifstream flux(chemin, std::ios::binary);
    if (!flux) {
        throw ErreurSTL("impossible d'ouvrir " + chemin.string());
    }
    return lireSTL(flux);
}

void ecrireSTLBinaire(const Maillage& maillage, std::ostream& flux) {
    char entete[80] = "STL ecrit par trancheur";
    flux.write(entete, sizeof entete);
    const auto nombre = static_cast<std::uint32_t>(maillage.faces.size());
    flux.write(reinterpret_cast<const char*>(&nombre), sizeof nombre);
    const float normale[3] = {0, 0, 0};
    const std::uint16_t attributs = 0;
    for (const auto& f : maillage.faces) {
        flux.write(reinterpret_cast<const char*>(normale), sizeof normale);
        for (std::uint32_t i : f) {
            const Vec3& v = maillage.sommets[i];
            const float xyz[3] = {v.x, v.y, v.z};
            flux.write(reinterpret_cast<const char*>(xyz), sizeof xyz);
        }
        flux.write(reinterpret_cast<const char*>(&attributs), sizeof attributs);
    }
}

}  // namespace trancheur
