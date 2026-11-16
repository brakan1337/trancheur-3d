#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <vector>

namespace trancheur {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    friend bool operator==(const Vec3&, const Vec3&) = default;
};

struct Boite {
    Vec3 min, max;
};

using Triangle = std::array<Vec3, 3>;

/// Un maillage indexé : chaque sommet n'existe qu'une fois, les faces y renvoient.
/// C'est ce partage qui permet ensuite de raccorder les segments d'une couche sans tolérance.
struct Maillage {
    std::vector<Vec3> sommets;
    std::vector<std::array<std::uint32_t, 3>> faces;

    [[nodiscard]] Boite boite() const;
    [[nodiscard]] std::size_t nbTriangles() const { return faces.size(); }
};

class ErreurSTL : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Construit un maillage indexé à partir de triangles isolés (ce que contient un STL) :
/// les sommets identiques sont fusionnés, les triangles dégénérés écartés.
Maillage depuisTriangles(const std::vector<Triangle>& triangles);

/// Lit un STL binaire ou texte ; le format est reconnu tout seul.
Maillage lireSTL(std::istream& flux);
Maillage lireSTL(const std::filesystem::path& chemin);

void ecrireSTLBinaire(const Maillage& maillage, std::ostream& flux);

}  // namespace trancheur
