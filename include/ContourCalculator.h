// ======================================================================
/*!
 * \file
 * \brief Interface of class ContourCalculator
 */
// ======================================================================
/*!
 * \class ContourCalculator
 * \brief Calculates and caches contours
 *
 * The ContourCalculator is able to calculate contours from the
 * given data, and then to cache the result in case the same
 * contour is calculator again.
 *
 */
// ======================================================================

#pragma once

#include "ContourInterpolation.h"
#include <boost/shared_ptr.hpp>
#include <memory>

template <typename T>
class NFmiDataMatrix;

class ContourCalculatorPimple;
class LazyQueryData;
class NFmiTime;

namespace Imagine
{
class NFmiPath;
}

class ContourCalculator
{
 public:
  ~ContourCalculator();
  ContourCalculator();

  Imagine::NFmiPath contour(const LazyQueryData &theData,
                            float theLoLimit,
                            float theHiLimit,
                            const NFmiTime &theTime,
                            ContourInterpolation theInterpolation);

  Imagine::NFmiPath contour(const LazyQueryData &theData,
                            float theValue,
                            const NFmiTime &theTime,
                            ContourInterpolation theInterpolation);

  void data(const NFmiDataMatrix<float> &theData);
  void clearCache();
  void cache(bool);
  bool wasCached(void) const;

 private:
  ContourCalculator(const ContourCalculator &theCalc);
  ContourCalculator &operator=(const ContourCalculator &theCalc);

  std::shared_ptr<ContourCalculatorPimple> itsPimple;

};  // class ContourCalculator

// ======================================================================
