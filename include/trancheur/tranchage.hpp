#pragma once

#include <clipper2/clipper.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "trancheur/maillage.hpp"

namespace trancheur {

/// Les polygones sont en entiers (ce qu'attend Clipper2) : une unité vaut 0,1 µm.
/// Les calculs de surfaces et de décalages sont ainsi exacts, sans dérive de virgule flottante.
inline constexpr double ECHELLE = 10'000.0;

inline std::int64_t versEntier(double mm) { return std::llround(mm * ECHELLE); }
inline double versMm(std::int64_t v) { return static_cast<double>(v) / ECHELLE; }

/// La section de la pièce à une hauteur : contours extérieurs et trous, déjà orientés
/// (Clipper2 les normalise : un extérieur tourne dans un sens, un trou dans l'autre).
struct Couche {
    double z = 0;  ///< hauteur de coupe, au milieu de la couche
    Clipper2Lib::Paths64 contours;
    std::size_t contoursOuverts = 0;  ///< segments qui ne se referment pas : maillage troué à cet endroit
};

/// Coupe le maillage par le plan horizontal z.
Couche couper(const Maillage& maillage, double z);

/// Découpe toute la pièce en couches de hauteur donnée, réparties sur `fils` cœurs (0 : tous).
std::vector<Couche> trancher(const Maillage& maillage, double hauteurCouche, unsigned fils = 0);

/// Surface d'une couche en mm².
double surfaceMm2(const Clipper2Lib::Paths64& polygones);

}  // namespace trancheur
