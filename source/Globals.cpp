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

#ifdef IMAGINE_WITH_CAIRO
#include "ImagineXr.h"
#else
#include <imagine/NFmiFace.h>
#include <imagine/NFmiFreeType.h>
#endif

#include <imagine/NFmiPath.h>

#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiTime.h>

#include <boost/foreach.hpp>

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
    : verbose(false),
      force(false),
      cmdline_querydata(),
      cmdline_files(),
      datapath(Optional<string>("qdcontour::querydata_path", ".")),
      mapspath(Optional<string>("qdcontour::maps_path", ".")),
      savepath("."),
      prefix(),
      suffix(),
      format("png")  // default format
#if 0                // def IMAGINE_WITH_CAIRO
  , antialias(true)
#endif
      ,
      gamma(-1),
      intent(),
      alphalimit(-1),
      pngquality(-1),
      jpegquality(-1),
      savealpha(true),
      reducecolors(false),
      wantpalette(false),
      forcepalette(false),
      contourinterpolation("Linear"),
      contourtriangles(1),
      smoother("None"),
      smootherradius(1),
      smootherfactor(1),
      expanddata(false),
      projection(),
      filter("none"),
      foregroundrule("Over"),
      background(),
      foreground(),
      mask(),
      combine(),
      combinex(0),
      combiney(0),
      combinerule("Over"),
      combinefactor(1),
      erase("transparent"),
      fillrule("Atop"),
      strokerule("Atop"),
      contourlinewidth(1),
      directionparam("WindDirection"),
      speedparam("WindSpeedMS"),
      speedxcomponent(),
      speedycomponent(),
      arrowscale(1),
      arrowfillcolor("white"),
      arrowstrokecolor("black"),
      arrowfillrule("Over"),
      arrowstrokerule("Over"),
      arrowfile(""),
      windarrowscaleA(0),
      windarrowscaleB(0),
      windarrowscaleC(1),
      windarrowdx(0),
      windarrowdy(0),
      windarrowsxyx0(0),
      windarrowsxyy0(0),
      windarrowsxydx(-1),
      windarrowsxydy(-1),
      arrowpoints(),
      queryfilelist(),
      queryfilenames(),
      queryinfo(),
      querydatalevel(-1),
      timesteps(24),
      timestep(0),
      timeinterval(0),
      timestepskip(0),
      timesteprounding(1),
      timestampflag(1),
      timestampzone("local"),
      timestampimagex(0),
      timestampimagey(0),
      timestampimageformat("hourdateyear"),
      timestampimagefont("misc/6x13B.pcf.gz:6x13"),
      timestampimagecolor(Imagine::NFmiColorTools::Black),
      timestampimagebackground(Imagine::NFmiColorTools::MakeColor(185, 185, 185, 185)),
      timestampimagexmargin(2),
      timestampimageymargin(2),
      contourlabelimagexmargin(20),
      contourlabelimageymargin(20),
      highpressureimage(),
      highpressurerule("Over"),
      highpressurefactor(1),
      highpressureminimum(980),
      lowpressureimage(),
      lowpressurerule("Over"),
      lowpressurefactor(1),
      lowpressuremaximum(1020),
      pressurelocator(),
      labellocator(),
      symbollocator(),
      imagelocator(),
      calculator(),
      maskcalculator(),
      maskqueryinfo(),
      querystreams(),
      shapespecs(),
      specs(),
      unitsconverter(),
      itsImageCache(),
      itsImageCacheOn(true),
      itsArrowCache(),
      graticulecolor(""),
      graticulelon1(),
      graticulelat1(),
      graticulelon2(),
      graticulelat2(),
      graticuledx(),
      graticuledy(),
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

Globals::~Globals() {}
// ----------------------------------------------------------------------
/*!
 * \brief Return the given image
 */
// ----------------------------------------------------------------------

const ImagineXr_or_NFmiImage &Globals::getImage(const string &theFile) const
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

// ----------------------------------------------------------------------
/*!
 * \brief Return the time stamp string to be rendered in the image
 */
// ----------------------------------------------------------------------

const std::string Globals::getImageStampText(const NFmiTime &theTime) const
{
  NFmiTime tobs = TimeTools::ConvertZone(theTime, timestampzone);

  const int obsyy = tobs.GetYear();
  const int obsmm = tobs.GetMonth();
  const int obsdd = tobs.GetDay();
  const int obshh = tobs.GetHour();
  const int obsmi = tobs.GetMin();

  // Interpretation: The age of the forecast is the age
  // of the oldest forecast

  NFmiTime tfor;

  for (unsigned int qi = 0; qi < querystreams.size(); qi++)
  {
    NFmiTime futctime = querystreams[qi]->OriginTime();
    NFmiTime tlocal = TimeTools::ConvertZone(futctime, timestampzone);
    if (qi == 0 || tlocal.IsLessThan(tfor)) tfor = tlocal;
  }

  const int foryy = tfor.GetYear();
  const int formm = tfor.GetMonth();
  const int fordd = tfor.GetDay();
  const int forhh = tfor.GetHour();
  const int formi = tfor.GetMin();

  char buffer[100];

  string stamp;
  if (timestampimage == "obs")
  {
    if (timestampimageformat == "hour")  // hh:mi
      sprintf(buffer, "%02d:%02d", obshh, obsmi);

    else if (timestampimageformat == "hourdate")  // hh:mi dd.mm.
      sprintf(buffer, "%02d:%02d %02d.%02d.", obshh, obsmi, obsdd, obsmm);

    else if (timestampimageformat == "datehour")  // d.m h:mi.
      sprintf(buffer, "%d.%d. %d:%02d", obsdd, obsmm, obshh, obsmi);

    else  // hh:mi dd.mm.yyyy
      sprintf(buffer, "%02d:%02d %02d.%02d.%04d", obshh, obsmi, obsdd, obsmm, obsyy);
    stamp = buffer;
  }
  else if (timestampimage == "for")
  {
    if (timestampimageformat == "hour")  // hh:mi
      sprintf(buffer, "%02d:%02d", forhh, formi);
    else if (timestampimageformat == "hourdate")  // hh:mi dd.mm.
      sprintf(buffer, "%02d:%02d %02d.%02d.", forhh, formi, fordd, formm);
    else if (timestampimageformat == "datehour")  // d.m h:mi
      sprintf(buffer, "%d.%d. %d:%02d", fordd, formm, forhh, formi);
    else  // hh:mi dd.mm.yyyy
      sprintf(buffer, "%02d:%02d %02d.%02d.%04d", forhh, formi, fordd, formm, foryy);
    stamp = buffer;
  }
  else if (timestampimage == "forobs")
  {
    if (timestampimageformat == "hour")  // hh:mi +hh
      sprintf(buffer, "%02d:%02d", forhh, formi);
    else if (timestampimageformat == "hourdate")  // dd.mm. hh:mi + hh
      sprintf(buffer, "%02d.%02d. %02d:%02d", fordd, formm, forhh, formi);
    else if (timestampimageformat == "datehour")  // d.m. h:mi + hh
      sprintf(buffer, "%d.%d. %d:%02d", fordd, formm, forhh, formi);
    else  // dd.mm.yy hh:mi +hh
      sprintf(buffer, "%02d.%02d.%04d %02d:%02d", fordd, formm, foryy, forhh, formi);

    stamp = buffer;

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

#ifdef IMAGINE_WITH_CAIRO
void Globals::drawImageStampText(ImagineXr &img, const std::string &text) const
{
  if (text.empty()) return;

  int x = timestampimagex;
  int y = timestampimagey;

  if (x < 0) x += img.Width();
  if (y < 0) y += img.Height();

  // ImagineXr version (AKa 5-Aug-2008)
  //
  img.MakeFace(timestampimagefont, timestampimagebackground);

  img.DrawFace(x,
               y,
               text,                  // should be UTF-8
               timestampimagecolor);  // font color
}
#else
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
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Draw the "combine" image over the given image
 */
// ----------------------------------------------------------------------

#ifdef IMAGINE_WITH_CAIRO
void Globals::drawCombine(ImagineXr &xr) const
{
  if (combine.empty()) return;

  Imagine::NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(combinerule);

  xr.Composite(
      getImage(combine), rule, Imagine::kFmiAlignNorthWest, combinex, combiney, combinefactor);
}
#else
/*** NFmiImage code (original) ***/
void Globals::drawCombine(Imagine::NFmiImage &theImage) const
{
  if (combine.empty()) return;

  Imagine::NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(combinerule);

  theImage.Composite(
      getImage(combine), rule, Imagine::kFmiAlignNorthWest, combinex, combiney, combinefactor);
}
#endif

// ----------------------------------------------------------------------
// Get specs for round arrows
// ----------------------------------------------------------------------

bool rangefits(float speed, float lolimit, float hilimit)
{
  if (speed == kFloatMissing) return false;
  if (lolimit != kFloatMissing && speed < lolimit) return false;
  if (hilimit != kFloatMissing && speed >= hilimit) return false;
  return true;
}

ArrowStyle Globals::getArrowFill(float speed) const
{
  BOOST_FOREACH (const ArrowStyle &c, arrowfillstyles)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return ArrowStyle(ColorTools::parsecolor(arrowfillcolor), ColorTools::checkrule(arrowfillrule));
}

ArrowStyle Globals::getArrowStroke(float speed) const
{
  BOOST_FOREACH (const ArrowStyle &c, arrowstrokestyles)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return ArrowStyle(ColorTools::parsecolor(arrowstrokecolor),
                    ColorTools::checkrule(arrowstrokerule));
}

RoundArrowColor Globals::getRoundArrowFillColor(float speed) const
{
  BOOST_FOREACH (const RoundArrowColor &c, roundarrowfillcolors)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return RoundArrowColor(Imagine::NFmiColorTools::MakeColor(255, 255, 255));
}

RoundArrowColor Globals::getRoundArrowStrokeColor(float speed) const
{
  BOOST_FOREACH (const RoundArrowColor &c, roundarrowstrokecolors)
  {
    if (rangefits(speed, c.lolimit, c.hilimit)) return c;
  }
  return RoundArrowColor(Imagine::NFmiColorTools::Black);
}

RoundArrowSize Globals::getRoundArrowSize(float speed) const
{
  BOOST_FOREACH (const RoundArrowSize &sz, roundarrowsizes)
  {
    if (rangefits(speed, sz.lolimit, sz.hilimit)) return sz;
  }
  return RoundArrowSize();
}

// ======================================================================
