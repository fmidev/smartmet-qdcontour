// ======================================================================
/*!
 * \file
 * \brief Implementation of class ContourCalculator
 */
// ======================================================================

#include "ContourCalculator.h"
#include "ContourCache.h"
#include "DataMatrixAdapter.h"
#include "LazyQueryData.h"

#include <newbase/NFmiDataMatrix.h>

#include <geos/version.h>

#include <tron/FmiBuilder.h>
#include <tron/Tron.h>

#include <newbase/NFmiGrid.h>
#include <newbase/NFmiMetTime.h>

#include <boost/make_shared.hpp>

#include <memory>
#include <stdexcept>

typedef Tron::Traits<double, double, Tron::FmiMissing> MyTraits;

typedef Tron::Contourer<DataMatrixAdapter, Tron::FmiBuilder, MyTraits, Tron::LinearInterpolation>
    MyLinearContourer;

typedef Tron::Contourer<DataMatrixAdapter, Tron::FmiBuilder, MyTraits, Tron::LogLinearInterpolation>
    MyLogLinearContourer;

typedef Tron::
    Contourer<DataMatrixAdapter, Tron::FmiBuilder, MyTraits, Tron::NearestNeighbourInterpolation>
        MyNearestContourer;

typedef Tron::Contourer<DataMatrixAdapter, Tron::FmiBuilder, MyTraits, Tron::DiscreteInterpolation>
    MyDiscreteContourer;

typedef Tron::Hints<DataMatrixAdapter, MyTraits> MyHints;

using namespace geos::geom;

// Forward declaration needed

void add_path(Imagine::NFmiPath &path, const Geometry *geom);

// ----------------------------------------------------------------------
/*!
 * \brief Handle LinearRing
 */
// ----------------------------------------------------------------------

void add_linearring(Imagine::NFmiPath &path, const LinearRing *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  for (unsigned long i = 0, n = geom->getNumPoints(); i < n - 1; ++i)
  {
    const Coordinate &coord = geom->getCoordinateN(boost::numeric_cast<int>(i));
    if (i == 0)
      path.MoveTo(coord.x, coord.y);
    else
      path.LineTo(coord.x, coord.y);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle LineString
 */
// ----------------------------------------------------------------------

void add_linestring(Imagine::NFmiPath &path, const LineString *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  unsigned long n = geom->getNumPoints();

  for (unsigned long i = 0; i < n; ++i)
  {
    const Coordinate &coord = geom->getCoordinateN(boost::numeric_cast<int>(i));
    if (i == 0)
      path.MoveTo(coord.x, coord.y);
    else
      path.LineTo(coord.x, coord.y);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle Polygon
 */
// ----------------------------------------------------------------------

void add_polygon(Imagine::NFmiPath &path, const Polygon *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  add_linestring(path, geom->getExteriorRing());

  for (size_t i = 0, n = geom->getNumInteriorRing(); i < n; ++i)
    add_linestring(path, geom->getInteriorRingN(i));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle MultiLineString
 */
// ----------------------------------------------------------------------

void add_multilinestring(Imagine::NFmiPath &path, const MultiLineString *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  for (size_t i = 0, n = geom->getNumGeometries(); i < n; ++i)
    add_linestring(path, dynamic_cast<const LineString *>(geom->getGeometryN(i)));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle MultiPolygon
 */
// ----------------------------------------------------------------------

void add_multipolygon(Imagine::NFmiPath &path, const MultiPolygon *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  for (size_t i = 0, n = geom->getNumGeometries(); i < n; ++i)
    add_polygon(path, dynamic_cast<const Polygon *>(geom->getGeometryN(i)));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle GeometryCollection
 */
// ----------------------------------------------------------------------

void add_geometrycollection(Imagine::NFmiPath &path, const GeometryCollection *geom)
{
  if (geom == nullptr || geom->isEmpty())
    return;

  for (size_t i = 0, n = geom->getNumGeometries(); i < n; ++i)
    add_path(path, geom->getGeometryN(i));
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a GEOS geometry to legacy NFmiPath
 */
// ----------------------------------------------------------------------

void add_path(Imagine::NFmiPath &path, const Geometry *geom)
{
  if (const LinearRing *lr = dynamic_cast<const LinearRing *>(geom))
    add_linearring(path, lr);
  else if (const LineString *ls = dynamic_cast<const LineString *>(geom))
    add_linestring(path, ls);
  else if (const Polygon *p = dynamic_cast<const Polygon *>(geom))
    add_polygon(path, p);
  else if (const MultiLineString *ml = dynamic_cast<const MultiLineString *>(geom))
    add_multilinestring(path, ml);
  else if (const MultiPolygon *mpg = dynamic_cast<const MultiPolygon *>(geom))
    add_multipolygon(path, mpg);
  else if (const GeometryCollection *g = dynamic_cast<const GeometryCollection *>(geom))
    add_geometrycollection(path, g);
  else
    throw std::runtime_error("Bad shit");

  // We ignore points, multipoints and unknown types
}

// ----------------------------------------------------------------------
/*!
 * \brief Implementation hiding pimple for ContourCalculator
 */
// ----------------------------------------------------------------------

class ContourCalculatorPimple
{
 public:
  ContourCalculatorPimple() {}

  ContourCache itsAreaCache;
  ContourCache itsLineCache;
  std::shared_ptr<DataMatrixAdapter> itsData;  // does not own!
  std::shared_ptr<MyHints> itsHints;
  bool itsHintsOK = false;
  bool isCacheOn = false;
  bool itWasCached = false;

  void require_hints();

};  // class ContourCalculatorPimple

// ----------------------------------------------------------------------
/*!
 * \brief Require the pimple to be up to date
 */
// ----------------------------------------------------------------------

void ContourCalculatorPimple::require_hints()
{
  if (itsHintsOK)
    return;

  itsHints.reset(new MyHints(*itsData));
  itsHintsOK = true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

ContourCalculator::~ContourCalculator() {}
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ContourCalculator::ContourCalculator()
{
  itsPimple.reset(new ContourCalculatorPimple());
}
// ----------------------------------------------------------------------
/*!
 * \brief Clear the cache
 */
// ----------------------------------------------------------------------

void ContourCalculator::clearCache()
{
  itsPimple->itsAreaCache.clear();
  itsPimple->itsLineCache.clear();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the cache on or off
 *
 * \param theFlag True if cache is to be set on
 */
// ----------------------------------------------------------------------

void ContourCalculator::cache(bool theFlag)
{
  itsPimple->isCacheOn = theFlag;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return whether the last contour was cached
 *
 * \return True, if last returned contour was cached
 */
// ----------------------------------------------------------------------

bool ContourCalculator::wasCached() const
{
  return itsPimple->itWasCached;
}
// ----------------------------------------------------------------------
/*!
 * \brief Set new active data on
 */
// ----------------------------------------------------------------------

void ContourCalculator::data(const NFmiDataMatrix<float> &theData)
{
  itsPimple->itsData.reset(new DataMatrixAdapter(theData));
  itsPimple->itsHintsOK = false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the desired contour
 *
 * \return The path object
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath ContourCalculator::contour(const LazyQueryData &theData,
                                             float theLoLimit,
                                             float theHiLimit,
                                             const NFmiTime &theTime,
                                             ContourInterpolation theInterpolation)
{
  if (itsPimple->itsData.get() == 0)
    throw std::runtime_error("ContourCalculator:: No data set before calling contour");

  if (itsPimple->isCacheOn &&
      itsPimple->itsAreaCache.contains(theLoLimit, theHiLimit, theTime, theData))
  {
    itsPimple->itWasCached = true;
    return itsPimple->itsAreaCache.find(theLoLimit, theHiLimit, theTime, theData);
  }

  itsPimple->require_hints();

  // Build the contours

#if GEOS_VERSION_MAJOR == 3
#if GEOS_VERSION_MINOR < 7
  boost::shared_ptr<GeometryFactory> geomFactory = boost::make_shared<GeometryFactory>();
  Tron::FmiBuilder builder(geomFactory);
#else
  geos::geom::GeometryFactory::Ptr geomFactory(geos::geom::GeometryFactory::create());
  Tron::FmiBuilder builder(*geomFactory);
#endif
#else
#pragma message(Cannot handle current GEOS version correctly)
#endif

  switch (theInterpolation)
  {
    case Linear:
    case Missing:
    {
      MyLinearContourer::fill(
          builder, *(itsPimple->itsData), theLoLimit, theHiLimit, *(itsPimple->itsHints));
      break;
    }
    case LogLinear:
    {
      MyLogLinearContourer::fill(
          builder, *(itsPimple->itsData), theLoLimit, theHiLimit, *(itsPimple->itsHints));
      break;
    }
    case Nearest:
    {
      MyNearestContourer::fill(
          builder, *(itsPimple->itsData), theLoLimit, theHiLimit, *(itsPimple->itsHints));
      break;
    }
    case Discrete:
    {
      MyDiscreteContourer::fill(
          builder, *(itsPimple->itsData), theLoLimit, theHiLimit, *(itsPimple->itsHints));
      break;
    }
  }

  boost::shared_ptr<Geometry> geom = builder.result();

  Imagine::NFmiPath path;
  add_path(path, geom.get());

  path.InvGrid(theData.Grid());

  if (itsPimple->isCacheOn)
    itsPimple->itsAreaCache.insert(path, theLoLimit, theHiLimit, theTime, theData);

  itsPimple->itWasCached = false;
  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the desired contour line
 *
 * \return The path object
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath ContourCalculator::contour(const LazyQueryData &theData,
                                             float theValue,
                                             const NFmiTime &theTime,
                                             ContourInterpolation theInterpolation)
{
  if (itsPimple->itsData.get() == 0)
    throw std::runtime_error("ContourCalculator:: No data set before calling contour");

  if (itsPimple->isCacheOn &&
      itsPimple->itsLineCache.contains(theValue, kFloatMissing, theTime, theData))
  {
    itsPimple->itWasCached = true;
    return itsPimple->itsLineCache.find(theValue, kFloatMissing, theTime, theData);
  }

  itsPimple->require_hints();

#if GEOS_VERSION_MAJOR == 3
#if GEOS_VERSION_MINOR < 7
  boost::shared_ptr<GeometryFactory> geomFactory = boost::make_shared<GeometryFactory>();
  Tron::FmiBuilder builder(geomFactory);
#else
  geos::geom::GeometryFactory::Ptr geomFactory(geos::geom::GeometryFactory::create());
  Tron::FmiBuilder builder(*geomFactory);
#endif
#else
#pragma message(Cannot handle current GEOS version correctly)
#endif

  switch (theInterpolation)
  {
    case Linear:
    case Missing:
    {
      MyLinearContourer::line(builder, *(itsPimple->itsData), theValue, *(itsPimple->itsHints));
      break;
    }
    case LogLinear:
    {
      MyLogLinearContourer::line(builder, *(itsPimple->itsData), theValue, *(itsPimple->itsHints));
      break;
    }
    case Nearest:
    {
      throw std::runtime_error("Contour lines not supported for nearest neighbour interpolation");
    }
    case Discrete:
    {
      throw std::runtime_error("Contour lines not supported for discrete neighbour interpolation");
      break;
    }
  }

  boost::shared_ptr<Geometry> geom = builder.result();

  Imagine::NFmiPath path;
  add_path(path, geom.get());

  path.InvGrid(theData.Grid());

  if (itsPimple->isCacheOn)
    itsPimple->itsLineCache.insert(path, theValue, kFloatMissing, theTime, theData);

  itsPimple->itWasCached = false;
  return path;
}

// ======================================================================
