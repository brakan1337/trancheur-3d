#include <doctest/doctest.h>

#include <sstream>

#include "trancheur/formes.hpp"
#include "trancheur/maillage.hpp"

using namespace trancheur;

TEST_CASE("les sommets répétés d'un STL sont fusionnés") {
    const Maillage c = cube(20);
    CHECK(c.nbTriangles() == 12);
    CHECK(c.sommets.size() == 8);
    const Boite b = c.boite();
    CHECK(b.max.x == doctest::Approx(20));
    CHECK(b.min.z == doctest::Approx(0));
}

TEST_CASE("les triangles dégénérés sont écartés") {
    // Une sphère se referme sur l'axe : les triangles des pôles n'ont que deux sommets distincts.
    const Maillage s = sphere(10, 32, 16);
    for (const auto& f : s.faces) {
        CHECK(f[0] != f[1]);
        CHECK(f[1] != f[2]);
        CHECK(f[0] != f[2]);
    }
}

TEST_CASE("STL binaire : écrire puis relire donne le même maillage") {
    const Maillage source = bagueEpaulee(48);
    std::stringstream flux;
    ecrireSTLBinaire(source, flux);
    const Maillage relu = lireSTL(flux);
    CHECK(relu.nbTriangles() == source.nbTriangles());
    CHECK(relu.sommets.size() == source.sommets.size());
}

TEST_CASE("STL texte") {
    std::istringstream flux(R"(solid essai
  facet normal 0 0 1
    outer loop
      vertex 0 0 0
      vertex 10 0 0
      vertex 0 10 0
    endloop
  endfacet
  facet normal 0 0 1
    outer loop
      vertex 10 0 0
      vertex 10 10 0
      vertex 0 10 0
    endloop
  endfacet
endsolid essai
)");
    const Maillage m = lireSTL(flux);
    CHECK(m.nbTriangles() == 2);
    CHECK(m.sommets.size() == 4);
}

TEST_CASE("un fichier qui n'est pas un STL est refusé clairement") {
    std::istringstream flux("ceci n'est pas une pièce");
    CHECK_THROWS_AS(lireSTL(flux), ErreurSTL);

    std::istringstream incomplet("solid x\n vertex 0 0 0\n vertex 1 0 0\nendsolid");
    CHECK_THROWS_WITH_AS(lireSTL(incomplet), "STL texte : le dernier triangle est incomplet", ErreurSTL);
}
