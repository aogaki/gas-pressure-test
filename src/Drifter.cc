#include "Drifter.hh"

#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include "DriftUtils.hh"
#include "Garfield/Random.hh"
#include "Garfield/RandomEngineSTL.hh"
#include "GasProperties.hh"

namespace {

// Half widths of the gas volume [cm], from TODO/01: 200 x 200 x 500 mm.
constexpr double kHalfX = 10.;
constexpr double kHalfY = 10.;
constexpr double kHalfZ = 25.;

constexpr double kTemperatureK = 293.15;
constexpr int kMagboltzCollisions = 10;
constexpr double kDistanceStepCm = 0.1;  // 1 mm
constexpr unsigned int kRandomSeed = 1;

// Makes Garfield++ reproducible: its default engine seeds itself from
// std::random_device.
void SeedGarfield() {
  Garfield::RandomEngineSTL engine;
  engine.SetSeed(kRandomSeed);
  Garfield::Random::SetEngine(engine);
}

}  // namespace

Drifter::Drifter(const std::string& gas, double pressureMbar, double fieldVcm,
                 const std::string& cacheDir)
    : m_box(0., 0., 0., kHalfX, kHalfY, kHalfZ) {
  SeedGarfield();

  const MagboltzMix mix = MagboltzComposition(gas);
  m_medium.SetComposition(mix.names[0], mix.fractions[0], mix.names[1],
                          mix.fractions[1], mix.names[2], mix.fractions[2],
                          mix.names[3], mix.fractions[3], mix.names[4],
                          mix.fractions[4], mix.names[5], mix.fractions[5]);
  m_medium.SetTemperature(kTemperatureK);
  m_medium.SetPressure(MbarToTorr(pressureMbar));

  const std::string gasFile =
      GasFileName(cacheDir, gas, pressureMbar, fieldVcm);
  std::error_code ignored;
  std::filesystem::create_directories(cacheDir, ignored);
  if (std::filesystem::exists(gasFile)) {
    std::printf("Drifter: reading the gas table %s\n", gasFile.c_str());
    if (!m_medium.LoadGasFile(gasFile)) {
      throw std::runtime_error("cannot read the gas table " + gasFile);
    }
  } else {
    std::printf("Drifter: running Magboltz for %s at %g mbar, %g V/cm\n",
                gas.c_str(), pressureMbar, fieldVcm);
    m_medium.SetFieldGrid(fieldVcm, fieldVcm, 1, false);
    m_medium.GenerateGasTable(kMagboltzCollisions, false);
    // Write to a private file and rename, so that two processes making the
    // same table at the same time never leave a half written one behind.
    const std::string tmpFile =
        gasFile + ".tmp" + std::to_string(static_cast<long>(getpid()));
    if (!m_medium.WriteGasFile(tmpFile)) {
      throw std::runtime_error("cannot write the gas table " + tmpFile);
    }
    std::filesystem::rename(tmpFile, gasFile);
  }

  // The transport parameters as AvalancheMC will see them.
  double vx = 0., vy = 0., vz = 0.;
  if (!m_medium.ElectronVelocity(0., fieldVcm, 0., 0., 0., 0., vx, vy, vz)) {
    throw std::runtime_error("no drift velocity at " +
                             FormatNumber(fieldVcm) + " V/cm");
  }
  // Garfield++ works in cm/ns, the gas tables of TODO/03 in cm/us.
  m_vdrift = 1000. * std::sqrt(vx * vx + vy * vy + vz * vz);
  if (!m_medium.ElectronDiffusion(0., fieldVcm, 0., 0., 0., 0., m_dl, m_dt)) {
    throw std::runtime_error("no diffusion coefficients at " +
                             FormatNumber(fieldVcm) + " V/cm");
  }

  m_geometry.AddSolid(&m_box, &m_medium);
  m_field.SetGeometry(&m_geometry);
  // +y field, so the electrons run down to the readout plane at y = -10 cm.
  m_field.SetElectricField(0., fieldVcm, 0.);
  m_sensor.AddComponent(&m_field);
  m_sensor.SetArea(-kHalfX, -kHalfY, -kHalfZ, kHalfX, kHalfY, kHalfZ);
  m_avalanche.SetSensor(&m_sensor);
  m_avalanche.EnableDiffusion();
  m_avalanche.SetDistanceSteps(kDistanceStepCm);
}

bool Drifter::Drift(double xMm, double yMm, double zMm, double tNs,
                    DriftEndpoint& end) {
  if (!m_avalanche.DriftElectron(0.1 * xMm, 0.1 * yMm, 0.1 * zMm, tNs)) {
    return false;
  }
  if (m_avalanche.GetNumberOfElectronEndpoints() == 0) return false;
  double x0 = 0., y0 = 0., z0 = 0., t0 = 0.;
  double x1 = 0., y1 = 0., z1 = 0., t1 = 0.;
  int status = 0;
  m_avalanche.GetElectronEndpoint(0, x0, y0, z0, t0, x1, y1, z1, t1, status);
  end.x = 10. * x1;
  end.y = 10. * y1;
  end.z = 10. * z1;
  end.t = t1;
  end.status = status;
  return true;
}
