#pragma once

#include "G4Types.hh"

// Ntuple and column ids, in the creation order of TODO/01.
namespace ntuple {

constexpr G4int kEvents = 0;
constexpr G4int kHits = 1;

enum EventColumn {
  kEventID = 0,
  kE0,
  kX0,
  kY0,
  kZ0,
  kDx0,
  kDy0,
  kDz0,
  kTrackLength,
  kXEnd,
  kYEnd,
  kZEnd,
  kTEnd,
  kEExit,
  kExited,
  kEdepTotal
};

enum HitColumn {
  kHitEventID = 0,
  kHitTrackID,
  kHitPdg,
  kHitX,
  kHitY,
  kHitZ,
  kHitT,
  kHitEdep,
  kHitStepLength
};

}  // namespace ntuple
