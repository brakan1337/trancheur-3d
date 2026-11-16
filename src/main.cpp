// trancheur : de la pièce (STL) aux trajectoires de la buse (G-code).
//
//   trancheur piece.stl -o piece.gcode [--couche 0.2] [--densite 20] [--parois 2] [--apercu apercu.svg]
//   trancheur --demo bague.stl          écrit la bague épaulée de démonstration

#include <charconv>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "trancheur/apercu.hpp"
#include "trancheur/formes.hpp"
#include "trancheur/gcode.hpp"
#include "trancheur/maillage.hpp"

using namespace trancheur;

namespace {

double nombre(std::string_view texte, std::string_view option) {
    double v = 0;
    const auto [fin, erreur] = std::from_chars(texte.data(), texte.data() + texte.size(), v);
    if (erreur != std::errc{} || fin != texte.data() + texte.size()) {
        throw std::invalid_argument(std::string(option) + " attend un nombre, pas « " + std::string(texte) + " »");
    }
    return v;
}

void usage() {
    std::cerr << "usage : trancheur piece.stl -o piece.gcode [--couche 0.2] [--largeur 0.45] [--parois 2]\n"
                 "                  [--densite 20] [--pleines 4] [--apercu apercu.svg]\n"
                 "        trancheur --demo bague.stl\n";
}

double ms(std::chrono::steady_clock::time_point depuis) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - depuis).count();
}

int executer(std::span<char*> args) {
    if (args.size() == 3 && std::string_view(args[1]) == "--demo") {
        std::ofstream f(args[2], std::ios::binary);
        ecrireSTLBinaire(bagueEpaulee(), f);
        std::cout << "Bague épaulée écrite dans " << args[2] << "\n";
        return 0;
    }

    std::string entree, sortie = "sortie.gcode", apercu;
    std::vector<std::size_t> couchesApercu;
    Reglages r;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view a = args[i];
        auto valeur = [&]() -> std::string_view {
            if (i + 1 >= args.size()) {
                throw std::invalid_argument(std::string(a) + " attend une valeur");
            }
            return args[++i];
        };
        if (a == "-o") sortie = valeur();
        else if (a == "--couche") r.hauteurCouche = nombre(valeur(), a);
        else if (a == "--largeur") r.largeurLigne = nombre(valeur(), a);
        else if (a == "--parois") r.perimetres = static_cast<int>(nombre(valeur(), a));
        else if (a == "--densite") r.densite = nombre(valeur(), a) / 100;
        else if (a == "--pleines") r.couchesPleines = static_cast<int>(nombre(valeur(), a));
        else if (a == "--apercu") apercu = valeur();
        else if (a == "--couches") {
            // Numéros de couches séparés par des virgules : 0,25,48,100
            std::string_view liste = valeur();
            while (!liste.empty()) {
                const auto virgule = liste.find(',');
                couchesApercu.push_back(static_cast<std::size_t>(nombre(liste.substr(0, virgule), a)));
                liste = virgule == std::string_view::npos ? std::string_view{} : liste.substr(virgule + 1);
            }
        }
        else if (a.starts_with("-")) throw std::invalid_argument("option inconnue : " + std::string(a));
        else entree = a;
    }
    if (entree.empty()) {
        usage();
        return 2;
    }

    auto t = std::chrono::steady_clock::now();
    const Maillage m = lireSTL(entree);
    const double tLecture = ms(t);

    t = std::chrono::steady_clock::now();
    const auto couches = trancher(m, r.hauteurCouche);
    const double tTranchage = ms(t);

    t = std::chrono::steady_clock::now();
    const auto plans = planifier(couches, r);
    const double tPlan = ms(t);

    // On pose la pièce au centre d'un plateau de 220 × 220 mm.
    const Boite b = m.boite();
    Machine machine;
    machine.origineX = 110 - (b.min.x + b.max.x) / 2;
    machine.origineY = 110 - (b.min.y + b.max.y) / 2;
    std::ofstream g(sortie);
    t = std::chrono::steady_clock::now();
    const Bilan bilan = ecrireGcode(plans, r, machine, g);
    const double tGcode = ms(t);

    std::size_t ouverts = 0;
    for (const Couche& c : couches) ouverts += c.contoursOuverts;

    if (!apercu.empty() && !couches.empty()) {
        // Par défaut, trois couches : la base, le premier tiers, le deuxième tiers.
        if (couchesApercu.empty()) {
            couchesApercu = {1, couches.size() / 3, couches.size() * 2 / 3};
        }
        std::vector<const Couche*> cs;
        std::vector<const PlanCouche*> ps;
        for (std::size_t k : couchesApercu) {
            if (k >= couches.size()) {
                throw std::invalid_argument("--couches : la pièce n'a que " + std::to_string(couches.size()) + " couches");
            }
            cs.push_back(&couches[k]);
            ps.push_back(&plans[k]);
        }
        std::ofstream svg(apercu);
        ecrireApercu(cs, ps, svg);
    }

    std::printf("%zu triangles, %zu sommets, %.1f x %.1f x %.1f mm\n", m.nbTriangles(), m.sommets.size(),
                static_cast<double>(b.max.x - b.min.x), static_cast<double>(b.max.y - b.min.y), static_cast<double>(b.max.z - b.min.z));
    std::printf("%zu couches de %.2f mm, %zu lignes de G-code dans %s\n", couches.size(), r.hauteurCouche, bilan.lignes, sortie.c_str());
    std::printf("filament : %.2f m · durée estimée : %d h %02d\n", bilan.filamentMm / 1000, static_cast<int>(bilan.dureeS / 3600),
                static_cast<int>(bilan.dureeS / 60) % 60);
    std::printf("lecture %.0f ms · tranchage %.0f ms · trajectoires %.0f ms · G-code %.0f ms\n", tLecture, tTranchage, tPlan, tGcode);
    if (ouverts > 0) {
        std::printf("attention : %zu contour(s) ouvert(s), le maillage n'est pas étanche\n", ouverts);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return executer(std::span(argv, static_cast<std::size_t>(argc)));
    } catch (const std::exception& e) {
        std::cerr << "erreur : " << e.what() << '\n';
        return 1;
    }
}
