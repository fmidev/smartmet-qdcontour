// ======================================================================
/*!
 * \file
 * \brief Main program for qdcontour
 */
// ======================================================================

#include "ColorTools.h"
#include "ContourInterpolation.h"
#include "ContourSpec.h"
#include "ExtremaLocator.h"
#include "Globals.h"
#include "GramTools.h"
#include "LazyCoordinates.h"
#include "LazyQueryData.h"
#include "MeridianTools.h"
#include "MetaFunctions.h"
#include "TimeTools.h"
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <gis/CoordinateMatrix.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <gis/SpatialReference.h>
#include <imagine/NFmiColorTools.h>
#include <imagine/NFmiGeoShape.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiDataModifierAvg.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiInterpolation.h>
#include <newbase/NFmiLevel.h>
#include <newbase/NFmiPreProcessor.h>
#include <newbase/NFmiSettings.h>  // Configuration
#include <newbase/NFmiSmoother.h>  // for smoothing data
#include <newbase/NFmiStringTools.h>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef IMAGINE_WITH_CAIRO
#include "ImagineXr.h"
typedef ImagineXr ImagineXr_or_NFmiImage;
#else
#include <imagine/NFmiFace.h>
#include <imagine/NFmiFreeType.h>
#include <imagine/NFmiImage.h>
typedef Imagine::NFmiImage ImagineXr_or_NFmiImage;
#endif

using namespace std;
using namespace boost;
using namespace Imagine;

const float pi = 3.141592658979323f;

// ----------------------------------------------------------------------
// Convert parameter description to a enum
// ----------------------------------------------------------------------

FmiParameterName toparam(const string &name)
{
  static NFmiEnumConverter converter;

  try
  {
    return FmiParameterName(boost::lexical_cast<int>(name));
  }
  catch (...)
  {
    return FmiParameterName(converter.ToEnum(name));
  }
}

// ----------------------------------------------------------------------
// Usage
// ----------------------------------------------------------------------

void Usage(void)
{
  cout << "Usage: qdcontour [options] [conffiles]" << endl
       << endl
       << "Available options:" << endl
       << "   -h\tDisplay this help information" << endl
       << "   -v\tVerbose mode" << endl
       << "   -f\tForce overwriting old images" << endl
       << "   -q [querydata]\tSpecify querydata to be rendered" << endl
       << "   -c \"config line\"\tPrecede with config line (i.e. \"format pdf\")" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * Test whether the given pixel coordinate is masked. This by definition
 * means the respective pixel in the given mask is not fully transparent.
 *
 * Any pixel outside the mask image is considered to be masker similarly
 * to the mask pixel nearest to it. This "extends" sea and land as is
 * usually expected when masking wind arrows etc.
 *
 * \param thePoint The pixel coordinate
 * \param theMask The mask filename
 * \param theMaskImage The mask image
 * \return True, if the pixel is masked out
 */
// ----------------------------------------------------------------------

bool IsMasked(const NFmiPoint &thePoint, const std::string &theMask)
{
  if (theMask.empty())
    return false;

  int x = static_cast<int>(round(thePoint.X()));
  int y = static_cast<int>(round(thePoint.Y()));

  // Get the mask

  const ImagineXr_or_NFmiImage &mask = globals.getImage(theMask);

  // Clip outside pixels

  x = min(max(x, 0), mask.Width() - 1);
  y = min(max(y, 0), mask.Height() - 1);

  const NFmiColorTools::Color c = mask(x, y);
  const int alpha = NFmiColorTools::GetAlpha(c);

  return (alpha != NFmiColorTools::Transparent);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line options
 */
// ----------------------------------------------------------------------

void parse_command_line(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "hvfq!c!");

  // Check for parsing errors

  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // Handle -h option

  if (cmdline.isOption('h'))
  {
    Usage();
    exit(0);
  }

  // Read -v option

  if (cmdline.isOption('v'))
    globals.verbose = true;

  // Read -f option

  if (cmdline.isOption('f'))
    globals.force = true;

  if (cmdline.isOption('q'))
    globals.cmdline_querydata = cmdline.OptionValue('q');

  // AKa 22-Aug-2008: Added for allowing "format pdf" enforcing (or any other
  //                  command) from the command line.
  //
  if (cmdline.isOption('c'))
    globals.cmdline_conf = cmdline.OptionValue('c');

  // Read command filenames

  if (cmdline.NumberofParameters() == 0)
    throw runtime_error("Atleast one command line parameter is required");

  for (int i = 1; i <= cmdline.NumberofParameters(); i++)
    globals.cmdline_files.push_back(cmdline.Parameter(i));
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the given configuration script
 *
 * \param theName The file to read
 * \return The contents of the file, preprocessed
 */
// ----------------------------------------------------------------------

const string read_script(const string &theName)
{
  extern char **environ;

  const bool strip_pound = false;
  NFmiPreProcessor processor(strip_pound);

  processor.SetDefine("#define");
  processor.SetIncluding("include", "", "");

  int i = 1;
  char *s = *environ;

  for (; s; i++)
  {
    std::string setting = s;
    auto parts = NFmiStringTools::Split(setting, "=");
    processor.AddReplaceString("$" + parts[0], parts[1]);
    s = *(environ + i);
  }

  if (!processor.ReadAndStripFile(theName))
  {
    if (!NFmiFileSystem::FileExists(theName))
      throw runtime_error("Script file '" + theName + "' does not exist");
    throw runtime_error("Preprocessor failed to parse '" + theName +
                        "': " + processor.GetMessage());
  }

  return processor.GetString();
}

// ----------------------------------------------------------------------
/*!
 * \brief Preprocess a configuration script for execution
 *
 * Currently the preprocessing consists only of handling the
 * possible -q command line option. When the option is present,
 * the equivalent 'querydata' command is inserted into the first
 * line of the script.
 *
 * \param theScript The script to preprocess
 * \return The preprocessed script
 */
// ----------------------------------------------------------------------

const string preprocess_script(const string &theScript)
{
  string ret;

  if (!globals.cmdline_querydata.empty())
  {
    ret += "querydata ";
    ret += globals.cmdline_querydata;
    ret += '\n';
  }
  ret += theScript;

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Flip fill inside out if contour limits are missing
 *
 * We assume the path has already been projected to pixel coordinates
 * so that we can use reasonable values for the external box around
 * the image.
 *
 */
// ----------------------------------------------------------------------

void invert_if_missing(NFmiPath &thePath, float lolimit, float hilimit)
{
  const float m = 10000;
  if (lolimit != kFloatMissing || hilimit != kFloatMissing)
    return;
  thePath.MoveTo(-m, -m);
  thePath.LineTo(m, -m);
  thePath.LineTo(m, m);
  thePath.LineTo(-m, m);
  thePath.LineTo(-m, -m);
}

// ----------------------------------------------------------------------
/*!
 * \brief Check input stream validity
 */
// ----------------------------------------------------------------------

void check_errors(istream &theInput, const string &theFunction)
{
  if (theInput.fail())
    throw runtime_error("Processing the '" + theFunction + "' command failed");
}

// ----------------------------------------------------------------------
/*!
 * \brief Set queryinfo level
 *
 * A negative level value implies the first level in the data
 *
 * \param theInfo The queryinfo
 * \param theLevel The level value
 */
// ----------------------------------------------------------------------

bool set_level(LazyQueryData &theInfo, int theLevel)
{
  if (theLevel < 0)
  {
    theInfo.FirstLevel();
    return true;
  }
  else
  {
    for (theInfo.ResetLevel(); theInfo.NextLevel();)
      if (theInfo.Level()->LevelValue() == static_cast<unsigned int>(theLevel))
        return true;
    return false;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print debugging information on area object
 */
// ----------------------------------------------------------------------

void report_area(const NFmiArea &theArea)
{
  cout << "Area corners are" << endl
       << "bottomleft\t= " << theArea.BottomLeftLatLon().X() << ','
       << theArea.BottomLeftLatLon().Y() << endl
       << "topright\t= " << theArea.TopRightLatLon().X() << ',' << theArea.TopRightLatLon().Y()
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print debugging information on chosen querydata
 */
// ----------------------------------------------------------------------

void report_queryinfo(const string &theParam, unsigned int theIndex)
{
  cout << "Param " << theParam << " from queryfile number " << theIndex + 1 << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print debugging information on data extrema
 */
// ----------------------------------------------------------------------

void report_extrema(const string &theParam, float theMin, float theMax)
{
  cout << "Data range for " << theParam << " is " << theMin << "..." << theMax << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Write image to file with desired format
 */
// ----------------------------------------------------------------------

#ifdef IMAGINE_WITH_CAIRO
static void write_image(const ImagineXr &xr)
{
  const string filename = xr.Filename();
  const string format = xr.Format();

  if (globals.verbose)
    cout << "Writing '" << filename << "'" << endl;

  if ((format == "pdf") || (format == "png" && (!globals.reducecolors)))
  {
    // Cairo native writing (faster)
    //
    xr.Write();
  }
  else
  {
    // Convert to 'NFmiImage'
    //
    // Both Cairo and NFmiImage use ARGB_32 format, but NFmiImage has A
    // as opaqueness (0..127, 0=transparent) while Cairo as alpha (0..255,
    // 255=transparent).
    //
    unsigned n = xr.Width() * xr.Height();
#if 0
        NFmiColorTools::Color buf[n];   // does not compile with VC++
        xr.NFmiColorBuf(buf);
        NFmiImage img( xr.Width(), xr.Height(), buf );
#else
    NFmiImage img;
    {
      NFmiColorTools::Color *buf = new NFmiColorTools::Color[n];
      xr.NFmiColorBuf(buf);
      img = NFmiImage(xr.Width(), xr.Height(), buf);
      delete[] buf;
    }
#endif
    globals.setImageModes(img);

    if (globals.reducecolors)
      img.ReduceColors();

    img.Write(filename, format);
  }

  if (!globals.itsImageCacheOn)
    globals.itsImageCache.clear();
}
#else
static void write_image(NFmiImage &theImage, const string &theName, const string &theFormat)
{
  if (globals.verbose)
    cout << "Writing '" << theName << "'" << endl;

  if (globals.reducecolors)
    theImage.ReduceColors();

  theImage.Write(theName, theFormat);

  if (!globals.itsImageCacheOn)
    globals.itsImageCache.clear();
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Create a face from a font specification string
 *
 * The string is of the form <fontname>:<width>x<height>
 * If width or height is zero, Freetype will calculate it
 * so that proper aspect ratio is preserved.
 */
// ----------------------------------------------------------------------

#ifndef IMAGINE_WITH_CAIRO
Imagine::NFmiFace make_face(const string &theSpec)
{
  return NFmiFace(theSpec);
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Handle a comment token
 */
// ----------------------------------------------------------------------

void do_comment(istream &theInput)
{
#ifdef PERKELEEN_296
  theInput.ignore(1000000, '\n');
#else
  theInput.ignore(numeric_limits<std::streamsize>::max(), '\n');
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle the "cache" command
 */
// ----------------------------------------------------------------------

void do_cache(istream &theInput)
{
  int flag;
  theInput >> flag;

  check_errors(theInput, "cache");

  globals.calculator.cache(flag != 0);
  globals.maskcalculator.cache(flag != 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle the "imagecache" command
 */
// ----------------------------------------------------------------------

void do_imagecache(istream &theInput)
{
  int flag;
  theInput >> flag;

  check_errors(theInput, "imagecache");

#ifdef USE_IMAGECACHE
  globals.itsImageCacheOn = (flag != 0);
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle the "querydata" command
 */
// ----------------------------------------------------------------------

void do_querydata(istream &theInput)
{
  string newnames;
  theInput >> newnames;

  check_errors(theInput, "querydata");

  if (globals.queryfilelist != newnames)
  {
    globals.queryfilelist = newnames;

    // Delete possible old infos

    globals.querystreams.clear();

    // Split the comma separated list into a real list

    vector<string> qnames = NFmiStringTools::Split(globals.queryfilelist);

    // Read the queryfiles

    {
      vector<string>::const_iterator iter;
      for (iter = qnames.begin(); iter != qnames.end(); ++iter)
      {
        std::shared_ptr<LazyQueryData> tmp(new LazyQueryData());
        string filename = NFmiFileSystem::FileComplete(*iter, globals.datapath);
        globals.queryfilenames.push_back(filename);
        tmp->Read(filename);
        globals.querystreams.push_back(tmp);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "level" command
 */
// ----------------------------------------------------------------------

void do_level(istream &theInput)
{
  theInput >> globals.querydatalevel;

  check_errors(theInput, "level");

  if (!globals.specs.empty())
    globals.specs.back().level(globals.querydatalevel);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "filter" command
 */
// ----------------------------------------------------------------------

void do_filter(istream &theInput)
{
  theInput >> globals.filter;

  check_errors(theInput, "filter");

  if (globals.filter != "none" && globals.filter != "linear" && globals.filter != "min" &&
      globals.filter != "max" && globals.filter != "mean" && globals.filter != "sum")
  {
    throw runtime_error("Filtering mode '" + globals.filter + "' is not recognized");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestepskip" command
 */
// ----------------------------------------------------------------------

void do_timestepskip(istream &theInput)
{
  theInput >> globals.timestepskip;

  check_errors(theInput, "timestepskip");

  if (globals.timestepskip < 0)
    throw runtime_error("timestepskip cannot be negative");

  const int ludicruous = 30 * 24 * 60;  // 1 month
  if (globals.timestepskip > ludicruous)
    throw runtime_error("timestepskip " + NFmiStringTools::Convert(globals.timestepskip) +
                        " is ridiculously large");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestep" command
 */
// ----------------------------------------------------------------------

void do_timestep(istream &theInput)
{
  theInput >> globals.timestep;
  globals.timeinterval = globals.timestep;

  check_errors(theInput, "timestep");

  if (globals.timestep < 0)
    throw runtime_error("timestep cannot be negative");

  const int ludicruous = 30 * 24 * 60;  // 1 month
  if (globals.timestep > ludicruous)
    throw runtime_error("timestep " + NFmiStringTools::Convert(globals.timestep) +
                        " is ridiculously large");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timeinterval" command
 */
// ----------------------------------------------------------------------

void do_timeinterval(istream &theInput)
{
  theInput >> globals.timeinterval;

  check_errors(theInput, "timeinterval");

  if (globals.timeinterval < 0)
    throw runtime_error("timeinterval cannot be negative");

  const int ludicruous = 30 * 24 * 60;  // 1 month
  if (globals.timeinterval > ludicruous)
    throw runtime_error("timestep " + NFmiStringTools::Convert(globals.timeinterval) +
                        " is ridiculously large");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timesteps" command
 */
// ----------------------------------------------------------------------

void do_timesteps(istream &theInput)
{
  theInput >> globals.timesteps;

  check_errors(theInput, "timesteps");

  if (globals.timesteps < 0)
    throw runtime_error("timesteps cannot be negative");

  const int ludicruous = 30 * 24 * 60;  // 1 month
  if (globals.timesteps > ludicruous)
    throw runtime_error("timesteps " + NFmiStringTools::Convert(globals.timesteps) +
                        " is ridiculously large");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestamp" command
 */
// ----------------------------------------------------------------------

void do_timestamp(istream &theInput)
{
  theInput >> globals.timestampflag;

  check_errors(theInput, "timestamp");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampformat" command
 */
// ----------------------------------------------------------------------

void do_timestampformat(istream &theInput)
{
  theInput >> globals.timestampformat;
  check_errors(theInput, "timestampformat");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampzone" command
 */
// ----------------------------------------------------------------------

void do_timestampzone(istream &theInput)
{
  theInput >> globals.timestampzone;

  check_errors(theInput, "timestampzone");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timesteprounding" command
 */
// ----------------------------------------------------------------------

void do_timesteprounding(istream &theInput)
{
  theInput >> globals.timesteprounding;

  check_errors(theInput, "timesteprounding");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimage" command
 */
// ----------------------------------------------------------------------

void do_timestampimage(istream &theInput)
{
  theInput >> globals.timestampimage;

  check_errors(theInput, "timestampimage");

  if (globals.timestampimage != "none" && globals.timestampimage != "obs" &&
      globals.timestampimage != "for" && globals.timestampimage != "forobs")
  {
    throw runtime_error("Unrecognized timestampimage mode '" + globals.timestampimage + "'");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagexy" command
 */
// ----------------------------------------------------------------------

void do_timestampimagexy(istream &theInput)
{
  theInput >> globals.timestampimagex >> globals.timestampimagey;

  check_errors(theInput, "timestampimagexy");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimageformat" command
 */
// ----------------------------------------------------------------------

void do_timestampimageformat(istream &theInput)
{
  theInput >> globals.timestampimageformat;

  check_errors(theInput, "timestampimageformat");

  if (globals.timestampimageformat != "hour" && globals.timestampimageformat != "hourdate" &&
      globals.timestampimageformat != "datehour" && globals.timestampimageformat != "hourdateyear")
  {
    throw runtime_error("Unrecognized timestampimageformat '" + globals.timestampimageformat + "'");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagefont" command
 */
// ----------------------------------------------------------------------

void do_timestampimagefont(istream &theInput)
{
  theInput >> globals.timestampimagefont;

  check_errors(theInput, "timestampimagefont");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagecolor" command
 */
// ----------------------------------------------------------------------

void do_timestampimagecolor(istream &theInput)
{
  string scolor;
  theInput >> scolor;

  check_errors(theInput, "timestampimagecolor");

  globals.timestampimagecolor = ColorTools::checkcolor(scolor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagebackground" command
 */
// ----------------------------------------------------------------------

void do_timestampimagebackground(istream &theInput)
{
  string scolor;
  theInput >> scolor;

  check_errors(theInput, "timestampimagebackground");

  globals.timestampimagebackground = ColorTools::checkcolor(scolor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagemargin" command
 */
// ----------------------------------------------------------------------

void do_timestampimagemargin(istream &theInput)
{
  theInput >> globals.timestampimagexmargin >> globals.timestampimageymargin;

  check_errors(theInput, "timestampimagemargin");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "projection" command
 */
// ----------------------------------------------------------------------

void do_projection(istream &theInput)
{
  theInput >> globals.projection;

  check_errors(theInput, "projection");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "erase" command
 */
// ----------------------------------------------------------------------

void do_erase(istream &theInput)
{
  theInput >> globals.erase;

  check_errors(theInput, "projection");

  ColorTools::checkcolor(globals.erase);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "fillrule" command
 */
// ----------------------------------------------------------------------

void do_fillrule(istream &theInput)
{
  theInput >> globals.fillrule;

  check_errors(theInput, "fillrule");

  ColorTools::checkrule(globals.fillrule);

  if (!globals.shapespecs.empty())
    globals.shapespecs.back().fillrule(globals.fillrule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "strokerule" command
 */
// ----------------------------------------------------------------------

void do_strokerule(istream &theInput)
{
  theInput >> globals.strokerule;

  check_errors(theInput, "strokerule");

  ColorTools::checkrule(globals.strokerule);

  if (!globals.shapespecs.empty())
    globals.shapespecs.back().strokerule(globals.strokerule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "directionparam" command
 */
// ----------------------------------------------------------------------

void do_directionparam(istream &theInput)
{
  globals.speedxcomponent = "";
  globals.speedycomponent = "";

  theInput >> globals.directionparam;

  check_errors(theInput, "directionparam");

  if (toparam(globals.directionparam) == kFmiBadParameter)
    throw runtime_error("Unrecognized directionparam '" + globals.directionparam + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "speedparam" command
 */
// ----------------------------------------------------------------------

void do_speedparam(istream &theInput)
{
  globals.speedxcomponent = "";
  globals.speedycomponent = "";

  theInput >> globals.speedparam;

  check_errors(theInput, "speedparam");

  if (toparam(globals.speedparam) == kFmiBadParameter)
    throw runtime_error("Unrecognized speedparam '" + globals.speedparam + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "speedcomponents" command
 */
// ----------------------------------------------------------------------

void do_speedcomponents(istream &theInput)
{
  globals.speedparam = "";
  globals.directionparam = "";

  theInput >> globals.speedxcomponent >> globals.speedycomponent;

  check_errors(theInput, "speedcomponents");

  if (toparam(globals.speedxcomponent) == kFmiBadParameter)
    throw runtime_error("Unrecognized speedcomponent '" + globals.speedxcomponent + "'");

  if (toparam(globals.speedycomponent) == kFmiBadParameter)
    throw runtime_error("Unrecognized speedcomponent '" + globals.speedycomponent + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowscale" command
 */
// ----------------------------------------------------------------------

void do_arrowscale(istream &theInput)
{
  theInput >> globals.arrowscale;

  check_errors(theInput, "arrowscale");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrowscale" command
 */
// ----------------------------------------------------------------------

void do_windarrowscale(istream &theInput)
{
  theInput >> globals.windarrowscaleA >> globals.windarrowscaleB >> globals.windarrowscaleC;

  check_errors(theInput, "windarrowscale");

  if (globals.windarrowscaleB < 0)
    throw runtime_error("Second parameter of windarrowscale must be nonnegative");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowfill" command
 */
// ----------------------------------------------------------------------

void do_arrowfill(istream &theInput)
{
  string token1, token2;

  theInput >> token1 >> token2;
  check_errors(theInput, "arrowfill");

  try
  {
    ColorTools::checkcolor(token1);
    ColorTools::checkrule(token2);

    globals.arrowfillcolor = token1;
    globals.arrowfillrule = token2;
    globals.arrowfillstyles.clear();
  }
  catch (...)
  {
    string scolor, srule;
    theInput >> scolor >> srule;
    check_errors(theInput, "arrowfill");

    ArrowStyle style;
    style.lolimit = (token1 == "-" ? kFloatMissing : boost::lexical_cast<float>(token1));
    style.hilimit = (token2 == "-" ? kFloatMissing : boost::lexical_cast<float>(token2));
    style.color = ColorTools::parsecolor(scolor);
    style.rule = ColorTools::checkrule(srule);

    globals.arrowfillstyles.push_back(style);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowstroke" command
 */
// ----------------------------------------------------------------------

void do_arrowstroke(istream &theInput)
{
  string token1, token2;

  theInput >> token1 >> token2;
  check_errors(theInput, "arrowstroke");

  try
  {
    ColorTools::checkcolor(token1);
    ColorTools::checkrule(token2);

    globals.arrowstrokecolor = token1;
    globals.arrowstrokerule = token2;
    globals.arrowstrokestyles.clear();
  }
  catch (...)
  {
    string scolor, srule;
    theInput >> scolor >> srule;
    check_errors(theInput, "arrowstroke");

    ArrowStyle style;
    style.lolimit = (token1 == "-" ? kFloatMissing : boost::lexical_cast<float>(token1));
    style.hilimit = (token2 == "-" ? kFloatMissing : boost::lexical_cast<float>(token2));
    style.color = ColorTools::parsecolor(scolor);
    style.rule = ColorTools::checkrule(srule);

    globals.arrowstrokestyles.push_back(style);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowpath" command
 */
// ----------------------------------------------------------------------

void do_arrowpath(istream &theInput)
{
  theInput >> globals.arrowfile;

  check_errors(theInput, "arrowpath");

  if (globals.arrowfile != "meteorological" && globals.arrowfile != "roundarrow" &&
      !NFmiFileSystem::FileExists(globals.arrowfile))
  {
    throw runtime_error("The arrowpath file '" + globals.arrowfile + "' does not exist");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "graticule" command
 *
 * graticule lon1 lon2 dlon lat1 lat2 dlat color
 */
// ----------------------------------------------------------------------

void do_graticule(istream &theInput)
{
  theInput >> globals.graticulelon1 >> globals.graticulelon2 >> globals.graticuledx >>
      globals.graticulelat1 >> globals.graticulelat2 >> globals.graticuledy >>
      globals.graticulecolor;
  check_errors(theInput, "graticule");
  ColorTools::checkcolor(globals.graticulecolor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "roundarrowfill" command
 */
// ----------------------------------------------------------------------

void do_roundarrowfill(istream &theInput)
{
  string slo, shi, scircle, striangle;
  theInput >> slo >> shi >> scircle >> striangle;
  check_errors(theInput, "roundarrowfill");

  RoundArrowColor color;
  color.lolimit = (slo == "-" ? kFloatMissing : boost::lexical_cast<float>(slo));
  color.hilimit = (shi == "-" ? kFloatMissing : boost::lexical_cast<float>(shi));

  color.circlecolor = ColorTools::checkcolor(scircle);
  color.trianglecolor = ColorTools::checkcolor(striangle);

  globals.roundarrowfillcolors.push_back(color);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "roundarrowstroke" command
 */
// ----------------------------------------------------------------------

void do_roundarrowstroke(istream &theInput)
{
  string slo, shi, scircle, striangle;
  theInput >> slo >> shi >> scircle >> striangle;
  check_errors(theInput, "roundarrowstroke");

  RoundArrowColor color;
  color.lolimit = (slo == "-" ? kFloatMissing : boost::lexical_cast<float>(slo));
  color.hilimit = (shi == "-" ? kFloatMissing : boost::lexical_cast<float>(shi));

  color.circlecolor = ColorTools::checkcolor(scircle);
  color.trianglecolor = ColorTools::checkcolor(striangle);

  globals.roundarrowstrokecolors.push_back(color);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "roundarrowsize" command
 */
// ----------------------------------------------------------------------

void do_roundarrowsize(istream &theInput)
{
  RoundArrowSize sz;

  string slo, shi;
  theInput >> slo >> shi >> sz.circleradius >> sz.triangleradius >> sz.trianglewidth >>
      sz.triangleangle;

  check_errors(theInput, "roundarrowsize");

  sz.lolimit = (slo == "-" ? kFloatMissing : boost::lexical_cast<float>(slo));
  sz.hilimit = (shi == "-" ? kFloatMissing : boost::lexical_cast<float>(shi));

  globals.roundarrowsizes.push_back(sz);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrow" command
 */
// ----------------------------------------------------------------------

void do_windarrow(istream &theInput)
{
  double lon, lat;
  theInput >> lon >> lat;

  check_errors(theInput, "windarrow");

  globals.arrowpoints.push_back(NFmiPoint(lon, lat));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrows" command
 */
// ----------------------------------------------------------------------

void do_windarrows(istream &theInput)
{
  theInput >> globals.windarrowdx >> globals.windarrowdy;

  check_errors(theInput, "windarrows");

  if (globals.windarrowdx < 0 || globals.windarrowdy < 0)
    throw runtime_error("windarrows parameters must be nonnegative");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrowsxy" command
 */
// ----------------------------------------------------------------------

void do_windarrowsxy(istream &theInput)
{
  theInput >> globals.windarrowsxyx0 >> globals.windarrowsxyy0 >> globals.windarrowsxydx >>
      globals.windarrowsxydy;

  check_errors(theInput, "windarrowsxy");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "background" command
 */
// ----------------------------------------------------------------------

void do_background(istream &theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.background;

  check_errors(theInput, "background");

  if (globals.background == "none")
    globals.background = "";
  else
    globals.background = FileComplete(globals.background, globals.mapspath);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "foreground" command
 */
// ----------------------------------------------------------------------

void do_foreground(istream &theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.foreground;

  check_errors(theInput, "foreground");

  if (globals.foreground == "none")
    globals.foreground = "";
  else
    globals.foreground = FileComplete(globals.foreground, globals.mapspath);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "mask" command
 */
// ----------------------------------------------------------------------

void do_mask(istream &theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.mask;

  check_errors(theInput, "mask");

  if (globals.mask == "none")
    globals.mask = "";
  else
    globals.mask = FileComplete(globals.mask, globals.mapspath);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "overlay" command
 */
// ----------------------------------------------------------------------

void do_overlay(istream &theInput)
{
  string paramname, imgname;

  theInput >> paramname >> imgname;

  if (imgname == "-" || imgname == "none")
    imgname = "";

  check_errors(theInput, "overlay");

  BOOST_FOREACH (ContourSpec &spec, globals.specs)
  {
    if (spec.param() == paramname)
    {
      spec.overlay(imgname);
      break;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "combine" command
 */
// ----------------------------------------------------------------------

void do_combine(istream &theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.combine;

  check_errors(theInput, "combine");

  if (globals.combine == "none")
    globals.combine = "";
  else
  {
    theInput >> globals.combinex >> globals.combiney >> globals.combinerule >>
        globals.combinefactor;

    ColorTools::checkrule(globals.combinerule);

    if (globals.combinefactor < 0 || globals.combinefactor > 1)
      throw runtime_error("combine blending factor must be in range 0-1");

    globals.combine = FileComplete(globals.combine, globals.mapspath);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "foregroundrule" command
 */
// ----------------------------------------------------------------------

void do_foregroundrule(istream &theInput)
{
  theInput >> globals.foregroundrule;

  check_errors(theInput, "foregroundrule");

  ColorTools::checkrule(globals.foregroundrule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "savepath" command
 */
// ----------------------------------------------------------------------

void do_savepath(istream &theInput)
{
  theInput >> globals.savepath;

  check_errors(theInput, "savepath");

  if (!NFmiFileSystem::DirectoryExists(globals.savepath))
    NFmiFileSystem::CreateDirectory(globals.savepath);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "prefix" command
 */
// ----------------------------------------------------------------------

void do_prefix(istream &theInput)
{
  theInput >> globals.prefix;

  check_errors(theInput, "prefix");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "suffix" command
 */
// ----------------------------------------------------------------------

void do_suffix(istream &theInput)
{
  theInput >> globals.suffix;

  check_errors(theInput, "suffix");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "format" command
 */
// ----------------------------------------------------------------------

void do_format(istream &theInput)
{
  theInput >> globals.format;

  check_errors(theInput, "format");

  if (globals.format != "png" && globals.format != "pdf" &&  // AKa 15-Aug-2008
      globals.format != "jpg" && globals.format != "jpeg" && globals.format != "pnm" &&
      globals.format != "pgm" && globals.format != "wbmp" && globals.format != "gif")
  {
    throw runtime_error("Image format +'" + globals.format + "' is not supported");
  }
}

/*
 * Handle "antialias" and other Cairo-specific commands
 */
#if 0  // def IMAGINE_WITH_CAIRO
  void do_antialias( istream &in ) {
    in >> globals.antialias;
    check_errors(in,"antialias");
  }
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Handle "gamma" command
 */
// ----------------------------------------------------------------------

void do_gamma(istream &theInput)
{
  theInput >> globals.gamma;

  check_errors(theInput, "gamma");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "intent" command
 */
// ----------------------------------------------------------------------

void do_intent(istream &theInput)
{
  theInput >> globals.intent;

  check_errors(theInput, "intent");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "pngquality" command
 */
// ----------------------------------------------------------------------

void do_pngquality(istream &theInput)
{
  theInput >> globals.pngquality;

  check_errors(theInput, "pngquality");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "jpegquality" command
 */
// ----------------------------------------------------------------------

void do_jpegquality(istream &theInput)
{
  theInput >> globals.jpegquality;

  check_errors(theInput, "jpegquality");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "savealpha" command
 */
// ----------------------------------------------------------------------

void do_savealpha(istream &theInput)
{
  theInput >> globals.savealpha;

  check_errors(theInput, "savealpha");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "reducecolors" command
 */
// ----------------------------------------------------------------------

void do_reducecolors(istream &theInput)
{
  theInput >> globals.reducecolors;

  check_errors(theInput, "reducecolors");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "wantpalette" command
 */
// ----------------------------------------------------------------------

void do_wantpalette(istream &theInput)
{
  theInput >> globals.wantpalette;

  check_errors(theInput, "wantpalette");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "forcepalette" command
 */
// ----------------------------------------------------------------------

void do_forcepalette(istream &theInput)
{
  theInput >> globals.forcepalette;

  check_errors(theInput, "forcepalette");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "alphalimit" command
 */
// ----------------------------------------------------------------------

void do_alphalimit(istream &theInput)
{
  theInput >> globals.alphalimit;

  check_errors(theInput, "alphalimit");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "hilimit" command
 */
// ----------------------------------------------------------------------

void do_hilimit(istream &theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput, "hilimit");

  if (!globals.specs.empty())
    globals.specs.back().exactHiLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datalolimit" command
 */
// ----------------------------------------------------------------------

void do_datalolimit(istream &theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput, "datalolimit");

  if (!globals.specs.empty())
    globals.specs.back().dataLoLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datahilimit" command
 */
// ----------------------------------------------------------------------

void do_datahilimit(istream &theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput, "datahilimit");

  if (!globals.specs.empty())
    globals.specs.back().dataHiLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datareplace" command
 */
// ----------------------------------------------------------------------

void do_datareplace(istream &theInput)
{
  float src, dst;
  theInput >> src >> dst;

  check_errors(theInput, "datareplace");

  if (!globals.specs.empty())
    globals.specs.back().replace(src, dst);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "despeckle" command
 *
 * Syntax: despeckle lo hi radius weight iterations
 */
// ----------------------------------------------------------------------

void do_despeckle(istream &theInput)
{
  string slo, shi;
  int radius, iterations;
  float weight;

  theInput >> slo >> shi >> radius >> weight >> iterations;

  check_errors(theInput, "despeckle");

  float lo = (slo == "-" ? kFloatMissing : NFmiStringTools::Convert<float>(slo));
  float hi = (shi == "-" ? kFloatMissing : NFmiStringTools::Convert<float>(shi));

  if (lo != kFloatMissing && hi != kFloatMissing && lo >= hi)
    throw runtime_error("despeckle hilimit must be > lolimit");

  if (radius < 1 || radius > 50)
    throw runtime_error("despeckle radius must be in the range 1-50");

  if (iterations < 1 || iterations > 50)
    throw runtime_error("despeckle iterations must be in the range 1-50");

  if (weight < 0 || weight > 100)
    throw runtime_error("despeckle weight must be in the range 0-100");

  if (!globals.specs.empty())
    globals.specs.back().despeckle(lo, hi, radius, weight, iterations);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "expanddata" command
 */
// ----------------------------------------------------------------------

void do_expanddata(istream &theInput)
{
  theInput >> globals.expanddata;
  check_errors(theInput, "expanddata");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourdepth" command
 */
// ----------------------------------------------------------------------

void do_contourdepth(istream &theInput)
{
  cerr << "Warning: contourdepth command is deprecated" << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourinterpolation" command
 */
// ----------------------------------------------------------------------

void do_contourinterpolation(istream &theInput)
{
  theInput >> globals.contourinterpolation;

  check_errors(theInput, "contourinterpolation");

  if (!globals.specs.empty())
    globals.specs.back().contourInterpolation(globals.contourinterpolation);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourtriangles" command
 */
// ----------------------------------------------------------------------

void do_contourtriangles(istream &theInput)
{
  theInput >> globals.contourtriangles;

  check_errors(theInput, "contourtriangles");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "smoother" command
 */
// ----------------------------------------------------------------------

void do_smoother(istream &theInput)
{
  theInput >> globals.smoother;

  check_errors(theInput, "smoother");

  if (!globals.specs.empty())
    globals.specs.back().smoother(globals.smoother);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "smootherradius" command
 */
// ----------------------------------------------------------------------

void do_smootherradius(istream &theInput)
{
  theInput >> globals.smootherradius;

  check_errors(theInput, "smootherradius");

  if (!globals.specs.empty())
    globals.specs.back().smootherRadius(globals.smootherradius);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "smootherfactor" command
 */
// ----------------------------------------------------------------------

void do_smootherfactor(istream &theInput)
{
  theInput >> globals.smootherfactor;

  check_errors(theInput, "smootherfactor");

  if (!globals.specs.empty())
    globals.specs.back().smootherFactor(globals.smootherfactor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "param" command
 */
// ----------------------------------------------------------------------

void do_param(istream &theInput)
{
  string param;

  theInput >> param;

  check_errors(theInput, "param");

  ContourSpec spec(param,
                   globals.contourinterpolation,
                   globals.smoother,
                   globals.querydatalevel,
                   globals.smootherradius,
                   globals.smootherfactor);

  globals.specs.push_back(spec);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "shape" command
 */
// ----------------------------------------------------------------------

void do_shape(istream &theInput)
{
  string shapename, arg1;

  theInput >> shapename >> arg1;

  check_errors(theInput, "shape");

  if (arg1 == "mark")
  {
    string marker, markerrule;
    float markeralpha;
    theInput >> marker >> markerrule >> markeralpha;

    ColorTools::checkrule(markerrule);
    ShapeSpec spec(shapename);
    spec.marker(marker, markerrule, markeralpha);
    globals.shapespecs.push_back(spec);
  }
  else
  {
    string fillcolor = arg1;
    string strokecolor;
    theInput >> strokecolor;
    NFmiColorTools::Color fill = ColorTools::checkcolor(fillcolor);
    NFmiColorTools::Color stroke = ColorTools::checkcolor(strokecolor);

    globals.shapespecs.push_back(
        ShapeSpec(shapename, fill, stroke, globals.fillrule, globals.strokerule));
  }

  check_errors(theInput, "shape");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourfill" command
 */
// ----------------------------------------------------------------------

void do_contourfill(istream &theInput)
{
  string slo, shi, scolor;
  theInput >> slo >> shi >> scolor;

  check_errors(theInput, "contourfill");

  float lo, hi;
  if (slo == "-")
    lo = kFloatMissing;
  else
    lo = NFmiStringTools::Convert<float>(slo);
  if (shi == "-")
    hi = kFloatMissing;
  else
    hi = NFmiStringTools::Convert<float>(shi);

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);

  if (!globals.specs.empty())
    globals.specs.back().add(ContourRange(lo, hi, color, globals.fillrule));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourpattern" command
 */
// ----------------------------------------------------------------------

void do_contourpattern(istream &theInput)
{
  string slo, shi, spattern, srule;
  float alpha;
  theInput >> slo >> shi >> spattern >> srule >> alpha;

  check_errors(theInput, "contourpattern");

  float lo, hi;
  if (slo == "-")
    lo = kFloatMissing;
  else
    lo = NFmiStringTools::Convert<float>(slo);
  if (shi == "-")
    hi = kFloatMissing;
  else
    hi = NFmiStringTools::Convert<float>(shi);

  if (!globals.specs.empty())
    globals.specs.back().add(ContourPattern(lo, hi, spattern, srule, alpha));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contoursymbol" command
 */
// ----------------------------------------------------------------------

void do_contoursymbol(istream &theInput)
{
  string slo, shi, spattern, srule;
  float alpha;
  theInput >> slo >> shi >> spattern >> srule >> alpha;

  check_errors(theInput, "contoursymbol");

  float lo, hi;
  if (slo == "-")
    lo = kFloatMissing;
  else
    lo = NFmiStringTools::Convert<float>(slo);
  if (shi == "-")
    hi = kFloatMissing;
  else
    hi = NFmiStringTools::Convert<float>(shi);

  if (!globals.specs.empty())
    globals.specs.back().add(ContourSymbol(lo, hi, spattern, srule, alpha));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourfont" command
 */
// ----------------------------------------------------------------------

void do_contourfont(istream &theInput)
{
  float value;
  int symbol;
  string scolor, font;
  theInput >> value >> symbol >> scolor >> font;

  check_errors(theInput, "contourfont");

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);

  if (!globals.specs.empty())
    globals.specs.back().add(ContourFont(value, color, symbol, font));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourline" command
 */
// ----------------------------------------------------------------------

void do_contourline(istream &theInput)
{
  string svalue, scolor;
  theInput >> svalue >> scolor;

  check_errors(theInput, "contourline");

  float value;
  if (svalue == "-")
    value = kFloatMissing;
  else
    value = NFmiStringTools::Convert<float>(svalue);

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);
  if (!globals.specs.empty())
    globals.specs.back().add(
        ContourValue(value, globals.contourlinewidth, color, globals.strokerule));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlinewidth" command
 */
// ----------------------------------------------------------------------

void do_contourlinewidth(istream &theInput)
{
  theInput >> globals.contourlinewidth;

  check_errors(theInput, "contourlinewidth");

  if (globals.contourlinewidth <= 0)
    throw runtime_error("conturlinewidth must be nonnegative");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourfills" command
 */
// ----------------------------------------------------------------------

void do_contourfills(istream &theInput)
{
  float lo, hi, step;
  string scolor1, scolor2;
  theInput >> lo >> hi >> step >> scolor1 >> scolor2;

  check_errors(theInput, "contourfills");

  int color1 = ColorTools::checkcolor(scolor1);
  int color2 = ColorTools::checkcolor(scolor2);

  int steps = static_cast<int>((hi - lo) / step);

  for (int i = 0; i < steps; i++)
  {
    float tmplo = lo + i * step;
    float tmphi = lo + (i + 1) * step;
    int color = color1;  // in case steps=1
    if (steps != 1)
      color = NFmiColorTools::Interpolate(color1, color2, i / (steps - 1.0f));
    if (!globals.specs.empty())
      globals.specs.back().add(ContourRange(tmplo, tmphi, color, globals.fillrule));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlines" command
 */
// ----------------------------------------------------------------------

void do_contourlines(istream &theInput)
{
  float lo, hi, step;
  string scolor1, scolor2;
  theInput >> lo >> hi >> step >> scolor1 >> scolor2;

  check_errors(theInput, "contourlines");

  int color1 = ColorTools::checkcolor(scolor1);
  int color2 = ColorTools::checkcolor(scolor2);

  int steps = static_cast<int>((hi - lo) / step);

  for (int i = 0; i <= steps; i++)
  {
    float tmplo = lo + i * step;
    int color = color1;  // in case steps=1
    if (steps != 0)
      color = NFmiColorTools::Interpolate(color1, color2, i / static_cast<float>(steps));
    if (!globals.specs.empty())
      globals.specs.back().add(
          ContourValue(tmplo, globals.contourlinewidth, color, globals.strokerule));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabel" command
 */
// ----------------------------------------------------------------------

void do_contourlabel(istream &theInput)
{
  float value;
  theInput >> value;

  check_errors(theInput, "contourlabel");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabel");

  globals.specs.back().add(ContourLabel(value));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabels" command
 */
// ----------------------------------------------------------------------

void do_contourlabels(istream &theInput)
{
  float lo, hi, step;
  theInput >> lo >> hi >> step;

  check_errors(theInput, "contourlabels");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabels");

  int steps = static_cast<int>((hi - lo) / step);

  for (int i = 0; i <= steps; i++)
  {
    float tmplo = lo + i * step;
    globals.specs.back().add(ContourLabel(tmplo));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabeltext" command
 */
// ----------------------------------------------------------------------

void do_contourlabeltext(istream &theInput)
{
  string value, text;
  theInput >> value >> text;
  check_errors(theInput, "contourlabeltext");
  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabeltext");

  globals.specs.back().addContourLabelText(NFmiStringTools::Convert<float>(value), text);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelfont" command
 */
// ----------------------------------------------------------------------

void do_contourlabelfont(istream &theInput)
{
  string font;

  theInput >> font;

  check_errors(theInput, "contourlabelfont");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabelfont");

  globals.specs.back().contourLabelFont(font);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelcolor" command
 */
// ----------------------------------------------------------------------

void do_contourlabelcolor(istream &theInput)
{
  string scolor;

  theInput >> scolor;

  check_errors(theInput, "contourlabelcolor");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabelcolor");

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);

  globals.specs.back().contourLabelColor(color);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelbackground" command
 */
// ----------------------------------------------------------------------

void do_contourlabelbackground(istream &theInput)
{
  string scolor;

  theInput >> scolor;

  check_errors(theInput, "contourlabelbackground");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabelbackground");

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);

  globals.specs.back().contourLabelBackgroundColor(color);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelmargin" command
 */
// ----------------------------------------------------------------------

void do_contourlabelmargin(istream &theInput)
{
  int dx, dy;

  theInput >> dx >> dy;

  check_errors(theInput, "contourlabelmargin");

  if (globals.specs.empty())
    throw runtime_error("Must define parameter before contourlabelmargin");

  globals.specs.back().contourLabelBackgroundXMargin(dx);
  globals.specs.back().contourLabelBackgroundYMargin(dy);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelimagemargin" command
 */
// ----------------------------------------------------------------------

void do_contourlabelimagemargin(istream &theInput)
{
  theInput >> globals.contourlabelimagexmargin >> globals.contourlabelimageymargin;

  check_errors(theInput, "contourlabelimagemargin");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelmindistsamevalue" command
 */
// ----------------------------------------------------------------------

void do_contourlabelmindistsamevalue(istream &theInput)
{
  float dist;
  theInput >> dist;

  check_errors(theInput, "contourlabelmindistsamevalue");

  globals.labellocator.minDistanceToSameValue(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelmindistdifferentvalue" command
 */
// ----------------------------------------------------------------------

void do_contourlabelmindistdifferentvalue(istream &theInput)
{
  float dist;
  theInput >> dist;

  check_errors(theInput, "contourlabelmindistdifferentvalue");

  globals.labellocator.minDistanceToDifferentValue(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelmindistdifferentparam" command
 */
// ----------------------------------------------------------------------

void do_contourlabelmindistdifferentparam(istream &theInput)
{
  float dist;

  theInput >> dist;

  check_errors(theInput, "contourlabelmindistdifferentparam");

  globals.labellocator.minDistanceToDifferentParameter(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourfontmindistsamevalue" command
 */
// ----------------------------------------------------------------------

void do_contourfontmindistsamevalue(istream &theInput)
{
  float dist;
  theInput >> dist;

  check_errors(theInput, "contourfontmindistsamevalue");

  globals.symbollocator.minDistanceToSameValue(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourfontmindistdifferentvalue" command
 */
// ----------------------------------------------------------------------

void do_contourfontmindistdifferentvalue(istream &theInput)
{
  float dist;
  theInput >> dist;

  check_errors(theInput, "contourfontmindistdifferentvalue");

  globals.symbollocator.minDistanceToDifferentValue(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourlabelmindistdifferentparam" command
 */
// ----------------------------------------------------------------------

void do_contourfontmindistdifferentparam(istream &theInput)
{
  float dist;

  theInput >> dist;

  check_errors(theInput, "contourfontmindistdifferentparam");

  globals.symbollocator.minDistanceToDifferentParameter(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contoursymbolmindist" command
 */
// ----------------------------------------------------------------------

void do_contoursymbolmindist(istream &theInput)
{
  float dist;

  theInput >> dist;

  check_errors(theInput, "contoursymbolmindist");

  globals.imagelocator.minDistanceToDifferentParameter(dist);
  globals.imagelocator.minDistanceToDifferentValue(dist);
  globals.imagelocator.minDistanceToSameValue(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "highpressure" command
 */
// ----------------------------------------------------------------------

void do_highpressure(istream &theInput)
{
  theInput >> globals.highpressureimage >> globals.highpressurerule >> globals.highpressurefactor;

  check_errors(theInput, "highpressure");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "lowpressure" command
 */
// ----------------------------------------------------------------------

void do_lowpressure(istream &theInput)
{
  theInput >> globals.lowpressureimage >> globals.lowpressurerule >> globals.lowpressurefactor;

  check_errors(theInput, "lowpressure");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "highpressureminimum" command
 */
// ----------------------------------------------------------------------

void do_highpressureminimum(istream &theInput)
{
  theInput >> globals.highpressureminimum;

  check_errors(theInput, "highpressureminimum");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "lowpressuremaximum" command
 */
// ----------------------------------------------------------------------

void do_lowpressuremaximum(istream &theInput)
{
  theInput >> globals.lowpressuremaximum;

  check_errors(theInput, "lowpressuremaximum");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "pressuremindistsame" command
 */
// ----------------------------------------------------------------------

void do_pressuremindistsame(istream &theInput)
{
  float dist;
  theInput >> dist;
  check_errors(theInput, "mindistsame");

  globals.pressurelocator.minDistanceToSame(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "pressuremindistdifferent" command
 */
// ----------------------------------------------------------------------

void do_pressuremindistdifferent(istream &theInput)
{
  float dist;
  theInput >> dist;
  check_errors(theInput, "mindistdifferent");

  globals.pressurelocator.minDistanceToDifferent(dist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelmarker" command
 */
// ----------------------------------------------------------------------

void do_labelmarker(istream &theInput)
{
  string filename, rule;
  float alpha;

  theInput >> filename >> rule >> alpha;

  check_errors(theInput, "labelmarker");

  if (!globals.specs.empty())
  {
    globals.specs.back().labelMarker(filename);
    globals.specs.back().labelMarkerRule(rule);
    globals.specs.back().labelMarkerAlphaFactor(alpha);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelfont" command
 */
// ----------------------------------------------------------------------

void do_labelfont(istream &theInput)
{
  string font;
  theInput >> font;

  check_errors(theInput, "labelfont");

  if (!globals.specs.empty())
    globals.specs.back().labelFont(font);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelcolor" command
 */
// ----------------------------------------------------------------------

void do_labelcolor(istream &theInput)
{
  string color;
  theInput >> color;

  check_errors(theInput, "labelcolor");

  if (!globals.specs.empty())
    globals.specs.back().labelColor(ColorTools::checkcolor(color));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelrule" command
 */
// ----------------------------------------------------------------------

void do_labelrule(istream &theInput)
{
  string rule;
  theInput >> rule;

  check_errors(theInput, "labelrule");

  ColorTools::checkrule(rule);

  if (!globals.specs.empty())
    globals.specs.back().labelRule(rule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelalign" command
 */
// ----------------------------------------------------------------------

void do_labelalign(istream &theInput)
{
  string align;
  theInput >> align;

  check_errors(theInput, "labelalign");

  if (!globals.specs.empty())
    globals.specs.back().labelAlignment(align);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelformat" command
 */
// ----------------------------------------------------------------------

void do_labelformat(istream &theInput)
{
  string format;
  theInput >> format;

  check_errors(theInput, "labelformat");

  if (format == "-")
    format = "";
  if (!globals.specs.empty())
    globals.specs.back().labelFormat(format);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelmissing" command
 */
// ----------------------------------------------------------------------

void do_labelmissing(istream &theInput)
{
  string label;
  theInput >> label;

  check_errors(theInput, "labelmissing");

  if (label == "none")
    label = "";

  if (!globals.specs.empty())
    globals.specs.back().labelMissing(label);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labeloffset" command
 */
// ----------------------------------------------------------------------

void do_labeloffset(istream &theInput)
{
  float dx, dy;
  theInput >> dx >> dy;

  check_errors(theInput, "labeloffset");

  if (!globals.specs.empty())
  {
    globals.specs.back().labelOffsetX(dx);
    globals.specs.back().labelOffsetY(dy);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelcaption" command
 */
// ----------------------------------------------------------------------

void do_labelcaption(istream &theInput)
{
  string name, align;
  float dx, dy;
  theInput >> name >> dx >> dy >> align;

  check_errors(theInput, "labelcaption");

  if (!globals.specs.empty())
  {
    globals.specs.back().labelCaption(name);
    globals.specs.back().labelCaptionDX(dx);
    globals.specs.back().labelCaptionDY(dy);
    globals.specs.back().labelCaptionAlignment(align);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "label" command
 */
// ----------------------------------------------------------------------

void do_label(istream &theInput)
{
  float lon, lat;
  theInput >> lon >> lat;

  check_errors(theInput, "label");

  if (!globals.specs.empty())
    globals.specs.back().add(NFmiPoint(lon, lat));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelxy" command
 */
// ----------------------------------------------------------------------

void do_labelxy(istream &theInput)
{
  float lon, lat;
  int dx, dy;
  theInput >> lon >> lat >> dx >> dy;

  check_errors(theInput, "labelxy");

  if (!globals.specs.empty())
    globals.specs.back().add(NFmiPoint(lon, lat), NFmiPoint(dx, dy));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labels" command
 */
// ----------------------------------------------------------------------

void do_labels(istream &theInput)
{
  float dx, dy;
  theInput >> dx >> dy;

  check_errors(theInput, "labels");

  if (dx < 0 || dy < 0)
    throw runtime_error("labels arguments must be nonnegative");

  if (!globals.specs.empty())
  {
    globals.specs.back().labelDX(dx);
    globals.specs.back().labelDY(dy);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelsxy" command
 */
// ----------------------------------------------------------------------

void do_labelsxy(istream &theInput)
{
  float x0, y0, dx, dy;
  theInput >> x0 >> y0 >> dx >> dy;

  check_errors(theInput, "labelsxy");

  if (dx < 0 || dy < 0)
    throw runtime_error("labelsxy arguments must be nonnegative");

  if (!globals.specs.empty())
  {
    globals.specs.back().labelXyX0(x0);
    globals.specs.back().labelXyY0(y0);
    globals.specs.back().labelXyDX(dx);
    globals.specs.back().labelXyDY(dy);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "labelfile" command
 */
// ----------------------------------------------------------------------

void do_labelfile(istream &theInput)
{
  string datafilename;
  theInput >> datafilename;

  check_errors(theInput, "labelfile");

  ifstream datafile(datafilename.c_str());
  if (!datafile)
    throw runtime_error("No data file named " + datafilename);
  string datacommand;
  while (datafile >> datacommand)
  {
    if (datacommand == "#" || datacommand == "//")
#ifdef PERKELEEN_296
      datafile.ignore(1000000, '\n');
#else
      datafile.ignore(numeric_limits<std::streamsize>::max(), '\n');
#endif
    else if (datacommand == "label")
    {
      float lon, lat;
      datafile >> lon >> lat;
      if (!globals.specs.empty())
        globals.specs.back().add(NFmiPoint(lon, lat));
    }
    else
      throw runtime_error("Unknown datacommand " + datacommand);
  }
  datafile.close();
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "units" command
 */
// ----------------------------------------------------------------------

void do_units(istream &theInput)
{
  string paramname, conversion;

  theInput >> paramname >> conversion;

  check_errors(theInput, "units");

  FmiParameterName param = toparam(paramname);
  if (param == kFmiBadParameter)
    throw runtime_error("Unknown parametername '" + paramname + "'");
  globals.unitsconverter.setConversion(param, conversion);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "clear" command
 */
// ----------------------------------------------------------------------

void do_clear(istream &theInput)
{
  string command;

  theInput >> command;

  check_errors(theInput, "clear");

  if (command == "contours")
  {
    globals.specs.clear();
    globals.labellocator.clear();
    globals.symbollocator.clear();
    globals.imagelocator.clear();
    globals.highpressureimage.clear();
    globals.lowpressureimage.clear();
  }
  else if (command == "shapes")
    globals.shapespecs.clear();
  else if (command == "cache")
  {
    globals.calculator.clearCache();
    globals.maskcalculator.clearCache();
  }
  else if (command == "imagecache")
  {
#ifdef USE_IMAGECACHE
    globals.itsImageCache.clear();
#endif
  }
  else if (command == "arrows")
  {
    globals.arrowpoints.clear();
    globals.windarrowdx = 0;
    globals.windarrowdy = 0;
    globals.windarrowsxydx = -1;
    globals.windarrowsxydy = -1;
  }
  else if (command == "roundarrow")
  {
    globals.roundarrowfillcolors.clear();
    globals.roundarrowstrokecolors.clear();
    globals.roundarrowsizes.clear();
  }
  else if (command == "labels")
  {
    list<ContourSpec>::iterator it;
    for (it = globals.specs.begin(); it != globals.specs.end(); ++it)
      it->clearLabels();
  }
  else if (command == "pressure")
  {
    globals.highpressureimage.clear();
    globals.lowpressureimage.clear();
  }
  else if (command == "units")
    globals.unitsconverter.clear();
  else if (command == "graticule")
    globals.graticulecolor = "";
  else
    throw runtime_error("Unknown clear target: " + command);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw shapes" command
 */
// ----------------------------------------------------------------------

void do_draw_shapes(istream &theInput)
{
  // The output filename

  string filename;
  theInput >> filename;

  check_errors(theInput, "draw shapes");

  auto area = globals.createArea();

  if (globals.verbose)
    report_area(*area);

  int imgwidth = static_cast<int>(area->Width() + 0.5);
  int imgheight = static_cast<int>(area->Height() + 0.5);

// Initialize the background
#ifdef IMAGINE_WITH_CAIRO
  ImagineXr image(imgwidth, imgheight, filename + "." + globals.format, globals.format);
#else
  NFmiImage image(imgwidth, imgheight);
  globals.setImageModes(image);
#endif
  image.Erase(ColorTools::checkcolor(globals.erase));

  // Draw all the shapes

  list<ShapeSpec>::const_iterator iter;
  list<ShapeSpec>::const_iterator begin = globals.shapespecs.begin();
  list<ShapeSpec>::const_iterator end = globals.shapespecs.end();

  for (iter = begin; iter != end; ++iter)
  {
    NFmiGeoShape geo(iter->filename(), kFmiGeoShapeEsri);
    geo.ProjectXY(*area);

    if (iter->marker() == "")
    {
      NFmiColorTools::NFmiBlendRule fillrule = ColorTools::checkrule(iter->fillrule());
      NFmiColorTools::NFmiBlendRule strokerule = ColorTools::checkrule(iter->strokerule());

      geo.Fill(image, iter->fillcolor(), fillrule);
      geo.Stroke(image, iter->strokecolor(), strokerule);
    }
    else
    {
      NFmiColorTools::NFmiBlendRule markerrule = ColorTools::checkrule(iter->markerrule());

      const ImagineXr_or_NFmiImage &marker = globals.getImage(iter->marker());
      geo.Mark(image, marker, markerrule, kFmiAlignCenter, iter->markeralpha());
    }
  }

#ifdef IMAGINE_WITH_CAIRO
  write_image(image);
#else
  write_image(image, filename + '.' + globals.format, globals.format);
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw imagemap" command
 */
// ----------------------------------------------------------------------

void do_draw_imagemap(istream &theInput)
{
  // The relevant field name and filenames

  string fieldname, filename;
  theInput >> fieldname >> filename;

  check_errors(theInput, "draw imagemap");

  auto area = globals.createArea();

  // Generate map from all shapes in the list

  string outfile = filename + ".map";
  ofstream out(outfile.c_str());
  if (!out)
    throw runtime_error("Failed to open " + outfile + " for writing");

  if (globals.verbose)
    cout << "Writing " << outfile << endl;

  list<ShapeSpec>::const_iterator iter;
  list<ShapeSpec>::const_iterator begin = globals.shapespecs.begin();
  list<ShapeSpec>::const_iterator end = globals.shapespecs.end();

  for (iter = begin; iter != end; ++iter)
  {
    NFmiGeoShape geo(iter->filename(), kFmiGeoShapeEsri);
    geo.ProjectXY(*area);
    geo.WriteImageMap(out, fieldname);
  }
  out.close();
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw graticule
 */
// ----------------------------------------------------------------------

void draw_graticule(ImagineXr_or_NFmiImage &img, const NFmiArea &theArea)
{
  if (globals.graticulecolor.empty())
    return;

  NFmiPath path;

  for (double lon = globals.graticulelon1; lon <= globals.graticulelon2; lon += globals.graticuledx)
  {
    path.MoveTo(lon, globals.graticulelat1);
    for (double lat = globals.graticulelat1 + globals.graticuledy; lat <= globals.graticulelat2;
         lat += 1.0)
      path.LineTo(lon, lat);
  }

  for (double lat = globals.graticulelat1; lat <= globals.graticulelat2; lat += globals.graticuledy)
  {
    path.MoveTo(globals.graticulelon1, lat);
    for (double lon = globals.graticulelon1 + globals.graticuledx; lon <= globals.graticulelon2;
         lon += 1.0)
      path.LineTo(lon, lat);
  }

  // MeridianTools::Relocate(path,theArea);
  path.Project(&theArea);

  NFmiColorTools::Color color = ColorTools::checkcolor(globals.graticulecolor);
  path.Stroke(img, color, NFmiColorTools::kFmiColorCopy);
}

// ----------------------------------------------------------------------
/*!
 * \brief Assign ID for parameter name
 *
 * This is needed so that Meta-functions would get an ID too
 *
 * \param theParam The parameter name
 * \return The unique ID, or kFmiBadParameter
 */
// ----------------------------------------------------------------------

int paramid(const string &theParam)
{
  if (MetaFunctions::isMeta(theParam))
    return MetaFunctions::id(theParam);
  else
    return toparam(theParam);
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose the queryinfo from the set of available datas
 */
// ----------------------------------------------------------------------

unsigned int choose_queryinfo(const string &theName, int theLevel)
{
  if (globals.querystreams.size() == 0)
    throw runtime_error("No querydata has been specified");

  if (MetaFunctions::isMeta(theName))
  {
    globals.queryinfo = globals.querystreams[0];
    return 0;
  }
  else
  {
    // Find the proper queryinfo to be used

    FmiParameterName param = toparam(theName);

    for (unsigned int qi = 0; qi < globals.querystreams.size(); qi++)
    {
      globals.queryinfo = globals.querystreams[qi];
      globals.queryinfo->Param(param);
      if (globals.queryinfo->IsParamUsable())
      {
        if (set_level(*globals.queryinfo, theLevel))
          return qi;
      }
    }
    if (theLevel < 0)
      throw runtime_error("Parameter '" + theName + "' is not available in the query files");
    else
      throw runtime_error("Parameter '" + theName + "' on level " +
                          NFmiStringTools::Convert(theLevel) +
                          " is not available in the query files");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand the data values
 *
 * First we try to calculate the mean from adjacent values.
 * If that fails, we try to calculate the mean from diagonal values.
 */
// ----------------------------------------------------------------------

void expand_data(NFmiDataMatrix<float> &theValues)
{
  NFmiDataModifierAvg calculator;

  NFmiDataMatrix<float> tmp(theValues);

  for (unsigned int j = 0; j < theValues.NY(); j++)
    for (unsigned int i = 0; i < theValues.NX(); i++)
    {
      if (theValues[i][j] == kFloatMissing)
      {
        calculator.Clear();
        calculator.Calculate(tmp.At(i - 1, j, kFloatMissing));
        calculator.Calculate(tmp.At(i + 1, j, kFloatMissing));
        calculator.Calculate(tmp.At(i, j - 1, kFloatMissing));
        calculator.Calculate(tmp.At(i, j + 1, kFloatMissing));
        if (calculator.CalculationResult() == kFloatMissing)
        {
          calculator.Calculate(tmp.At(i - 1, j - 1, kFloatMissing));
          calculator.Calculate(tmp.At(i - 1, j + 1, kFloatMissing));
          calculator.Calculate(tmp.At(i + 1, j - 1, kFloatMissing));
          calculator.Calculate(tmp.At(i + 1, j + 1, kFloatMissing));
        }
        theValues[i][j] = calculator.CalculationResult();
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Filter the data values
 */
// ----------------------------------------------------------------------

void filter_values(NFmiDataMatrix<float> &theValues,
                   const NFmiTime &theTime,
                   const ContourSpec &theSpec)
{
  if (globals.filter == "none")
  {
    // The time is known to be exact
  }
  else if (globals.filter == "linear")
  {
    NFmiTime tnow = globals.queryinfo->ValidTime();
    bool isexact = theTime.IsEqual(tnow);

    if (!isexact)
    {
      NFmiDataMatrix<float> tmpvals;
      NFmiTime t2 = globals.queryinfo->ValidTime();
      globals.queryinfo->PreviousTime();
      NFmiTime t1 = globals.queryinfo->ValidTime();
      if (!MetaFunctions::isMeta(theSpec.param()))
      {
        tmpvals = globals.queryinfo->Values();
        globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()),
                                       tmpvals);
      }
      else
        tmpvals = MetaFunctions::values(theSpec.param(), *globals.queryinfo);
      if (theSpec.replace())
        tmpvals.Replace(theSpec.replaceSourceValue(), theSpec.replaceTargetValue());

      // Data from t1,t2, we want t

      long offset = theTime.DifferenceInMinutes(t1);
      long range = t2.DifferenceInMinutes(t1);

      float weight = (static_cast<float>(offset)) / range;

      theValues.LinearCombination(tmpvals, weight, 1 - weight);
    }
  }
  else
  {
    NFmiTime tprev = theTime;
    tprev.ChangeByMinutes(-globals.timeinterval);

    if (MetaFunctions::isMeta(theSpec.param()))
      throw runtime_error("Unable to filter metafunctions - use newbase parameters only");

    NFmiMetTime tnow(theTime, 60);
    NFmiDataMatrix<float> tmpvals;
    int steps = 1;
    for (;;)
    {
      tmpvals = globals.queryinfo->Values(tnow);
      globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()), tmpvals);

      if (theSpec.replace())
        tmpvals.Replace(theSpec.replaceSourceValue(), theSpec.replaceTargetValue());

      if (globals.filter == "min")
        theValues.Min(tmpvals);
      else if (globals.filter == "max")
        theValues.Max(tmpvals);
      else if (globals.filter == "mean")
        theValues += tmpvals;
      else if (globals.filter == "sum")
        theValues += tmpvals;

      ++steps;
      --tnow;

      if (tnow.IsLessThan(tprev))
        break;
    }

    if (globals.filter == "mean")
      theValues /= static_cast<float>(steps);
  }

  // Noise reduction

  theSpec.despeckle(theValues);
}

// ----------------------------------------------------------------------
/*!
 * \brief Save grid values for later labelling
 */
// ----------------------------------------------------------------------

void add_label_grid_values(ContourSpec &theSpec,
                           const NFmiArea &theArea,
                           const LazyCoordinates &thePoints)
{
  const float dx = theSpec.labelDX();
  const float dy = theSpec.labelDY();

  if (dx > 0 && dy > 0)
  {
    // Fast code for lattice coordinates
    if (dx == static_cast<int>(dx) && dy == static_cast<int>(dy))
    {
      const int dj = static_cast<int>(dy);
      const int di = static_cast<int>(dx);
      for (unsigned int j = 0; j < thePoints.NY(); j += dj)
        for (unsigned int i = 0; i < thePoints.NX(); i += di)
          theSpec.add(theArea.WorldXYToLatLon(thePoints(i, j)));
    }
    else
    {
      for (float y = 0; y <= thePoints.NY() - 1; y += dy)
      {
        const int j = static_cast<int>(floor(y));
        const float dj = y - j;
        for (float x = 0; x <= thePoints.NX() - 1; x += dx)
        {
          const int i = static_cast<int>(floor(x));
          const float di = x - i;
          const NFmiPoint bad(kFloatMissing, kFloatMissing);
          const NFmiPoint xy = NFmiInterpolation::BiLinear(di,
                                                           dj,
                                                           thePoints(i, j + 1, bad),
                                                           thePoints(i + 1, j + 1, bad),
                                                           thePoints(i, j, bad),
                                                           thePoints(i + 1, j, bad));
          theSpec.add(theArea.WorldXYToLatLon(xy));
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save pixelgrid values for later labelling
 */
// ----------------------------------------------------------------------

void add_label_pixelgrid_values(ContourSpec &theSpec,
                                const NFmiArea &theArea,
                                const ImagineXr_or_NFmiImage &img,
                                const NFmiDataMatrix<float> &theValues)
{
  theSpec.clearPixelLabels();

  const float x0 = theSpec.labelXyX0();
  const float y0 = theSpec.labelXyY0();
  const float dx = theSpec.labelXyDX();
  const float dy = theSpec.labelXyDY();

  if (dx > 0 && dy > 0)
  {
    for (float y = y0; y <= img.Height(); y += dy)
      for (float x = x0; x <= img.Width(); x += dx)
      {
        NFmiPoint latlon = theArea.ToLatLon(NFmiPoint(x, y));
        NFmiPoint ij = globals.queryinfo->LatLonToGrid(latlon);

        int i = static_cast<int>(ij.X());  // rounds down
        int j = static_cast<int>(ij.Y());
        float value = static_cast<float>(
            NFmiInterpolation::BiLinear(ij.X() - floor(ij.X()),
                                        ij.Y() - floor(ij.Y()),
                                        theValues.At(i, j + 1, kFloatMissing),
                                        theValues.At(i + 1, j + 1, kFloatMissing),
                                        theValues.At(i, j, kFloatMissing),

                                        theValues.At(i + 1, j, kFloatMissing)));
        theSpec.addPixelLabel(NFmiPoint(x, y), value);
      }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save point values for later labelling
 */
// ----------------------------------------------------------------------

void add_label_point_values(ContourSpec &theSpec,
                            const NFmiArea &theArea,
                            const NFmiDataMatrix<float> &theValues)
{
  theSpec.clearLabelValues();
  if ((theSpec.labelFormat() != "") && !theSpec.labelPoints().empty())
  {
    list<pair<NFmiPoint, NFmiPoint>>::const_iterator it;

    for (it = theSpec.labelPoints().begin(); it != theSpec.labelPoints().end(); ++it)
    {
      NFmiPoint latlon = it->first;
      NFmiPoint ij = globals.queryinfo->LatLonToGrid(latlon);

      int i = static_cast<int>(ij.X());  // rounds down
      int j = static_cast<int>(ij.Y());
      float value =
          static_cast<float>(NFmiInterpolation::BiLinear(ij.X() - floor(ij.X()),
                                                         ij.Y() - floor(ij.Y()),
                                                         theValues.At(i, j + 1, kFloatMissing),
                                                         theValues.At(i + 1, j + 1, kFloatMissing),
                                                         theValues.At(i, j, kFloatMissing),
                                                         theValues.At(i + 1, j, kFloatMissing)));

      theSpec.addLabelValue(value);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw label markers
 */
// ----------------------------------------------------------------------

void draw_label_markers(ImagineXr_or_NFmiImage &img,
                        const ContourSpec &theSpec,
                        const NFmiArea &theArea)
{
  if (theSpec.labelMarker().empty())
    return;

  // Establish that something is to be done

  if (theSpec.labelPoints().empty() && theSpec.pixelLabels().empty())
    return;

  // Establish the marker specs

  const ImagineXr_or_NFmiImage &marker = globals.getImage(theSpec.labelMarker());

  NFmiColorTools::NFmiBlendRule markerrule = ColorTools::checkrule(theSpec.labelMarkerRule());

  float markeralpha = theSpec.labelMarkerAlphaFactor();

  // Draw individual points

  {
    unsigned int pointnumber = 0;
    list<pair<NFmiPoint, NFmiPoint>>::const_iterator iter;
    for (iter = theSpec.labelPoints().begin(); iter != theSpec.labelPoints().end(); ++iter)
    {
      // The point in question

      // NFmiPoint xy = MeridianTools::Relocate(iter->first,theArea);
      // xy = theArea.ToXY(xy);
      NFmiPoint xy = theArea.ToXY(iter->first);

      // Skip rendering if LabelMissing is "" and value is missing
      if (theSpec.labelMissing().empty())
      {
        float value = theSpec.labelValues()[pointnumber++];
        if (value == kFloatMissing)
          continue;
      }

      // Skip rendering if the start point is masked

      if (IsMasked(xy, globals.mask))
        continue;

      img.Composite(marker,
                    markerrule,
                    kFmiAlignCenter,
                    static_cast<int>(round(xy.X())),
                    static_cast<int>(round(xy.Y())),
                    markeralpha);
    }
  }

  {
    list<pair<NFmiPoint, float>>::const_iterator iter;

    for (iter = theSpec.pixelLabels().begin(); iter != theSpec.pixelLabels().end(); ++iter)
    {
      // The point in question

      double x = iter->first.X();
      double y = iter->first.Y();

      // Skip rendering if the start point is masked

      if (IsMasked(NFmiPoint(x, y), globals.mask))
        continue;

      float value = iter->second;

      // Skip rendering if LabelMissing is "" and value is missing
      if (theSpec.labelMissing().empty() && value == kFloatMissing)
        continue;

      img.Composite(marker,
                    markerrule,
                    kFmiAlignCenter,
                    static_cast<int>(x),
                    static_cast<int>(y),
                    markeralpha);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw label texts
 */
// ----------------------------------------------------------------------

void draw_label_texts(ImagineXr_or_NFmiImage &img,
                      const ContourSpec &theSpec,
                      const NFmiArea &theArea)
{
  // Establish that something is to be done

  if (theSpec.labelPoints().empty() && theSpec.pixelLabels().empty())
    return;

  // Quick exit if no labels are desired for this parameter

  if (theSpec.labelFormat() == "")
    return;

    // Create the face object to be used

#ifdef IMAGINE_WITH_CAIRO
  img.MakeFace(theSpec.labelFont());
#else
  Imagine::NFmiFace face = make_face(theSpec.labelFont());
  face.Background(false);
#endif

  // Draw labels at specifing latlon points if requested

  {
    list<pair<NFmiPoint, NFmiPoint>>::const_iterator iter;

    int pointnumber = 0;
    for (iter = theSpec.labelPoints().begin(); iter != theSpec.labelPoints().end(); ++iter)
    {
      // The point in question

      double x, y;
      if (iter->second.X() == kFloatMissing)
      {
        // NFmiPoint xy = MeridianTools::Relocate(iter->first,theArea);
        // xy = theArea.ToXY(xy);
        NFmiPoint xy = theArea.ToXY(iter->first);
        x = xy.X();
        y = xy.Y();
      }
      else
      {
        x = iter->second.X();
        y = iter->second.Y();
      }

      // Fetch the value to be rendered

      float value = theSpec.labelValues()[pointnumber++];

      // Skip rendering if the point is much too far from the image

      const int safetymargin = 50;
      if (x < -safetymargin || y < -safetymargin || x > img.Width() + safetymargin ||
          y > img.Height() + safetymargin)
        continue;

      // Skip rendering if the start point is masked

      if (IsMasked(NFmiPoint(x, y), globals.mask))
        continue;

      // Convert value to string
      string strvalue = theSpec.labelMissing();

      if (value != kFloatMissing)
      {
        char tmp[20];
        sprintf(tmp, theSpec.labelFormat().c_str(), value);
        strvalue = tmp;
      }

      // Don't bother drawing empty strings
      if (strvalue.empty())
        continue;

#ifdef IMAGINE_WITH_CAIRO
      /* Cairo text (must be in UTF-8!)
       */
      img.DrawFace(static_cast<int>(round(x + theSpec.labelOffsetX())),
                   static_cast<int>(round(y + theSpec.labelOffsetY())),
                   strvalue,
                   theSpec.labelColor(),
                   AlignmentValue(theSpec.labelAlignment()),
                   ColorTools::checkrule(theSpec.labelRule()));

      // Then the label caption

      if (!theSpec.labelCaption().empty())
      {
        img.DrawFace(static_cast<int>(round(x + theSpec.labelCaptionDX())),
                     static_cast<int>(round(y + theSpec.labelCaptionDY())),
                     theSpec.labelCaption(),
                     theSpec.labelColor(),
                     AlignmentValue(theSpec.labelCaptionAlignment()),
                     ColorTools::checkrule(theSpec.labelRule()));
      }
#else
      // Set new text properties

      face.Draw(img,
                static_cast<int>(round(x + theSpec.labelOffsetX())),
                static_cast<int>(round(y + theSpec.labelOffsetY())),
                strvalue,
                AlignmentValue(theSpec.labelAlignment()),
                theSpec.labelColor(),
                ColorTools::checkrule(theSpec.labelRule()));

      // Then the label caption

      if (!theSpec.labelCaption().empty())
      {
        face.Draw(img,
                  static_cast<int>(round(x + theSpec.labelCaptionDX())),
                  static_cast<int>(round(y + theSpec.labelCaptionDY())),
                  theSpec.labelCaption(),
                  AlignmentValue(theSpec.labelCaptionAlignment()),
                  theSpec.labelColor(),
                  ColorTools::checkrule(theSpec.labelRule()));
      }
#endif
    }
  }

  // Draw labels at specifing pixel coordinates if requested

  {
    list<pair<NFmiPoint, float>>::const_iterator iter;

    for (iter = theSpec.pixelLabels().begin(); iter != theSpec.pixelLabels().end(); ++iter)
    {
      // The point in question

      double x = iter->first.X();
      double y = iter->first.Y();
      float value = iter->second;

      // Skip rendering if the point is much too far from the image

      const int safetymargin = 50;
      if (x < -safetymargin || y < -safetymargin || x > img.Width() + safetymargin ||
          y > img.Height() + safetymargin)
        continue;

      // Skip rendering if the start point is masked

      if (IsMasked(NFmiPoint(x, y), globals.mask))
        continue;

      // Convert value to string
      string strvalue = theSpec.labelMissing();

      if (value != kFloatMissing)
      {
        char tmp[20];
        sprintf(tmp, theSpec.labelFormat().c_str(), value);
        strvalue = tmp;
      }

      // Don't bother drawing empty strings
      if (strvalue.empty())
        continue;

#ifdef IMAGINE_WITH_CAIRO
      /* Cairo text (must be in UTF-8!)
       */
      // Set new text properties

      img.DrawFace(static_cast<int>(round(x + theSpec.labelOffsetX())),
                   static_cast<int>(round(y + theSpec.labelOffsetY())),
                   strvalue,
                   theSpec.labelColor(),
                   AlignmentValue(theSpec.labelAlignment()),
                   ColorTools::checkrule(theSpec.labelRule()));

      // Then the label caption

      if (!theSpec.labelCaption().empty())
      {
        img.DrawFace(static_cast<int>(round(x + theSpec.labelCaptionDX())),
                     static_cast<int>(round(y + theSpec.labelCaptionDY())),
                     theSpec.labelCaption(),
                     theSpec.labelColor(),
                     AlignmentValue(theSpec.labelCaptionAlignment()),
                     ColorTools::checkrule(theSpec.labelRule()));
      }
#else
      // Set new text properties

      face.Draw(img,
                static_cast<int>(round(x + theSpec.labelOffsetX())),
                static_cast<int>(round(y + theSpec.labelOffsetY())),
                strvalue,
                AlignmentValue(theSpec.labelAlignment()),
                theSpec.labelColor(),
                ColorTools::checkrule(theSpec.labelRule()));

      // Then the label caption

      if (!theSpec.labelCaption().empty())
      {
        face.Draw(img,
                  static_cast<int>(round(x + theSpec.labelCaptionDX())),
                  static_cast<int>(round(y + theSpec.labelCaptionDY())),
                  theSpec.labelCaption(),
                  AlignmentValue(theSpec.labelCaptionAlignment()),
                  theSpec.labelColor(),
                  ColorTools::checkrule(theSpec.labelRule()));
      }
#endif
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the circle for a round arrow
 */
// ----------------------------------------------------------------------

NFmiPath roundarrow_circle(const NFmiPoint &xy, const RoundArrowSize &sz)
{
  NFmiPath path;

  if (sz.circleradius <= 0)
    return path;

  float circumference = 2 * pi * sz.circleradius;
  int segments = static_cast<int>(circumference / 5);

  for (int i = 0; i < segments; i++)
  {
    float angle = i * 2 * pi / segments;
    float x = xy.X() + sz.circleradius * sin(angle);
    float y = xy.Y() + sz.circleradius * cos(angle);
    if (i == 0)
      path.MoveTo(x, y);
    else
      path.LineTo(x, y);
  }
  path.CloseLineTo();

  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the triangle for a round arrow
 */
// ----------------------------------------------------------------------

NFmiPath roundarrow_triangle(const NFmiPoint &xy, float angle, const RoundArrowSize &sz)
{
  NFmiPath path;

  if (sz.trianglewidth <= 0)
    return path;

  float triangleh = sz.trianglewidth / 2 / tan(pi * sz.triangleangle / 2 / 180);
  path.MoveTo(0, -sz.triangleradius - triangleh);
  path.LineTo(sz.trianglewidth / 2, -sz.triangleradius);
  path.LineTo(-sz.trianglewidth / 2, -sz.triangleradius);
  path.CloseLineTo();
  path.Rotate(angle);
  path.Translate(xy.X(), xy.Y());

  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the standard round arrow
 */
// ----------------------------------------------------------------------

void draw_roundarrow(NFmiImage &img, const NFmiPoint &xy, float speed, float angle)
{
  RoundArrowColor fillcolor = globals.getRoundArrowFillColor(speed);
  RoundArrowColor strokecolor = globals.getRoundArrowStrokeColor(speed);
  RoundArrowSize sz = globals.getRoundArrowSize(speed);

  NFmiPath circle = roundarrow_circle(xy, sz);
  NFmiPath triangle = roundarrow_triangle(xy, angle, sz);

  triangle.Fill(img, fillcolor.trianglecolor);
  triangle.Stroke(img, strokecolor.trianglecolor);

  circle.Fill(img, fillcolor.circlecolor);
  circle.Stroke(img, strokecolor.circlecolor);
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish speed and direction in a grid
 */
// ----------------------------------------------------------------------

void get_speed_direction(const Fmi::CoordinateTransformation &transformation,
                         float speed_src,
                         float speed_dst,
                         float direction_src,
                         float direction_dst,
                         NFmiDataMatrix<float> &speed,
                         NFmiDataMatrix<float> &direction)
{
  if (!globals.directionparam.empty())
  {
    if (globals.queryinfo->Param(toparam(globals.speedparam)))
    {
      speed = globals.queryinfo->Values();
      speed.Replace(speed_src, speed_dst);
      globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()), speed);
    }

    if (globals.queryinfo->Param(toparam(globals.directionparam)))
    {
      direction = globals.queryinfo->Values();
      direction.Replace(direction_src, direction_dst);
      globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()),
                                     direction);
    }
  }
  else
  {
    NFmiDataMatrix<float> dx;
    NFmiDataMatrix<float> dy;

    if (globals.queryinfo->Param(toparam(globals.speedxcomponent)))
      dx = globals.queryinfo->Values();
    if (globals.queryinfo->Param(toparam(globals.speedycomponent)))
      dy = globals.queryinfo->Values();

    auto latlon = globals.queryinfo->Locations();

    if (dx.NX() != 0 && dx.NY() != 0 && dy.NX() != 0 && dy.NY() != 0)
    {
      speed.Resize(dx.NX(), dx.NY(), kFloatMissing);
      direction.Resize(dx.NX(), dy.NY(), kFloatMissing);
      for (size_t j = 0; j < dx.NY(); j++)
        for (size_t i = 0; i < dx.NX(); i++)
        {
          if (dx[i][j] != kFloatMissing && dy[i][j] != kFloatMissing)
          {
            speed[i][j] = sqrt(dx[i][j] * dx[i][j] + dy[i][j] * dy[i][j]);
            if (dx[i][j] != 0 || dy[i][j] != 0)
            {
              auto north = Fmi::OGR::gridNorth(transformation, latlon->x(i, j), latlon->y(i, j));
              if (north)
                direction[i][j] = fmod(180 + *north + FmiDeg(atan2(dx[i][j], dy[i][j])), 360.0);
              else
                direction[i][j] = kFloatMissing;
            }
          }
        }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish speed & direction at the given point
 */
// ----------------------------------------------------------------------

void get_speed_direction(const Fmi::CoordinateTransformation &transformation,
                         const NFmiPoint &latlon,
                         float speed_src,
                         float speed_dst,
                         float direction_src,
                         float direction_dst,
                         float &speed,
                         float &direction)
{
  speed = direction = kFloatMissing;

  if (!globals.directionparam.empty())
  {
    if (globals.queryinfo->Param(toparam(globals.directionparam)))
    {
      direction = globals.queryinfo->InterpolatedValue(latlon);
      if (direction == direction_src)
        direction = direction_dst;

      direction = globals.unitsconverter.convert(
          FmiParameterName(globals.queryinfo->GetParamIdent()), direction);
    }

    if (globals.queryinfo->Param(toparam(globals.speedparam)))
    {
      speed = globals.queryinfo->InterpolatedValue(latlon);
      if (speed == speed_src)
        speed = speed_dst;
      speed = globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()),
                                             speed);
    }
    globals.queryinfo->Param(toparam(globals.directionparam));
  }

  else
  {
    float dx = kFloatMissing;
    float dy = kFloatMissing;

    if (globals.queryinfo->Param(toparam(globals.speedxcomponent)))
      dx = globals.queryinfo->InterpolatedValue(latlon);
    if (globals.queryinfo->Param(toparam(globals.speedycomponent)))
      dy = globals.queryinfo->InterpolatedValue(latlon);

    if (dx != kFloatMissing && dy != kFloatMissing)
    {
      speed = sqrt(dx * dx + dy * dy);
      if (dx != 0 || dy != 0)
      {
        auto north = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());
        if (north)
          direction = fmod(180 + *north + FmiDeg(atan2f(dx, dy)), 360.0);
        else
          direction = kFloatMissing;
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the listed wind arrow points
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_points(ImagineXr_or_NFmiImage &img,
                             const NFmiArea &theArea,
                             const NFmiPath &theArrow,
                             float direction_src,
                             float direction_dst,
                             float speed_src,
                             float speed_dst)
{
  // Handle all given coordinates

  list<NFmiPoint>::const_iterator iter;

  Fmi::CoordinateTransformation transformation("WGS84", theArea.SpatialReference());

  for (iter = globals.arrowpoints.begin(); iter != globals.arrowpoints.end(); ++iter)
  {
    // The start point
    // NFmiPoint latlon = MeridianTools::Relocate(*iter,theArea);
    NFmiPoint latlon = *iter;
    NFmiPoint xy0 = theArea.ToXY(latlon);

    // Skip rendering if the start point is masked

    if (IsMasked(xy0, globals.mask))
      continue;

    float dir, speed;

    get_speed_direction(
        transformation, latlon, speed_src, speed_dst, direction_src, direction_dst, speed, dir);

    // Ignore missing values
    if (dir == kFloatMissing || speed == kFloatMissing)
      continue;

    // Direction calculations

    auto north = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());

    if (!north)
      continue;

    // Render the arrow

    if (globals.arrowfile == "roundarrow")
    {
      draw_roundarrow(img, xy0, speed, -dir + *north + 180);
    }
    else
    {
      if (globals.arrowfile == "meteorological")
      {
        NFmiPath strokes;
        NFmiPath flags;

        strokes.Add(GramTools::metarrowlines(speed, latlon));
        flags.Add(GramTools::metarrowflags(speed, latlon));

        if (speed > 0 && speed != kFloatMissing)
        {
          strokes.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                        globals.windarrowscaleC);
          flags.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                      globals.windarrowscaleC);
        }

        strokes.Scale(globals.arrowscale);
        strokes.Rotate(-dir + *north + 180);
        strokes.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

        flags.Scale(globals.arrowscale);
        flags.Rotate(-dir + *north + 180);
        flags.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

        ArrowStyle style = globals.getArrowStroke(speed);
        strokes.Stroke(img, style.color, style.rule);
        flags.Fill(img, style.color, style.rule);
      }
      else
      {
        NFmiPath arrowpath;
        arrowpath.Add(theArrow);

        if (speed > 0 && speed != kFloatMissing)
          arrowpath.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                          globals.windarrowscaleC);
        arrowpath.Scale(globals.arrowscale);
        arrowpath.Rotate(-dir + *north + 180);
        arrowpath.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

        // And render it

        ArrowStyle fillstyle = globals.getArrowFill(speed);
        arrowpath.Fill(img, fillstyle.color, fillstyle.rule);

        ArrowStyle strokestyle = globals.getArrowStroke(speed);
        arrowpath.Stroke(img, strokestyle.color, strokestyle.rule);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the data gridded wind arrow points
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_grid(ImagineXr_or_NFmiImage &img,
                           const NFmiArea &theArea,
                           const NFmiPath &theArrow,
                           float direction_src,
                           float direction_dst,
                           float speed_src,
                           float speed_dst)
{
  // Draw the full grid if so desired

  if (globals.windarrowdx <= 0 || globals.windarrowdy <= 0)
    return;

  NFmiDataMatrix<float> speedvalues, dirvalues;

  Fmi::CoordinateTransformation wgs84transformation("WGS84", theArea.SpatialReference());

  get_speed_direction(wgs84transformation,
                      speed_src,
                      speed_dst,
                      direction_src,
                      direction_dst,
                      speedvalues,
                      dirvalues);

  if (dirvalues.NX() == 0 || dirvalues.NY() == 0)
  {
    return;
  }

  bool speedok = (speedvalues.NX() != 0 && speedvalues.NY() != 0);

  // Data coordinates to target area worldxy coordinates

  auto coordinates = globals.queryinfo->CoordinateMatrix();

  Fmi::CoordinateTransformation transformation(globals.queryinfo->SpatialReference(),
                                               theArea.SpatialReference());

  if (!coordinates.transform(transformation))
    return;

  // Needed for grid to latlon conversions
  const auto *grid = globals.queryinfo->Grid();

  for (float y = 0; y < coordinates.height() - 1; y += globals.windarrowdy)
    for (float x = 0; x < coordinates.width() - 1; x += globals.windarrowdx)
    {
      // The start point

      const int i = static_cast<int>(floor(x));
      const int j = static_cast<int>(floor(y));

      NFmiPoint xy = NFmiInterpolation::BiLinear(x - i,
                                                 y - j,
                                                 coordinates(i, j + 1),
                                                 coordinates(i + 1, j + 1),
                                                 coordinates(i, j),
                                                 coordinates(i + 1, j));

      NFmiPoint xy0 = theArea.WorldXYToXY(xy);

      // Skip rendering if the start point is masked
      if (IsMasked(xy0, globals.mask))
        continue;

      // Render the arrow

      double dir = NFmiInterpolation::ModBiLinear(x - i,
                                                  y - j,
                                                  dirvalues.At(i, j + 1, kFloatMissing),
                                                  dirvalues.At(i + 1, j + 1, kFloatMissing),
                                                  dirvalues.At(i, j, kFloatMissing),
                                                  dirvalues.At(i + 1, j, kFloatMissing),
                                                  360);

      // Skip rendering if the start point is way outside the image

      const int safetymargin = 50;
      if (xy0.X() < -safetymargin || xy0.Y() < -safetymargin ||
          xy0.X() > img.Width() + safetymargin || xy0.Y() > img.Height() + safetymargin)
        continue;

      if (dir == kFloatMissing)  // ignore missing
        continue;

      double speed = NFmiInterpolation::BiLinear(x - i,
                                                 y - j,
                                                 speedvalues.At(i, j + 1, kFloatMissing),
                                                 speedvalues.At(i + 1, j + 1, kFloatMissing),
                                                 speedvalues.At(i, j, kFloatMissing),
                                                 speedvalues.At(i + 1, j, kFloatMissing));

      if (speedok && speed == kFloatMissing)  // ignore missing
        continue;

      // Direction calculations

      const auto latlon = grid->GridToLatLon(x, y);

      auto north = Fmi::OGR::gridNorth(wgs84transformation, latlon.X(), latlon.Y());

      if (!north)
        continue;

      // Render the arrow

      if (globals.arrowfile == "roundarrow")
      {
        draw_roundarrow(img, xy0, speed, -dir + *north + 180);
      }
      else
      {
        if (globals.arrowfile == "meteorological")
        {
          NFmiPath strokes;
          NFmiPath flags;

          strokes.Add(GramTools::metarrowlines(speed, latlon));
          flags.Add(GramTools::metarrowflags(speed, latlon));

          if (speed > 0 && speed != kFloatMissing)
          {
            strokes.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                          globals.windarrowscaleC);
            flags.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                        globals.windarrowscaleC);
          }

          strokes.Scale(globals.arrowscale);
          strokes.Rotate(-dir + *north + 180);
          strokes.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          flags.Scale(globals.arrowscale);
          flags.Rotate(-dir + *north + 180);
          flags.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          ArrowStyle style = globals.getArrowStroke(speed);
          strokes.Stroke(img, style.color, style.rule);
          flags.Fill(img, style.color, style.rule);
        }
        else
        {
          NFmiPath arrowpath;
          arrowpath.Add(theArrow);

          if (speed > 0 && speed != kFloatMissing)
            arrowpath.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                            globals.windarrowscaleC);
          arrowpath.Scale(globals.arrowscale);
          arrowpath.Rotate(-dir + *north + 180);
          arrowpath.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          // And render it

          ArrowStyle fillstyle = globals.getArrowFill(speed);
          arrowpath.Fill(img, fillstyle.color, fillstyle.rule);

          ArrowStyle strokestyle = globals.getArrowStroke(speed);
          arrowpath.Stroke(img, strokestyle.color, strokestyle.rule);
        }
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the wind arrows in image coordinates
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_pixelgrid(ImagineXr_or_NFmiImage &img,
                                const NFmiArea &theArea,
                                const NFmiPath &theArrow,
                                float direction_src,
                                float direction_dst,
                                float speed_src,
                                float speed_dst)
{
  // Draw the full grid if so desired

  if (globals.windarrowsxydx <= 0 || globals.windarrowsxydy <= 0)
    return;

  Fmi::CoordinateTransformation transformation("WGS84", theArea.SpatialReference());

  for (float y = globals.windarrowsxyy0; y <= img.Height(); y += globals.windarrowsxydy)
    for (float x = globals.windarrowsxyx0; x <= img.Width(); x += globals.windarrowsxydx)
    {
      NFmiPoint xy0(x, y);

      // Skip the point if it is masked
      if (IsMasked(xy0, globals.mask))
        continue;

      // Calculate the latlon value

      NFmiPoint latlon = theArea.ToLatLon(xy0);

      // Calculate the speed & direction values

      float dir, speed;

      get_speed_direction(
          transformation, latlon, speed_src, speed_dst, direction_src, direction_dst, speed, dir);

      // Ignore missing values

      if (dir == kFloatMissing || speed == kFloatMissing)
        continue;

      // Direction calculations

      auto north = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());

      if (!north)
        continue;

      // Render the arrow

      if (globals.arrowfile == "roundarrow")
      {
        draw_roundarrow(img, xy0, speed, -dir + *north + 180);
      }
      else
      {
        if (globals.arrowfile == "meteorological")
        {
          NFmiPath strokes;
          NFmiPath flags;

          strokes.Add(GramTools::metarrowlines(speed, latlon));
          flags.Add(GramTools::metarrowflags(speed, latlon));

          if (speed > 0 && speed != kFloatMissing)
          {
            strokes.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                          globals.windarrowscaleC);
            flags.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                        globals.windarrowscaleC);
          }

          strokes.Scale(globals.arrowscale);
          strokes.Rotate(-dir + *north + 180);
          strokes.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          flags.Scale(globals.arrowscale);
          flags.Rotate(-dir + *north + 180);
          flags.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          ArrowStyle style = globals.getArrowStroke(speed);
          strokes.Stroke(img, style.color, style.rule);
          flags.Fill(img, style.color, style.rule);
        }
        else
        {
          NFmiPath arrowpath;
          arrowpath.Add(theArrow);

          if (speed > 0 && speed != kFloatMissing)
            arrowpath.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                            globals.windarrowscaleC);
          arrowpath.Scale(globals.arrowscale);
          arrowpath.Rotate(-dir + *north + 180);
          arrowpath.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

          // And render it

          ArrowStyle fillstyle = globals.getArrowFill(speed);
          arrowpath.Fill(img, fillstyle.color, fillstyle.rule);

          ArrowStyle strokestyle = globals.getArrowStroke(speed);
          arrowpath.Stroke(img, strokestyle.color, strokestyle.rule);
        }
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw wind arrows onto the image
 */
// ----------------------------------------------------------------------

void draw_wind_arrows(ImagineXr_or_NFmiImage &img, const NFmiArea &theArea)
{
  if ((!globals.arrowpoints.empty() || (globals.windarrowdx > 0 && globals.windarrowdy > 0) ||
       (globals.windarrowsxydx > 0 && globals.windarrowsxydy > 0)) &&
      (globals.arrowfile != ""))
  {
    FmiParameterName param;
    std::string name;
    if (!globals.directionparam.empty())
    {
      param = toparam(globals.directionparam);
      name = globals.directionparam;
    }
    else
    {
      param = toparam(globals.speedxcomponent);
      name = globals.speedxcomponent;
    }

    if (param == kFmiBadParameter)
      throw runtime_error("Unknown parameter " + name);

    // Find the proper queryinfo to be used

    bool ok = false;
    for (unsigned int qi = 0; qi < globals.querystreams.size(); qi++)
    {
      globals.queryinfo = globals.querystreams[qi];
      globals.queryinfo->Param(param);
      ok = globals.queryinfo->IsParamUsable();
      if (ok)
        break;
    }

    if (!ok)
      throw runtime_error("Parameter is not usable: " + name);

    // Read the arrow definition

    NFmiPath arrowpath;
    if (globals.arrowfile != "meteorological" && globals.arrowfile != "roundarrow")
    {
      const string &arr = globals.itsArrowCache.find(globals.arrowfile);
      arrowpath.Add(arr);
    }

    // Establish data replacement values

    list<ContourSpec>::iterator piter;
    list<ContourSpec>::iterator pbegin = globals.specs.begin();
    list<ContourSpec>::iterator pend = globals.specs.end();

    float direction_src = kFloatMissing;
    float direction_dst = kFloatMissing;
    float speed_src = kFloatMissing;
    float speed_dst = kFloatMissing;

    for (piter = pbegin; piter != pend; ++piter)
    {
      if (piter->param() == globals.directionparam && piter->replace())
      {
        direction_src = piter->replaceSourceValue();
        direction_dst = piter->replaceTargetValue();
      }
      else if (piter->param() == globals.speedparam && piter->replace())
      {
        speed_src = piter->replaceSourceValue();
        speed_dst = piter->replaceTargetValue();
      }
    }

    draw_wind_arrows_points(
        img, theArea, arrowpath, direction_src, direction_dst, speed_src, speed_dst);
    draw_wind_arrows_grid(
        img, theArea, arrowpath, direction_src, direction_dst, speed_src, speed_dst);
    draw_wind_arrows_pixelgrid(
        img, theArea, arrowpath, direction_src, direction_dst, speed_src, speed_dst);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour fills
 */
// ----------------------------------------------------------------------

void draw_contour_fills(ImagineXr_or_NFmiImage &img,
                        const NFmiArea &theArea,
                        const ContourSpec &theSpec,
                        const NFmiTime &theTime,
                        ContourInterpolation theInterpolation)
{
  list<ContourRange>::const_iterator it;
  list<ContourRange>::const_iterator begin;
  list<ContourRange>::const_iterator end;

  begin = theSpec.contourFills().begin();
  end = theSpec.contourFills().end();

  for (it = begin; it != end; ++it)
  {
    // Contour the actual data

    if (globals.verbose)
      cout << "Calculating " << it->lolimit() << " - " << it->hilimit() << endl;

    NFmiPath path = globals.calculator.contour(
        *globals.queryinfo, it->lolimit(), it->hilimit(), theTime, theInterpolation);

    if (globals.verbose && globals.calculator.wasCached())
      cout << "Using cached " << it->lolimit() << " - " << it->hilimit() << endl;

    // Avoid unnecessary work if the path is empty
    if (path.Empty() && it->lolimit() != kFloatMissing && it->hilimit() != kFloatMissing)
      continue;

    // MeridianTools::Relocate(path,theArea);
    path.Project(&theArea);
    invert_if_missing(path, it->lolimit(), it->hilimit());

    NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(it->rule());

    path.Fill(img, it->color(), rule);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour patterns
 */
// ----------------------------------------------------------------------

void draw_contour_patterns(ImagineXr_or_NFmiImage &img,
                           const NFmiArea &theArea,
                           const ContourSpec &theSpec,
                           const NFmiTime &theTime,
                           ContourInterpolation theInterpolation)
{
  list<ContourPattern>::const_iterator it;
  list<ContourPattern>::const_iterator begin;
  list<ContourPattern>::const_iterator end;

  begin = theSpec.contourPatterns().begin();
  end = theSpec.contourPatterns().end();

  for (it = begin; it != end; ++it)
  {
    NFmiPath path = globals.calculator.contour(
        *globals.queryinfo, it->lolimit(), it->hilimit(), theTime, theInterpolation);

    if (globals.verbose && globals.calculator.wasCached())
      cout << "Using cached " << it->lolimit() << " - " << it->hilimit() << endl;

    NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(it->rule());
    const ImagineXr_or_NFmiImage &pattern = globals.getImage(it->pattern());

    // MeridianTools::Relocate(path,theArea);
    path.Project(&theArea);
    invert_if_missing(path, it->lolimit(), it->hilimit());

    path.Fill(img, pattern, rule, it->factor());
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour strokes
 */
// ----------------------------------------------------------------------

void draw_contour_strokes(ImagineXr_or_NFmiImage &img,
                          const NFmiArea &theArea,
                          const ContourSpec &theSpec,
                          const NFmiTime &theTime,
                          ContourInterpolation theInterpolation)
{
  list<ContourValue>::const_iterator it;
  list<ContourValue>::const_iterator begin;
  list<ContourValue>::const_iterator end;

  begin = theSpec.contourValues().begin();
  end = theSpec.contourValues().end();

  for (it = begin; it != end; ++it)
  {
    NFmiPath path =
        globals.calculator.contour(*globals.queryinfo, it->value(), theTime, theInterpolation);

    if (globals.verbose && globals.calculator.wasCached())
      cout << "Using cached " << it->value() << endl;

    NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(it->rule());
    // MeridianTools::Relocate(path,theArea);
    path.Project(&theArea);
    path.SimplifyLines(10);
    float width = it->linewidth();
    if (width == 1)
      path.Stroke(img, it->color(), rule);
    else
      path.Stroke(img, width, it->color(), rule);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Collect contour label candidate coordinates
 */
// ----------------------------------------------------------------------

void save_contour_labels(ImagineXr_or_NFmiImage &img,
                         const NFmiArea &theArea,
                         const ContourSpec &theSpec,
                         const NFmiTime &theTime,
                         ContourInterpolation theInterpolation)
{
  // The ID under which the coordinates will be stored

  int id = paramid(theSpec.param());
  globals.labellocator.parameter(id);

  // Start saving candindate coordinates

  list<ContourLabel>::const_iterator it;
  list<ContourLabel>::const_iterator begin;
  list<ContourLabel>::const_iterator end;

  begin = theSpec.contourLabels().begin();
  end = theSpec.contourLabels().end();

  for (it = begin; it != end; ++it)
  {
    NFmiPath path =
        globals.calculator.contour(*globals.queryinfo, it->value(), theTime, theInterpolation);

    // MeridianTools::Relocate(path,theArea);
    path.Project(&theArea);

    for (NFmiPathData::const_iterator pit = path.Elements().begin(); pit != path.Elements().end();
         ++pit)
    {
      if (pit->op == kFmiLineTo)
      {
        globals.labellocator.add(
            it->value(), static_cast<int>(round(pit->x)), static_cast<int>(round(pit->y)));
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour labels
 */
// ----------------------------------------------------------------------

void draw_contour_labels(ImagineXr_or_NFmiImage &img)
{
  const LabelLocator::ParamCoordinates &coords = globals.labellocator.chooseLabels();

  if (coords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  list<ContourSpec>::iterator pbegin = globals.specs.begin();
  list<ContourSpec>::iterator pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    LabelLocator::ParamCoordinates::const_iterator pit = coords.find(id);
    if (pit == coords.end())
      continue;

    // Rended the contours

    const int fontcolor = piter->contourLabelColor();
    const std::string &fontspec = piter->contourLabelFont();
    const int backcolor = piter->contourLabelBackgroundColor();
    const int xmargin = piter->contourLabelBackgroundXMargin();
    const int ymargin = piter->contourLabelBackgroundYMargin();

#ifdef IMAGINE_WITH_CAIRO
    img.MakeFace(fontspec, backcolor, xmargin, ymargin);
#else
    Imagine::NFmiFace face = make_face(fontspec);
    face.Background(true);
    face.BackgroundColor(backcolor);
    face.BackgroundMargin(xmargin, ymargin);
#endif

    for (LabelLocator::ContourCoordinates::const_iterator cit = pit->second.begin();
         cit != pit->second.end();
         ++cit)
    {
      const float value = cit->first;
      string text = NFmiStringTools::Convert(value);

      // Handle possible contourlabeltext override
      std::map<float, std::string>::const_iterator it = piter->contourLabelTexts().find(value);
      if (it != piter->contourLabelTexts().end())
        text = it->second;

      for (LabelLocator::Coordinates::const_iterator it = cit->second.begin();
           it != cit->second.end();
           ++it)
      {
#ifdef IMAGINE_WITH_CAIRO
        // 'text' must be in UTF-8!
        //
        img.DrawFace(
            it->second.first, it->second.second, text, fontcolor, Imagine::kFmiAlignCenter);
#else
        face.Draw(
            img, it->second.first, it->second.second, text, Imagine::kFmiAlignCenter, fontcolor);
#endif
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save contour symbols
 */
// ----------------------------------------------------------------------

void save_contour_symbols(ImagineXr_or_NFmiImage &img,
                          const NFmiArea &theArea,
                          const ContourSpec &theSpec,
                          const LazyCoordinates &thePoints,
                          const NFmiDataMatrix<float> &theValues)
{
  // The ID under which the coordinates will be stored

  int id = paramid(theSpec.param());
  globals.imagelocator.parameter(id);

  list<ContourSymbol>::const_iterator it;
  list<ContourSymbol>::const_iterator begin;
  list<ContourSymbol>::const_iterator end;

  begin = theSpec.contourSymbols().begin();
  end = theSpec.contourSymbols().end();

  for (it = begin; it != end; ++it)
  {
    const float lo = it->lolimit();
    const float hi = it->hilimit();

    for (unsigned int j = 0; j < theValues.NY(); j++)
      for (unsigned int i = 0; i < theValues.NX(); i++)
      {
        const float z = theValues[i][j];
        bool inside = false;
        if (z == kFloatMissing)
          inside = (lo == kFloatMissing && hi == kFloatMissing);
        else if (lo != kFloatMissing && z < lo)
          inside = false;
        else if (hi != kFloatMissing && z >= hi)
          inside = false;
        else if (lo == kFloatMissing && hi == kFloatMissing)
          inside = false;
        else
          inside = true;

        if (inside)
        {
          NFmiPoint latlon = theArea.WorldXYToLatLon(thePoints(i, j));
          // latlon = MeridianTools::Relocate(latlon,theArea);
          NFmiPoint xy = theArea.ToXY(latlon);

          globals.imagelocator.add(
              z, static_cast<int>(round(xy.X())), static_cast<int>(round(xy.Y())));
        }
      }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour symbols
 */
// ----------------------------------------------------------------------

void draw_contour_symbols(ImagineXr_or_NFmiImage &img)
{
  const LabelLocator::ParamCoordinates &paramcoords = globals.imagelocator.chooseLabels();

  if (paramcoords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  list<ContourSpec>::iterator pbegin = globals.specs.begin();
  list<ContourSpec>::iterator pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    LabelLocator::ParamCoordinates::const_iterator pit = paramcoords.find(id);
    if (pit == paramcoords.end())
      continue;

    const LabelLocator::ContourCoordinates &coords = pit->second;

    // Loop through all the values

    for (LabelLocator::ContourCoordinates::const_iterator cit = coords.begin(); cit != coords.end();
         ++cit)
    {
      const float z = cit->first;

      // Find the specs for the font value

      list<ContourSymbol>::const_iterator fit;
      for (fit = piter->contourSymbols().begin(); fit != piter->contourSymbols().end(); ++fit)
      {
        const float lo = fit->lolimit();
        const float hi = fit->hilimit();
        bool inside = false;
        if (z == kFloatMissing)
          inside = (lo == kFloatMissing && hi == kFloatMissing);
        else if (lo != kFloatMissing && z < lo)
          inside = false;
        else if (hi != kFloatMissing && z >= hi)
          inside = false;
        else if (lo == kFloatMissing && hi == kFloatMissing)
          inside = false;
        else
          inside = true;
        if (inside)
          break;
      }

      // Should never happen
      if (fit == piter->contourSymbols().end())
        throw runtime_error("Internal error while contouring with symbols");

      // Render the symbols

      NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(fit->rule());
      const ImagineXr_or_NFmiImage &symbol = globals.getImage(fit->pattern());
      const float factor = fit->factor();

      for (LabelLocator::Coordinates::const_iterator it = cit->second.begin();
           it != cit->second.end();
           ++it)
      {
        img.Composite(symbol, rule, kFmiAlignCenter, it->second.first, it->second.second, factor);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour fonts
 */
// ----------------------------------------------------------------------

void draw_contour_fonts(ImagineXr_or_NFmiImage &img)
{
  const LabelLocator::ParamCoordinates &paramcoords = globals.symbollocator.chooseLabels();

  if (paramcoords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  list<ContourSpec>::iterator pbegin = globals.specs.begin();
  list<ContourSpec>::iterator pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    LabelLocator::ParamCoordinates::const_iterator pit = paramcoords.find(id);
    if (pit == paramcoords.end())
      continue;

    const LabelLocator::ContourCoordinates &coords = pit->second;

    // Loop through all the values

    for (LabelLocator::ContourCoordinates::const_iterator cit = coords.begin(); cit != coords.end();
         ++cit)
    {
      const float value = cit->first;

      // Find the specs for the font value

      list<ContourFont>::const_iterator fit;
      for (fit = piter->contourFonts().begin(); fit != piter->contourFonts().end(); ++fit)
      {
        if (fit->value() == value)
          break;
      }

      // Should never happen
      if (fit == piter->contourFonts().end())
        throw runtime_error("Internal error while contouring with fonts");

      // Render the symbols

      const int fontcolor = fit->color();
      const int symbol = fit->symbol();

      string text = "";
      text += symbol;

      const std::string &fontspec = fit->font();

#ifdef IMAGINE_WITH_CAIRO
      img.MakeFace(fontspec);
#else
      Imagine::NFmiFace face = make_face(fontspec);
      face.Background(false);
#endif
      for (LabelLocator::Coordinates::const_iterator it = cit->second.begin();
           it != cit->second.end();
           ++it)
      {
#ifdef IMAGINE_WITH_CAIRO
        // 'text' must be in UTF-8!
        //
        img.DrawFace(
            it->second.first, it->second.second, text, fontcolor, Imagine::kFmiAlignCenter);
#else
        face.Draw(
            img, it->second.first, it->second.second, text, Imagine::kFmiAlignCenter, fontcolor);
#endif
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save contour font coordinates
 */
// ----------------------------------------------------------------------

void save_contour_fonts(ImagineXr_or_NFmiImage &img,
                        const NFmiArea &theArea,
                        const ContourSpec &theSpec,
                        const LazyCoordinates &thePoints,
                        const NFmiDataMatrix<float> &theValues)
{
  // The ID under which the coordinates will be stored

  int id = paramid(theSpec.param());
  globals.symbollocator.parameter(id);

  // For speed we prefer to iterate only once through the data, and
  // instead use a fast way to test if a given value is to be contoured

  set<float> okvalues;

  list<ContourFont>::const_iterator it;
  list<ContourFont>::const_iterator begin;
  list<ContourFont>::const_iterator end;

  begin = theSpec.contourFonts().begin();
  end = theSpec.contourFonts().end();

  for (it = begin; it != end; ++it)
  {
    okvalues.insert(it->value());
  }

  // Now iterate through the data once, saving candidate points

  for (unsigned int j = 0; j < theValues.NY(); j++)
    for (unsigned int i = 0; i < theValues.NX(); i++)
    {
      if (okvalues.find(theValues[i][j]) != okvalues.end())
      {
        NFmiPoint latlon = theArea.WorldXYToLatLon(thePoints(i, j));
        // latlon = MeridianTools::Relocate(latlon,theArea);
        NFmiPoint xy = theArea.ToXY(latlon);

        globals.symbollocator.add(
            theValues[i][j], static_cast<int>(round(xy.X())), static_cast<int>(round(xy.Y())));
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw overlay
 */
// ----------------------------------------------------------------------

void draw_overlay(ImagineXr_or_NFmiImage &img, const ContourSpec &theSpec)
{
  if (theSpec.overlay().empty())
    return;

  img.Composite(globals.getImage(theSpec.overlay()),
                NFmiColorTools::kFmiColorOver,
                kFmiAlignNorthWest,
                0,
                0,
                1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the type of extremum at the given point
 *
 * \param theValues The matrix of values
 * \param i The i-coordinate of the value
 * \param j The j-coordinate of the value
 * \param DX The search region in X-direction
 * \param DY The search region in Y-direction
 * \param mingradient Minimum required difference in area
 * \return -2/2 for absolute minima/maxima, -1/1 for minima/maxima, 0 for none
 */
// ----------------------------------------------------------------------

int extrematype(
    const NFmiDataMatrix<float> &theValues, int i, int j, int DX, int DY, float mingradient)
{
  int smaller = 0;
  int bigger = 0;

  // minimum/maximum on the frame
  float minimum = theValues[i - DX][j - DY];
  float maximum = theValues[i - DX][j - DY];

  for (int dy = -DY; dy <= DY; dy++)
    for (int dx = -DX; dx <= DX; dx++)
    {
      // quick exit for missing values
      if (theValues[i + dx][j + dy] == kFloatMissing)
        return 0;

      if (dx != 0 && dy != 0)
      {
        if (theValues[i + dx][j + dy] < theValues[i][j])
          ++smaller;
        else if (theValues[i + dx][j + dy] > theValues[i][j])
          ++bigger;
      }

      // quick exit for non-extrema
      if (smaller > 0 && bigger > 0)
        return 0;

      // update extrema values

      if (dx == -DX || dx == DX || dy == -DY || dy == DY)
      {
        minimum = min(minimum, theValues[i + dx][j + dy]);
        maximum = max(maximum, theValues[i + dx][j + dy]);
      }
    }

  // minimum change from center to rim

  float change = min(abs(theValues[i][j] - minimum), abs(theValues[i][j] - maximum));

#if 0
  if(change >= mingradient)
	cout << "Change = " << change << " at " << i << ' ' << j << endl;
#endif

  if (change < mingradient)
    return 0;

  else if (smaller == (DX * 2 + 1) * (DY * 2 + 1) - 1)
    return 2;
  else if (bigger == (DX * 2 + 1) * (DY * 2 + 1) - 1)
    return -2;
  else if (smaller > 0)
    return 1;
  else if (bigger > 0)
    return -1;
  else
    return 0;  // should never happen due to quick exit in the loop
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw high/low pressure markers
 */
// ----------------------------------------------------------------------

void draw_pressure_markers(ImagineXr_or_NFmiImage &img, const NFmiArea &theArea)
{
  // Establish which markers are to be drawn

  bool dohigh = !globals.highpressureimage.empty();
  bool dolow = !globals.lowpressureimage.empty();

  // Exit if none

  if (!dohigh && !dolow)
    return;

  // Get the data to be analyzed

  choose_queryinfo("Pressure", 0);

  auto worldpts = globals.queryinfo->LocationsWorldXY(theArea);

  auto vals = globals.queryinfo->Values();
  globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()), vals);

  // Insert candidate coordinates into the system

  // const double dx = theArea.WorldXYWidth()/theArea.Grid()->XNumber();
  // const double dy = theArea.WorldXYHeight()/theArea.Grid()->YNumber();

  const int DX = 7;  // ceil(500*1000/dx/2);	// 500km radius required
  const int DY = 7;  // ceil(500*1000/dx/2);
  const float required_gradient = 1.0;

  for (unsigned int j = DY; j < vals.NY() - DY; j++)
    for (unsigned int i = DX; i < vals.NX() - DX; i++)
    {
      int extrem = extrematype(vals, i, j, DX, DY, required_gradient);
      if (extrem != 0)
      {
        // the point in kilometer units
        NFmiPoint point(worldpts->x(i, j) / 1000, worldpts->y(i, j) / 1000);

        if (extrem < 0)
        {
          if (dolow)
            globals.pressurelocator.add(ExtremaLocator::Minimum, point.X(), point.Y());
        }
        else
        {
          if (dohigh)
            globals.pressurelocator.add(ExtremaLocator::Maximum, point.X(), point.Y());
        }
      }
    }

  // Now choose the marker positions and draw them

  const ExtremaLocator::ExtremaCoordinates &extrema = globals.pressurelocator.chooseCoordinates();

  NFmiColorTools::NFmiBlendRule lowrule = ColorTools::checkrule(globals.lowpressurerule);
  NFmiColorTools::NFmiBlendRule highrule = ColorTools::checkrule(globals.highpressurerule);

  for (ExtremaLocator::ExtremaCoordinates::const_iterator eit = extrema.begin();
       eit != extrema.end();
       ++eit)
  {
    for (ExtremaLocator::Coordinates::const_iterator it = eit->second.begin();
         it != eit->second.end();
         ++it)
    {
      NFmiPoint wxy(it->first * 1000, it->second * 1000);
      NFmiPoint latlon = theArea.WorldXYToLatLon(wxy);
      /// latlon = MeridianTools::Relocate(latlon,theArea);
      NFmiPoint xy = theArea.ToXY(latlon);

      switch (eit->first)
      {
        case ExtremaLocator::Minimum:
          img.Composite(globals.lowpressureimage,
                        lowrule,
                        kFmiAlignCenter,
                        static_cast<int>(round(xy.X())),
                        static_cast<int>(round(xy.Y())),
                        globals.lowpressurefactor);
          break;
        case ExtremaLocator::Maximum:
          img.Composite(globals.highpressureimage,
                        highrule,
                        kFmiAlignCenter,
                        static_cast<int>(round(xy.X())),
                        static_cast<int>(round(xy.Y())),
                        globals.highpressurefactor);
          break;
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the foreground onto the image
 */
// ----------------------------------------------------------------------

void draw_foreground(ImagineXr_or_NFmiImage &img)
{
  if (globals.foreground.empty())
    return;

  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(globals.foregroundrule);

  img.Composite(globals.getImage(globals.foreground), rule, kFmiAlignNorthWest, 0, 0, 1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw contours" command
 */
// ----------------------------------------------------------------------

void do_draw_contours(istream &theInput)
{
  // 1. Make sure query data has been read
  // 2. Make sure image has been initialized
  // 3. Loop over all times
  //   4. If the time is acceptable,
  //   5. Loop over all parameters
  //     6. Fill all specified intervals
  //     7. Patternfill all specified intervals
  //     8. Stroke all specified contours
  //   9. Overwrite with foreground if so desired
  //   10. Loop over all parameters
  //     11. Label all specified points
  //   12. Draw arrows if requested
  //   13. Save the image

  globals.labellocator.clear();
  globals.pressurelocator.clear();
  globals.symbollocator.clear();
  globals.imagelocator.clear();

  if (globals.querystreams.empty())
    throw runtime_error("No query data has been read!");

  auto area = globals.createArea();

  // This message intentionally ignores globals.verbose

  if (!globals.background.empty())
    cout << "Contouring for background " << globals.background << endl;

  if (globals.verbose)
    report_area(*area);

  // Establish querydata timelimits and initialize
  // the XY-coordinates simultaneously.

  // Note that we use world-coordinates when smoothing
  // so that we can use meters as the smoothing radius.
  // Also, this means the contours are independent of
  // the image size.

  NFmiTime time1, time2;

  NFmiDataMatrix<float> vals;
  NFmiDataMatrix<float> maskvalues;

  unsigned int qi;
  for (qi = 0; qi < globals.querystreams.size(); qi++)
  {
    // Establish time limits

    globals.queryinfo = globals.querystreams[qi];

    globals.queryinfo->LastTime();
    NFmiTime t2 = globals.queryinfo->ValidTime();

    globals.queryinfo->FirstTime();
    NFmiTime t1 = globals.queryinfo->ValidTime();

    if (qi == 0)
    {
      time1 = t1;
      time2 = t2;
    }
    else
    {
      if (time1.IsLessThan(t1))
        time1 = t1;
      if (!time2.IsLessThan(t2))
        time2 = t2;
    }
  }

  if (globals.verbose)
  {
    cout << "Data start time " << time1 << endl << "Data end time " << time2 << endl;
  }

  // Skip to first time

  NFmiMetTime tmptime(time1,
                      globals.timesteprounding ? (globals.timestep > 0 ? globals.timestep : 1) : 1);

  tmptime.ChangeByMinutes(globals.timestepskip);
  if (globals.timesteprounding)
    tmptime.PreviousMetTime();
  NFmiTime t = tmptime;

  // Loop over all times

  int imagesdone = 0;
  bool labeldxdydone = false;
  for (;;)
  {
    if (imagesdone >= globals.timesteps)
      break;

    // Skip to next time to be drawn

    t.ChangeByMinutes(globals.timestep > 0 ? globals.timestep : 1);

    // If the time is after time2, we're done

    if (time2.IsLessThan(t))
      break;

    // Search first time >= the desired time
    // This is quaranteed to succeed since we've
    // already tested against time2, the last available
    // time.

    bool ok = true;
    for (qi = 0; ok && qi < globals.querystreams.size(); qi++)
    {
      globals.queryinfo = globals.querystreams[qi];
      globals.queryinfo->ResetTime();
      while (globals.queryinfo->NextTime())
      {
        NFmiTime loc = globals.queryinfo->ValidTime();
        if (!loc.IsLessThan(t))
          break;
      }
      NFmiTime tnow = globals.queryinfo->ValidTime();

      // we wanted

      if (globals.timestep == 0)
        t = tnow;

      // If time is before time1, ignore it

      if (t.IsLessThan(time1))
      {
        ok = false;
        break;
      }

      // Is the time exact?

      bool isexact = t.IsEqual(tnow);

      // The previous acceptable time step in calculations
      // Use NFmiTime, not NFmiMetTime to avoid rounding up!

      NFmiTime tprev = t;
      tprev.ChangeByMinutes(-globals.timeinterval);

      bool hasprevious = !tprev.IsLessThan(time1);

      // Skip this image if we are unable to render it

      if (globals.filter == "none")
      {
        // Cannot draw time with filter none
        // if time is not exact.

        ok = isexact;
      }
      else if (globals.filter == "linear")
      {
        // OK if is exact, otherwise previous step required

        ok = !(!isexact && !hasprevious);
      }
      else
      {
        // Time must be exact, and previous steps
        // are required

        ok = !(!isexact || !hasprevious);
      }
    }

    if (!ok)
      continue;

    // The image is accepted for rendering, but
    // we might not overwrite an existing one.
    // Hence we update the counter here already.

    imagesdone++;

    // Create the filename

    // The timestamp as a string

    NFmiString datatimestr = t.ToStr(globals.timestampformat);

    if (globals.verbose)
      cout << "Time is " << datatimestr.CharPtr() << endl;

    string filename = globals.savepath + "/" + globals.prefix + datatimestr.CharPtr();

    if (globals.timestampflag)
    {
      for (qi = 0; qi < globals.queryfilenames.size(); qi++)
      {
        time_t secs = NFmiFileSystem::FileModificationTime(globals.queryfilenames[qi]);
        NFmiTime tstamp = TimeTools::ToUTC(secs);
        filename += "_" + tstamp.ToStr(globals.timestampformat);
      }
    }

    filename += globals.suffix + "." + globals.format;

    // In force-mode we always write, but otherwise
    // we first check if the output image already
    // exists. If so, we assume it is up to date
    // and skip to the next time stamp.

    if (!globals.force && !NFmiFileSystem::FileEmpty(filename))
    {
      if (globals.verbose)
        cout << "Not overwriting " << filename << endl;
      continue;
    }

    // Initialize the background

    int imgwidth = static_cast<int>(area->Width() + 0.5);
    int imgheight = static_cast<int>(area->Height() + 0.5);

    NFmiColorTools::Color erasecolor = ColorTools::checkcolor(globals.erase);

#ifdef IMAGINE_WITH_CAIRO
    ImagineXr *xr = new ImagineXr(imgwidth, imgheight, filename, globals.format);

    if (globals.background.empty())
    {
      xr->Erase(erasecolor);
    }
    else
    {
      const ImagineXr &xr2 = globals.getImage(globals.background);

      if ((xr2.Width() != xr->Width()) || (xr2.Height() != xr->Height()))
        throw runtime_error("Background image size does not match area size");

      xr->Composite(xr2);
    }
#else
    std::shared_ptr<Imagine::NFmiImage> image;
    if (globals.background.empty())
    {
      image.reset(new Imagine::NFmiImage(imgwidth, imgheight, erasecolor));
    }
    else
    {
      image.reset(new Imagine::NFmiImage(globals.getImage(globals.background)));
      if (imgwidth != image->Width() || imgheight != image->Height())
      {
        throw runtime_error("Background image size does not match area size");
      }
    }
    if (image.get() == 0)
      throw runtime_error("Failed to allocate a new image for rendering");

    globals.setImageModes(*image);
#define xr image /* HACK */
#endif

    // Initialize label locator bounding box

    globals.labellocator.boundingBox(globals.contourlabelimagexmargin,
                                     globals.contourlabelimageymargin,
                                     xr->Width() - globals.contourlabelimagexmargin,
                                     xr->Height() - globals.contourlabelimageymargin);

    // Initialize symbol locator bounding box with reasonably safety
    // for large symbols

    globals.symbollocator.boundingBox(-30, -30, xr->Width() + 30, xr->Height() + 30);
    globals.imagelocator.boundingBox(-30, -30, xr->Width() + 30, xr->Height() + 30);

    // Loop over all parameters
    // The loop collects all contour label information, but
    // does not render it yet

    list<ContourSpec>::iterator piter;
    list<ContourSpec>::iterator pbegin = globals.specs.begin();
    list<ContourSpec>::iterator pend = globals.specs.end();

    for (piter = pbegin; piter != pend; ++piter)
    {
      // Establish the parameter

      string name = piter->param();
      int level = piter->level();

      qi = choose_queryinfo(name, level);

      if (globals.verbose)
        report_queryinfo(name, qi);

      // Establish the contour method

      string interpname = piter->contourInterpolation();
      ContourInterpolation interp = ContourInterpolationValue(interpname);
      if (interp == Missing)
        throw runtime_error("Unknown contour interpolation method " + interpname);

      // Get the values.

      if (!MetaFunctions::isMeta(name))
      {
        vals = globals.queryinfo->Values();
        globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()), vals);
      }
      else
        vals = MetaFunctions::values(piter->param(), *globals.queryinfo);

      // Replace values if so requested

      if (piter->replace())
        vals.Replace(piter->replaceSourceValue(), piter->replaceTargetValue());

      // Filter the values if so requested

      filter_values(vals, t, *piter);

      // Expand the data if so requested

      if (globals.expanddata)
        expand_data(vals);

      // Call smoother only if necessary to avoid LazyCoordinates dereferencing

      LazyCoordinates worldpts(*area);

      if (piter->smoother() != "None")
      {
        NFmiSmoother smoother(piter->smoother(), piter->smootherFactor(), piter->smootherRadius());

        vals = smoother.Smoothen(*worldpts, vals);
      }

      // Setup the contourer with the values

      globals.calculator.data(vals);

      // Save the data values at desired points for later
      // use, this lets us avoid using InterpolatedValue()
      // which does not use smoothened values.

      // First, however, if this is the first image, we add
      // the grid points to the set of points, if so requested

      if (!labeldxdydone)
        add_label_grid_values(*piter, *area, worldpts);

      // For pixelgrids we must repeat the process for all new
      // background images, since the pixel spacing changes
      // every time. Note! We assume the following calling order!

      add_label_point_values(*piter, *area, vals);
      add_label_pixelgrid_values(*piter, *area, *xr, vals);

      // Fill the contours

      draw_contour_fills(*xr, *area, *piter, t, interp);

      // Pattern fill the contours

      draw_contour_patterns(*xr, *area, *piter, t, interp);

      // Stroke the contours

      draw_contour_strokes(*xr, *area, *piter, t, interp);

      // Save contour symbol coordinates

      save_contour_symbols(*xr, *area, *piter, worldpts, vals);

      // Save symbol fill coordinates

      save_contour_fonts(*xr, *area, *piter, worldpts, vals);

      // Save contour label coordinates

      save_contour_labels(*xr, *area, *piter, t, interp);

      // Draw optional overlay

      draw_overlay(*xr, *piter);
    }

    // Draw graticule

    draw_graticule(*xr, *area);

    // Bang the foreground

    draw_foreground(*xr);

    // Draw wind arrows if so requested

    draw_wind_arrows(*xr, *area);

    // Draw contour symbols

    draw_contour_symbols(*xr);

    // Draw contour fonts

    draw_contour_fonts(*xr);

    // Label the contours

    draw_contour_labels(*xr);

    // Draw labels

    for (piter = pbegin; piter != pend; ++piter)
    {
      draw_label_markers(*xr, *piter, *area);
      draw_label_texts(*xr, *piter, *area);
    }

    // Draw high/low pressure markers

    draw_pressure_markers(*xr, *area);

    // Bang the combine image (legend, logo, whatever)

    globals.drawCombine(*xr);

    // Finally, draw a time stamp on the image if so
    // requested

    const string stamp = globals.getImageStampText(t);
    globals.drawImageStampText(*xr, stamp);

    // dx and dy labels have now been extracted into a list,
    // disable adding them again and again and again..

    labeldxdydone = true;

    // Save

#ifdef IMAGINE_WITH_CAIRO
    assert(xr->Filename() != "");
    write_image(*xr);
    delete xr;
#else
    write_image(*image, filename, globals.format);
#endif

    // Advance in time

    globals.labellocator.nextTime();
    globals.pressurelocator.nextTime();
    globals.symbollocator.nextTime();
    globals.imagelocator.nextTime();
  }
}

/****/
static void process_cmd(const string &text)
{
  istringstream in(text);
  string cmd;
  while (in >> cmd)
  {
    // Handle comments

    if (cmd == "#")
      do_comment(in);
    else if (cmd[0] == '#')
      do_comment(in);
    else if (cmd == "//")
      do_comment(in);
    else if (cmd == "cache")
      do_cache(in);
    else if (cmd == "imagecache")
      do_imagecache(in);
    else if (cmd == "querydata")
      do_querydata(in);
    else if (cmd == "filter")
      do_filter(in);
    else if (cmd == "timestepskip")
      do_timestepskip(in);
    else if (cmd == "timestep")
      do_timestep(in);
    else if (cmd == "timeinterval")
      do_timeinterval(in);
    else if (cmd == "timesteps")
      do_timesteps(in);
    else if (cmd == "timestamp")
      do_timestamp(in);
    else if (cmd == "timestampzone")
      do_timestampzone(in);
    else if (cmd == "timesteprounding")
      do_timesteprounding(in);
    else if (cmd == "timestampimage")
      do_timestampimage(in);
    else if (cmd == "timestampimagexy")
      do_timestampimagexy(in);
    else if (cmd == "timestampimageformat")
      do_timestampimageformat(in);
    else if (cmd == "timestampimagefont")
      do_timestampimagefont(in);
    else if (cmd == "timestampimagecolor")
      do_timestampimagecolor(in);
    else if (cmd == "timestampimagebackground")
      do_timestampimagebackground(in);
    else if (cmd == "timestampimagemargin")
      do_timestampimagemargin(in);
    else if (cmd == "timestampformat")
      do_timestampformat(in);
    else if (cmd == "projection")
      do_projection(in);
    else if (cmd == "erase")
      do_erase(in);
    else if (cmd == "fillrule")
      do_fillrule(in);
    else if (cmd == "strokerule")
      do_strokerule(in);
    else if (cmd == "directionparam")
      do_directionparam(in);
    else if (cmd == "speedparam")
      do_speedparam(in);
    else if (cmd == "speedcomponents")
      do_speedcomponents(in);
    else if (cmd == "arrowscale")
      do_arrowscale(in);
    else if (cmd == "windarrowscale")
      do_windarrowscale(in);
    else if (cmd == "arrowfill")
      do_arrowfill(in);
    else if (cmd == "arrowstroke")
      do_arrowstroke(in);
    else if (cmd == "arrowpath")
      do_arrowpath(in);
    else if (cmd == "roundarrowfill")
      do_roundarrowfill(in);
    else if (cmd == "roundarrowstroke")
      do_roundarrowstroke(in);
    else if (cmd == "roundarrowsize")
      do_roundarrowsize(in);
    else if (cmd == "windarrow")
      do_windarrow(in);
    else if (cmd == "windarrows")
      do_windarrows(in);
    else if (cmd == "windarrowsxy")
      do_windarrowsxy(in);
    else if (cmd == "background")
      do_background(in);
    else if (cmd == "foreground")
      do_foreground(in);
    else if (cmd == "mask")
      do_mask(in);
    else if (cmd == "overlay")
      do_overlay(in);
    else if (cmd == "combine")
      do_combine(in);
    else if (cmd == "foregroundrule")
      do_foregroundrule(in);
    else if (cmd == "savepath")
      do_savepath(in);
    else if (cmd == "prefix")
      do_prefix(in);
    else if (cmd == "suffix")
      do_suffix(in);
    else if (cmd == "format")
      do_format(in);
    else if (cmd == "graticule")
      do_graticule(in);
#if 0  // def IMAGINE_WITH_CAIRO
    // AKa 22-Aug-2008: Extension conf for Cairo
    //
      else if(cmd == "antialias")				do_antialias(in);
#endif
    else if (cmd == "gamma")
      do_gamma(in);
    else if (cmd == "intent")
      do_intent(in);
    else if (cmd == "pngquality")
      do_pngquality(in);
    else if (cmd == "jpegquality")
      do_jpegquality(in);
    else if (cmd == "savealpha")
      do_savealpha(in);
    else if (cmd == "reducecolors")
      do_reducecolors(in);
    else if (cmd == "wantpalette")
      do_wantpalette(in);
    else if (cmd == "forcepalette")
      do_forcepalette(in);
    else if (cmd == "alphalimit")
      do_alphalimit(in);
    else if (cmd == "hilimit")
      do_hilimit(in);
    else if (cmd == "datalolimit")
      do_datalolimit(in);
    else if (cmd == "datahilimit")
      do_datahilimit(in);
    else if (cmd == "datareplace")
      do_datareplace(in);
    else if (cmd == "despeckle")
      do_despeckle(in);
    else if (cmd == "expanddata")
      do_expanddata(in);
    else if (cmd == "contourdepth")
      do_contourdepth(in);
    else if (cmd == "contourinterpolation")
      do_contourinterpolation(in);
    else if (cmd == "contourtriangles")
      do_contourtriangles(in);
    else if (cmd == "smoother")
      do_smoother(in);
    else if (cmd == "smootherradius")
      do_smootherradius(in);
    else if (cmd == "smootherfactor")
      do_smootherfactor(in);
    else if (cmd == "level")
      do_level(in);
    else if (cmd == "param")
      do_param(in);
    else if (cmd == "shape")
      do_shape(in);
    else if (cmd == "contourfill")
      do_contourfill(in);
    else if (cmd == "contourpattern")
      do_contourpattern(in);
    else if (cmd == "contoursymbol")
      do_contoursymbol(in);
    else if (cmd == "contoursymbolmindist")
      do_contoursymbolmindist(in);
    else if (cmd == "contourfont")
      do_contourfont(in);
    else if (cmd == "contourlinewidth")
      do_contourlinewidth(in);
    else if (cmd == "contourline")
      do_contourline(in);
    else if (cmd == "contourfills")
      do_contourfills(in);
    else if (cmd == "contourlines")
      do_contourlines(in);

    else if (cmd == "contourlabel")
      do_contourlabel(in);
    else if (cmd == "contourlabels")
      do_contourlabels(in);
    else if (cmd == "contourlabeltext")
      do_contourlabeltext(in);
    else if (cmd == "contourlabelfont")
      do_contourlabelfont(in);
    else if (cmd == "contourlabelcolor")
      do_contourlabelcolor(in);
    else if (cmd == "contourlabelbackground")
      do_contourlabelbackground(in);
    else if (cmd == "contourlabelmargin")
      do_contourlabelmargin(in);
    else if (cmd == "contourlabelimagemargin")
      do_contourlabelimagemargin(in);
    else if (cmd == "contourlabelmindistsamevalue")
      do_contourlabelmindistsamevalue(in);
    else if (cmd == "contourlabelmindistdifferentvalue")
      do_contourlabelmindistdifferentvalue(in);
    else if (cmd == "contourlabelmindistdifferentparam")
      do_contourlabelmindistdifferentparam(in);
    else if (cmd == "contourfontmindistsamevalue")
      do_contourfontmindistsamevalue(in);
    else if (cmd == "contourfontmindistdifferentvalue")
      do_contourfontmindistdifferentvalue(in);
    else if (cmd == "contourfontmindistdifferentparam")
      do_contourfontmindistdifferentparam(in);

    else if (cmd == "highpressure")
      do_highpressure(in);
    else if (cmd == "lowpressure")
      do_lowpressure(in);
    else if (cmd == "lowpressuremaximum")
      do_lowpressuremaximum(in);
    else if (cmd == "highpressureminimum")
      do_highpressureminimum(in);
    else if (cmd == "pressuremindistsame")
      do_pressuremindistsame(in);
    else if (cmd == "pressuremindistdifferent")
      do_pressuremindistdifferent(in);
    else if (cmd == "labelmarker")
      do_labelmarker(in);
    else if (cmd == "labelfont")
      do_labelfont(in);
    else if (cmd == "labelcolor")
      do_labelcolor(in);
    else if (cmd == "labelrule")
      do_labelrule(in);
    else if (cmd == "labelalign")
      do_labelalign(in);
    else if (cmd == "labelformat")
      do_labelformat(in);
    else if (cmd == "labelmissing")
      do_labelmissing(in);
    else if (cmd == "labeloffset")
      do_labeloffset(in);
    else if (cmd == "labelcaption")
      do_labelcaption(in);
    else if (cmd == "label")
      do_label(in);
    else if (cmd == "labelxy")
      do_labelxy(in);
    else if (cmd == "labels")
      do_labels(in);
    else if (cmd == "labelsxy")
      do_labelsxy(in);
    else if (cmd == "labelfile")
      do_labelfile(in);
    else if (cmd == "units")
      do_units(in);
    else if (cmd == "clear")
      do_clear(in);

    else if (cmd == "draw")
    {
      in >> cmd;

      if (cmd == "shapes")
        do_draw_shapes(in);
      else if (cmd == "imagemap")
        do_draw_imagemap(in);
      else if (cmd == "contours")
        do_draw_contours(in);
      else
        throw runtime_error("draw " + cmd + " not implemented");
    }
    else
      throw runtime_error("Unknown command " + cmd);
  }
}

// ----------------------------------------------------------------------
// Main program.
// ----------------------------------------------------------------------

int domain(int argc, const char *argv[])
{
  // Initialize configuration variables

  NFmiSettings::Init();

  // Parse command line

  parse_command_line(argc, argv);

  // Handle command line config text; if any
  //
  if (!globals.cmdline_conf.empty())
  {
    process_cmd(globals.cmdline_conf);
  }

  // Process all command files
  // ~~~~~~~~~~~~~~~~~~~~~~~~~

  list<string>::const_iterator fileiter = globals.cmdline_files.begin();
  for (; fileiter != globals.cmdline_files.end(); ++fileiter)
  {
    // Get the script to be executed

    if (globals.verbose)
      cout << "Processing file: " << *fileiter << endl;

    string text = read_script(*fileiter);
    text = preprocess_script(text);

    // Process the commands

    process_cmd(text);
  }
  return 0;
}

// ----------------------------------------------------------------------
// Main program.
// ----------------------------------------------------------------------

int main(int argc, const char *argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (const runtime_error &e)
  {
    cerr << "Error: qdcontour failed due to" << endl << "--> " << e.what() << endl;
    return 1;
  }
}

// ======================================================================
