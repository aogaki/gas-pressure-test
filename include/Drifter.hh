#pragma once

#include <string>

#include "Garfield/AvalancheMC.hh"
#include "Garfield/ComponentConstant.hh"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/SolidBox.hh"

// End point of one drifted electron, in the units of Stage 1 (mm, ns).
struct DriftEndpoint {
  double x = 0.;
  double y = 0.;
  double z = 0.;
  double t = 0.;
  int status = 0;  // Garfield++ status code, see GarfieldConstants.hh
};

// Drifts single electrons through the uniform field of the TPC with
// Garfield++. The gas table is read from the cache when it is there and
// computed with Magboltz (about a minute) and cached when it is not.
class Drifter {
 public:
  Drifter(const std::string& gas, double pressureMbar, double fieldVcm,
          const std::string& cacheDir);

  // The Garfield++ members point at each other, so a copy would be broken.
  Drifter(const Drifter&) = delete;
  Drifter& operator=(const Drifter&) = delete;

  // Drifts one electron from a point in Stage 1 units (mm, ns).
  bool Drift(double xMm, double yMm, double zMm, double tNs,
             DriftEndpoint& end);

  double DriftVelocity() const { return m_vdrift; }  // cm/us
  double Dl() const { return m_dl; }                 // sqrt(cm)
  double Dt() const { return m_dt; }                 // sqrt(cm)
  double W() const { return m_medium.GetW(); }       // eV
  double Fano() const { return m_medium.GetFanoFactor(); }

 private:
  Garfield::MediumMagboltz m_medium;
  Garfield::SolidBox m_box;
  Garfield::GeometrySimple m_geometry;
  Garfield::ComponentConstant m_field;
  Garfield::Sensor m_sensor;
  Garfield::AvalancheMC m_avalanche;
  double m_vdrift = 0.;
  double m_dl = 0.;
  double m_dt = 0.;
};
