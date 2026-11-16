#include "trancheur/tranchage.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_map>

namespace trancheur {

using namespace Clipper2Lib;

namespace {

/// Une arête du maillage, désignée par ses deux sommets, le plus petit d'abord.
/// Deux triangles voisins coupent leur arête commune au même point : on raccorde les segments
/// par cette clé, sans comparer des coordonnées à une tolérance près.
std::uint64_t cleArete(std::uint32_t a, std::uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (std::uint64_t{a} << 32) | b;
}

struct Segment {
    std::uint64_t debut, fin;
    Point64 pDebut, pFin;
};

Point64 intersection(const Vec3& a, const Vec3& b, double z) {
    const double t = (z - a.z) / (static_cast<double>(b.z) - a.z);
    return {versEntier(a.x + t * (b.x - a.x)), versEntier(a.y + t * (b.y - a.y))};
}

/// Coupe un triangle : 0 ou 1 segment. Un sommet pile à la hauteur z compte « en dessous » :
/// chaque triangle est ainsi soit traversé franchement, soit ignoré, sans cas particulier.
bool couperTriangle(const Maillage& m, const std::array<std::uint32_t, 3>& f, double z, Segment& sortie) {
    std::uint64_t cles[2];
    Point64 points[2];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t ia = f[static_cast<std::size_t>(i)];
        const std::uint32_t ib = f[static_cast<std::size_t>((i + 1) % 3)];
        const Vec3& a = m.sommets[ia];
        const Vec3& b = m.sommets[ib];
        if ((a.z > z) != (b.z > z)) {
            // Toujours calculé du plus petit indice vers le plus grand : le point est identique au bit près
            // pour les deux triangles qui partagent l'arête.
            points[n] = ia < ib ? intersection(a, b, z) : intersection(b, a, z);
            cles[n] = cleArete(ia, ib);
            ++n;
        }
    }
    if (n != 2) {
        return false;
    }
    sortie = {cles[0], cles[1], points[0], points[1]};
    return true;
}

/// Raccorde les segments en boucles fermées. Chaque arête coupée est partagée par exactement
/// deux segments dans un maillage étanche ; une chaîne qui ne se referme pas trahit un trou.
Couche chainer(const std::vector<Segment>& segments, double z) {
    Couche couche{z, {}, 0};
    std::unordered_multimap<std::uint64_t, std::size_t> parArete;
    parArete.reserve(segments.size() * 2);
    for (std::size_t i = 0; i < segments.size(); ++i) {
        parArete.emplace(segments[i].debut, i);
        parArete.emplace(segments[i].fin, i);
    }

    std::vector<bool> utilise(segments.size(), false);
    Paths64 boucles;
    for (std::size_t depart = 0; depart < segments.size(); ++depart) {
        if (utilise[depart]) {
            continue;
        }
        Path64 boucle{segments[depart].pDebut};
        utilise[depart] = true;
        std::uint64_t cle = segments[depart].fin;
        Point64 point = segments[depart].pFin;
        const std::uint64_t cleDepart = segments[depart].debut;
        bool fermee = false;

        while (true) {
            if (cle == cleDepart) {
                fermee = true;
                break;
            }
            boucle.push_back(point);
            std::size_t suivant = segments.size();
            auto [debut, fin] = parArete.equal_range(cle);
            for (auto it = debut; it != fin; ++it) {
                if (!utilise[it->second]) {
                    suivant = it->second;
                    break;
                }
            }
            if (suivant == segments.size()) {
                break;  // chaîne interrompue : maillage non étanche
            }
            utilise[suivant] = true;
            const Segment& s = segments[suivant];
            // Les segments ne sont pas orientés : on repart par l'autre bout.
            if (s.debut == cle) {
                cle = s.fin;
                point = s.pFin;
            } else {
                cle = s.debut;
                point = s.pDebut;
            }
        }
        if (fermee && boucle.size() >= 3) {
            boucles.push_back(std::move(boucle));
        } else {
            ++couche.contoursOuverts;
        }
    }

    // Les boucles brutes n'ont pas d'orientation fiable. Une union pair-impair en fait des extérieurs
    // et des trous correctement orientés, quel que soit le sens des triangles d'origine.
    couche.contours = Union(boucles, FillRule::EvenOdd);
    return couche;
}

}  // namespace

Couche couper(const Maillage& maillage, double z) {
    std::vector<Segment> segments;
    Segment s;
    for (const auto& f : maillage.faces) {
        if (couperTriangle(maillage, f, z, s)) {
            segments.push_back(s);
        }
    }
    return chainer(segments, z);
}

std::vector<Couche> trancher(const Maillage& maillage, double hauteurCouche, unsigned fils) {
    if (maillage.faces.empty() || hauteurCouche <= 0) {
        return {};
    }
    const Boite b = maillage.boite();
    const auto nbCouches = static_cast<std::size_t>(std::floor((b.max.z - b.min.z) / hauteurCouche));
    auto hauteur = [&](std::size_t k) { return b.min.z + (static_cast<double>(k) + 0.5) * hauteurCouche; };

    // On range chaque triangle dans les couches qu'il traverse : chaque couche ne regarde ensuite
    // que ses triangles, au lieu de parcourir toute la pièce.
    std::vector<std::vector<std::uint32_t>> parCouche(nbCouches);
    for (std::uint32_t i = 0; i < maillage.faces.size(); ++i) {
        const auto& f = maillage.faces[i];
        const auto [zmin, zmax] = std::minmax({maillage.sommets[f[0]].z, maillage.sommets[f[1]].z, maillage.sommets[f[2]].z});
        const double premiere = std::ceil((zmin - b.min.z) / hauteurCouche - 0.5);
        const double derniere = std::floor((zmax - b.min.z) / hauteurCouche - 0.5);
        for (auto k = static_cast<std::ptrdiff_t>(std::max(premiere, 0.0));
             k <= static_cast<std::ptrdiff_t>(derniere) && k < static_cast<std::ptrdiff_t>(nbCouches); ++k) {
            parCouche[static_cast<std::size_t>(k)].push_back(i);
        }
    }

    std::vector<Couche> couches(nbCouches);
    auto travailler = [&](std::size_t premier, std::size_t pas) {
        std::vector<Segment> segments;
        Segment s;
        for (std::size_t k = premier; k < nbCouches; k += pas) {
            segments.clear();
            for (std::uint32_t i : parCouche[k]) {
                if (couperTriangle(maillage, maillage.faces[i], hauteur(k), s)) {
                    segments.push_back(s);
                }
            }
            couches[k] = chainer(segments, hauteur(k));
        }
    };

    // Les couches sont indépendantes : chaque fil en prend une sur n, sans verrou.
    const unsigned n = std::max(1u, fils == 0 ? std::thread::hardware_concurrency() : fils);
    std::vector<std::jthread> ouvriers;
    for (unsigned t = 1; t < n; ++t) {
        ouvriers.emplace_back(travailler, t, n);
    }
    travailler(0, n);
    // On attend explicitement tous les fils : la valeur de retour est construite avant la destruction
    // des variables locales, donc avant que les jthread ne se rejoignent d'eux-mêmes.
    ouvriers.clear();
    return couches;
}

double surfaceMm2(const Paths64& polygones) { return Area(polygones) / (ECHELLE * ECHELLE); }

}  // namespace trancheur
