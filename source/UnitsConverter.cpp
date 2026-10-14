// ======================================================================
/*!
 * \file
 * \brief Implementation of class UnitsConverter
 */
// ======================================================================

#include "UnitsConverter.h"
#include <algorithm>
#include <stdexcept>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Available conversions
 */
// ----------------------------------------------------------------------

enum ConversionType
{
  NoConversion = 0,
  CelsiusToFahrenheit,
  FahrenheitToCelsius,
  MetersPerSecondToKnots,
  MetersToFeet,
  KiloMetersToFeet,
  KiloMetersToFlightLevel
};

// ----------------------------------------------------------------------
/*!
 * \brief Convert degrees Celsius to Fahrenheit
 */
// ----------------------------------------------------------------------

namespace
{
inline float celsius_to_fahrenheit(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return (1.8f * theValue + 32);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert degrees Fahrenheit to Celsius
 */
// ----------------------------------------------------------------------

inline float fahrenheit_to_celsius(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return (theValue - 32) / 1.8f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert meters per second to knots
 *
 * 1 knot = 1,852 km/h = 0.5144444444444.. m/s
 */
// ----------------------------------------------------------------------

inline float meterspersecond_to_knots(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return theValue / 0.51444444444444444444444444f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert meters to feet
 *
 * 1 foot = 0.3048 m
 */
// ----------------------------------------------------------------------

inline float meters_to_feet(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return theValue / 0.3048f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert kilometers to feet
 *
 * 1 foot = 0.3048 m
 */
// ----------------------------------------------------------------------

inline float kilometers_to_feet(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return 1000.0f * theValue / 0.3048f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert kilometers to flight level ( = feet/100)
 *
 * 1 foot = 0.3048 m
 */
// ----------------------------------------------------------------------

inline float kilometers_to_flightlevel(float theValue)
{
  if (theValue == kFloatMissing)
    return kFloatMissing;
  else
    return 10.0f * theValue / 0.3048f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert degrees Celsius to Fahrenheit
 */
// ----------------------------------------------------------------------

void celsius_to_fahrenheit(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = celsius_to_fahrenheit(theValues[i][j]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert degrees Fahrenheit to Celsius
 */
// ----------------------------------------------------------------------

void fahrenheit_to_celsius(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = fahrenheit_to_celsius(theValues[i][j]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert meters per second to knots
 */
// ----------------------------------------------------------------------

void meterspersecond_to_knots(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = meterspersecond_to_knots(theValues[i][j]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert meters to feet
 */
// ----------------------------------------------------------------------

void meters_to_feet(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = meters_to_feet(theValues[i][j]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert kilometers to feet
 */
// ----------------------------------------------------------------------

void kilometers_to_feet(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = kilometers_to_feet(theValues[i][j]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert kilometers to flight level
 */
// ----------------------------------------------------------------------

void kilometers_to_flightlevel(NFmiDataMatrix<float> &theValues)
{
  for (NFmiDataMatrix<float>::size_type j = 0; j < theValues.NY(); j++)
    for (NFmiDataMatrix<float>::size_type i = 0; i < theValues.NX(); i++)
      theValues[i][j] = kilometers_to_flightlevel(theValues[i][j]);
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the converter
 */
// ----------------------------------------------------------------------

UnitsConverter::UnitsConverter() : itsConversions(kFmiLastParameter, NoConversion) {}
// ----------------------------------------------------------------------
/*!
 * \brief Clear all conversions
 */
// ----------------------------------------------------------------------

void UnitsConverter::clear() { fill(itsConversions.begin(), itsConversions.end(), NoConversion); }
// ----------------------------------------------------------------------
/*!
 * \brief Set a new conversion
 */
// ----------------------------------------------------------------------

void UnitsConverter::setConversion(FmiParameterName theParam, const std::string &theConversion)
{
  if (theConversion == "celsius_to_fahrenheit")
    itsConversions[theParam] = CelsiusToFahrenheit;
  else if (theConversion == "fahrenheit_to_celsius")
    itsConversions[theParam] = FahrenheitToCelsius;
  else if (theConversion == "meterspersecond_to_knots")
    itsConversions[theParam] = MetersPerSecondToKnots;
  else if (theConversion == "meters_to_feet")
    itsConversions[theParam] = MetersToFeet;
  else if (theConversion == "kilometers_to_feet")
    itsConversions[theParam] = KiloMetersToFeet;
  else if (theConversion == "kilometers_to_flightlevel")
    itsConversions[theParam] = KiloMetersToFlightLevel;
  else
    throw runtime_error("Unknown unit conversion '" + theConversion + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a single value
 */
// ----------------------------------------------------------------------

float UnitsConverter::convert(FmiParameterName theParam, float theValue) const
{
  switch (itsConversions[theParam])
  {
    case CelsiusToFahrenheit:
      return celsius_to_fahrenheit(theValue);
    case FahrenheitToCelsius:
      return fahrenheit_to_celsius(theValue);
    case MetersPerSecondToKnots:
      return meterspersecond_to_knots(theValue);
    case MetersToFeet:
      return meters_to_feet(theValue);
    case KiloMetersToFeet:
      return kilometers_to_feet(theValue);
    case KiloMetersToFlightLevel:
      return kilometers_to_flightlevel(theValue);
    default:
      return theValue;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a datamatrix
 */
// ----------------------------------------------------------------------

void UnitsConverter::convert(FmiParameterName theParam, NFmiDataMatrix<float> &theValues) const
{
  switch (itsConversions[theParam])
  {
    case NoConversion:
      return;
    case CelsiusToFahrenheit:
      celsius_to_fahrenheit(theValues);
      break;
    case FahrenheitToCelsius:
      fahrenheit_to_celsius(theValues);
      break;
    case MetersPerSecondToKnots:
      meterspersecond_to_knots(theValues);
      break;
    case MetersToFeet:
      meters_to_feet(theValues);
      break;
    case KiloMetersToFeet:
      kilometers_to_feet(theValues);
      break;
    case KiloMetersToFlightLevel:
      kilometers_to_flightlevel(theValues);
      break;
  }
}
