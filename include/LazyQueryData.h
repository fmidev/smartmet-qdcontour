// ======================================================================
/*!
 * \file
 * \brief Interface of class LazyQueryData
 */
// ======================================================================
/*!
 * \class LazyQueryData
 * \brief Non-greedy access to querydata
 *
 * The purpose of LazuQueryData is to provide access to the query data
 * in a specific file without reading the full file until absolutely
 * necessary.
 *
 * The basic idea is to always read the header, but the data part
 * only when it is required.
 *
 */
// ======================================================================

#ifndef LAZYQUERYDATA_H
#define LAZYQUERYDATA_H

#include <memory>
#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiParameterName.h>
#include <memory>
#include <string>

class NFmiArea;
class NFmiFastQueryInfo;
class NFmiGrid;
class NFmiLevel;
class NFmiPoint;
class NFmiQueryData;

namespace Fmi
{
class CoordinateMatrix;
class SpatialReference;
}  // namespace Fmi

class LazyQueryData
{
 public:
  ~LazyQueryData();
  LazyQueryData();

  const std::string &Filename() const { return itsDataFile; }
  std::string GetParamName() const;
  unsigned long GetParamIdent() const;
  float GetLevelNumber() const;

  // These do not require the data values

  void Read(const std::string &theDataFile);

  void ResetTime();
  void ResetLevel();
  bool FirstLevel();
  bool FirstTime();
  bool LastTime();
  bool NextLevel();
  bool NextTime();
  bool PreviousTime();
  const NFmiLevel *Level() const;

  bool Param(FmiParameterName theParam);

  // LastTime();
  const NFmiMetTime &ValidTime() const;
  const NFmiMetTime &OriginTime() const;

  bool IsParamUsable() const;

  std::shared_ptr<Fmi::CoordinateMatrix> Locations() const;
  std::shared_ptr<Fmi::CoordinateMatrix> LocationsWorldXY(const NFmiArea &theArea) const;
  std::shared_ptr<Fmi::CoordinateMatrix> LocationsXY(const NFmiArea &theArea) const;

  Fmi::CoordinateMatrix CoordinateMatrix() const;
  const Fmi::SpatialReference &SpatialReference() const;

  bool BiLinearInterpolation(double x,
                             double y,
                             float &theValue,
                             float topLeftValue,
                             float topRightValue,
                             float bottomLeftValue,
                             float bottomRightValue);

  // Grid()

  NFmiPoint LatLonToGrid(const NFmiPoint &theLatLonPoint);
  const NFmiGrid *Grid(void) const;
  const NFmiArea *Area(void) const;

  // These require the data values

  float InterpolatedValue(const NFmiPoint &theLatLonPoint);

  NFmiDataMatrix<float> Values();
  NFmiDataMatrix<float> Values(const NFmiMetTime &theTime);

 private:
  LazyQueryData(const LazyQueryData &theQD);
  LazyQueryData &operator=(const LazyQueryData &theQD);

  std::string itsInputName;
  std::string itsDataFile;
  std::shared_ptr<NFmiFastQueryInfo> itsInfo;
  std::shared_ptr<NFmiQueryData> itsData;

  mutable std::shared_ptr<Fmi::CoordinateMatrix> itsLocations;
  mutable std::shared_ptr<Fmi::CoordinateMatrix> itsLocationsWorldXY;
  mutable std::shared_ptr<Fmi::CoordinateMatrix> itsLocationsXY;
  mutable std::string itsLocationsArea;

};  // class LazyQueryData

#endif  // LAZYQUERYDATA_H

// ======================================================================
