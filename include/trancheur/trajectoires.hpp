#pragma once

#include <clipper2/clipper.h>

#include <vector>

#include "trancheur/tranchage.hpp"

namespace trancheur {

struct Reglages {
    double largeurLigne = 0.45;   ///< mm, largeur d'un cordon
    double hauteurCouche = 0.2;   ///< mm
    int perimetres = 2;           ///< nombre de tours de paroi
    double densite = 0.20;        ///< remplissage de l'intérieur, de 0 à 1
    int couchesPleines = 4;       ///< épaisseur pleine sous les dessus et au-dessus des dessous
    double recouvrement = 0.15;   ///< part de cordon où le remplissage chevauche la paroi, pour bien l'accrocher
};

enum class Role { Perimetre, Plein, Remplissage };

/// Un trajet de buse qui dépose de la matière, en coordonnées entières (voir ECHELLE).
struct Trace {
    Role role;
    Clipper2Lib::Path64 points;
    bool ferme;  ///< une paroi revient à son point de départ
};

struct PlanCouche {
    std::size_t indice = 0;
    double z = 0;                 ///< hauteur de la buse en fin de couche
    std::vector<Trace> traces;
    double surfacePleineMm2 = 0;  ///< surface remplie à 100 % (dessus, dessous)
};

/// Hachures parallèles d'une zone, en aller-retour pour limiter les déplacements à vide.
Clipper2Lib::Paths64 hachurer(const Clipper2Lib::Paths64& zone, double espacementMm, double angleDegres);

/// Transforme les sections en trajets : parois, zones pleines là où la pièce a un dessus
/// ou un dessous proche, remplissage partiel ailleurs.
std::vector<PlanCouche> planifier(const std::vector<Couche>& couches, const Reglages& reglages);

}  // namespace trancheur
