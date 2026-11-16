#include "trancheur/gcode.hpp"

#include <cmath>
#include <format>
#include <numbers>
#include <ostream>
#include <string>

namespace trancheur {

double extrusionParMm(const Reglages& r, const Machine& m) {
    const double rayon = m.diametreFil / 2;
    return r.largeurLigne * r.hauteurCouche / (std::numbers::pi * rayon * rayon);
}

namespace {

class Ecrivain {
public:
    Ecrivain(const Reglages& r, const Machine& m, std::ostream& o) : m_(m), o_(o), eParMm_(extrusionParMm(r, m)) {}

    void couche(const PlanCouche& plan) {
        ligne(std::format(";COUCHE {} z={:.3f}", plan.indice, plan.z));
        deplacerZ(plan.z);
        for (const Trace& t : plan.traces) {
            if (t.points.size() < 2) {
                continue;
            }
            const double vitesse = t.role == Role::Perimetre ? m_.vitesseParoi : m_.vitesseRemplissage;
            aller(t.points.front());
            for (std::size_t i = 1; i < t.points.size(); ++i) {
                deposer(t.points[i], vitesse);
            }
            if (t.ferme) {
                deposer(t.points.front(), vitesse);
            }
        }
    }

    Bilan bilan() const { return bilan_; }

private:
    double mmX(std::int64_t v) const { return versMm(v) + m_.origineX; }
    double mmY(std::int64_t v) const { return versMm(v) + m_.origineY; }

    void ligne(const std::string& texte) {
        o_ << texte << '\n';
        ++bilan_.lignes;
    }

    void deplacerZ(double z) {
        ligne(std::format("G0 Z{:.3f} F600", z));
        bilan_.dureeS += 0.2;
    }

    /// Déplacement à vide. Au-delà d'une courte distance, on retire le fil pour ne pas laisser de traînée.
    void aller(const Clipper2Lib::Point64& p) {
        const double distance = std::hypot(mmX(p.x) - x_, mmY(p.y) - y_);
        const bool retirer = distance > m_.distanceRetrait && m_.retrait > 0;
        if (retirer) {
            ligne(std::format("G1 E{:.4f} F2400", -m_.retrait));
        }
        ligne(std::format("G0 X{:.3f} Y{:.3f} F{:.0f}", mmX(p.x), mmY(p.y), m_.vitesseDeplacement * 60));
        if (retirer) {
            ligne(std::format("G1 E{:.4f} F2400", m_.retrait));
        }
        bilan_.dureeS += distance / m_.vitesseDeplacement;
        x_ = mmX(p.x);
        y_ = mmY(p.y);
    }

    void deposer(const Clipper2Lib::Point64& p, double vitesse) {
        const double x = mmX(p.x), y = mmY(p.y);
        const double distance = std::hypot(x - x_, y - y_);
        const double e = distance * eParMm_;
        ligne(std::format("G1 X{:.3f} Y{:.3f} E{:.5f} F{:.0f}", x, y, e, vitesse * 60));
        bilan_.filamentMm += e;
        bilan_.dureeS += distance / vitesse;
        x_ = x;
        y_ = y;
    }

    const Machine& m_;
    std::ostream& o_;
    double eParMm_;
    double x_ = 0, y_ = 0;
    Bilan bilan_;
};

}  // namespace

Bilan ecrireGcode(const std::vector<PlanCouche>& plans, const Reglages& r, const Machine& m, std::ostream& sortie) {
    Ecrivain e(r, m, sortie);
    sortie << "; G-code produit par trancheur\n"
           << std::format("; couche {:.2f} mm, cordon {:.2f} mm, {} parois, remplissage {:.0f} %\n", r.hauteurCouche,
                          r.largeurLigne, r.perimetres, r.densite * 100)
           << "G21 ; millimetres\nG90 ; coordonnees absolues\nM83 ; extrusion relative\n";
    for (const PlanCouche& p : plans) {
        e.couche(p);
    }
    sortie << "G0 Z" << std::format("{:.3f}", (plans.empty() ? 0.0 : plans.back().z) + 10) << " ; degager la buse\n";
    return e.bilan();
}

}  // namespace trancheur
