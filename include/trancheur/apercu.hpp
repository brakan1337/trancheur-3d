#pragma once

#include <iosfwd>
#include <vector>

#include "trancheur/trajectoires.hpp"

namespace trancheur {

/// Dessine des couches côte à côte en SVG : la section en gris, les parois en bleu,
/// les zones pleines en rouge et le remplissage en orange.
void ecrireApercu(const std::vector<const Couche*>& couches, const std::vector<const PlanCouche*>& plans, std::ostream& sortie);

}  // namespace trancheur
