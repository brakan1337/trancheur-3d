#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "trancheur/formes.hpp"
#include "trancheur/gcode.hpp"

using namespace trancheur;

TEST_CASE("la quantité de fil se calcule par la section du cordon") {
    // Cordon de 0,45 × 0,2 mm, fil de 1,75 mm : 0,09 / (π × 0,875²) = 0,03742 mm de fil par mm.
    CHECK(extrusionParMm(Reglages{}, Machine{}) == doctest::Approx(0.037418).epsilon(1e-4));
}

TEST_CASE("un trait de 10 mm pousse 0,374 mm de fil") {
    PlanCouche plan;
    plan.z = 0.2;
    plan.traces.push_back({Role::Remplissage, {{0, 0}, {100'000, 0}}, false});
    Machine machine;
    machine.retrait = 0;
    std::ostringstream g;
    const Bilan b = ecrireGcode({plan}, Reglages{}, machine, g);

    CHECK(b.filamentMm == doctest::Approx(0.37418).epsilon(1e-4));
    CHECK(g.str().find("G1 X10.000 Y0.000 E0.37418 F3600") != std::string::npos);
}

TEST_CASE("un long déplacement à vide retire le fil, un court non") {
    PlanCouche plan;
    plan.z = 0.2;
    plan.traces.push_back({Role::Remplissage, {{0, 0}, {10'000, 0}}, false});        // départ à 0 : pas de retrait
    plan.traces.push_back({Role::Remplissage, {{10'000, 5'000}, {0, 5'000}}, false});  // 0,5 mm plus loin : pas de retrait
    plan.traces.push_back({Role::Remplissage, {{500'000, 0}, {510'000, 0}}, false});   // 50 mm plus loin : retrait
    std::ostringstream g;
    ecrireGcode({plan}, Reglages{}, Machine{}, g);
    const std::string texte = g.str();

    std::size_t retraits = 0;
    for (auto pos = texte.find("E-0.8000"); pos != std::string::npos; pos = texte.find("E-0.8000", pos + 1)) {
        ++retraits;
    }
    CHECK(retraits == 1);
}

TEST_CASE("une paroi revient à son point de départ") {
    PlanCouche plan;
    plan.z = 0.2;
    plan.traces.push_back({Role::Perimetre, {{0, 0}, {100'000, 0}, {100'000, 100'000}}, true});
    std::ostringstream g;
    const Bilan b = ecrireGcode({plan}, Reglages{}, Machine{}, g);
    // 10 + 10 + diagonale de retour 14,14 mm
    CHECK(b.filamentMm == doctest::Approx((20 + 14.1421356) * extrusionParMm(Reglages{}, Machine{})).epsilon(1e-4));
}

TEST_CASE("toute une pièce : en-tête, une section par couche, fin") {
    const auto couches = trancher(bagueEpaulee(48), 0.3);
    const auto plans = planifier(couches, Reglages{});
    std::ostringstream g;
    const Bilan b = ecrireGcode(plans, Reglages{}, Machine{}, g);
    const std::string texte = g.str();

    CHECK(texte.starts_with("; G-code produit par trancheur"));
    CHECK(texte.find(";COUCHE 0 z=0.200") != std::string::npos);
    CHECK(texte.find(";COUCHE 99 ") != std::string::npos);
    CHECK(b.dureeS > 60);
    CHECK(b.filamentMm > 100);
}
