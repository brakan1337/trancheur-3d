// Mesure de vitesse : une sphère très fine, du fichier STL au G-code.
//   banc [segments]   (2 000 segments : environ 8 millions de triangles)

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>

#include "trancheur/formes.hpp"
#include "trancheur/gcode.hpp"
#include "trancheur/maillage.hpp"

using namespace trancheur;

namespace {

template <typename F>
double chrono(F&& f) {
    const auto debut = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - debut).count();
}

}  // namespace

int main(int argc, char** argv) {
    const int segments = argc > 1 ? std::atoi(argv[1]) : 1000;
    const Maillage source = sphere(50, segments, segments / 2);

    std::stringstream stl;
    ecrireSTLBinaire(source, stl);
    Maillage m;
    const double tLecture = chrono([&] { m = lireSTL(stl); });

    std::vector<Couche> couches, couches1;
    const double tUnFil = chrono([&] { couches1 = trancher(m, 0.2, 1); });
    const double tTousFils = chrono([&] { couches = trancher(m, 0.2); });

    Reglages r;
    std::vector<PlanCouche> plans;
    const double tPlan = chrono([&] { plans = planifier(couches, r); });
    std::ostringstream gcode;
    Bilan bilan;
    const double tGcode = chrono([&] { bilan = ecrireGcode(plans, r, Machine{}, gcode); });

    std::printf("Sphère de 100 mm : %zu triangles, %zu couches de 0,2 mm\n", m.nbTriangles(), couches.size());
    std::printf("  lecture du STL et fusion des sommets : %8.0f ms\n", tLecture);
    std::printf("  tranchage, 1 fil                    : %8.0f ms\n", tUnFil);
    std::printf("  tranchage, %2u fils                   : %8.0f ms\n", std::thread::hardware_concurrency(), tTousFils);
    std::printf("  parois, peaux et remplissage         : %8.0f ms\n", tPlan);
    std::printf("  écriture du G-code (%zu lignes)    : %8.0f ms\n", bilan.lignes, tGcode);
    return 0;
}
