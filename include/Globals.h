// ======================================================================
/*!
 * \file
 * \brief Declaration of global variables
 */
// ======================================================================

#ifndef GLOBALS_H
#define GLOBALS_H

#include "ArrowCache.h"
#include "ContourCalculator.h"
#include "ContourSpec.h"
#include "ExtremaLocator.h"

#include "ImageCache.h"

#include "LabelLocator.h"
#include "ShapeSpec.h"
#include "UnitsConverter.h"

#include <imagine/imagine-config.h>
#ifdef IMAGINE_WITH_CAIRO
#include "ImagineXr.h"
#else
#include <imagine/NFmiColorTools.h>
#endif

#include <imagine/NFmiImage.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>

#include <boost/shared_ptr.hpp>

#include <list>
#include <memory>
#include <string>
#include <vector>

class LazyQueryData;
class NFmiTime;

// using Imagine::NFmiImage;

struct ArrowStyle
{
  float lolimit;
  float hilimit;
  Imagine::NFmiColorTools::Color color;
  Imagine::NFmiColorTools::NFmiBlendRule rule;

  ArrowStyle(Imagine::NFmiColorTools::Color c = Imagine::NFmiColorTools::NoColor,
             Imagine::NFmiColorTools::NFmiBlendRule r = Imagine::NFmiColorTools::kFmiColorOver)
      : lolimit(kFloatMissing), hilimit(kFloatMissing), color(c), rule(r)
  {
  }
};

struct RoundArrowColor
{
  float lolimit;
  float hilimit;
  Imagine::NFmiColorTools::Color circlecolor;
  Imagine::NFmiColorTools::Color trianglecolor;

  RoundArrowColor(Imagine::NFmiColorTools::Color c = Imagine::NFmiColorTools::NoColor)
      : lolimit(kFloatMissing), hilimit(kFloatMissing), circlecolor(c), trianglecolor(c)
  {
  }
};

struct RoundArrowSize
{
  float lolimit;
  float hilimit;
  float circleradius;
  float triangleradius;
  float trianglewidth;
  float triangleangle;

  RoundArrowSize()
      : lolimit(kFloatMissing),
        hilimit(kFloatMissing),
        circleradius(9),
        triangleradius(8),
        trianglewidth(9),
        triangleangle(60)
  {
  }
};

struct Globals
{
  ~Globals();
  Globals();

  void setImageModes(Imagine::NFmiImage &) const;
  boost::shared_ptr<NFmiArea> createArea() const;
  const std::string getImageStampText(const NFmiTime &theTime) const;

  void drawImageStampText(ImagineXr_or_NFmiImage &d, const std::string &text) const;
  void drawCombine(ImagineXr_or_NFmiImage &d) const;

  const ImagineXr_or_NFmiImage &getImage(const std::string &filename) const;

  RoundArrowColor getRoundArrowFillColor(float speed) const;
  RoundArrowColor getRoundArrowStrokeColor(float speed) const;
  RoundArrowSize getRoundArrowSize(float speed) const;

  ArrowStyle getArrowFill(float speed) const;
  ArrowStyle getArrowStroke(float speed) const;

  // Command line options

  bool verbose;                          // -v option
  bool force;                            // -f option
  std::string cmdline_querydata;         // -q option
  std::string cmdline_conf;              // -c option
  std::list<std::string> cmdline_files;  // command line parameters

  // Status variables

  std::string datapath;  // default searchpath for data
  std::string mapspath;  // default searchpath for maps

  std::string savepath;  // image output path
  std::string prefix;    // filename prefix
  std::string suffix;    // filename suffix
  std::string format;    // image format name

#if 0  // def IMAGINE_WITH_CAIRO
  bool antialias;                   // on/off
#endif

  float gamma;         // image gamma correction
  std::string intent;  // image rendering intent
  int alphalimit;      // alpha limit for binary alpha conversion
  int pngquality;      // png quality, -1 = default
  int jpegquality;     // jpeg quality, -1 = default
  bool savealpha;      // save alpha channel?

  bool reducecolors;  // reduce colors before saving?

  bool wantpalette;   // attempt to save as palette image?
  bool forcepalette;  // force palette image?

  std::string contourinterpolation;  // contouring interpolation method
  int contourtriangles;              // keep triangles in result or simplify?

  std::string smoother;  // smoothing method
  float smootherradius;  // smoothing radius
  int smootherfactor;    // smoothing sharpness factor

  bool expanddata;  // whether to expand data or not?

  std::string projection;  // projection definition
  std::string filter;      // filtering mode

  std::string foregroundrule;  // foreground blending rule
  std::string background;      // background image name
  std::string foreground;      // foreground image name
  std::string mask;            // mask image name
  std::string combine;         // combine image name

  int combinex;
  int combiney;
  std::string combinerule;
  float combinefactor;

  std::string erase;       // background color
  std::string fillrule;    // normal filling rule
  std::string strokerule;  // normal stroking rule

  float contourlinewidth;  // width of contour lines

  std::string directionparam;  // direction parameter for arrows
  std::string speedparam;      // speed parameter for arrows

  std::string speedxcomponent;  // X-component for speed
  std::string speedycomponent;  // Y-component for speed

  float arrowscale;  // scale factor for arrows

  std::string arrowfillcolor;
  std::string arrowstrokecolor;
  std::string arrowfillrule;
  std::string arrowstrokerule;
  std::string arrowfile;

  float windarrowscaleA;  // a*log10(b*x+1)
  float windarrowscaleB;  // default:
  float windarrowscaleC;  // 0*log10(0+1)+1 = 1

  float windarrowdx;  // wind arrow grid spacing
  float windarrowdy;

  float windarrowsxyx0;  // windarrowsxy [x0] [y0] [dx] [dy]
  float windarrowsxyy0;  // dx/dy must be positive for rendering to start
  float windarrowsxydx;
  float windarrowsxydy;

  std::list<NFmiPoint> arrowpoints;  // Active wind arrows

  std::string queryfilelist;                // querydata files in use
  std::vector<std::string> queryfilenames;  // querydata files in use

  std::shared_ptr<LazyQueryData> queryinfo;  // active data, does not own pointer
  int querydatalevel;                        // level value (-1 for first)
  int timesteps;                             // how many images to draw?
  int timestep;                              // timestep, 0 = all valid
  int timeinterval;                          // inclusive time interval
  int timestepskip;                          // initial time to skip in minutes
  int timesteprounding;                      // rounding flag
  int timestampflag;                         // put timestamp into image name?
  std::string timestampzone;                 // timezone for the timestamp
  std::string timestampimage;                // image timestamping mode
  int timestampimagex;
  int timestampimagey;
  std::string timestampimageformat;  // hour or hourdate
  std::string timestampimagefont;    // font to draw the timestamp with
  Imagine::NFmiColorTools::Color timestampimagecolor;
  Imagine::NFmiColorTools::Color timestampimagebackground;
  int timestampimagexmargin;
  int timestampimageymargin;

  int contourlabelimagexmargin;  // minimum distance from borders
  int contourlabelimageymargin;

  std::string highpressureimage;  // high pressure image
  std::string highpressurerule;
  float highpressurefactor;
  float highpressureminimum;

  std::string lowpressureimage;  // low pressure image
  std::string lowpressurerule;
  float lowpressurefactor;
  float lowpressuremaximum;

  // Active storage

  ExtremaLocator pressurelocator;  // high/low pressure locator
  LabelLocator labellocator;       // label coordinate calculator
  LabelLocator symbollocator;      // symbol coordinate calculator
  LabelLocator imagelocator;       // contour symbol coordinate calculator

  ContourCalculator calculator;                  // data contourer
  ContourCalculator maskcalculator;              // mask contourer
  std::shared_ptr<LazyQueryData> maskqueryinfo;  // active mask data, does not own pointer
  std::vector<std::shared_ptr<LazyQueryData>> querystreams;

  std::list<ShapeSpec> shapespecs;
  std::list<ContourSpec> specs;

  UnitsConverter unitsconverter;

  ImageCache itsImageCache;
  bool itsImageCacheOn;

  ArrowCache itsArrowCache;

  std::string graticulecolor;
  double graticulelon1;
  double graticulelat1;
  double graticulelon2;
  double graticulelat2;
  double graticuledx;
  double graticuledy;

  std::list<RoundArrowColor> roundarrowfillcolors;
  std::list<RoundArrowColor> roundarrowstrokecolors;
  std::list<RoundArrowSize> roundarrowsizes;

  std::list<ArrowStyle> arrowfillstyles;
  std::list<ArrowStyle> arrowstrokestyles;

  unsigned long timestampformat;
};

// For global use
static Globals globals;

#endif  // GLOBALS_H

// ======================================================================
