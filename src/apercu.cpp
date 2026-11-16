#include "trancheur/apercu.hpp"

#include <algorithm>
#include <format>
#include <ostream>
#include <string>

namespace trancheur {

using namespace Clipper2Lib;

namespace {

std::string chemin(const Path64& p, bool ferme, double x0, double y0) {
    std::string d;
    for (std::size_t i = 0; i < p.size(); ++i) {
        // L'axe Y d'un SVG descend : on le retourne pour garder la vue de dessus habituelle.
        d += std::format("{}{:.2f},{:.2f}", i == 0 ? "M" : " L", versMm(p[i].x) - x0, y0 - versMm(p[i].y));
    }
    return ferme ? d + " Z" : d;
}

}  // namespace

void ecrireApercu(const std::vector<const Couche*>& couches, const std::vector<const PlanCouche*>& plans, std::ostream& o) {
    Rect64 bornes(true);
    for (const Couche* c : couches) {
        const Rect64 b = GetBounds(c->contours);
        bornes.left = std::min(bornes.left, b.left);
        bornes.top = std::min(bornes.top, b.top);
        bornes.right = std::max(bornes.right, b.right);
        bornes.bottom = std::max(bornes.bottom, b.bottom);
    }
    const double marge = 4, largeur = versMm(bornes.Width()) + 2 * marge, hauteur = versMm(bornes.Height()) + 2 * marge;
    const double legende = 9;
    const double total = largeur * static_cast<double>(couches.size());

    o << std::format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {:.1f} {:.1f}" width="100%" font-family="sans-serif">)",
                     total, hauteur + legende)
      << '\n'
      << std::format(R"(<rect width="{:.1f}" height="{:.1f}" fill="#fafafa"/>)", total, hauteur + legende) << '\n';

    for (std::size_t i = 0; i < couches.size(); ++i) {
        const double x0 = versMm(bornes.left) - marge - static_cast<double>(i) * largeur;
        const double y0 = versMm(bornes.bottom) + marge;
        o << std::format(R"(<path d="{}" fill="#e6e6e6" fill-rule="evenodd" stroke="#888" stroke-width="0.12"/>)",
                         [&] {
                             std::string d;
                             for (const Path64& p : couches[i]->contours) {
                                 d += chemin(p, true, x0, y0) + " ";
                             }
                             return d;
                         }())
          << '\n';
        for (const Trace& t : plans[i]->traces) {
            const char* couleur = t.role == Role::Perimetre ? "#1f5fbf" : t.role == Role::Plein ? "#c0392b" : "#e67e22";
            o << std::format(R"(<path d="{}" fill="none" stroke="{}" stroke-width="0.32" stroke-linejoin="round"/>)",
                             chemin(t.points, t.ferme, x0, y0), couleur)
              << '\n';
        }
        std::string z = std::format("{:.1f}", plans[i]->z);
        std::ranges::replace(z, '.', ',');
        o << std::format(R"(<text x="{:.1f}" y="{:.1f}" font-size="3.2" text-anchor="middle" fill="#333">couche {} · z = {} mm</text>)",
                         static_cast<double>(i) * largeur + largeur / 2, hauteur + legende - 2.5, plans[i]->indice, z)
          << '\n';
    }
    o << "</svg>\n";
}

}  // namespace trancheur
