#pragma once

#include <iosfwd>
#include <vector>

#include "trancheur/trajectoires.hpp"

namespace trancheur {

struct Machine {
    double diametreFil = 1.75;         ///< mm
    double vitesseParoi = 30;          ///< mm/s
    double vitesseRemplissage = 60;    ///< mm/s
    double vitesseDeplacement = 150;   ///< mm/s, buse à vide
    double retrait = 0.8;              ///< mm de fil retiré avant un long déplacement, contre les fils d'ange
    double distanceRetrait = 2.0;      ///< mm, au-delà de laquelle on retire
    double origineX = 0, origineY = 0; ///< décalage appliqué aux coordonnées (centrer sur le plateau)
};

struct Bilan {
    double filamentMm = 0;
    double dureeS = 0;  ///< estimation sans accélérations
    std::size_t lignes = 0;
};

/// Millimètres de fil à pousser pour un millimètre de cordon : section du cordon / section du fil.
double extrusionParMm(const Reglages& reglages, const Machine& machine);

Bilan ecrireGcode(const std::vector<PlanCouche>& plans, const Reglages& reglages, const Machine& machine, std::ostream& sortie);

}  // namespace trancheur
