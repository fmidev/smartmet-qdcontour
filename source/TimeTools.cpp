// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace TimeTools
 */
// ======================================================================

#include "TimeTools.h"
#include <newbase/NFmiMetTime.h>
#include <stdexcept>

namespace TimeTools
{
// ----------------------------------------------------------------------
/*!
 * \brief Convert time to another time zone
 *
 * The recognized zones are:
 *
 *  - <em>local</em> for local time
 *  - <em>utc</em> for UTC time (no conversion is done)
 *
 * An exception is thrown if the time zone is not recognized
 *
 * \param theTime The UTC time to convert
 * \param theZone Textual description of the zone
 * \return The converted time
 */
// ----------------------------------------------------------------------

NFmiTime ConvertZone(const NFmiTime &theTime, const std::string &theZone)
{
  if (theZone == "utc")
  {
    return theTime;
  }
  if (theZone == "local")
  {
    return NFmiMetTime(theTime, 1).CorrectLocalTime();
  }
  throw std::runtime_error("ConvertZone: Unrecognized time zone " + theZone);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert time_t to NFmiTime in UTC timezone
 *
 * \param theTime The epoch seconds
 * \return The time
 */
// ----------------------------------------------------------------------

NFmiTime ToUTC(::time_t theTime)
{
  struct ::tm *t = ::gmtime(&theTime);
  return NFmiTime(static_cast<short>(t->tm_year + 1900),
                  static_cast<short>(t->tm_mon + 1),
                  static_cast<short>(t->tm_mday),
                  static_cast<short>(t->tm_hour),
                  static_cast<short>(t->tm_min),
                  static_cast<short>(t->tm_sec));
}
}  // namespace TimeTools

// ======================================================================
