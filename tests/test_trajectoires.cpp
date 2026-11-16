#include <doctest/doctest.h>

#include <algorithm>

#include "trancheur/formes.hpp"
#include "trancheur/trajectoires.hpp"

using namespace trancheur;
using namespace Clipper2Lib;

namespace {

std::size_t compter(const PlanCouche& p, Role role) {
    return static_cast<std::size_t>(std::ranges::count_if(p.traces, [&](const Trace& t) { return t.role == role; }));
}

bool dans(const Paths64& zone, const Point64& point) {
    // Pair-impair : un point dans un trou n'est pas dans la pièce.
    int traverses = 0;
    for (const Path64& p : zone) {
        if (PointInPolygon(point, p) != PointInPolygonResult::IsOutside) {
            ++traverses;
        }
    }
    return traverses % 2 == 1;
}

}  // namespace

TEST_CASE("chaque contour reçoit le nombre de parois demandé") {
    const auto couches = trancher(bagueEpaulee(64), 0.2);
    Reglages r;
    r.perimetres = 3;
    const auto plans = planifier(couches, r);
    // La bague a un extérieur et un trou : trois tours pour chacun.
    CHECK(compter(plans[50], Role::Perimetre) == 6);
}

TEST_CASE("tout le remplissage reste dans la pièce") {
    const auto couches = trancher(bagueEpaulee(96), 0.2);
    const auto plans = planifier(couches, Reglages{});
    for (std::size_t i : {std::size_t{2}, std::size_t{25}, std::size_t{120}}) {
        for (const Trace& t : plans[i].traces) {
            for (const Point64& p : t.points) {
                CHECK(dans(couches[i].contours, p));
            }
        }
    }
}

TEST_CASE("les dessous, les dessus et l'épaulement sont pleins, le cœur ne l'est pas") {
    const auto couches = trancher(bagueEpaulee(96), 0.2);
    Reglages r;
    r.couchesPleines = 4;
    const auto plans = planifier(couches, r);

    CHECK(plans[0].surfacePleineMm2 > 500);    // le dessous de la pièce
    CHECK(plans[25].surfacePleineMm2 < 1);     // à mi-hauteur de la grosse partie : remplissage partiel
    CHECK(plans[48].surfacePleineMm2 > 300);   // juste sous l'épaulement (z = 10) : sa couronne est un dessus
    CHECK(plans[100].surfacePleineMm2 < 1);    // dans la partie fine
    CHECK(plans.back().surfacePleineMm2 > 100); // le dessus de la pièce
    CHECK(compter(plans[25], Role::Plein) == 0);
    CHECK(compter(plans[25], Role::Remplissage) > 0);
}

TEST_CASE("hachures : une rangée sur deux à l'envers") {
    const Paths64 carre{{{0, 0}, {100'000, 0}, {100'000, 100'000}, {0, 100'000}}};  // 10 × 10 mm
    const Paths64 traits = hachurer(carre, 1.0, 0);
    REQUIRE(traits.size() == 10);
    for (std::size_t i = 1; i < traits.size(); ++i) {
        // Aller-retour : chaque trait commence du côté où le précédent s'est arrêté.
        const bool precedentVersLaDroite = traits[i - 1].back().x > traits[i - 1].front().x;
        const bool versLaDroite = traits[i].back().x > traits[i].front().x;
        CHECK(precedentVersLaDroite != versLaDroite);
    }
}

TEST_CASE("une densité plus faible espace le remplissage") {
    const auto couches = trancher(bagueEpaulee(64), 0.2);
    Reglages dense, leger;
    dense.densite = 0.4;
    leger.densite = 0.1;
    CHECK(compter(planifier(couches, dense)[25], Role::Remplissage) > compter(planifier(couches, leger)[25], Role::Remplissage) * 3);
}
