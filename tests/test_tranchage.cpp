#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include "trancheur/formes.hpp"
#include "trancheur/tranchage.hpp"

using namespace trancheur;

TEST_CASE("un cube coupé en son milieu donne un carré") {
    const Couche c = couper(cube(20), 10);
    REQUIRE(c.contours.size() == 1);
    CHECK(c.contoursOuverts == 0);
    CHECK(surfaceMm2(c.contours) == doctest::Approx(400));
}

TEST_CASE("une bague donne un contour extérieur et un trou") {
    const int segments = 256;
    const Couche c = couper(bagueEpaulee(segments), 5);
    REQUIRE(c.contours.size() == 2);

    // Un polygone régulier à n côtés inscrit dans un cercle de rayon r a pour aire (n/2)·r²·sin(2π/n).
    auto polygone = [&](double r) { return segments / 2.0 * r * r * std::sin(2 * std::numbers::pi / segments); };
    CHECK(surfaceMm2(c.contours) == doctest::Approx(polygone(20) - polygone(6)).epsilon(1e-6));

    // L'extérieur et le trou tournent en sens contraires : c'est ce qui fait du trou un trou.
    const double a0 = Clipper2Lib::Area(c.contours[0]), a1 = Clipper2Lib::Area(c.contours[1]);
    CHECK(a0 * a1 < 0);
}

TEST_CASE("couper pile à la hauteur d'un sommet ne casse rien") {
    // L'épaulement de la bague est exactement à 10 mm : des sommets sont posés sur le plan de coupe.
    const Couche c = couper(bagueEpaulee(64), 10.0);
    CHECK(c.contoursOuverts == 0);
    for (const auto& p : c.contours) {
        CHECK(p.size() >= 3);
    }
}

TEST_CASE("un maillage troué est signalé, sans planter") {
    Maillage m = cube(20);
    m.faces.erase(m.faces.begin() + 4);  // on retire un triangle d'une face latérale
    const Couche c = couper(m, 10);
    CHECK(c.contoursOuverts == 1);
}

TEST_CASE("trancher toute la pièce") {
    const auto couches = trancher(bagueEpaulee(64), 0.2);
    CHECK(couches.size() == 150);  // 30 mm / 0,2 mm
    CHECK(couches.front().z == doctest::Approx(0.1));
    CHECK(couches.back().z == doctest::Approx(29.9));

    // Sous l'épaulement, la grande section ; au-dessus, la petite.
    CHECK(surfaceMm2(couches[10].contours) > surfaceMm2(couches[100].contours) * 2);
}

TEST_CASE("le résultat ne dépend pas du nombre de fils") {
    const Maillage s = sphere(25, 96, 48);
    const auto un = trancher(s, 0.3, 1);
    const auto plusieurs = trancher(s, 0.3, 8);
    REQUIRE(un.size() == plusieurs.size());
    for (std::size_t i = 0; i < un.size(); ++i) {
        CHECK(surfaceMm2(un[i].contours) == surfaceMm2(plusieurs[i].contours));
    }
}

TEST_CASE("la section d'une sphère suit la formule du disque") {
    const auto couches = trancher(sphere(25, 512, 256), 0.5);
    for (std::size_t i = 10; i < couches.size() - 10; i += 10) {
        const double h = couches[i].z - 25;
        const double attendu = std::numbers::pi * (25 * 25 - h * h);
        CHECK(surfaceMm2(couches[i].contours) == doctest::Approx(attendu).epsilon(0.002));
    }
}
