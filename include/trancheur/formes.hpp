#pragma once

#include <utility>
#include <vector>

#include "trancheur/maillage.hpp"

namespace trancheur {

/// Un profil dans le plan (rayon, hauteur), fermé, que l'on fait tourner autour de l'axe Z.
using Profil = std::vector<std::pair<double, double>>;

/// Solide de révolution : la forme la plus courante des pièces tournées (bagues, axes, buses).
/// Un point de rayon nul se replie sur l'axe ; les triangles dégénérés qui en résultent sont écartés.
Maillage revolution(const Profil& profil, int segments);

/// Bague épaulée de démonstration : Ø40 sur 10 mm, puis Ø24 jusqu'à 30 mm, alésage Ø12 traversant.
Maillage bagueEpaulee(int segments = 128);

/// Sphère de rayon donné, pour les mesures de vitesse : 2 × segments × anneaux triangles environ.
Maillage sphere(double rayon, int segments, int anneaux);

/// Cube de côté donné, posé sur le plateau.
Maillage cube(double cote);

}  // namespace trancheur
