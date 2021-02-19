#pragma once

#include <newbase/NFmiDataMatrix.h>

class DataMatrixAdapter
{
 public:
  typedef float value_type;
  typedef float coord_type;

  typedef NFmiDataMatrix<float>::size_type size_type;

  DataMatrixAdapter(const NFmiDataMatrix<float> &theMatrix)
      : itsMatrix(theMatrix), itsWidth(theMatrix.NX()), itsHeight(theMatrix.NY())
  {
  }

  // Provide wrap-around capability for world data
  const value_type &operator()(size_type i, size_type j) const
  {
    return itsMatrix[i % itsWidth][j];
  }

  // No wrap-around for coordinates, we need both left and right
  // edge coordinates for world data
  coord_type x(size_type i, size_type j) const { return static_cast<float>(i); }
  coord_type y(size_type i, size_type j) const { return static_cast<float>(j); }
  size_type width() const { return itsWidth; }
  size_type height() const { return itsHeight; }
  bool valid(size_type i, size_type j) const { return true; }

 private:
  DataMatrixAdapter();
  const NFmiDataMatrix<float> &itsMatrix;
  const size_type itsWidth;
  const size_type itsHeight;

};  // class DataMatrixAdapter
