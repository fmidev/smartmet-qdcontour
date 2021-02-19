// ======================================================================
/*!
 * \brief Interface of class LazyCoordinates
 */
// ======================================================================
/*!
 * \class LazyCoordinates
 *
 * Fetching the world coordinates for some projection is a very
 * expensive operation for large grids. qdcontour may sometimes
 * fetch the coordinates for a projection even when they are not
 * needed (when the counters are already available in the cache).
 *
 * To optimize the code we hence use a lazy matrix of coordinates,
 * which acts like a NFmiDataMatrix<NFmiPoint>, except that
 * the coordinates are only fetched from the global querydata
 * holder if necessary.
 *
 */
// ======================================================================

#ifndef LAZYCOORDINATES_H
#define LAZYCOORDINATES_H

#include "Globals.h"
#include "LazyQueryData.h"
#include <gis/CoordinateMatrix.h>
#include <newbase/NFmiPoint.h>

namespace
{
NFmiPoint dummy;
}

class LazyCoordinates
{
 public:
  typedef Fmi::CoordinateMatrix data_type;
  typedef std::size_t size_type;

  LazyCoordinates(const NFmiArea &theArea);
  NFmiPoint operator()(size_type i, size_type j) const;
  NFmiPoint operator()(int i, int j, const NFmiPoint &theDefault) const;
  const data_type &operator*() const;
  data_type &operator*();

  size_type NX() const;
  size_type NY() const;

 private:
  const NFmiArea &itsArea;
  mutable bool itsInitialized;
  mutable data_type itsData;

  void init() const;

};  // class LazyCoordinates

// ----------------------------------------------------------------------
/*!
 * \brief Data accessor
 */
// ----------------------------------------------------------------------

inline NFmiPoint LazyCoordinates::operator()(size_type i, size_type j) const
{
  init();
  return {itsData.x(i, j), itsData.y(i, j)};
}

// ----------------------------------------------------------------------
/*!
 * \brief Data accessor
 */
// ----------------------------------------------------------------------

inline NFmiPoint LazyCoordinates::operator()(int i, int j, const NFmiPoint &theDefault) const
{
  init();
  if (i >= 0 && j >= 0 && static_cast<size_type>(i) < itsData.width() &&
      static_cast<size_type>(j) < itsData.height())
  {
    return {itsData.x(i, j), itsData.y(i, j)};
  }
  return dummy;
}

// ----------------------------------------------------------------------
/*!
 * \brief Const dereference
 */
// ----------------------------------------------------------------------

inline const LazyCoordinates::data_type &LazyCoordinates::operator*() const
{
  init();
  return itsData;
}

// ----------------------------------------------------------------------
/*!
 * \brief Non-const dereference
 */
// ----------------------------------------------------------------------

inline LazyCoordinates::data_type &LazyCoordinates::operator*()
{
  init();
  return itsData;
}

// ----------------------------------------------------------------------
/*!
 * \brief Width accessor
 */
// ----------------------------------------------------------------------

inline LazyCoordinates::size_type LazyCoordinates::NX() const
{
  init();
  return itsData.width();
}

// ----------------------------------------------------------------------
/*!
 * \brief Height accessor
 */
// ----------------------------------------------------------------------

inline LazyCoordinates::size_type LazyCoordinates::NY() const
{
  init();
  return itsData.height();
}

// ----------------------------------------------------------------------
/*!
 * \brief Data initializer
 */
// ----------------------------------------------------------------------

inline void LazyCoordinates::init() const
{
  if (itsInitialized)
    return;

  itsData = *globals.queryinfo->LocationsWorldXY(itsArea);
  itsInitialized = true;
}

#endif  // LAZYCOORDINATES_H

// ======================================================================
