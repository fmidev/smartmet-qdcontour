// ======================================================================
/*!
 * \file
 * \brief Definition of global variables
 */
// ======================================================================

#include "Globals.h"
#include "ColorTools.h"
#include "LazyQueryData.h"
#include "TimeTools.h"

#include <imagine/NFmiEsriBox.h>

#include <imagine/NFmiFace.h>
#include <imagine/NFmiFreeType.h>

#include <imagine/NFmiPath.h>

#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiTime.h>


#include <string>

using NFmiSettings::Optional;
// using namespace Imagine;
using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Constructor for global variables
 */
// ----------------------------------------------------------------------

Globals::Globals()
    :
      datapath(Optional<string>("qdcontour::querydata_path", ".")),
      mapspath(Optional<string>("qdcontour::maps_path", ".")),
      savepath("."),
      format("png"),
      contourinterpolation("Linear"),
      smoother("None"),
      filter("none"),
      foregroundrule("Over"),
      combinerule("Over"),
      erase("transparent"),
      fillrule("Atop"),
      strokerule("Atop"),
      directionparam("WindDirection"),
      speedparam("WindSpeedMS"),
      arrowfillcolor("white"),
      arrowstrokecolor("black"),
      arrowfillrule("Over"),
      arrowstrokerule("Over"),
      timestampzone("local"),
      timestampimageformat("hourdateyear"),
      timestampimagefont("misc/6x13B.pcf.gz:6x13"),
      timestampimagecolor(Imagine::NFmiColorTools::Black),
      timestampimagebackground(Imagine::NFmiColorTools::MakeColor(185, 185, 185, 185)),
      highpressurerule("Over"),
      lowpressurerule("Over"),
      itsImageCache(),
      itsArrowCache(),
      timestampformat(kYYYYMMDDHHMM)
{
  symbollocator.minDistanceToDifferentParameter(8);
  symbollocator.minDistanceToDifferentValue(8);
  symbollocator.minDistanceToSameValue(8);

  imagelocator.minDistanceToDifferentParameter(4);
  imagelocator.minDistanceToDifferentValue(4);
  imagelocator.minDistanceToSameValue(4);
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Globals::~Globals() = default;
// ----------------------------------------------------------------------
/*!
 * \brief Return the given image
 */
// ----------------------------------------------------------------------

const Imagine::NFmiImage& Globals::getImage(const string& theFile) const
{
  return itsImageCache.getImage(theFile);
}

// ----------------------------------------------------------------------
/*!
 * \brief Set image modes
 */
// ----------------------------------------------------------------------

void Globals::setImageModes(Imagine::NFmiImage &theImage) const
{
  theImage.SaveAlpha(savealpha);
  theImage.WantPalette(wantpalette);
  theImage.ForcePalette(forcepalette);
  if (gamma > 0) theImage.Gamma(gamma);
  if (!intent.empty()) theImage.Intent(intent);
  if (pngquality >= 0) theImage.PngQuality(pngquality);
  if (jpegquality >= 0) theImage.JpegQuality(jpegquality);
  if (alphalimit >= 0) theImage.AlphaLimit(alphalimit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the area object
 */
// ----------------------------------------------------------------------

std::shared_ptr<NFmiArea> Globals::createArea() const
{
  if (projection.empty()) throw runtime_error("A projection specification is required");

  return NFmiAreaFactory::Create(projection);
}

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Format the observation or forecast time stamp
 */
// ----------------------------------------------------------------------

std::string format_stamp_time(const std::string& theFormat, const NFmiTime& theTime)
{
  const int yy = theTime.GetYear();
  const int mm = theTime.GetMonth();
  const int dd = theTime.GetDay();
  const int hh = theTime.GetHour();
  const int mi = theTime.GetMin();

  char buffer[100];

  if (theFormat == "hour")  // hh:mi
    sprintf(buffer, "%02d:%02d", hh, mi);

  else if (theFormat == "hourdate")  // hh:mi dd.mm.
    sprintf(buffer, "%02d:%02d %02d.%02d.", hh, mi, dd, mm);

  else if (theFormat == "datehour")  // d.m h:mi.
    sprintf(buffer, "%d.%d. %d:%02d", dd, mm, hh, mi);

  else  // hh:mi dd.mm.yyyy
    sprintf(buffer, "%02d:%02d %02d.%02d.%04d", hh, mi, dd, mm, yy);

  return buffer;
}

// ----------------------------------------------------------------------
/*!
 * \brief Format the forecast time of a forecast+observation time stamp
 */
// ----------------------------------------------------------------------

std::string format_forobs_stamp_time(const std::string& theFormat, const NFmiTime& theTime)
{
  const int yy = theTime.GetYear();
  const int mm = theTime.GetMonth();
  const int dd = theTime.GetDay();
  const int hh = theTime.GetHour();
  const int mi = theTime.GetMin();

  char buffer[100];

  if (theFormat == "hour")  // hh:mi +hh
    sprintf(buffer, "%02d:%02d", hh, mi);
  else if (theFormat == "hourdate")  // dd.mm. hh:mi + hh
    sprintf(buffer, "%02d.%02d. %02d:%02d", dd, mm, hh, mi);
  else if (theFormat == "datehour")  // d.m. h:mi + hh
    sprintf(buffer, "%d.%d. %d:%02d", dd, mm, hh, mi);
  else  // dd.mm.yy hh:mi +hh
    sprintf(buffer, "%02d.%02d.%04d %02d:%02d", dd, mm, yy, hh, mi);

  return buffer;
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Return the time stamp string to be rendered in the image
 */
// ----------------------------------------------------------------------

std::string Globals::getImageStampText(const NFmiTime& theTime) const
{
  NFmiTime tobs = TimeTools::ConvertZone(theTime, timestampzone);

  // Interpretation: The age of the forecast is the age
  // of the oldest forecast

  NFmiTime tfor;

  for (unsigned int qi = 0; qi < querystreams.size(); qi++)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
    NFmiTime futctime = querystreams[qi]->OriginTime();
    NFmiTime tlocal = TimeTools::ConvertZone(futctime, timestampzone);
    if (qi == 0 || tlocal.IsLessThan(tfor)) tfor = tlocal;
  }

  char buffer[100];

  string stamp;
  if (timestampimage == "obs")
  {
    stamp = format_stamp_time(timestampimageformat, tobs);
  }
  else if (timestampimage == "for")
  {
    stamp = format_stamp_time(timestampimageformat, tfor);
  }
  else if (timestampimage == "forobs")
  {
    stamp = format_forobs_stamp_time(timestampimageformat, tfor);

    const long diff = tobs.DifferenceInMinutes(tfor);
    if (diff % 60 == 0 && timestep % 60 == 0)
      sprintf(buffer, " %s%ldh", (diff < 0 ? "" : "+"), diff / 60);
    else
      sprintf(buffer, " %s%ldm", (diff < 0 ? "" : "+"), diff);

    stamp += buffer;
  }
  return stamp;
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the given text into the time stamp position in the image
 */
// ----------------------------------------------------------------------

/*** NFmiImage version (original) ***/
void Globals::drawImageStampText(Imagine::NFmiImage &theImage, const std::string &theText) const
{
  if (theText.empty()) return;

  Imagine::NFmiFace face(timestampimagefont);

  int x = timestampimagex;
  int y = timestampimagey;

  if (x < 0) x += theImage.Width();
  if (y < 0) y += theImage.Height();

  if (timestampimagebackground != Imagine::NFmiColorTools::NoColor)
  {
    face.Background(true);
    face.BackgroundMargin(timestampimagexmargin, timestampimageymargin);
    face.BackgroundColor(timestampimagebackground);
  }

  face.Draw(theImage, x, y, theText, Imagine::kFmiAlignNorthWest, timestampimagecolor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the "combine" image over the given image
 */
// ----------------------------------------------------------------------

/*** NFmiImage code (original) ***/
void Globals::drawCombine(Imagine::NFmiImage &theImage) const
{
  if (combine.empty()) return;

  Imagine::NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(combinerule);

  theImage.Composite(
      getImage(combine), rule, Imagine::kFmiAlignNorthWest, combinex, combiney, combinefactor);
}

// ----------------------------------------------------------------------
// Get specs for round arrows
// ----------------------------------------------------------------------

namespace
{
bool rangefits(float speed, float lolimit, float hilimit)
{
  if (speed == kFloatMissing) return false;
  if (lolimit != kFloatMissing && speed < lolimit) return false;
  if (hilimit != kFloatMissing && speed >= hilimit) return false;
  return true;
}
}  // namespace

ArrowStyle Globals::getArrowFill(float speed) const
{
  for (const ArrowStyle &c : arrowfillstyles)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return ArrowStyle(ColorTools::parsecolor(arrowfillcolor), ColorTools::checkrule(arrowfillrule));
}

ArrowStyle Globals::getArrowStroke(float speed) const
{
  for (const ArrowStyle &c : arrowstrokestyles)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return ArrowStyle(ColorTools::parsecolor(arrowstrokecolor),
                    ColorTools::checkrule(arrowstrokerule));
}

RoundArrowColor Globals::getRoundArrowFillColor(float speed) const
{
  for (const RoundArrowColor &c : roundarrowfillcolors)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return RoundArrowColor(Imagine::NFmiColorTools::MakeColor(255, 255, 255));
}

RoundArrowColor Globals::getRoundArrowStrokeColor(float speed) const
{
  for (const RoundArrowColor &c : roundarrowstrokecolors)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return RoundArrowColor(Imagine::NFmiColorTools::Black);
}

RoundArrowSize Globals::getRoundArrowSize(float speed) const
{
  for (const RoundArrowSize &sz : roundarrowsizes)
  {
    if (rangefits(speed, sz.lolimit, sz.hilimit)) return sz;
  }
  return {};
}

// ======================================================================
