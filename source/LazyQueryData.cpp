// ======================================================================
/*!
 * \file
 * \brief Implementation of class LazyQueryData
 */
// ======================================================================

#include "LazyQueryData.h"
#include <gis/CoordinateMatrix.h>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiInterpolation.h>
#include <newbase/NFmiQueryData.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

LazyQueryData::~LazyQueryData() {}
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

LazyQueryData::LazyQueryData() : itsInfo(), itsData() {}
// ----------------------------------------------------------------------
/*!
 * \brief Return the parameter name
 *
 * \return The parameter name
 */
// ----------------------------------------------------------------------

std::string LazyQueryData::GetParamName() const
{
  return (itsInfo->Param().GetParamName().CharPtr());
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the parameter ID number
 *
 * \return The number
 */
// ----------------------------------------------------------------------

unsigned long LazyQueryData::GetParamIdent() const
{
  return (itsInfo->Param().GetParamIdent());
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the level number
 *
 * \return The number
 */
// ----------------------------------------------------------------------

float LazyQueryData::GetLevelNumber() const
{
  return (itsInfo->Level()->LevelValue());
}
// ----------------------------------------------------------------------
/*!
 * \brief Lazy-read the given query data file
 *
 * Throws if an error occurs.
 *
 * \param theDataFile The filename (or directory) to read
 */
// ----------------------------------------------------------------------

void LazyQueryData::Read(const std::string &theDataFile)
{
  itsInputName = theDataFile;
  itsDataFile = theDataFile;

  itsData.reset(new NFmiQueryData(theDataFile));
  itsInfo.reset(new NFmiFastQueryInfo(itsData.get()));
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

void LazyQueryData::ResetTime()
{
  itsInfo->ResetTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

void LazyQueryData::ResetLevel()
{
  itsInfo->ResetLevel();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::FirstLevel()
{
  return itsInfo->FirstLevel();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::FirstTime()
{
  return itsInfo->FirstTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::LastTime()
{
  return itsInfo->LastTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::NextLevel()
{
  return itsInfo->NextLevel();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

const NFmiLevel *LazyQueryData::Level() const
{
  return itsInfo->Level();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::NextTime()
{
  return itsInfo->NextTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::PreviousTime()
{
  return itsInfo->PreviousTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::Param(FmiParameterName theParam)
{
  return itsInfo->Param(theParam);
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

const NFmiMetTime &LazyQueryData::ValidTime() const
{
  return itsInfo->ValidTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

const NFmiMetTime &LazyQueryData::OriginTime() const
{
  return itsInfo->OriginTime();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::IsParamUsable() const
{
  return itsInfo->IsParamUsable();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

std::shared_ptr<Fmi::CoordinateMatrix> LazyQueryData::Locations() const
{
  if (itsLocations.get() == 0)
  {
    itsLocations.reset(new Fmi::CoordinateMatrix(itsInfo->CoordinateMatrix()));
    Fmi::CoordinateTransformation transformation(itsInfo->SpatialReference(), "WGS84");
    itsLocations->transform(transformation);
  }

  return itsLocations;
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

std::shared_ptr<Fmi::CoordinateMatrix> LazyQueryData::LocationsWorldXY(
    const NFmiArea &theArea) const
{
  ostringstream os;
  os << theArea;

  if (itsLocationsWorldXY.get() == 0 || os.str() != itsLocationsArea)
  {
    itsLocationsArea = os.str();
    itsLocationsWorldXY.reset(new Fmi::CoordinateMatrix(itsInfo->LocationsWorldXY(theArea)));
  }
  return itsLocationsWorldXY;
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

std::shared_ptr<Fmi::CoordinateMatrix> LazyQueryData::LocationsXY(const NFmiArea &theArea) const
{
  ostringstream os;
  os << theArea;

  if (itsLocationsXY.get() == 0 || os.str() != itsLocationsArea)
  {
    itsLocationsArea = os.str();
    itsLocationsXY.reset(new Fmi::CoordinateMatrix(itsInfo->LocationsXY(theArea)));
  }
  return itsLocationsXY;
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

bool LazyQueryData::BiLinearInterpolation(double x,
                                          double y,
                                          float &theValue,
                                          float topLeftValue,
                                          float topRightValue,
                                          float bottomLeftValue,
                                          float bottomRightValue)
{
  theValue = static_cast<float>(NFmiInterpolation::BiLinear(
      x - floor(x), y - floor(y), topLeftValue, topRightValue, bottomLeftValue, bottomRightValue));
  return (theValue != kFloatMissing);
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

NFmiPoint LazyQueryData::LatLonToGrid(const NFmiPoint &theLatLonPoint)
{
  return itsInfo->Grid()->LatLonToGrid(theLatLonPoint);
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

const NFmiGrid *LazyQueryData::Grid(void) const
{
  return itsInfo->Grid();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

const NFmiArea *LazyQueryData::Area(void) const
{
  return itsInfo->Area();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

float LazyQueryData::InterpolatedValue(const NFmiPoint &theLatLonPoint)
{
  return itsInfo->InterpolatedValue(theLatLonPoint);
}

// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> LazyQueryData::Values()
{
  return itsInfo->Values();
}
// ----------------------------------------------------------------------
/*!
 *
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> LazyQueryData::Values(const NFmiMetTime &theTime)
{
  return itsInfo->Values(theTime);
}

Fmi::CoordinateMatrix LazyQueryData::CoordinateMatrix() const
{
  return itsInfo->CoordinateMatrix();
}

const Fmi::SpatialReference &LazyQueryData::SpatialReference() const
{
  return itsInfo->SpatialReference();
}

// ======================================================================
