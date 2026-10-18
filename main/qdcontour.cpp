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
#include <boost/lexical_cast.hpp>
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
#include <cmath>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>  // environ

#include <imagine/NFmiFace.h>
#include <imagine/NFmiFreeType.h>
#include <imagine/NFmiImage.h>

using namespace std;
using namespace boost;
using namespace Imagine;

const float pi = 3.141592658979323F;

// ----------------------------------------------------------------------
// Convert parameter description to a enum
// ----------------------------------------------------------------------

namespace
{
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

void Usage()
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

  const Imagine::NFmiImage& mask = globals.getImage(theMask);

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
    // NOLINTNEXTLINE(concurrency-mt-unsafe): single-threaded command line tool
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
    globals.cmdline_files.emplace_back(cmdline.Parameter(i));
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Read the given configuration script
 *
 * \param theName The file to read
 * \return The contents of the file, preprocessed
 */
// ----------------------------------------------------------------------

namespace
{
string read_script(const string &theName)
{
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
}  // namespace

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

namespace
{
string preprocess_script(const string &theScript)
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
  for (theInfo.ResetLevel(); theInfo.NextLevel();)
    if (theInfo.Level()->LevelValue() == static_cast<unsigned int>(theLevel))
      return true;
  return false;
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

// ----------------------------------------------------------------------
/*!
 * \brief Write image to file with desired format
 */
// ----------------------------------------------------------------------

void write_image(NFmiImage &theImage, const string &theName, const string &theFormat)
{
  if (globals.verbose)
    cout << "Writing '" << theName << "'" << endl;

  if (globals.reducecolors)
    theImage.ReduceColors();

  theImage.Write(theName, theFormat);

  if (!globals.itsImageCacheOn)
    globals.itsImageCache.clear();
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a face from a font specification string
 *
 * The string is of the form <fontname>:<width>x<height>
 * If width or height is zero, Freetype will calculate it
 * so that proper aspect ratio is preserved.
 */
// ----------------------------------------------------------------------

Imagine::NFmiFace make_face(const string &theSpec)
{
  return NFmiFace(theSpec);
}

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
  int flag = 0;
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
  int flag = 0;
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
 * \brief handle "uvorientation" command
 */
// ----------------------------------------------------------------------

void do_uvorientation(istream &theInput)
{
  theInput >> globals.uvorientation;

  check_errors(theInput, "uvorientation");
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
  string token1;
  string token2;

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
    string scolor;
    string srule;
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
  string token1;
  string token2;

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
    string scolor;
    string srule;
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
  string slo;
  string shi;
  string scircle;
  string striangle;
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
  string slo;
  string shi;
  string scircle;
  string striangle;
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

  string slo;
  string shi;
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
  double lon = NAN;
  double lat = NAN;
  theInput >> lon >> lat;

  check_errors(theInput, "windarrow");

  globals.arrowpoints.emplace_back(lon, lat);
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
  string paramname;
  string imgname;

  theInput >> paramname >> imgname;

  if (imgname == "-" || imgname == "none")
    imgname = "";

  check_errors(theInput, "overlay");

  for (ContourSpec &spec : globals.specs)
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
  float limit = NAN;
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
  float limit = NAN;
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
  float limit = NAN;
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
  float src = NAN;
  float dst = NAN;
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
  string slo;
  string shi;
  int radius = 0;
  int iterations = 0;
  float weight = NAN;

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

void do_contourdepth(istream& /*theInput*/)
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
  string shapename;
  string arg1;

  theInput >> shapename >> arg1;

  check_errors(theInput, "shape");

  if (arg1 == "mark")
  {
    string marker;
    string markerrule;
    float markeralpha = NAN;
    theInput >> marker >> markerrule >> markeralpha;

    ColorTools::checkrule(markerrule);
    ShapeSpec spec(shapename);
    spec.marker(marker, markerrule, markeralpha);
    globals.shapespecs.push_back(spec);
  }
  else
  {
    const string& fillcolor = arg1;
    string strokecolor;
    theInput >> strokecolor;
    NFmiColorTools::Color fill = ColorTools::checkcolor(fillcolor);
    NFmiColorTools::Color stroke = ColorTools::checkcolor(strokecolor);

    globals.shapespecs.emplace_back(shapename, fill, stroke, globals.fillrule, globals.strokerule);
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
  string slo;
  string shi;
  string scolor;
  theInput >> slo >> shi >> scolor;

  check_errors(theInput, "contourfill");

  float lo = NAN;
  float hi = NAN;
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
  string slo;
  string shi;
  string spattern;
  string srule;
  float alpha = NAN;
  theInput >> slo >> shi >> spattern >> srule >> alpha;

  check_errors(theInput, "contourpattern");

  float lo = NAN;
  float hi = NAN;
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
  string slo;
  string shi;
  string spattern;
  string srule;
  float alpha = NAN;
  theInput >> slo >> shi >> spattern >> srule >> alpha;

  check_errors(theInput, "contoursymbol");

  float lo = NAN;
  float hi = NAN;
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
  float value = NAN;
  int symbol = 0;
  string scolor;
  string font;
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
  string svalue;
  string scolor;
  theInput >> svalue >> scolor;

  check_errors(theInput, "contourline");

  float value = NAN;
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
  float lo = NAN;
  float hi = NAN;
  float step = NAN;
  string scolor1;
  string scolor2;
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
      color = NFmiColorTools::Interpolate(color1, color2, i / (steps - 1.0F));
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
  float lo = NAN;
  float hi = NAN;
  float step = NAN;
  string scolor1;
  string scolor2;
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
  float value = NAN;
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
  float lo = NAN;
  float hi = NAN;
  float step = NAN;
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
  string value;
  string text;
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
  int dx = 0;
  int dy = 0;

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
  float dist = NAN;
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
  float dist = NAN;
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
  float dist = NAN;

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
  float dist = NAN;
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
  float dist = NAN;
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
  float dist = NAN;

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
  float dist = NAN;

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
  float dist = NAN;
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
  float dist = NAN;
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
  string filename;
  string rule;
  float alpha = NAN;

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
  float dx = NAN;
  float dy = NAN;
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
  string name;
  string align;
  float dx = NAN;
  float dy = NAN;
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
  float lon = NAN;
  float lat = NAN;
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
  float lon = NAN;
  float lat = NAN;
  int dx = 0;
  int dy = 0;
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
  float dx = NAN;
  float dy = NAN;
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
  float x0 = NAN;
  float y0 = NAN;
  float dx = NAN;
  float dy = NAN;
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
      float lon = NAN;
      float lat = NAN;
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
  string paramname;
  string conversion;

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

  int imgwidth = static_cast<int>(std::lround(area->Width()));
  int imgheight = static_cast<int>(std::lround(area->Height()));

  // Initialize the background
  NFmiImage image(imgwidth, imgheight);
  globals.setImageModes(image);
  image.Erase(ColorTools::checkcolor(globals.erase));

  // Draw all the shapes

  list<ShapeSpec>::const_iterator iter;
  auto begin = globals.shapespecs.begin();
  auto end = globals.shapespecs.end();

  for (iter = begin; iter != end; ++iter)
  {
    NFmiGeoShape geo(iter->filename(), kFmiGeoShapeEsri);
    geo.ProjectXY(*area);

    if (iter->marker().empty())
    {
      NFmiColorTools::NFmiBlendRule fillrule = ColorTools::checkrule(iter->fillrule());
      NFmiColorTools::NFmiBlendRule strokerule = ColorTools::checkrule(iter->strokerule());

      geo.Fill(image, iter->fillcolor(), fillrule);
      geo.Stroke(image, iter->strokecolor(), strokerule);
    }
    else
    {
      NFmiColorTools::NFmiBlendRule markerrule = ColorTools::checkrule(iter->markerrule());

      const Imagine::NFmiImage& marker = globals.getImage(iter->marker());
      geo.Mark(image, marker, markerrule, kFmiAlignCenter, iter->markeralpha());
    }
  }

  write_image(image, filename + '.' + globals.format, globals.format);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw imagemap" command
 */
// ----------------------------------------------------------------------

void do_draw_imagemap(istream &theInput)
{
  // The relevant field name and filenames

  string fieldname;
  string filename;
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
  auto begin = globals.shapespecs.begin();
  auto end = globals.shapespecs.end();

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

void draw_graticule(Imagine::NFmiImage& img, const NFmiArea& theArea)
{
  if (globals.graticulecolor.empty())
    return;

  NFmiPath path;

  for (int nlon = 0; globals.graticulelon1 + nlon * globals.graticuledx <= globals.graticulelon2;
       nlon++)
  {
    const double lon = globals.graticulelon1 + nlon * globals.graticuledx;
    path.MoveTo(lon, globals.graticulelat1);
    for (int nlat = 0; globals.graticulelat1 + globals.graticuledy + nlat <= globals.graticulelat2;
         nlat++)
      path.LineTo(lon, globals.graticulelat1 + globals.graticuledy + nlat);
  }

  for (int nlat = 0; globals.graticulelat1 + nlat * globals.graticuledy <= globals.graticulelat2;
       nlat++)
  {
    const double lat = globals.graticulelat1 + nlat * globals.graticuledy;
    path.MoveTo(globals.graticulelon1, lat);
    for (int nlon = 0; globals.graticulelon1 + globals.graticuledx + nlon <= globals.graticulelon2;
         nlon++)
      path.LineTo(globals.graticulelon1 + globals.graticuledx + nlon, lat);
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
  return toparam(theParam);
}

// ----------------------------------------------------------------------
/*!
 * \brief Choose the queryinfo from the set of available datas
 */
// ----------------------------------------------------------------------

unsigned int choose_queryinfo(const string &theName, int theLevel)
{
  if (globals.querystreams.empty())
    throw runtime_error("No querydata has been specified");

  if (MetaFunctions::isMeta(theName))
  {
    globals.queryinfo = globals.querystreams[0];
    return 0;
  }
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
  throw runtime_error("Parameter '" + theName + "' on level " +
                      NFmiStringTools::Convert(theLevel) + " is not available in the query files");
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
 * \brief Interpolate the data values linearly in time
 */
// ----------------------------------------------------------------------

void filter_values_linear(NFmiDataMatrix<float> &theValues,
                          const NFmiTime &theTime,
                          const ContourSpec &theSpec)
{
  // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
  NFmiTime tnow = globals.queryinfo->ValidTime();
  bool isexact = theTime.IsEqual(tnow);

  if (!isexact)
  {
    NFmiDataMatrix<float> tmpvals;
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
    NFmiTime t2 = globals.queryinfo->ValidTime();
    globals.queryinfo->PreviousTime();
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
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

// ----------------------------------------------------------------------
/*!
 * \brief Aggregate the data values over the time interval (min, max, mean or sum)
 */
// ----------------------------------------------------------------------

void filter_values_aggregate(NFmiDataMatrix<float> &theValues,
                             const NFmiTime &theTime,
                             const ContourSpec &theSpec)
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
    filter_values_linear(theValues, theTime, theSpec);
  }
  else
  {
    filter_values_aggregate(theValues, theTime, theSpec);
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
      for (int ny = 0; ny * dy <= thePoints.NY() - 1; ny++)
      {
        const float y = ny * dy;
        const int j = static_cast<int>(floor(y));
        const float dj = y - j;
        for (int nx = 0; nx * dx <= thePoints.NX() - 1; nx++)
        {
          const float x = nx * dx;
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

void add_label_pixelgrid_values(ContourSpec& theSpec,
                                const NFmiArea& theArea,
                                const Imagine::NFmiImage& img,
                                const NFmiDataMatrix<float>& theValues)
{
  theSpec.clearPixelLabels();

  const float x0 = theSpec.labelXyX0();
  const float y0 = theSpec.labelXyY0();
  const float dx = theSpec.labelXyDX();
  const float dy = theSpec.labelXyDY();

  if (dx > 0 && dy > 0)
  {
    for (int ny = 0; y0 + ny * dy <= img.Height(); ny++)
    {
      const float y = y0 + ny * dy;
      for (int nx = 0; x0 + nx * dx <= img.Width(); nx++)
      {
        const float x = x0 + nx * dx;
        NFmiPoint latlon = theArea.ToLatLon(NFmiPoint(x, y));
        NFmiPoint ij = globals.queryinfo->LatLonToGrid(latlon);

        int i = static_cast<int>(ij.X());  // rounds down
        int j = static_cast<int>(ij.Y());
        auto value = static_cast<float>(
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
}

// ----------------------------------------------------------------------
/*!
 * \brief Save point values for later labelling
 */
// ----------------------------------------------------------------------

void add_label_point_values(ContourSpec& theSpec,
                            const NFmiArea& /*theArea*/,
                            const NFmiDataMatrix<float>& theValues)
{
  theSpec.clearLabelValues();
  if ((!theSpec.labelFormat().empty()) && !theSpec.labelPoints().empty())
  {
    list<pair<NFmiPoint, NFmiPoint>>::const_iterator it;

    for (it = theSpec.labelPoints().begin(); it != theSpec.labelPoints().end(); ++it)
    {
      NFmiPoint latlon = it->first;
      NFmiPoint ij = globals.queryinfo->LatLonToGrid(latlon);

      int i = static_cast<int>(ij.X());  // rounds down
      int j = static_cast<int>(ij.Y());
      auto value =
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

void draw_label_markers(Imagine::NFmiImage& img,
                        const ContourSpec& theSpec,
                        const NFmiArea& theArea)
{
  if (theSpec.labelMarker().empty())
    return;

  // Establish that something is to be done

  if (theSpec.labelPoints().empty() && theSpec.pixelLabels().empty())
    return;

  // Establish the marker specs

  const Imagine::NFmiImage& marker = globals.getImage(theSpec.labelMarker());

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
 * \brief Draw a single label and its caption unless the point is outside the image or masked
 */
// ----------------------------------------------------------------------

void draw_label(Imagine::NFmiImage& img,
                Imagine::NFmiFace& face,
                const ContourSpec& theSpec,
                double x,
                double y,
                float value)
{
  // Skip rendering if the point is much too far from the image

  const int safetymargin = 50;
  if (x < -safetymargin || y < -safetymargin || x > img.Width() + safetymargin ||
      y > img.Height() + safetymargin)
    return;

  // Skip rendering if the start point is masked

  if (IsMasked(NFmiPoint(x, y), globals.mask))
    return;

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
    return;

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
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw label texts
 */
// ----------------------------------------------------------------------

void draw_label_texts(Imagine::NFmiImage& img, const ContourSpec& theSpec, const NFmiArea& theArea)
{
  // Establish that something is to be done

  if (theSpec.labelPoints().empty() && theSpec.pixelLabels().empty())
    return;

  // Quick exit if no labels are desired for this parameter

  if (theSpec.labelFormat().empty())
    return;

    // Create the face object to be used

  Imagine::NFmiFace face = make_face(theSpec.labelFont());
  face.Background(false);

  // Draw labels at specifing latlon points if requested

  {
    list<pair<NFmiPoint, NFmiPoint>>::const_iterator iter;

    int pointnumber = 0;
    for (iter = theSpec.labelPoints().begin(); iter != theSpec.labelPoints().end(); ++iter)
    {
      // The point in question

      double x = NAN;
      double y = NAN;
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

      draw_label(img, face, theSpec, x, y, value);
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

      draw_label(img, face, theSpec, x, y, value);
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
 * \brief Set the speed and direction of a grid point from the U and V components
 *
 * The direction is corrected by the grid north only when globals.uvorientation
 * is set, as in the point version.
 */
// ----------------------------------------------------------------------

void set_grid_speed_direction(const Fmi::CoordinateTransformation &transformation,
                              double lon,
                              double lat,
                              float dx,
                              float dy,
                              float &speed,
                              float &direction)
{
  speed = sqrt(dx * dx + dy * dy);
  if (dx != 0 || dy != 0)
  {
    if (!globals.uvorientation)
      direction = fmod(180 + FmiDeg(atan2(dx, dy)), 360.0);
    else
    {
      auto north = Fmi::OGR::gridNorth(transformation, lon, lat);
      if (north)
        direction = fmod(180 + *north + FmiDeg(atan2(dx, dy)), 360.0);
      else
        direction = kFloatMissing;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the grid values of the parameter, replacing src by dst and converting units
 *
 * The values are left untouched if the parameter is not available.
 */
// ----------------------------------------------------------------------

void get_parameter_values(const std::string &theParam,
                          float src,
                          float dst,
                          NFmiDataMatrix<float> &values)
{
  if (globals.queryinfo->Param(toparam(theParam)))
  {
    values = globals.queryinfo->Values();
    values.Replace(src, dst);
    globals.unitsconverter.convert(FmiParameterName(globals.queryinfo->GetParamIdent()), values);
  }
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
    get_parameter_values(globals.speedparam, speed_src, speed_dst, speed);
    get_parameter_values(globals.directionparam, direction_src, direction_dst, direction);
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
            set_grid_speed_direction(
                transformation, latlon->x(i, j), latlon->y(i, j), dx[i][j], dy[i][j],
                speed[i][j], direction[i][j]);
        }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the speed and direction at a point from the U and V components
 */
// ----------------------------------------------------------------------

void set_point_speed_direction(const Fmi::CoordinateTransformation &transformation,
                               const NFmiPoint &latlon,
                               float dx,
                               float dy,
                               float &speed,
                               float &direction)
{
  speed = sqrt(dx * dx + dy * dy);
  if (dx != 0 || dy != 0)
  {
    if (!globals.uvorientation)
      direction = fmod(180 + FmiDeg(atan2f(dx, dy)), 360.0);
    else
    {
      auto north = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());
      if (north)
        direction = fmod(180 + *north + FmiDeg(atan2f(dx, dy)), 360.0);
      else
        direction = kFloatMissing;
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
      set_point_speed_direction(transformation, latlon, dx, dy, speed, direction);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The direction of north at the wind arrow location
 *
 * Returns nothing if the direction cannot be calculated.
 */
// ----------------------------------------------------------------------

std::optional<double> arrow_north(const Fmi::CoordinateTransformation& transformation,
                                   const NFmiPoint& latlon)
{
  std::optional<double> north;
  if (globals.uvorientation)
    north = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());
  else
    north = 0.0;
  return north;
}

// ----------------------------------------------------------------------
/*!
 * \brief Render a single wind arrow
 *
 * The value type is a template parameter so that the calculations are done
 * in the same precision as the caller's speed and direction values.
 */
// ----------------------------------------------------------------------

template <typename Value>
void render_wind_arrow(Imagine::NFmiImage& img,
                       const NFmiPath& theArrow,
                       const NFmiPoint& xy0,
                       const NFmiPoint& latlon,
                       Value speed,
                       Value dir,
                       double north)
{
  if (globals.arrowfile == "roundarrow")
  {
    draw_roundarrow(img, xy0, speed, -dir + north + 180);
    return;
  }

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
    strokes.Rotate(-dir + north + 180);
    strokes.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

    flags.Scale(globals.arrowscale);
    flags.Rotate(-dir + north + 180);
    flags.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

    ArrowStyle style = globals.getArrowStroke(speed);
    strokes.Stroke(img, style.color, style.rule);
    flags.Fill(img, style.color, style.rule);
    return;
  }

  NFmiPath arrowpath;
  arrowpath.Add(theArrow);

  if (speed > 0 && speed != kFloatMissing)
    arrowpath.Scale(globals.windarrowscaleA * log10(globals.windarrowscaleB * speed + 1) +
                    globals.windarrowscaleC);
  arrowpath.Scale(globals.arrowscale);
  arrowpath.Rotate(-dir + north + 180);
  arrowpath.Translate(static_cast<float>(xy0.X()), static_cast<float>(xy0.Y()));

  // And render it

  ArrowStyle fillstyle = globals.getArrowFill(speed);
  arrowpath.Fill(img, fillstyle.color, fillstyle.rule);

  ArrowStyle strokestyle = globals.getArrowStroke(speed);
  arrowpath.Stroke(img, strokestyle.color, strokestyle.rule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the listed wind arrow points
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_points(Imagine::NFmiImage& img,
                             const NFmiArea& theArea,
                             const NFmiPath& theArrow,
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

    float dir = NAN;
    float speed = NAN;

    get_speed_direction(
        transformation, latlon, speed_src, speed_dst, direction_src, direction_dst, speed, dir);

    // Ignore missing values
    if (dir == kFloatMissing || speed == kFloatMissing)
      continue;

    // Direction calculations

    std::optional<double> north = arrow_north(transformation, latlon);
    if (!north)
      continue;

    // Render the arrow

    render_wind_arrow(img, theArrow, xy0, latlon, speed, dir, *north);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether the point is way outside the image
 */
// ----------------------------------------------------------------------

bool is_far_outside_image(const Imagine::NFmiImage& img, const NFmiPoint& xy0)
{
  const int safetymargin = 50;
  return (xy0.X() < -safetymargin || xy0.Y() < -safetymargin ||
          xy0.X() > img.Width() + safetymargin || xy0.Y() > img.Height() + safetymargin);
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the first querydata in which the wind direction (or the
 *        speed x-component) parameter is usable
 */
// ----------------------------------------------------------------------

void select_wind_queryinfo()
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
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the data replacement values for wind direction and speed
 */
// ----------------------------------------------------------------------

void get_wind_replacements(float& direction_src,
                           float& direction_dst,
                           float& speed_src,
                           float& speed_dst)
{
  list<ContourSpec>::iterator piter;
  auto pbegin = globals.specs.begin();
  auto pend = globals.specs.end();

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
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the data gridded wind arrow points
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_grid(Imagine::NFmiImage& img,
                           const NFmiArea& theArea,
                           const NFmiPath& theArrow,
                           float direction_src,
                           float direction_dst,
                           float speed_src,
                           float speed_dst)
{
  // Draw the full grid if so desired

  if (globals.windarrowdx <= 0 || globals.windarrowdy <= 0)
    return;

  NFmiDataMatrix<float> speedvalues;
  NFmiDataMatrix<float> dirvalues;

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

  for (int ny = 0; ny * globals.windarrowdy < coordinates.height() - 1; ny++)
  {
    const float y = ny * globals.windarrowdy;
    for (int nx = 0; nx * globals.windarrowdx < coordinates.width() - 1; nx++)
    {
      const float x = nx * globals.windarrowdx;
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

      if (is_far_outside_image(img, xy0))
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

      std::optional<double> north = arrow_north(wgs84transformation, latlon);
      if (!north)
        continue;

      // Render the arrow

      render_wind_arrow(img, theArrow, xy0, latlon, speed, dir, *north);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw the wind arrows in image coordinates
 */
// ----------------------------------------------------------------------

void draw_wind_arrows_pixelgrid(Imagine::NFmiImage& img,
                                const NFmiArea& theArea,
                                const NFmiPath& theArrow,
                                float direction_src,
                                float direction_dst,
                                float speed_src,
                                float speed_dst)
{
  // Draw the full grid if so desired

  if (globals.windarrowsxydx <= 0 || globals.windarrowsxydy <= 0)
    return;

  Fmi::CoordinateTransformation transformation("WGS84", theArea.SpatialReference());

  for (int ny = 0; globals.windarrowsxyy0 + ny * globals.windarrowsxydy <= img.Height(); ny++)
  {
    const float y = globals.windarrowsxyy0 + ny * globals.windarrowsxydy;
    for (int nx = 0; globals.windarrowsxyx0 + nx * globals.windarrowsxydx <= img.Width(); nx++)
    {
      const float x = globals.windarrowsxyx0 + nx * globals.windarrowsxydx;
      NFmiPoint xy0(x, y);

      // Skip the point if it is masked
      if (IsMasked(xy0, globals.mask))
        continue;

      // Calculate the latlon value

      NFmiPoint latlon = theArea.ToLatLon(xy0);

      // Calculate the speed & direction values

      float dir = NAN;
      float speed = NAN;

      get_speed_direction(
          transformation, latlon, speed_src, speed_dst, direction_src, direction_dst, speed, dir);

      // Ignore missing values

      if (dir == kFloatMissing || speed == kFloatMissing)
        continue;

      // Direction calculations

      std::optional<double> north = arrow_north(transformation, latlon);
      if (!north)
        continue;

      // Render the arrow

      render_wind_arrow(img, theArrow, xy0, latlon, speed, dir, *north);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw wind arrows onto the image
 */
// ----------------------------------------------------------------------

void draw_wind_arrows(Imagine::NFmiImage& img, const NFmiArea& theArea)
{
  if ((!globals.arrowpoints.empty() || (globals.windarrowdx > 0 && globals.windarrowdy > 0) ||
       (globals.windarrowsxydx > 0 && globals.windarrowsxydy > 0)) &&
      (!globals.arrowfile.empty()))
  {
    select_wind_queryinfo();

    // Read the arrow definition

    NFmiPath arrowpath;
    if (globals.arrowfile != "meteorological" && globals.arrowfile != "roundarrow")
    {
      const string &arr = globals.itsArrowCache.find(globals.arrowfile);
      arrowpath.Add(arr);
    }

    // Establish data replacement values

    float direction_src = kFloatMissing;
    float direction_dst = kFloatMissing;
    float speed_src = kFloatMissing;
    float speed_dst = kFloatMissing;

    get_wind_replacements(direction_src, direction_dst, speed_src, speed_dst);

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

void draw_contour_fills(Imagine::NFmiImage& img,
                        const NFmiArea& theArea,
                        const ContourSpec& theSpec,
                        const NFmiTime& theTime,
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

void draw_contour_patterns(Imagine::NFmiImage& img,
                           const NFmiArea& theArea,
                           const ContourSpec& theSpec,
                           const NFmiTime& theTime,
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
    const Imagine::NFmiImage& pattern = globals.getImage(it->pattern());

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

void draw_contour_strokes(Imagine::NFmiImage& img,
                          const NFmiArea& theArea,
                          const ContourSpec& theSpec,
                          const NFmiTime& theTime,
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

void save_contour_labels(Imagine::NFmiImage& /*img*/,
                         const NFmiArea& theArea,
                         const ContourSpec& theSpec,
                         const NFmiTime& theTime,
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

    for (const auto& pit : path.Elements())
    {
      if (pit.op == kFmiLineTo)
      {
        globals.labellocator.add(
            it->value(), static_cast<int>(round(pit.x)), static_cast<int>(round(pit.y)));
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour labels
 */
// ----------------------------------------------------------------------

void draw_contour_labels(Imagine::NFmiImage& img)
{
  const LabelLocator::ParamCoordinates &coords = globals.labellocator.chooseLabels();

  if (coords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  auto pbegin = globals.specs.begin();
  auto pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    auto pit = coords.find(id);
    if (pit == coords.end())
      continue;

    // Rended the contours

    const int fontcolor = piter->contourLabelColor();
    const std::string &fontspec = piter->contourLabelFont();
    const int backcolor = piter->contourLabelBackgroundColor();
    const int xmargin = piter->contourLabelBackgroundXMargin();
    const int ymargin = piter->contourLabelBackgroundYMargin();

    Imagine::NFmiFace face = make_face(fontspec);
    face.Background(true);
    face.BackgroundColor(backcolor);
    face.BackgroundMargin(xmargin, ymargin);

    for (const auto& cit : pit->second)
    {
      const float value = cit.first;
      string text = NFmiStringTools::Convert(value);

      // Handle possible contourlabeltext override
      auto it = piter->contourLabelTexts().find(value);
      if (it != piter->contourLabelTexts().end())
        text = it->second;

      for (auto it = cit.second.begin(); it != cit.second.end(); ++it)
      {
        face.Draw(
            img, it->second.first, it->second.second, text, Imagine::kFmiAlignCenter, fontcolor);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save contour symbols
 */
// ----------------------------------------------------------------------

void save_contour_symbols(Imagine::NFmiImage& /*img*/,
                          const NFmiArea& theArea,
                          const ContourSpec& theSpec,
                          const LazyCoordinates& thePoints,
                          const NFmiDataMatrix<float>& theValues)
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
 * \brief Test whether the value is within the range [lo,hi) of a contour symbol
 *
 * A missing value matches only a range with both limits missing.
 */
// ----------------------------------------------------------------------

bool is_inside_symbol_range(float z, float lo, float hi)
{
  if (z == kFloatMissing)
    return (lo == kFloatMissing && hi == kFloatMissing);
  if (lo != kFloatMissing && z < lo)
    return false;
  if (hi != kFloatMissing && z >= hi)
    return false;
  if (lo == kFloatMissing && hi == kFloatMissing)
    return false;
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw contour symbols
 */
// ----------------------------------------------------------------------

void draw_contour_symbols(Imagine::NFmiImage& img)
{
  const LabelLocator::ParamCoordinates &paramcoords = globals.imagelocator.chooseLabels();

  if (paramcoords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  auto pbegin = globals.specs.begin();
  auto pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    auto pit = paramcoords.find(id);
    if (pit == paramcoords.end())
      continue;

    const LabelLocator::ContourCoordinates &coords = pit->second;

    // Loop through all the values

    for (const auto& coord : coords)
    {
      const float z = coord.first;

      // Find the specs for the font value

      list<ContourSymbol>::const_iterator fit;
      for (fit = piter->contourSymbols().begin(); fit != piter->contourSymbols().end(); ++fit)
      {
        if (is_inside_symbol_range(z, fit->lolimit(), fit->hilimit()))
          break;
      }

      // Should never happen
      if (fit == piter->contourSymbols().end())
        throw runtime_error("Internal error while contouring with symbols");

      // Render the symbols

      NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(fit->rule());
      const Imagine::NFmiImage& symbol = globals.getImage(fit->pattern());
      const float factor = fit->factor();

      for (auto it = coord.second.begin(); it != coord.second.end(); ++it)
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

void draw_contour_fonts(Imagine::NFmiImage& img)
{
  const LabelLocator::ParamCoordinates &paramcoords = globals.symbollocator.chooseLabels();

  if (paramcoords.empty())
    return;

  // Iterate through all parameters

  list<ContourSpec>::iterator piter;
  auto pbegin = globals.specs.begin();
  auto pend = globals.specs.end();

  for (piter = pbegin; piter != pend; ++piter)
  {
    // Ignore the param if we could not assign any coordinates for it

    const int id = paramid(piter->param());

    auto pit = paramcoords.find(id);
    if (pit == paramcoords.end())
      continue;

    const LabelLocator::ContourCoordinates &coords = pit->second;

    // Loop through all the values

    for (const auto& coord : coords)
    {
      const float value = coord.first;

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

      string text;
      text += symbol;

      const std::string &fontspec = fit->font();

      Imagine::NFmiFace face = make_face(fontspec);
      face.Background(false);
      for (auto it = coord.second.begin(); it != coord.second.end(); ++it)
      {
        face.Draw(
            img, it->second.first, it->second.second, text, Imagine::kFmiAlignCenter, fontcolor);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Save contour font coordinates
 */
// ----------------------------------------------------------------------

void save_contour_fonts(Imagine::NFmiImage& /*img*/,
                        const NFmiArea& theArea,
                        const ContourSpec& theSpec,
                        const LazyCoordinates& thePoints,
                        const NFmiDataMatrix<float>& theValues)
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

void draw_overlay(Imagine::NFmiImage& img, const ContourSpec& theSpec)
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
 * \brief Test whether the offset is on the frame of the search region
 */
// ----------------------------------------------------------------------

bool is_on_frame(int dx, int dy, int DX, int DY)
{
  return (dx == -DX || dx == DX || dy == -DY || dy == DY);
}

// ----------------------------------------------------------------------
/*!
 * \brief Classify an extremum based on the counts of smaller and bigger values
 *
 * \return -2/2 for absolute minima/maxima, -1/1 for minima/maxima, 0 for none
 */
// ----------------------------------------------------------------------

int classify_extremum(int smaller, int bigger, int DX, int DY)
{
  if (smaller == (DX * 2 + 1) * (DY * 2 + 1) - 1)
    return 2;
  if (bigger == (DX * 2 + 1) * (DY * 2 + 1) - 1)
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

      if (is_on_frame(dx, dy, DX, DY))
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

  return classify_extremum(smaller, bigger, DX, DY);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a pressure minimum or maximum candidate if such markers are drawn
 */
// ----------------------------------------------------------------------

void add_pressure_extremum(int extrem, const NFmiPoint& point, bool dohigh, bool dolow)
{
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

// ----------------------------------------------------------------------
/*!
 * \brief Draw high/low pressure markers
 */
// ----------------------------------------------------------------------

void draw_pressure_markers(Imagine::NFmiImage& img, const NFmiArea& theArea)
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
        add_pressure_extremum(extrem, point, dohigh, dolow);
      }
    }

  // Now choose the marker positions and draw them

  const ExtremaLocator::ExtremaCoordinates &extrema = globals.pressurelocator.chooseCoordinates();

  NFmiColorTools::NFmiBlendRule lowrule = ColorTools::checkrule(globals.lowpressurerule);
  NFmiColorTools::NFmiBlendRule highrule = ColorTools::checkrule(globals.highpressurerule);

  for (const auto& eit : extrema)
  {
    for (auto it = eit.second.begin(); it != eit.second.end(); ++it)
    {
      NFmiPoint wxy(it->first * 1000, it->second * 1000);
      NFmiPoint latlon = theArea.WorldXYToLatLon(wxy);
      /// latlon = MeridianTools::Relocate(latlon,theArea);
      NFmiPoint xy = theArea.ToXY(latlon);

      switch (eit.first)
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

void draw_foreground(Imagine::NFmiImage& img)
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

// ----------------------------------------------------------------------
/*!
 * \brief Establish the common time limits of the querydata
 */
// ----------------------------------------------------------------------

void establish_time_limits(NFmiTime& time1, NFmiTime& time2)
{
  unsigned int qi = 0;
  for (qi = 0; qi < globals.querystreams.size(); qi++)
  {
    // Establish time limits

    globals.queryinfo = globals.querystreams[qi];

    globals.queryinfo->LastTime();
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
    NFmiTime t2 = globals.queryinfo->ValidTime();

    globals.queryinfo->FirstTime();
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
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
}

// ----------------------------------------------------------------------
/*!
 * \brief Search the first time >= the desired time in all querydata
 *
 * Returns false if the image cannot be rendered for the time.
 */
// ----------------------------------------------------------------------

bool find_render_time(NFmiTime& t, const NFmiTime& time1)
{
  unsigned int qi = 0;
  bool ok = true;
  for (qi = 0; ok && qi < globals.querystreams.size(); qi++)
  {
    globals.queryinfo = globals.querystreams[qi];
    globals.queryinfo->ResetTime();
    while (globals.queryinfo->NextTime())
    {
      // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
      NFmiTime loc = globals.queryinfo->ValidTime();
      if (!loc.IsLessThan(t))
        break;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
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

      ok = isexact || hasprevious;
    }
    else
    {
      // Time must be exact, and previous steps
      // are required

      ok = isexact && hasprevious;
    }
  }
  return ok;
}

// ----------------------------------------------------------------------
/*!
 * \brief The name of the image file for the given time stamp
 */
// ----------------------------------------------------------------------

string image_filename(const NFmiString& datatimestr)
{
  unsigned int qi = 0;
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
  return filename;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the image to draw on, possibly from the background image
 */
// ----------------------------------------------------------------------

std::shared_ptr<Imagine::NFmiImage> create_image(const NFmiArea& theArea)
{
  int imgwidth = static_cast<int>(std::lround(theArea.Width()));
  int imgheight = static_cast<int>(std::lround(theArea.Height()));

  NFmiColorTools::Color erasecolor = ColorTools::checkcolor(globals.erase);

  std::shared_ptr<Imagine::NFmiImage> image;
  if (globals.background.empty())
  {
    image = std::make_shared<Imagine::NFmiImage>(imgwidth, imgheight, erasecolor);
  }
  else
  {
    image = std::make_shared<Imagine::NFmiImage>(globals.getImage(globals.background));
    if (imgwidth != image->Width() || imgheight != image->Height())
    {
      throw runtime_error("Background image size does not match area size");
    }
  }
  if (image == nullptr)
    throw runtime_error("Failed to allocate a new image for rendering");
  return image;
}

// ----------------------------------------------------------------------
/*!
 * \brief Contour a parameter and collect its label information
 *
 * The contour label information is collected, but not rendered yet.
 */
// ----------------------------------------------------------------------

void draw_parameter(Imagine::NFmiImage& img,
                    const NFmiArea& theArea,
                    ContourSpec& spec,
                    const NFmiTime& t,
                    bool labeldxdydone,
                    NFmiDataMatrix<float>& vals)
{
  // Establish the parameter

  string name = spec.param();
  int level = spec.level();

  unsigned int qi = choose_queryinfo(name, level);

  if (globals.verbose)
    report_queryinfo(name, qi);

  // Establish the contour method

  string interpname = spec.contourInterpolation();
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
    vals = MetaFunctions::values(spec.param(), *globals.queryinfo);

  // Replace values if so requested

  if (spec.replace())
    vals.Replace(spec.replaceSourceValue(), spec.replaceTargetValue());

  // Filter the values if so requested

  filter_values(vals, t, spec);

  // Expand the data if so requested

  if (globals.expanddata)
    expand_data(vals);

  // Call smoother only if necessary to avoid LazyCoordinates dereferencing

  LazyCoordinates worldpts(theArea);

  if (spec.smoother() != "None")
  {
    NFmiSmoother smoother(spec.smoother(), spec.smootherFactor(), spec.smootherRadius());

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
    add_label_grid_values(spec, theArea, worldpts);

  // For pixelgrids we must repeat the process for all new
  // background images, since the pixel spacing changes
  // every time. Note! We assume the following calling order!

  add_label_point_values(spec, theArea, vals);
  add_label_pixelgrid_values(spec, theArea, img, vals);

  // Fill the contours

  draw_contour_fills(img, theArea, spec, t, interp);

  // Pattern fill the contours

  draw_contour_patterns(img, theArea, spec, t, interp);

  // Stroke the contours

  draw_contour_strokes(img, theArea, spec, t, interp);

  // Save contour symbol coordinates

  save_contour_symbols(img, theArea, spec, worldpts, vals);

  // Save symbol fill coordinates

  save_contour_fonts(img, theArea, spec, worldpts, vals);

  // Save contour label coordinates

  save_contour_labels(img, theArea, spec, t, interp);

  // Draw optional overlay

  draw_overlay(img, spec);
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw everything on top of the contours: graticule, foreground, arrows, symbols,
 *        labels, pressure markers, the combine image and the time stamp
 */
// ----------------------------------------------------------------------

void draw_decorations(Imagine::NFmiImage& img, const NFmiArea& theArea, const NFmiTime& t)
{
  // Draw graticule

  draw_graticule(img, theArea);

  // Bang the foreground

  draw_foreground(img);

  // Draw wind arrows if so requested

  draw_wind_arrows(img, theArea);

  // Draw contour symbols

  draw_contour_symbols(img);

  // Draw contour fonts

  draw_contour_fonts(img);

  // Label the contours

  draw_contour_labels(img);

  // Draw labels

  for (auto& spec : globals.specs)
  {
    draw_label_markers(img, spec, theArea);
    draw_label_texts(img, spec, theArea);
  }

  // Draw high/low pressure markers

  draw_pressure_markers(img, theArea);

  // Bang the combine image (legend, logo, whatever)

  globals.drawCombine(img);

  // Finally, draw a time stamp on the image if so
  // requested

  const string stamp = globals.getImageStampText(t);
  globals.drawImageStampText(img, stamp);
}

// ----------------------------------------------------------------------
/*!
 * \brief The time preceding the first time to be drawn
 */
// ----------------------------------------------------------------------

NFmiTime first_time(const NFmiTime& time1)
{
  NFmiMetTime tmptime(time1,
                      (globals.timesteprounding && globals.timestep > 0) ? globals.timestep : 1);

  tmptime.ChangeByMinutes(globals.timestepskip);
  if (globals.timesteprounding)
    tmptime.PreviousMetTime();
  // NOLINTNEXTLINE(cppcoreguidelines-slicing): only the time is wanted, not the time step
  NFmiTime t = tmptime;
  return t;
}

void do_draw_contours(istream& /*theInput*/)
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

  NFmiTime time1;
  NFmiTime time2;
  establish_time_limits(time1, time2);

  NFmiDataMatrix<float> vals;
  NFmiDataMatrix<float> maskvalues;

  if (globals.verbose)
  {
    cout << "Data start time " << time1 << endl << "Data end time " << time2 << endl;
  }

  // Skip to first time

  NFmiTime t = first_time(time1);

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

    bool ok = find_render_time(t, time1);

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

    string filename = image_filename(datatimestr);

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

    std::shared_ptr<Imagine::NFmiImage> image = create_image(*area);

    globals.setImageModes(*image);
#define xr image /* HACK */

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
    auto pbegin = globals.specs.begin();
    auto pend = globals.specs.end();

    for (piter = pbegin; piter != pend; ++piter)
      draw_parameter(*xr, *area, *piter, t, labeldxdydone, vals);

    draw_decorations(*xr, *area, t);

    // dx and dy labels have now been extracted into a list,
    // disable adding them again and again and again..

    labeldxdydone = true;

    // Save

    write_image(*image, filename, globals.format);

    // Advance in time

    globals.labellocator.nextTime();
    globals.pressurelocator.nextTime();
    globals.symbollocator.nextTime();
    globals.imagelocator.nextTime();
  }
}

/****/
void process_cmd(const string &text)
{
  using Command = void (*)(istream &);
  static const std::map<std::string, Command> commands = {
      {"//", do_comment},
      {"cache", do_cache},
      {"imagecache", do_imagecache},
      {"querydata", do_querydata},
      {"filter", do_filter},
      {"timestepskip", do_timestepskip},
      {"timestep", do_timestep},
      {"timeinterval", do_timeinterval},
      {"timesteps", do_timesteps},
      {"timestamp", do_timestamp},
      {"timestampzone", do_timestampzone},
      {"timesteprounding", do_timesteprounding},
      {"timestampimage", do_timestampimage},
      {"timestampimagexy", do_timestampimagexy},
      {"timestampimageformat", do_timestampimageformat},
      {"timestampimagefont", do_timestampimagefont},
      {"timestampimagecolor", do_timestampimagecolor},
      {"timestampimagebackground", do_timestampimagebackground},
      {"timestampimagemargin", do_timestampimagemargin},
      {"timestampformat", do_timestampformat},
      {"projection", do_projection},
      {"erase", do_erase},
      {"fillrule", do_fillrule},
      {"strokerule", do_strokerule},
      {"directionparam", do_directionparam},
      {"speedparam", do_speedparam},
      {"speedcomponents", do_speedcomponents},
      {"uvorientation", do_uvorientation},
      {"arrowscale", do_arrowscale},
      {"windarrowscale", do_windarrowscale},
      {"arrowfill", do_arrowfill},
      {"arrowstroke", do_arrowstroke},
      {"arrowpath", do_arrowpath},
      {"roundarrowfill", do_roundarrowfill},
      {"roundarrowstroke", do_roundarrowstroke},
      {"roundarrowsize", do_roundarrowsize},
      {"windarrow", do_windarrow},
      {"windarrows", do_windarrows},
      {"windarrowsxy", do_windarrowsxy},
      {"background", do_background},
      {"foreground", do_foreground},
      {"mask", do_mask},
      {"overlay", do_overlay},
      {"combine", do_combine},
      {"foregroundrule", do_foregroundrule},
      {"savepath", do_savepath},
      {"prefix", do_prefix},
      {"suffix", do_suffix},
      {"format", do_format},
      {"graticule", do_graticule},
      {"gamma", do_gamma},
      {"intent", do_intent},
      {"pngquality", do_pngquality},
      {"jpegquality", do_jpegquality},
      {"savealpha", do_savealpha},
      {"reducecolors", do_reducecolors},
      {"wantpalette", do_wantpalette},
      {"forcepalette", do_forcepalette},
      {"alphalimit", do_alphalimit},
      {"hilimit", do_hilimit},
      {"datalolimit", do_datalolimit},
      {"datahilimit", do_datahilimit},
      {"datareplace", do_datareplace},
      {"despeckle", do_despeckle},
      {"expanddata", do_expanddata},
      {"contourdepth", do_contourdepth},
      {"contourinterpolation", do_contourinterpolation},
      {"contourtriangles", do_contourtriangles},
      {"smoother", do_smoother},
      {"smootherradius", do_smootherradius},
      {"smootherfactor", do_smootherfactor},
      {"level", do_level},
      {"param", do_param},
      {"shape", do_shape},
      {"contourfill", do_contourfill},
      {"contourpattern", do_contourpattern},
      {"contoursymbol", do_contoursymbol},
      {"contoursymbolmindist", do_contoursymbolmindist},
      {"contourfont", do_contourfont},
      {"contourlinewidth", do_contourlinewidth},
      {"contourline", do_contourline},
      {"contourfills", do_contourfills},
      {"contourlines", do_contourlines},
      {"contourlabel", do_contourlabel},
      {"contourlabels", do_contourlabels},
      {"contourlabeltext", do_contourlabeltext},
      {"contourlabelfont", do_contourlabelfont},
      {"contourlabelcolor", do_contourlabelcolor},
      {"contourlabelbackground", do_contourlabelbackground},
      {"contourlabelmargin", do_contourlabelmargin},
      {"contourlabelimagemargin", do_contourlabelimagemargin},
      {"contourlabelmindistsamevalue", do_contourlabelmindistsamevalue},
      {"contourlabelmindistdifferentvalue", do_contourlabelmindistdifferentvalue},
      {"contourlabelmindistdifferentparam", do_contourlabelmindistdifferentparam},
      {"contourfontmindistsamevalue", do_contourfontmindistsamevalue},
      {"contourfontmindistdifferentvalue", do_contourfontmindistdifferentvalue},
      {"contourfontmindistdifferentparam", do_contourfontmindistdifferentparam},
      {"highpressure", do_highpressure},
      {"lowpressure", do_lowpressure},
      {"lowpressuremaximum", do_lowpressuremaximum},
      {"highpressureminimum", do_highpressureminimum},
      {"pressuremindistsame", do_pressuremindistsame},
      {"pressuremindistdifferent", do_pressuremindistdifferent},
      {"labelmarker", do_labelmarker},
      {"labelfont", do_labelfont},
      {"labelcolor", do_labelcolor},
      {"labelrule", do_labelrule},
      {"labelalign", do_labelalign},
      {"labelformat", do_labelformat},
      {"labelmissing", do_labelmissing},
      {"labeloffset", do_labeloffset},
      {"labelcaption", do_labelcaption},
      {"label", do_label},
      {"labelxy", do_labelxy},
      {"labels", do_labels},
      {"labelsxy", do_labelsxy},
      {"labelfile", do_labelfile},
      {"units", do_units},
      {"clear", do_clear},
  };

  istringstream in(text);
  string cmd;
  while (in >> cmd)
  {
    // Handle comments

    if (cmd[0] == '#')
      do_comment(in);
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
    {
      auto it = commands.find(cmd);
      if (it == commands.end())
        throw runtime_error("Unknown command " + cmd);
      it->second(in);
    }
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

  auto fileiter = globals.cmdline_files.begin();
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
}  // namespace

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
