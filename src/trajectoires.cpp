#include "trancheur/trajectoires.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <thread>

namespace trancheur {

using namespace Clipper2Lib;

namespace {

Paths64 decaler(const Paths64& p, double mm) {
    // Angles vifs conservés (Miter) jusqu'à deux fois le décalage, arrondis au-delà.
    return InflatePaths(p, mm * ECHELLE, JoinType::Miter, EndType::Polygon, 2.0);
}

/// Applique f à 0..n-1 sur tous les cœurs, une valeur sur k par fil. Chaque appel n'écrit que
/// dans sa propre case : aucun verrou n'est nécessaire, et Clipper2 n'a pas d'état global.
template <typename F>
void enParallele(std::size_t n, F f) {
    const std::size_t fils = std::max(1u, std::thread::hardware_concurrency());
    {
        std::vector<std::jthread> ouvriers;
        for (std::size_t k = 1; k < fils; ++k) {
            ouvriers.emplace_back([&, k] {
                for (std::size_t i = k; i < n; i += fils) f(i);
            });
        }
        for (std::size_t i = 0; i < n; i += fils) f(i);
    }  // les jthread se rejoignent ici, avant que l'appelant ne lise les résultats
}

double longueur(const Path64& p) {
    double total = 0;
    for (std::size_t i = 1; i < p.size(); ++i) {
        total += std::hypot(static_cast<double>(p[i].x - p[i - 1].x), static_cast<double>(p[i].y - p[i - 1].y));
    }
    return total;
}

}  // namespace

Paths64 hachurer(const Paths64& zone, double espacementMm, double angleDegres) {
    if (zone.empty() || espacementMm <= 0) {
        return {};
    }
    const Rect64 bornes = GetBounds(zone);
    const double cx = (static_cast<double>(bornes.left) + static_cast<double>(bornes.right)) / 2;
    const double cy = (static_cast<double>(bornes.top) + static_cast<double>(bornes.bottom)) / 2;
    const double rayon = std::hypot(static_cast<double>(bornes.Width()), static_cast<double>(bornes.Height())) / 2 + 1;
    const double a = angleDegres * std::numbers::pi / 180;
    const double ux = std::cos(a), uy = std::sin(a);  // direction des hachures
    const double nx = -uy, ny = ux;                   // direction perpendiculaire
    const double pas = espacementMm * ECHELLE;

    // Des droites qui couvrent toute la zone, puis découpées par elle.
    Paths64 droites;
    for (double d = -rayon; d <= rayon; d += pas) {
        const double px = cx + d * nx, py = cy + d * ny;
        droites.push_back({Point64(px - rayon * ux, py - rayon * uy), Point64(px + rayon * ux, py + rayon * uy)});
    }
    Clipper64 decoupe;
    decoupe.AddOpenSubject(droites);
    decoupe.AddClip(zone);
    Paths64 fermes, ouverts;
    decoupe.Execute(ClipType::Intersection, FillRule::NonZero, fermes, ouverts);

    // Rangées dans l'ordre, et une rangée sur deux à l'envers : la buse fait des allers-retours.
    struct Rangee {
        long long rang;
        double position;
        Path64 trait;
    };
    std::vector<Rangee> rangees;
    for (Path64& t : ouverts) {
        if (t.size() < 2) {
            continue;
        }
        const double mx = (static_cast<double>(t.front().x) + static_cast<double>(t.back().x)) / 2 - cx;
        const double my = (static_cast<double>(t.front().y) + static_cast<double>(t.back().y)) / 2 - cy;
        rangees.push_back({std::llround((mx * nx + my * ny) / pas), mx * ux + my * uy, std::move(t)});
    }
    std::ranges::sort(rangees, [](const Rangee& g, const Rangee& h) {
        return g.rang != h.rang ? g.rang < h.rang : g.position < h.position;
    });

    Paths64 resultat;
    for (auto debut = rangees.begin(); debut != rangees.end();) {
        auto fin = std::find_if(debut, rangees.end(), [&](const Rangee& r) { return r.rang != debut->rang; });
        const bool retour = (debut->rang % 2) != 0;
        if (retour) {
            std::reverse(debut, fin);
        }
        for (auto it = debut; it != fin; ++it) {
            Path64 trait = std::move(it->trait);
            // Le trait suit la direction de la rangée.
            const double sens = static_cast<double>(trait.back().x - trait.front().x) * ux +
                                static_cast<double>(trait.back().y - trait.front().y) * uy;
            if ((sens < 0) != retour) {
                std::reverse(trait.begin(), trait.end());
            }
            resultat.push_back(std::move(trait));
        }
        debut = fin;
    }
    return resultat;
}

std::vector<PlanCouche> planifier(const std::vector<Couche>& couches, const Reglages& r) {
    const double w = r.largeurLigne;
    const std::size_t n = couches.size();

    // L'intérieur de chaque couche : ce qui reste une fois les parois posées.
    std::vector<Paths64> interieurs(n);
    enParallele(n, [&](std::size_t i) { interieurs[i] = decaler(couches[i].contours, -(w * r.perimetres) + w * r.recouvrement); });

    // Ce qui est recouvert sur les k couches au-dessus (ou au-dessous). Hors de la pièce : rien.
    const auto k = static_cast<std::size_t>(std::max(r.couchesPleines, 0));
    auto couvert = [&](std::size_t i, bool dessus) {
        Paths64 commun;
        for (std::size_t j = 1; j <= k; ++j) {
            if (dessus ? i + j >= n : j > i) {
                return Paths64{};
            }
            const Paths64& autre = interieurs[dessus ? i + j : i - j];
            commun = j == 1 ? autre : Intersect(commun, autre, FillRule::NonZero);
            if (commun.empty()) {
                break;
            }
        }
        return commun;
    };

    std::vector<PlanCouche> plans(n);
    enParallele(n, [&](std::size_t i) {
        PlanCouche& plan = plans[i];
        plan.indice = i;
        plan.z = (static_cast<double>(i) + 1) * r.hauteurCouche;

        // Les parois, de l'intérieur vers l'extérieur : la paroi visible est posée en dernier, contre une paroi déjà là.
        for (int p = r.perimetres - 1; p >= 0; --p) {
            for (Path64& tour : decaler(couches[i].contours, -w * (0.5 + p))) {
                plan.traces.push_back({Role::Perimetre, std::move(tour), true});
            }
        }

        // Plein là où l'intérieur n'est pas recouvert à la fois dessus et dessous : c'est une peau.
        const Paths64 soutien = k == 0 ? interieurs[i] : Intersect(couvert(i, true), couvert(i, false), FillRule::NonZero);
        const Paths64 plein = Difference(interieurs[i], soutien, FillRule::NonZero);
        const Paths64 creux = Difference(interieurs[i], plein, FillRule::NonZero);
        plan.surfacePleineMm2 = surfaceMm2(plein);

        const double angle = i % 2 == 0 ? 45.0 : -45.0;  // croisé d'une couche à l'autre
        for (Path64& t : hachurer(plein, w, angle)) {
            if (longueur(t) >= w * ECHELLE) {
                plan.traces.push_back({Role::Plein, std::move(t), false});
            }
        }
        if (r.densite > 0) {
            for (Path64& t : hachurer(creux, w / std::min(r.densite, 1.0), angle)) {
                if (longueur(t) >= w * ECHELLE) {
                    plan.traces.push_back({Role::Remplissage, std::move(t), false});
                }
            }
        }
    });
    return plans;
}

}  // namespace trancheur
