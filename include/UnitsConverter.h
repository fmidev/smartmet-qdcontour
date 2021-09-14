// ======================================================================
/*!
 * \file
 * \brief Interface of class UnitsConverter
 */
// ======================================================================

#ifndef UNITSCONVERTER_H
#define UNITSCONVERTER_H

#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiParameterName.h>

#include <string>
#include <vector>

class UnitsConverter
{
 public:
  UnitsConverter();

  void clear();

  void setConversion(FmiParameterName theParam, const std::string &theConversion);

  float convert(FmiParameterName theParam, float theValue) const;
  void convert(FmiParameterName theParam, NFmiDataMatrix<float> &theValue) const;

 private:
  typedef std::vector<int> storage_type;
  storage_type itsConversions;

};  // class UnitsConverter

#endif  // UNITSCONVERTER_H
