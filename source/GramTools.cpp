// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace GramTools
 */
// ======================================================================

#include "GramTools.h"
#include "UnitsConverter.h"

namespace GramTools
{
// size of the spot at the origin
const float spot_size = 1.40;

// length of the initial line before flags are added
const float stem_length = 18;

// Separation between barbs
const float barb_interval = stem_length / 5;

// Barb length
const float barb_length = 12;

// Barb angle
const float barb_angle = 45.F / 180.F * 3.14159265358979323846F;

// Flag side length
const float flag_length = 7;

// ----------------------------------------------------------------------
/*!
 * \brief Return meteorological arrow flags for the given wind speed
 *
 * Note: the speed is converted to knots and then rounded so that
 *       if the user prints the values rounded to integers next
 *       to the arrow, the value and the arrow will match
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath metarrowflags(float theSpeed)
{
  Imagine::NFmiPath path;

  if (theSpeed == kFloatMissing) return path;

  // Mark the spot with a small dot
  path.MoveTo(spot_size, spot_size);
  path.LineTo(spot_size, -spot_size);
  path.LineTo(-spot_size, -spot_size);
  path.LineTo(-spot_size, spot_size);
  path.LineTo(spot_size, spot_size);

  // The actual speed in knots
  const int speed = static_cast<int>(round(theSpeed / 0.5144444444F));

  // Handle bad cases
  if (speed < 50) return path;

  // The respective number of flags
  const int flags = speed / 50;

  // The full length of the stem
  const float full_stem_length =
      stem_length + (flags == 0.0 ? 0.0 : flags * flag_length + barb_interval);

  // Flags

  float y = spot_size + full_stem_length;

  for (int i = 0; i < flags; i++)
  {
    path.MoveTo(0, y);
    path.LineTo(-barb_length * cos(barb_angle), y - flag_length + barb_length * sin(barb_angle));
    path.LineTo(0, y - flag_length);
    path.LineTo(0, y);
    y -= flag_length;
  }

  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return meteorological arrow lines for the given wind speed
 *
 * Note: the speed is converted to knots and then rounded so that
 *       if the user prints the values rounded to integers next
 *       to the arrow, the value and the arrow will match
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath metarrowlines(float theSpeed)
{
  Imagine::NFmiPath path;

  // Handle bad cases
  if (theSpeed == kFloatMissing) return path;

  // The actual speed in knots
  const int speed = static_cast<int>(round(theSpeed / 0.5144444444F));

  // Handle bad cases
  if (speed < 5) return path;

  // The respective number of flags
  const int flags = speed / 50;

  // The number of long barbs
  const int long_barbs = (speed - flags * 50) / 10;

  // The number of short barbs
  const int short_barbs = (speed - flags * 50 - long_barbs * 10) / 5;

  // The full length of the stem
  const float full_stem_length =
      stem_length + (flags == 0.0 ? 0.0 : flags * flag_length + barb_interval);

  // The stem

  float y = spot_size + full_stem_length;

  path.MoveTo(0, 0);
  path.LineTo(0, y);

  y -= flags * flag_length;

  // Long barbs

  if (flags > 0)
  {
    y -= barb_interval;
  }

  for (int i = 0; i < long_barbs; i++)
  {
    path.MoveTo(0, y);
    path.LineTo(-barb_length * cos(barb_angle), y + barb_length * sin(barb_angle));
    y -= barb_interval;
  }

  // Short barbs. We use factor 1.5 to make 5 knot arrow more readable

  if (long_barbs == 0 && flags == 0 && short_barbs > 0) y -= 1.5 * barb_interval;

  for (int i = 0; i < short_barbs; i++)
  {
    path.MoveTo(0, y);
    path.LineTo(-0.5 * barb_length * cos(barb_angle), y + 0.5 * barb_length * sin(barb_angle));
    y -= barb_interval;
  }

  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return meteorological arrow flags for the given wind speed
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath metarrowflags(float theSpeed, const NFmiPoint &theLatLon)
{
  Imagine::NFmiPath path = metarrowflags(theSpeed);
  if (theLatLon.Y() < 0) path.Scale(-1, 1);
  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return meteorological arrow lines for the given wind speed
 */
// ----------------------------------------------------------------------

Imagine::NFmiPath metarrowlines(float theSpeed, const NFmiPoint &theLatLon)
{
  Imagine::NFmiPath path = metarrowlines(theSpeed);
  if (theLatLon.Y() < 0) path.Scale(-1, 1);
  return path;
}

}  // namespace GramTools

// ======================================================================
