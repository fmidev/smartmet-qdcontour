#pragma once

#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiGlobals.h>
#include <trax/Grid.h>
#include <cstddef>
#include <limits>
#include <stdexcept>

// Adapts a newbase data matrix to the Trax grid API. The coordinates are plain
// grid indices, the caller projects the resulting path afterwards. Trax marks
// missing values with NaN and newbase with kFloatMissing, hence the conversion
// on read. The matrix is referenced and not owned, so the grid is read only.

class DataMatrixAdapter : public Trax::Grid
{
 public:
  DataMatrixAdapter() = delete;

  DataMatrixAdapter(const NFmiDataMatrix<float> &theMatrix)
      : itsMatrix(theMatrix), itsWidth(theMatrix.NX()), itsHeight(theMatrix.NY())
  {
  }

  // Provide wrap-around capability for world data
  float operator()(long i, long j) const override
  {
    const auto value = itsMatrix[static_cast<std::size_t>(i) % itsWidth][j];
    return (value == kFloatMissing ? std::numeric_limits<float>::quiet_NaN() : value);
  }

  void set(long i, long j, float z) override
  {
    throw std::runtime_error("DataMatrixAdapter does not own the data and cannot modify it");
  }

  // Now wrap-around for coordinates, we need both left and right
  // edge coordinates for world data
  double x(long i, long j) const override { return i; }
  double y(long i, long j) const override { return j; }

  bool valid(long i, long j) const override { return true; }
  std::size_t width() const override { return itsWidth; }
  std::size_t height() const override { return itsHeight; }

 private:
  const NFmiDataMatrix<float> &itsMatrix;
  const std::size_t itsWidth;
  const std::size_t itsHeight;

};  // class DataMatrixAdapter
