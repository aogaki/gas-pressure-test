#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// The readout board of Stage 3 (channel-response): the pad map of
// geometry/pads.csv and the lookup of the pad under a point. Pure code, free
// of any ROOT, Geant4 and Garfield++ dependency.

// One rhombic pad (edge 1.0 mm), in simulation coordinates [mm]: x
// horizontal, z the alpha-source axis, both in the readout plane y = -100.
struct Pad {
  int padId = 0;
  std::string stripDir;  // "U", "V" or "W"
  int stripNo = 0;
  int padNo = 0;
  int aget = 0;    // 0..3
  int chGraw = 0;  // raw GRAW channel 0..67
  int chGeom = 0;  // geometry channel 0..63
  double cx = 0., cz = 0.;      // centre
  double x[4] = {0., 0., 0., 0.};  // vertices, counter-clockwise
  double z[4] = {0., 0., 0., 0.};
};

// True when (px, pz) lies inside the convex pad. The vertices are
// counter-clockwise, so the point is inside when every edge cross product is
// non-negative; a point on an edge counts as inside.
bool PadContains(const Pad& pad, double px, double pz);

// The whole board, with a 1 mm lookup grid over its bounding box. Every pad
// is registered in the grid cells its bounding box overlaps, so a lookup only
// tests the few candidates of one cell.
class PadMap {
 public:
  // Reads a pads.csv: '#' comment lines are skipped and the header line has
  // to carry the documented column names in the documented order. Throws
  // std::runtime_error when the file cannot be read or is malformed.
  explicit PadMap(const std::string& csvPath);

  const std::vector<Pad>& Pads() const { return pads_; }
  std::size_t Size() const { return pads_.size(); }

  // Bounding box of the pad vertices [mm].
  double MinX() const { return minX_; }
  double MaxX() const { return maxX_; }
  double MinZ() const { return minZ_; }
  double MaxZ() const { return maxZ_; }

  // Index into Pads() of the pad under (px, pz), or -1 for a point that is on
  // no pad (between pads or off the board).
  int Find(double px, double pz) const;

 private:
  // Side of a lookup grid cell [mm]. The pads are 1 mm rhombi, so a cell
  // holds only a handful of candidates.
  static constexpr double kCellMm = 1.;

  std::vector<Pad> pads_;
  double minX_ = 0., maxX_ = 0., minZ_ = 0., maxZ_ = 0.;
  int nx_ = 0, nz_ = 0;
  std::vector<std::vector<int>> cells_;  // nx_ * nz_, row-major in x
};

// The implementation is inline so that the acceptance ROOT macros can use
// the same pad lookup as the application, by including this header.

namespace padmap_detail {

// The columns of geometry/pads.csv, in order (TODO/06).
inline constexpr const char* kColumns[] = {
    "pad_id", "strip_dir", "strip_no", "pad_no", "aget", "ch_graw",
    "ch_geom", "cx", "cz", "x0", "z0", "x1", "z1", "x2", "z2", "x3", "z3"};
inline constexpr std::size_t kNColumns =
    sizeof(kColumns) / sizeof(kColumns[0]);

inline std::vector<std::string> SplitOnComma(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream stream(line);
  while (std::getline(stream, field, ',')) fields.push_back(field);
  return fields;
}

inline double ToDouble(const std::string& text, const std::string& where) {
  try {
    return std::stod(text);
  } catch (const std::exception&) {
    throw std::runtime_error("pads.csv: not a number '" + text + "' " + where);
  }
}

inline int ToInt(const std::string& text, const std::string& where) {
  try {
    return std::stoi(text);
  } catch (const std::exception&) {
    throw std::runtime_error("pads.csv: not an integer '" + text + "' " +
                             where);
  }
}

}  // namespace padmap_detail

inline bool PadContains(const Pad& pad, double px, double pz) {
  for (int i = 0; i < 4; ++i) {
    const int j = (i + 1) % 4;
    const double ex = pad.x[j] - pad.x[i];
    const double ez = pad.z[j] - pad.z[i];
    const double cross = ex * (pz - pad.z[i]) - ez * (px - pad.x[i]);
    if (cross < 0.) return false;
  }
  return true;
}

inline PadMap::PadMap(const std::string& csvPath) {
  std::ifstream in(csvPath);
  if (!in) {
    throw std::runtime_error("cannot read pad map " + csvPath);
  }

  std::string line;
  bool sawHeader = false;
  long lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;

    const std::vector<std::string> fields = padmap_detail::SplitOnComma(line);
    if (!sawHeader) {
      if (fields.size() != padmap_detail::kNColumns) {
        throw std::runtime_error("pads.csv: header has " +
                                 std::to_string(fields.size()) +
                                 " columns, expected " +
                                 std::to_string(padmap_detail::kNColumns));
      }
      for (std::size_t i = 0; i < padmap_detail::kNColumns; ++i) {
        if (fields[i] != padmap_detail::kColumns[i]) {
          throw std::runtime_error("pads.csv: column " + std::to_string(i) +
                                   " is '" + fields[i] + "', expected '" +
                                   padmap_detail::kColumns[i] + "'");
        }
      }
      sawHeader = true;
      continue;
    }

    const std::string where = "on line " + std::to_string(lineNo);
    if (fields.size() != padmap_detail::kNColumns) {
      throw std::runtime_error("pads.csv: " + std::to_string(fields.size()) +
                               " fields " + where + ", expected " +
                               std::to_string(padmap_detail::kNColumns));
    }
    Pad pad;
    pad.padId = padmap_detail::ToInt(fields[0], where);
    pad.stripDir = fields[1];
    pad.stripNo = padmap_detail::ToInt(fields[2], where);
    pad.padNo = padmap_detail::ToInt(fields[3], where);
    pad.aget = padmap_detail::ToInt(fields[4], where);
    pad.chGraw = padmap_detail::ToInt(fields[5], where);
    pad.chGeom = padmap_detail::ToInt(fields[6], where);
    pad.cx = padmap_detail::ToDouble(fields[7], where);
    pad.cz = padmap_detail::ToDouble(fields[8], where);
    for (int v = 0; v < 4; ++v) {
      pad.x[v] = padmap_detail::ToDouble(fields[9 + 2 * v], where);
      pad.z[v] = padmap_detail::ToDouble(fields[10 + 2 * v], where);
    }
    pads_.push_back(pad);
  }
  if (!sawHeader) {
    throw std::runtime_error("pads.csv: no header line in " + csvPath);
  }
  if (pads_.empty()) {
    throw std::runtime_error("pads.csv: no pads in " + csvPath);
  }

  // Bounding box of every vertex.
  minX_ = maxX_ = pads_.front().x[0];
  minZ_ = maxZ_ = pads_.front().z[0];
  for (const Pad& pad : pads_) {
    for (int v = 0; v < 4; ++v) {
      minX_ = std::min(minX_, pad.x[v]);
      maxX_ = std::max(maxX_, pad.x[v]);
      minZ_ = std::min(minZ_, pad.z[v]);
      maxZ_ = std::max(maxZ_, pad.z[v]);
    }
  }

  nx_ = static_cast<int>((maxX_ - minX_) / kCellMm) + 1;
  nz_ = static_cast<int>((maxZ_ - minZ_) / kCellMm) + 1;
  cells_.resize(static_cast<std::size_t>(nx_) * nz_);
  for (std::size_t i = 0; i < pads_.size(); ++i) {
    const Pad& pad = pads_[i];
    double lowX = pad.x[0], highX = pad.x[0];
    double lowZ = pad.z[0], highZ = pad.z[0];
    for (int v = 1; v < 4; ++v) {
      lowX = std::min(lowX, pad.x[v]);
      highX = std::max(highX, pad.x[v]);
      lowZ = std::min(lowZ, pad.z[v]);
      highZ = std::max(highZ, pad.z[v]);
    }
    const int ix0 =
        std::max(0, static_cast<int>((lowX - minX_) / kCellMm));
    const int ix1 =
        std::min(nx_ - 1, static_cast<int>((highX - minX_) / kCellMm));
    const int iz0 =
        std::max(0, static_cast<int>((lowZ - minZ_) / kCellMm));
    const int iz1 =
        std::min(nz_ - 1, static_cast<int>((highZ - minZ_) / kCellMm));
    for (int ix = ix0; ix <= ix1; ++ix) {
      for (int iz = iz0; iz <= iz1; ++iz) {
        cells_[static_cast<std::size_t>(iz) * nx_ + ix].push_back(
            static_cast<int>(i));
      }
    }
  }
}

inline int PadMap::Find(double px, double pz) const {
  if (px < minX_ || px > maxX_ || pz < minZ_ || pz > maxZ_) return -1;
  const int ix = std::min(nx_ - 1, static_cast<int>((px - minX_) / kCellMm));
  const int iz = std::min(nz_ - 1, static_cast<int>((pz - minZ_) / kCellMm));
  if (ix < 0 || iz < 0) return -1;
  for (int index : cells_[static_cast<std::size_t>(iz) * nx_ + ix]) {
    if (PadContains(pads_[index], px, pz)) return index;
  }
  return -1;
}
