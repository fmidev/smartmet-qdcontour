// ======================================================================
/*!
 * \file
 * \brief Declaration of global variables
 */
// ======================================================================

#ifndef GLOBALS_H
#define GLOBALS_H

#include "ContourCalculator.h"
#include "ContourSpec.h"
#include "ExtremaLocator.h"
#include "ImageCache.h"
#include "LabelLocator.h"
#include "ShapeSpec.h"

#include "NFmiColorTools.h"
#include "NFmiImage.h"
#include "NFmiPoint.h"

#include <boost/shared_ptr.hpp>

#include <list>
#include <memory>
#include <string>
#include <vector>

class LazyQueryData;
class NFmiTime;

using Imagine::NFmiImage;

struct Globals
{
  ~Globals();
  Globals();

  void setImageModes(NFmiImage & theImage) const;
  std::auto_ptr<NFmiArea> createArea() const;
  const std::string getImageStampText(const NFmiTime & theTime) const;
  void drawImageStampText(NFmiImage & theImage, const std::string & theText) const;
  void drawCombine(NFmiImage & theImage) const;
  const Imagine::NFmiImage & getImage(const std::string & theFile) const;

  bool isOutdated() const;

  // Command line options

  bool verbose;								// -v option
  bool force;								// -f option
  std::string cmdline_querydata;			// -q option
  std::list<std::string> cmdline_files;		// command line parameters

  // Status variables

  std::string datapath;				// default searchpath for data
  std::string mapspath;				// default searchpath for maps

  std::string savepath;				// image output path
  std::string prefix;				// filename prefix
  std::string suffix;				// filename suffix
  std::string format;				// image format name
  float gamma;						// image gamma correction
  std::string intent;				// image rendering intent
  int alphalimit;					// alpha limit for binary alpha conversion
  int pngquality;					// png quality, -1 = default
  int jpegquality;					// jpeg quality, -1 = default
  bool savealpha;					// save alpha channel?
  bool reducecolors;				// reduce colors before saving?
  bool wantpalette;					// attempt to save as palette image?
  bool forcepalette;				// force palette image?

  std::string contourinterpolation;	// contouring interpolation method
  int contourtriangles;				// keep triangles in result or simplify?

  std::string smoother;				// smoothing method
  float smootherradius;				// smoothing radius
  int smootherfactor;				// smoothing sharpness factor

  bool expanddata;					// whether to expand data or not?

  std::string projection;			// projection definition
  std::string filter;				// filtering mode

  std::string foregroundrule;		// foreground blending rule
  std::string background;			// background image name
  std::string foreground;			// foreground image name
  std::string mask;					// mask image name
  std::string combine;				// combine image name

  int combinex;
  int combiney;
  std::string combinerule;
  float combinefactor;

  
  std::string erase;				// background color
  std::string fillrule;				// normal filling rule
  std::string strokerule;			// normal stroking rule

  std::string directionparam;		// direction parameter for arrows
  std::string speedparam;			// speed parameter for arrows
  float arrowscale;					// scale factor for arrows

  std::string arrowfillcolor;
  std::string arrowstrokecolor;
  std::string arrowfillrule;
  std::string arrowstrokerule;
  std::string arrowfile;

  float windarrowscaleA;			// a*log10(b*x+1)
  float windarrowscaleB;			// default:
  float windarrowscaleC;			// 0*log10(0+1)+1 = 1

  float windarrowdx;					// wind arrow grid spacing
  float windarrowdy;

  float windarrowsxyx0;				// windarrowsxy [x0] [y0] [dx] [dy]
  float windarrowsxyy0;				// dx/dy must be positive for rendering to start
  float windarrowsxydx;
  float windarrowsxydy;

  std::list<NFmiPoint> arrowpoints;	// Active wind arrows

  std::string queryfilelist;		// querydata files in use
  std::vector<std::string> queryfilenames;	// querydata files in use

  boost::shared_ptr<LazyQueryData> queryinfo;		// active data, does not own pointer
  int querydatalevel;				// level value (-1 for first)
  int timesteps;					// how many images to draw?
  int timestep;						// timestep, 0 = all valid
  int timeinterval;					// inclusive time interval
  int timestepskip;					// initial time to skip in minutes
  int timesteprounding;				// rounding flag
  int timestampflag;				// put timestamp into image name?
  std::string timestampzone;		// timezone for the timestamp
  std::string timestampimage;		// image timestamping mode
  int timestampimagex;
  int timestampimagey;
  std::string timestampimageformat;	// hour or hourdate
  std::string timestampimagefont;	// font to draw the timestamp with
  Imagine::NFmiColorTools::Color timestampimagecolor;
  Imagine::NFmiColorTools::Color timestampimagebackground;
  int timestampimagexmargin;
  int timestampimageymargin;


  int contourlabelimagexmargin;		// minimum distance from borders
  int contourlabelimageymargin;

  std::string highpressureimage;		// high pressure image
  std::string highpressurerule;
  float highpressurefactor;
  float highpressureminimum;

  std::string lowpressureimage;		// low pressure image
  std::string lowpressurerule;
  float lowpressurefactor;
  float lowpressuremaximum;

  // Active storage

  ExtremaLocator pressurelocator;	// high/low pressure locator
  LabelLocator labellocator;		// label coordinate calculator
  LabelLocator symbollocator;		// symbol coordinate calculator

  ContourCalculator calculator;		// data contourer
  ContourCalculator maskcalculator;	// mask contourer
  boost::shared_ptr<LazyQueryData> maskqueryinfo;	// active mask data, does not own pointer
  std::vector<boost::shared_ptr<LazyQueryData> > querystreams;

  std::list<ShapeSpec> shapespecs;
  std::list<ContourSpec> specs;

  std::string contourmaskparam;
  float contourmasklolimit;
  float contourmaskhilimit;

  ImageCache itsImageCache;

};

// For global use
static Globals globals;

#endif // GLOBALS_H

// ======================================================================
