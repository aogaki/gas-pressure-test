#ifndef AT_CHECK_H
#define AT_CHECK_H

// Shared helpers for the acceptance check macros. Every macro reports each
// check and finally calls AtReport(), which exits with 1 if anything failed.

#include <cmath>
#include <cstdio>

#include "TFile.h"
#include "TString.h"
#include "TSystem.h"
#include "TTree.h"

int atFailures = 0;

void AtCheck(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "[ok]  " : "[FAIL]", what);
  if (!ok) ++atFailures;
}

void AtCheckNear(double value, double expected, double tolerance,
                 const char* what) {
  const bool ok = std::fabs(value - expected) <= tolerance;
  std::printf("%s %s = %.6g (expected %.6g +- %.6g, ratio %.4f)\n",
              ok ? "[ok]  " : "[FAIL]", what, value, expected, tolerance,
              expected != 0. ? value / expected : 0.);
  if (!ok) ++atFailures;
}

void AtCheckRange(double value, double low, double high, const char* what) {
  const bool ok = (value >= low) && (value <= high);
  std::printf("%s %s = %.6g (expected %.6g .. %.6g)\n",
              ok ? "[ok]  " : "[FAIL]", what, value, low, high);
  if (!ok) ++atFailures;
}

TFile* AtOpen(const char* fileName) {
  TFile* file = TFile::Open(fileName);
  if (file == nullptr || file->IsZombie()) {
    std::printf("[FAIL] cannot open %s\n", fileName);
    gSystem->Exit(1);
  }
  std::printf("[ok]   opened %s\n", fileName);
  return file;
}

TTree* AtTree(TFile* file, const char* name) {
  TTree* tree = dynamic_cast<TTree*>(file->Get(name));
  if (tree == nullptr) {
    std::printf("[FAIL] ntuple '%s' is missing in %s\n", name,
                file->GetName());
    gSystem->Exit(1);
  }
  return tree;
}

void AtReport() {
  if (atFailures > 0) {
    std::printf("%d check(s) failed\n", atFailures);
    gSystem->Exit(1);
  }
  std::printf("all checks passed\n");
}

#endif
