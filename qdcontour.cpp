// ======================================================================
/*!
 * \file
 * \brief Main program for qdcontour
 */
// ======================================================================

#include "Globals.h"
#include "ColorTools.h"
#include "ContourSpec.h"
#include "GramTools.h"
#include "LazyQueryData.h"
#include "MetaFunctions.h"
#include "ProjectionFactory.h"
#include "TimeTools.h"

#include "imagine/NFmiColorTools.h"
#include "imagine/NFmiImage.h"			// for rendering
#include "imagine/NFmiGeoShape.h"		// for esri data
#include "imagine/NFmiText.h"			// for labels
#include "imagine/NFmiFontHershey.h"	// for Hershey fonts

#include "newbase/NFmiAreaFactory.h"
#include "newbase/NFmiCmdLine.h"			// command line options
#include "newbase/NFmiDataMatrix.h"
#include "newbase/NFmiDataModifierClasses.h"
#include "newbase/NFmiEnumConverter.h"		// FmiParameterName<-->string
#include "newbase/NFmiFileSystem.h"			// FileExists()
#include "newbase/NFmiLatLonArea.h"			// Geographic projection
#include "newbase/NFmiSettings.h"			// Configuration
#include "newbase/NFmiSmoother.h"		// for smoothing data
#include "newbase/NFmiStereographicArea.h"	// Stereographic projection
#include "newbase/NFmiStringTools.h"
#include "newbase/NFmiPreProcessor.h"

#include "boost/shared_ptr.hpp"

#include <fstream>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace boost;
using namespace Imagine;

// ----------------------------------------------------------------------
// Global instance of global variables
// ----------------------------------------------------------------------

static Globals globals;

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
	   << endl;
}

// ----------------------------------------------------------------------
/*!
 * Test whether the given pixel coordinate is masked. This by definition
 * means the respective pixel in the given mask is not fully transparent.
 * Also, we define all pixels outside the mask image to be masked similarly
 * as pixel(0,0).
 *
 * \param thePoint The pixel coordinate
 * \param theMask The mask filename
 * \param theMaskImage The mask image
 * \return True, if the pixel is masked out
 */
// ----------------------------------------------------------------------

bool IsMasked(const NFmiPoint & thePoint,
			  const std::string & theMask,
			  const NFmiImage & theMaskImage)
{
  if(theMask.empty())
	return false;

  long x = static_cast<int>(FmiRound(thePoint.X()));
  long y = static_cast<int>(FmiRound(thePoint.Y()));

  // Handle outside pixels the same way as pixel 0,0
  if( x<0 ||
	  y<0 ||
	  x>=theMaskImage.Width() ||
	  y>=theMaskImage.Height())
	{
	  x = 0;
	  y = 0;
	}

  const NFmiColorTools::Color c = theMaskImage(x,y);
  const int alpha = NFmiColorTools::GetAlpha(c);

  return (alpha != NFmiColorTools::Transparent);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line options
 */
// ----------------------------------------------------------------------

void parse_command_line(int argc, const char * argv[])
{

  NFmiCmdLine cmdline(argc,argv,"hvfq!");

  // Check for parsing errors

  if(cmdline.Status().IsError())
	throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // Handle -h option

  if(cmdline.isOption('h'))
	{
	  Usage();
	  exit(0);
	}

  // Read -v option

  if(cmdline.isOption('v'))
    globals.verbose = true;

  // Read -f option

  if(cmdline.isOption('f'))
    globals.force = true;

  if(cmdline.isOption('q'))
	globals.cmdline_querydata = cmdline.OptionValue('q');

  // Read command filenames

  if(cmdline.NumberofParameters() == 0)
	throw runtime_error("Atleast one command line parameter is required");

  for(int i=1; i<=cmdline.NumberofParameters(); i++)
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

const string read_script(const string & theName)
{
  const bool strip_pound = false;
  NFmiPreProcessor processor(strip_pound);

  processor.SetDefine("#define");
  processor.SetIncluding("include", "", "");

  if(!processor.ReadAndStripFile(theName))
	{
	  if(!NFmiFileSystem::FileExists(theName))
		throw runtime_error("Script file '"+theName+"' does not exist");
	  throw runtime_error("Preprocessor failed to parse '"+theName+"'");

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

const string preprocess_script(const string & theScript)
{
  string ret;

  if(!globals.cmdline_querydata.empty())
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
 * \brief Check input stream validity
 */
// ----------------------------------------------------------------------

void check_errors(istream & theInput, const string & theFunction)
{
  if(theInput.fail())
	throw runtime_error("Processing the '"+theFunction+"' command failed");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle a comment token
 */
// ----------------------------------------------------------------------

void do_comment(istream & theInput)
{
  theInput.ignore(numeric_limits<std::streamsize>::max(),'\n');
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle the "cache" command
 */
// ----------------------------------------------------------------------

void do_cache(istream & theInput)
{
  int flag;
  theInput >> flag;

  check_errors(theInput,"cache");

  globals.calculator.cache(flag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle the "querydata" command
 */
// ----------------------------------------------------------------------

void do_querydata(istream & theInput)
{
  string newnames;
  theInput >> newnames;

  check_errors(theInput,"querydata");

  if(globals.queryfilelist != newnames)
	{
	  globals.queryfilelist = newnames;

	  // Delete possible old infos

	  globals.clear_querystreams();

	  // Split the comma separated list into a real list

	  vector<string> qnames = NFmiStringTools::Split(globals.queryfilelist);

	  // Read the queryfiles

	  {
		vector<string>::const_iterator iter;
		for(iter=qnames.begin(); iter!=qnames.end(); ++iter)
		  {
			LazyQueryData * tmp = new LazyQueryData();
			string filename = NFmiFileSystem::FileComplete(*iter,globals.datapath);
			globals.queryfilenames.push_back(filename);
			tmp->Read(filename);
			globals.querystreams.push_back(tmp);
		  }
	  }
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "querydatalevel" command
 */
// ----------------------------------------------------------------------

void do_querydatalevel(istream & theInput)
{
  theInput >> globals.querydatalevel;

  check_errors(theInput,"querydatalevel");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "filter" command
 */
// ----------------------------------------------------------------------

void do_filter(istream & theInput)
{
  theInput >> globals.filter;

  check_errors(theInput,"filter");

  if(globals.filter != "none" &&
	 globals.filter != "linear" &&
	 globals.filter != "min" &&
	 globals.filter != "max" &&
	 globals.filter != "mean" &&
	 globals.filter != "msum")
	{
	  throw runtime_error("Filtering mode '"+globals.filter+"' is not recognized");
	}

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestepskip" command
 */
// ----------------------------------------------------------------------

void do_timestepskip(istream & theInput)
{
  theInput >> globals.timestepskip;

  check_errors(theInput,"timestepskip");

  if(globals.timestepskip < 0)
	throw runtime_error("timestepskip cannot be negative");

  const int ludicruous = 30*24*60;	// 1 month
  if(globals.timestepskip > ludicruous)
	throw runtime_error("timestepskip "+NFmiStringTools::Convert(globals.timestepskip)+" is ridiculously large");

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestep" command
 */
// ----------------------------------------------------------------------

void do_timestep(istream & theInput)
{
  theInput >> globals.timestep;
  globals.timeinterval = globals.timestep;

  check_errors(theInput,"timestep");

  if(globals.timestep < 0)
	throw runtime_error("timestep cannot be negative");

  const int ludicruous = 30*24*60;	// 1 month
  if(globals.timestep > ludicruous)
	throw runtime_error("timestep "+NFmiStringTools::Convert(globals.timestep)+" is ridiculously large");

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timeinterval" command
 */
// ----------------------------------------------------------------------

void do_timeinterval(istream & theInput)
{
  theInput >> globals.timeinterval;

  check_errors(theInput,"timeinterval");

  if(globals.timeinterval < 0)
	throw runtime_error("timeinterval cannot be negative");

  const int ludicruous = 30*24*60;	// 1 month
  if(globals.timeinterval > ludicruous)
	throw runtime_error("timestep "+NFmiStringTools::Convert(globals.timeinterval)+" is ridiculously large");

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timesteps" command
 */
// ----------------------------------------------------------------------

void do_timesteps(istream & theInput)
{
  theInput >> globals.timesteps;

  check_errors(theInput,"timeinterval");

  if(globals.timesteps < 0)
	throw runtime_error("timesteps cannot be negative");

  const int ludicruous = 30*24*60;	// 1 month
  if(globals.timesteps > ludicruous)
	throw runtime_error("timesteps "+NFmiStringTools::Convert(globals.timesteps)+" is ridiculously large");

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestamp" command
 */
// ----------------------------------------------------------------------

void do_timestamp(istream & theInput)
{
  theInput >> globals.timestampflag;

  check_errors(theInput,"timestamp");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampzone" command
 */
// ----------------------------------------------------------------------

void do_timestampzone(istream & theInput)
{
  theInput >> globals.timestampzone;

  check_errors(theInput,"timestampzone");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timesteprounding" command
 */
// ----------------------------------------------------------------------

void do_timesteprounding(istream & theInput)
{
  theInput >> globals.timesteprounding;

  check_errors(theInput,"timesteprounding");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimage" command
 */
// ----------------------------------------------------------------------

void do_timestampimage(istream & theInput)
{
  theInput >> globals.timestampimage;

  check_errors(theInput,"timestampimage");

  if(globals.timestampimage != "none" &&
	 globals.timestampimage != "obs" &&
	 globals.timestampimage != "for" &&
	 globals.timestampimage != "forobs")
	{
	  throw runtime_error("Unrecognized timestampimage mode '"+globals.timestampimage+"'");
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "timestampimagexy" command
 */
// ----------------------------------------------------------------------

void do_timestampimagexy(istream & theInput)
{
  theInput >> globals.timestampimagex >> globals.timestampimagey;

  check_errors(theInput,"timestampimagexy");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "projection" command
 */
// ----------------------------------------------------------------------

void do_projection(istream & theInput)
{
  theInput >> globals.projection;

  check_errors(theInput,"projection");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "erase" command
 */
// ----------------------------------------------------------------------

void do_erase(istream & theInput)
{
  theInput >> globals.erase;

  check_errors(theInput,"projection");

  ColorTools::checkcolor(globals.erase);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "fillrule" command
 */
// ----------------------------------------------------------------------

void do_fillrule(istream & theInput)
{
  theInput >> globals.fillrule;

  check_errors(theInput,"fillrule");

  ColorTools::checkrule(globals.fillrule);

  if(!globals.shapespecs.empty())
	globals.shapespecs.back().fillrule(globals.fillrule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "strokerule" command
 */
// ----------------------------------------------------------------------

void do_strokerule(istream & theInput)
{
  theInput >> globals.strokerule;

  check_errors(theInput,"strokerule");

  ColorTools::checkrule(globals.strokerule);

  if(!globals.shapespecs.empty())
	globals.shapespecs.back().strokerule(globals.strokerule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "directionparam" command
 */
// ----------------------------------------------------------------------

void do_directionparam(istream & theInput)
{
  theInput >> globals.directionparam;

  check_errors(theInput,"directionparam");

  if(NFmiEnumConverter().ToEnum(globals.directionparam) == kFmiBadParameter)
	throw runtime_error("Unrecognized directionparam '"+globals.directionparam+"'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "speedparam" command
 */
// ----------------------------------------------------------------------

void do_speedparam(istream & theInput)
{
  theInput >> globals.speedparam;

  check_errors(theInput,"speedparam");

  if(NFmiEnumConverter().ToEnum(globals.speedparam) == kFmiBadParameter)
	throw runtime_error("Unrecognized speedparam '"+globals.speedparam+"'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowscale" command
 */
// ----------------------------------------------------------------------

void do_arrowscale(istream & theInput)
{
  theInput >> globals.arrowscale;

  check_errors(theInput,"arrowscale");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrowscale" command
 */
// ----------------------------------------------------------------------

void do_windarrowscale(istream & theInput)
{
  theInput >> globals.windarrowscaleA
		   >> globals.windarrowscaleB
		   >> globals.windarrowscaleC;

  check_errors(theInput,"windarrowscale");

  if(globals.windarrowscaleB < 0)
	throw runtime_error("Second parameter of windarrowscale must be nonnegative");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowfill" command
 */
// ----------------------------------------------------------------------

void do_arrowfill(istream & theInput)
{
  theInput >> globals.arrowfillcolor  >> globals.arrowfillrule;

  check_errors(theInput,"arrowfill");

  ColorTools::checkcolor(globals.arrowfillcolor);
  ColorTools::checkrule(globals.arrowfillrule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowstroke" command
 */
// ----------------------------------------------------------------------

void do_arrowstroke(istream & theInput)
{
  theInput >> globals.arrowstrokecolor  >> globals.arrowstrokerule;

  check_errors(theInput,"arrowstroke");

  ColorTools::checkcolor(globals.arrowstrokecolor);
  ColorTools::checkrule(globals.arrowstrokerule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "arrowpath" command
 */
// ----------------------------------------------------------------------

void do_arrowpath(istream & theInput)
{
  theInput >> globals.arrowfile;

  check_errors(theInput,"arrowpath");

  if(!NFmiFileSystem::FileExists(globals.arrowfile) &&
	 globals.arrowfile != "meteorological")
	{
	  throw runtime_error("The arrowpath file '"+globals.arrowfile+"' does not exist");
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrow" command
 */
// ----------------------------------------------------------------------

void do_windarrow(istream & theInput)
{
  double lon,lat;
  theInput >> lon >> lat;

  check_errors(theInput,"windarrow");

  globals.arrowpoints.push_back(NFmiPoint(lon,lat));

}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "windarrows" command
 */
// ----------------------------------------------------------------------

void do_windarrows(istream & theInput)
{
  theInput >> globals.windarrowdx >> globals.windarrowdy;

  check_errors(theInput,"windarrow");

  if(globals.windarrowdx < 0 || globals.windarrowdy < 0)
	throw runtime_error("windarrows parameters must be nonnegative");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "background" command
 */
// ----------------------------------------------------------------------

void do_background(istream & theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.background;

  check_errors(theInput,"background");

  if(globals.background == "none")
	globals.background = "";
  else
	globals.backgroundimage.Read(FileComplete(globals.background,
											  globals.mapspath));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "foreground" command
 */
// ----------------------------------------------------------------------

void do_foreground(istream & theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.foreground;

  check_errors(theInput,"foreground");

  if(globals.foreground == "none")
	globals.foreground = "";
  else
	globals.foregroundimage.Read(FileComplete(globals.foreground,
											  globals.mapspath));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "mask" command
 */
// ----------------------------------------------------------------------

void do_mask(istream & theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.mask;

  check_errors(theInput,"mask");

  if(globals.mask == "none")
	globals.mask = "";
  else
	globals.maskimage.Read(FileComplete(globals.mask,
										globals.mapspath));
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "combine" command
 */
// ----------------------------------------------------------------------

void do_combine(istream & theInput)
{
  using NFmiFileSystem::FileComplete;

  theInput >> globals.combine;

  check_errors(theInput,"combine");

  if(globals.combine == "none")
	globals.combine = "";
  else
	{
	  theInput >> globals.combinex
			   >> globals.combiney
			   >> globals.combinerule
			   >> globals.combinefactor;

	  ColorTools::checkrule(globals.combinerule);

	  if(globals.combinefactor < 0 || globals.combinefactor > 1)
		throw runtime_error("combine blending factor must be in range 0-1");

	  globals.combineimage.Read(FileComplete(globals.combine,
											 globals.mapspath));
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "foregroundrule" command
 */
// ----------------------------------------------------------------------

void do_foregroundrule(istream & theInput)
{
  theInput >> globals.foregroundrule;

  check_errors(theInput,"foregroundrule");

  ColorTools::checkrule(globals.foregroundrule);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "savepath" command
 */
// ----------------------------------------------------------------------

void do_savepath(istream & theInput)
{
  theInput >> globals.savepath;

  check_errors(theInput,"savepath");

  if(!NFmiFileSystem::DirectoryExists(globals.savepath))
	throw runtime_error("savepath "+globals.savepath+" does not exist");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "prefix" command
 */
// ----------------------------------------------------------------------

void do_prefix(istream & theInput)
{
  theInput >> globals.prefix;

  check_errors(theInput,"prefix");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "suffix" command
 */
// ----------------------------------------------------------------------

void do_suffix(istream & theInput)
{
  theInput >> globals.suffix;

  check_errors(theInput,"suffix");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "format" command
 */
// ----------------------------------------------------------------------

void do_format(istream & theInput)
{
  theInput >> globals.format;

  check_errors(theInput,"format");

  if(globals.format != "png" &&
	 globals.format != "jpg" &&
	 globals.format != "jpeg" &&
	 globals.format != "gif")
	{
	  throw runtime_error("Image format +'"+globals.format+"' is not supported");
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "gamma" command
 */
// ----------------------------------------------------------------------

void do_gamma(istream & theInput)
{
  theInput >> globals.gamma;

  check_errors(theInput,"gamma");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "intent" command
 */
// ----------------------------------------------------------------------

void do_intent(istream & theInput)
{
  theInput >> globals.intent;

  check_errors(theInput,"intent");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "pngquality" command
 */
// ----------------------------------------------------------------------

void do_pngquality(istream & theInput)
{
  theInput >> globals.pngquality;

  check_errors(theInput,"pngquality");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "jpegquality" command
 */
// ----------------------------------------------------------------------

void do_jpegquality(istream & theInput)
{
  theInput >> globals.jpegquality;

  check_errors(theInput,"jpegquality");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "savealpha" command
 */
// ----------------------------------------------------------------------

void do_savealpha(istream & theInput)
{
  theInput >> globals.savealpha;

  check_errors(theInput,"savealpha");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "wantpalette" command
 */
// ----------------------------------------------------------------------

void do_wantpalette(istream & theInput)
{
  theInput >> globals.wantpalette;

  check_errors(theInput,"wantpalette");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "forcepalette" command
 */
// ----------------------------------------------------------------------

void do_forcepalette(istream & theInput)
{
  theInput >> globals.forcepalette;

  check_errors(theInput,"forcepalette");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "alphalimit" command
 */
// ----------------------------------------------------------------------

void do_alphalimit(istream & theInput)
{
  theInput >> globals.alphalimit;

  check_errors(theInput,"alphalimit");
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "hilimit" command
 */
// ----------------------------------------------------------------------

void do_hilimit(istream & theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput,"hilimit");

  if(!globals.specs.empty())
	globals.specs.back().exactHiLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datalolimit" command
 */
// ----------------------------------------------------------------------

void do_datalolimit(istream & theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput,"datalolimit");

  if(!globals.specs.empty())
	globals.specs.back().dataLoLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datahilimit" command
 */
// ----------------------------------------------------------------------

void do_datahilimit(istream & theInput)
{
  float limit;
  theInput >> limit;

  check_errors(theInput,"datahilimit");

  if(!globals.specs.empty())
	globals.specs.back().dataHiLimit(limit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "datareplace" command
 */
// ----------------------------------------------------------------------

void do_datareplace(istream & theInput)
{
  float src, dst;
  theInput >> src >> dst;

  check_errors(theInput,"datareplace");

  if(!globals.specs.empty())
	globals.specs.back().replace(src,dst);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourdepth" command
 */
// ----------------------------------------------------------------------

void do_contourdepth(istream & theInput)
{
  theInput >> globals.contourdepth;

  check_errors(theInput,"contourdepth");

  if(globals.contourdepth < 0)
	throw runtime_error("Contour depth must be nonnegative");

  if(!globals.specs.empty())
	globals.specs.back().contourDepth(globals.contourdepth);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourinterpolation" command
 */
// ----------------------------------------------------------------------

void do_contourinterpolation(istream & theInput)
{
  theInput >> globals.contourinterpolation;

  check_errors(theInput,"contourinterpolation");

  if(!globals.specs.empty())
	globals.specs.back().contourInterpolation(globals.contourinterpolation);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "contourtriangles" command
 */
// ----------------------------------------------------------------------

void do_contourtriangles(istream & theInput)
{
  theInput >> globals.contourtriangles;

  check_errors(theInput,"contourtriangles");
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "smoother" command
 */
// ----------------------------------------------------------------------

void do_smoother(istream & theInput)
{
  theInput >> globals.smoother;

  check_errors(theInput,"smoother");

  if(!globals.specs.empty())
	globals.specs.back().smoother(globals.smoother);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "smootherradius" command
 */
// ----------------------------------------------------------------------

void do_smootherradius(istream & theInput)
{
  theInput >> globals.smootherradius;

  check_errors(theInput,"smootherradius");

  if(!globals.specs.empty())
	globals.specs.back().smootherRadius(globals.smootherradius);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "smootherfactor" command
 */
// ----------------------------------------------------------------------

void do_smootherfactor(istream & theInput)
{
  theInput >> globals.smootherfactor;

  check_errors(theInput,"smootherfactor");

  if(!globals.specs.empty())
	globals.specs.back().smootherFactor(globals.smootherfactor);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "param" command
 */
// ----------------------------------------------------------------------

void do_param(istream & theInput)
{
  string param;

  theInput >> param;

  check_errors(theInput,"param");

  globals.specs.push_back(ContourSpec(param,
									  globals.contourinterpolation,
									  globals.smoother,
									  globals.contourdepth,
									  globals.smootherradius,
									  globals.smootherfactor));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "shape" command
 */
// ----------------------------------------------------------------------

void do_shape(istream & theInput)
{
  string shapename, arg1;

  theInput >> shapename >> arg1;

  check_errors(theInput,"shape");

  if(arg1=="mark")
	{
	  string marker, markerrule;
	  float markeralpha;
	  theInput >> marker >> markerrule >> markeralpha;

	  ColorTools::checkrule(markerrule);
	  ShapeSpec spec(shapename);
	  spec.marker(marker,markerrule,markeralpha);
	  globals.shapespecs.push_back(spec);
	}
  else
	{
	  string fillcolor = arg1;
	  string strokecolor;
	  theInput >> strokecolor;
	  NFmiColorTools::Color fill = ColorTools::checkcolor(fillcolor);
	  NFmiColorTools::Color stroke = ColorTools::checkcolor(strokecolor);

	  globals.shapespecs.push_back(ShapeSpec(shapename,
											 fill,stroke,
											 globals.fillrule,
											 globals.strokerule));
	}

  check_errors(theInput,"shape");
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "contourfill" command
 */
// ----------------------------------------------------------------------

void do_contourfill(istream & theInput)
{
  string slo,shi,scolor;
  theInput >> slo >> shi >> scolor;

  check_errors(theInput,"contourfill");

  float lo,hi;
  if(slo == "-")
	lo = kFloatMissing;
  else
	lo = NFmiStringTools::Convert<float>(slo);
  if(shi == "-")
	hi = kFloatMissing;
  else
	hi = NFmiStringTools::Convert<float>(shi);

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);

  if(!globals.specs.empty())
	globals.specs.back().add(ContourRange(lo,hi,color,globals.fillrule));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "contourpattern" command
 */
// ----------------------------------------------------------------------

void do_contourpattern(istream & theInput)
{
  string slo,shi,spattern,srule;
  float alpha;
  theInput >> slo >> shi >> spattern >> srule >> alpha;

  check_errors(theInput,"contourpattern");

  float lo,hi;
  if(slo == "-")
	lo = kFloatMissing;
  else
	lo = NFmiStringTools::Convert<float>(slo);
  if(shi == "-")
	hi = kFloatMissing;
  else
	hi = NFmiStringTools::Convert<float>(shi);

  if(!globals.specs.empty())
	globals.specs.back().add(ContourPattern(lo,hi,spattern,srule,alpha));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "contourline" command
 */
// ----------------------------------------------------------------------

void do_contourline(istream & theInput)
{
  string svalue,scolor;
  theInput >> svalue >> scolor;

  check_errors(theInput,"contourline");

  float value;
  if(svalue == "-")
	value = kFloatMissing;
  else
	value = NFmiStringTools::Convert<float>(svalue);

  NFmiColorTools::Color color = ColorTools::checkcolor(scolor);
  if(!globals.specs.empty())
	globals.specs.back().add(ContourValue(value,color,globals.strokerule));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "contourfills" command
 */
// ----------------------------------------------------------------------

void do_contourfills(istream & theInput)
{
  float lo,hi,step;
  string scolor1,scolor2;
  theInput >> lo >> hi >> step >> scolor1 >> scolor2;

  check_errors(theInput,"contourfills");

  int color1 = ColorTools::checkcolor(scolor1);
  int color2 = ColorTools::checkcolor(scolor2);

  int steps = static_cast<int>((hi-lo)/step);

  for(int i=0; i<steps; i++)
	{
	  float tmplo=lo+i*step;
	  float tmphi=lo+(i+1)*step;
	  int color = color1;	// in case steps=1
	  if(steps!=1)
		color = NFmiColorTools::Interpolate(color1,color2,i/(steps-1.0));
	  if(!globals.specs.empty())
		globals.specs.back().add(ContourRange(tmplo,
											  tmphi,
											  color,
											  globals.fillrule));
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "contourlines" command
 */
// ----------------------------------------------------------------------

void do_contourlines(istream & theInput)
{
  float lo,hi,step;
  string scolor1,scolor2;
  theInput >> lo >> hi >> step >> scolor1 >> scolor2;

  check_errors(theInput,"contourlines");

  int color1 = ColorTools::checkcolor(scolor1);
  int color2 = ColorTools::checkcolor(scolor2);

  int steps = static_cast<int>((hi-lo)/step);

  for(int i=0; i<=steps; i++)
	{
	  float tmplo=lo+i*step;
	  int color = color1;	// in case steps=1
	  if(steps!=0)
		color = NFmiColorTools::Interpolate(color1,color2,i/static_cast<float>(steps));
	  if(!globals.specs.empty())
		globals.specs.back().add(ContourValue(tmplo,
											  color,
											  globals.strokerule));
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelmarker" command
 */
// ----------------------------------------------------------------------

void do_labelmarker(istream & theInput)
{
  string filename, rule;
  float alpha;

  theInput >> filename >> rule >> alpha;

  check_errors(theInput,"labelmarker");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelMarker(filename);
	  globals.specs.back().labelMarkerRule(rule);
	  globals.specs.back().labelMarkerAlphaFactor(alpha);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelfont" command
 */
// ----------------------------------------------------------------------

void do_labelfont(istream & theInput)
{
  string font;
  theInput >> font;

  check_errors(theInput,"labelfont");

  if(!globals.specs.empty())
	globals.specs.back().labelFont(font);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelsize" command
 */
// ----------------------------------------------------------------------

void do_labelsize(istream & theInput)
{
  float size;
  theInput >> size;

  check_errors(theInput,"labelsize");

  if(!globals.specs.empty())
	globals.specs.back().labelSize(size);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelstroke" command
 */
// ----------------------------------------------------------------------

void do_labelstroke(istream & theInput)
{
  string color,rule;
  theInput >> color >> rule;

  check_errors(theInput,"labelstroke");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelStrokeColor(ColorTools::checkcolor(color));
	  globals.specs.back().labelStrokeRule(rule);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelfill" command
 */
// ----------------------------------------------------------------------

void do_labelfill(istream & theInput)
{
  string color,rule;
  theInput >> color >> rule;

  check_errors(theInput,"labelfill");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelFillColor(ColorTools::checkcolor(color));
	  globals.specs.back().labelFillRule(rule);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelalign" command
 */
// ----------------------------------------------------------------------

void do_labelalign(istream & theInput)
{
  string align;
  theInput >> align;

  check_errors(theInput,"labelalign");

  if(!globals.specs.empty())
	globals.specs.back().labelAlignment(align);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelformat" command
 */
// ----------------------------------------------------------------------

void do_labelformat(istream & theInput)
{
  string format;
  theInput >> format;

  check_errors(theInput,"labelformat");

  if(format == "-") format = "";
  if(!globals.specs.empty())
	globals.specs.back().labelFormat(format);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelmissing" command
 */
// ----------------------------------------------------------------------

void do_labelmissing(istream & theInput)
{
  string label;
  theInput >> label;

  check_errors(theInput,"labelmissing");

  if(label == "none")
	label = "";

  if(!globals.specs.empty())
	globals.specs.back().labelMissing(label);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labeloffset" command
 */
// ----------------------------------------------------------------------

void do_labeloffset(istream & theInput)
{
  float dx,dy;
  theInput >> dx >> dy;

  check_errors(theInput,"labeloffset");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelOffsetX(dx);
	  globals.specs.back().labelOffsetY(dy);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelangle" command
 */
// ----------------------------------------------------------------------

void do_labelangle(istream & theInput)
{
  float angle;
  theInput >> angle;

  check_errors(theInput,"labelangle");

  if(!globals.specs.empty())
	globals.specs.back().labelAngle(angle);
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelcaption" command
 */
// ----------------------------------------------------------------------

void do_labelcaption(istream & theInput)
{
  string name,align;
  float dx,dy;
  theInput >> name >> dx >> dy >> align;

  check_errors(theInput,"labelcaption");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelCaption(name);
	  globals.specs.back().labelCaptionDX(dx);
	  globals.specs.back().labelCaptionDY(dy);
	  globals.specs.back().labelCaptionAlignment(align);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "label" command
 */
// ----------------------------------------------------------------------

void do_label(istream & theInput)
{
  float lon,lat;
  theInput >> lon >> lat;

  check_errors(theInput,"label");

  if(!globals.specs.empty())
	globals.specs.back().add(NFmiPoint(lon,lat));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelxy" command
 */
// ----------------------------------------------------------------------

void do_labelxy(istream & theInput)
{
  float lon,lat;
  int dx, dy;
  theInput >> lon >> lat >> dx >> dy;

  check_errors(theInput,"labelxy");

  if(!globals.specs.empty())
	globals.specs.back().add(NFmiPoint(lon,lat),NFmiPoint(dx,dy));
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labels" command
 */
// ----------------------------------------------------------------------

void do_labels(istream & theInput)
{
  int dx,dy;
  theInput >> dx >> dy;

  check_errors(theInput,"labels");

  if(!globals.specs.empty())
	{
	  globals.specs.back().labelDX(dx);
	  globals.specs.back().labelDY(dy);
	}
}

// ----------------------------------------------------------------------
/*!
 * \bried Handle "labelfile" command
 */
// ----------------------------------------------------------------------

void do_labelfile(istream & theInput)
{
  string datafilename;
  theInput >> datafilename;

  check_errors(theInput,"labelfile");

  ifstream datafile(datafilename.c_str());
  if(!datafile)
	throw runtime_error("No data file named " + datafilename);
  string datacommand;
  while( datafile >> datacommand)
	{
	  if(datacommand == "#" || datacommand == "//")
		datafile.ignore(numeric_limits<std::streamsize>::max(),'\n');
	  else if(datacommand == "label")
		{
		  float lon,lat;
		  datafile >> lon >> lat;
		  if(!globals.specs.empty())
			globals.specs.back().add(NFmiPoint(lon,lat));
		}
	  else
		throw runtime_error("Unknown datacommand " + datacommand);
	}
  datafile.close();
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "clear" command
 */
// ----------------------------------------------------------------------

void do_clear(istream & theInput)
{
  string command;

  theInput >> command;

  check_errors(theInput,"clear");

  if(command=="contours")
	globals.specs.clear();
  else if(command=="shapes")
	globals.shapespecs.clear();
  else if(command=="cache")
	globals.calculator.clearCache();
  else if(command=="arrows")
	{
	  globals.arrowpoints.clear();
	  globals.windarrowdx = 0;
	  globals.windarrowdy = 0;
	}
  else if(command=="labels")
	{
	  list<ContourSpec>::iterator it;
	  for(it=globals.specs.begin(); it!=globals.specs.end(); ++it)
		it->clearLabels();
	}
  else
	throw runtime_error("Unknown clear target: " + command);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw shapes" command
 */
// ----------------------------------------------------------------------

void do_draw_shapes(istream & theInput)
{
  // The output filename

  string filename;
  theInput >> filename;

  check_errors(theInput,"draw shapes");

  auto_ptr<NFmiArea> area;

  if(globals.projection.empty())
	throw runtime_error("No projection has been specified for rendering shapes");
  else
	area.reset(NFmiAreaFactory::Create(globals.projection).release());

  if(globals.verbose)
	cout << "Area corners are"
		 << endl
		 << "bottomleft\t= "
		 << area->BottomLeftLatLon().X()
		 << ','
		 << area->BottomLeftLatLon().Y()
		 << endl
		 << "topright\t= "
		 << area->TopRightLatLon().X()
		 << ','
		 << area->TopRightLatLon().Y()
		 << endl;

  int imgwidth = static_cast<int>(area->Width()+0.5);
  int imgheight = static_cast<int>(area->Height()+0.5);

  // Initialize the background

  NFmiImage image(imgwidth, imgheight);
  globals.setImageModes(image);

  NFmiColorTools::Color erasecolor = ColorTools::checkcolor(globals.erase);
  image.Erase(erasecolor);

  // Draw all the shapes

  list<ShapeSpec>::const_iterator iter;
  list<ShapeSpec>::const_iterator begin = globals.shapespecs.begin();
  list<ShapeSpec>::const_iterator end   = globals.shapespecs.end();

  for(iter=begin; iter!=end; ++iter)
	{
	  NFmiGeoShape geo(iter->filename(),kFmiGeoShapeEsri);
	  geo.ProjectXY(*area);

	  if(iter->marker()=="")
		{
		  NFmiColorTools::NFmiBlendRule fillrule = ColorTools::checkrule(iter->fillrule());
		  NFmiColorTools::NFmiBlendRule strokerule = ColorTools::checkrule(iter->strokerule());
		  geo.Fill(image,iter->fillcolor(),fillrule);
		  geo.Stroke(image,iter->strokecolor(),strokerule);
		}
	  else
		{
		  NFmiColorTools::NFmiBlendRule markerrule = ColorTools::checkrule(iter->markerrule());

		  NFmiImage marker;
		  marker.Read(iter->marker());
		  geo.Mark(image,marker,markerrule,
				   kFmiAlignCenter,
				   iter->markeralpha());
		}
	}

  string outfile = filename + "." + globals.format;
  if(globals.verbose)
	cout << "Writing " << outfile << endl;
  if(globals.format=="png")
	image.WritePng(outfile);
  else if(globals.format=="jpg" || globals.format=="jpeg")
	image.WriteJpeg(outfile);
  else if(globals.format=="gif")
	image.WriteGif(outfile);
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw imagemap" command
 */
// ----------------------------------------------------------------------

void do_draw_imagemap(istream & theInput)
{
  // The relevant field name and filenames

  string fieldname, filename;
  theInput >> fieldname >> filename;

  check_errors(theInput,"draw imagemap");

  auto_ptr<NFmiArea> area;

  if(globals.projection.empty())
	throw runtime_error("No projection has been specified for rendering shapes");
  else
	area.reset(NFmiAreaFactory::Create(globals.projection).release());

  // Generate map from all shapes in the list

  string outfile = filename + ".map";
  ofstream out(outfile.c_str());
  if(!out)
	throw runtime_error("Failed to open "+outfile+" for writing");

  if(globals.verbose)
	cout << "Writing " << outfile << endl;

  list<ShapeSpec>::const_iterator iter;
  list<ShapeSpec>::const_iterator begin = globals.shapespecs.begin();
  list<ShapeSpec>::const_iterator end   = globals.shapespecs.end();

  for(iter=begin; iter!=end; ++iter)
	{
	  NFmiGeoShape geo(iter->filename(),kFmiGeoShapeEsri);
	  geo.ProjectXY(*area);
	  geo.WriteImageMap(out,fieldname);
	}
  out.close();
}

// ----------------------------------------------------------------------
/*!
 * \brief Handle "draw contours" command
 */
// ----------------------------------------------------------------------

void do_draw_contours(istream & theInput)
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

  if(globals.querystreams.empty())
	throw runtime_error("No query data has been read!");

  auto_ptr<NFmiArea> area;

  if(globals.projection.empty())
	throw runtime_error("No projection has been specified for rendering shapes");
  else
	area.reset(NFmiAreaFactory::Create(globals.projection).release());

  // This message intentionally ignores globals.verbose

  if(!globals.background.empty())
	cout << "Contouring for background " << globals.background << endl;

  if(globals.verbose)
	cout << "Area corners are"
		 << endl
		 << "bottomleft\t= "
		 << area->BottomLeftLatLon().X()
		 << ','
		 << area->BottomLeftLatLon().Y()
		 << endl
		 << "topright\t= "
		 << area->TopRightLatLon().X()
		 << ','
		 << area->TopRightLatLon().Y()
		 << endl;

  // Establish querydata timelimits and initialize
  // the XY-coordinates simultaneously.

  // Note that we use world-coordinates when smoothing
  // so that we can use meters as the smoothing radius.
  // Also, this means the contours are independent of
  // the image size.

  NFmiTime utctime, time1, time2;

  NFmiDataMatrix<float> vals;

  unsigned int qi;
  for(qi=0; qi<globals.querystreams.size(); qi++)
	{
	  // Initialize the queryinfo

	  globals.queryinfo = globals.querystreams[qi];
	  globals.queryinfo->FirstLevel();
	  if(globals.querydatalevel>0)
		{
		  int level = globals.querydatalevel;
		  while(--level > 0)
			globals.queryinfo->NextLevel();
		}

	  // Establish time limits
	  globals.queryinfo->LastTime();
	  utctime = globals.queryinfo->ValidTime();
	  NFmiTime t2 = TimeTools::ConvertZone(utctime,globals.timestampzone);
	  globals.queryinfo->FirstTime();
	  utctime = globals.queryinfo->ValidTime();
	  NFmiTime t1 = TimeTools::ConvertZone(utctime,globals.timestampzone);

	  if(qi==0)
		{
		  time1 = t1;
		  time2 = t2;
		}
	  else
		{
		  if(time1.IsLessThan(t1))
			time1 = t1;
		  if(!time2.IsLessThan(t2))
			time2 = t2;
		}

	}

  if(globals.verbose)
	{
	  cout << "Data start time " << time1 << endl
		   << "Data end time " << time2 << endl;
	}

  // Skip to first time

  NFmiMetTime tmptime(time1,
					  globals.timesteprounding ?
					  (globals.timestep>0 ? globals.timestep : 1) :
					  1);

  tmptime.ChangeByMinutes(globals.timestepskip);
  if(globals.timesteprounding)
	tmptime.PreviousMetTime();
  NFmiTime t = tmptime;

  // Loop over all times

  int imagesdone = 0;
  bool labeldxdydone = false;
  for(;;)
	{
	  if(imagesdone>=globals.timesteps)
		break;

	  // Skip to next time to be drawn

	  t.ChangeByMinutes(globals.timestep > 0 ? globals.timestep : 1);

	  // If the time is after time2, we're done

	  if(time2.IsLessThan(t))
		break;

	  // Search first time >= the desired time
	  // This is quaranteed to succeed since we've
	  // already tested against time2, the last available
	  // time.

	  bool ok = true;
	  for(qi=0; ok && qi<globals.querystreams.size(); qi++)
		{
		  globals.queryinfo = globals.querystreams[qi];
		  globals.queryinfo->ResetTime();
		  while(globals.queryinfo->NextTime())
			{
			  NFmiTime utc = globals.queryinfo->ValidTime();
			  NFmiTime loc = TimeTools::ConvertZone(utc,globals.timestampzone);
			  if(!loc.IsLessThan(t))
				break;
			}
		  NFmiTime utc = globals.queryinfo->ValidTime();
		  NFmiTime tnow = TimeTools::ConvertZone(utc,globals.timestampzone);

		  // we wanted

		  if(globals.timestep==0)
			t = tnow;

		  // If time is before time1, ignore it

		  if(t.IsLessThan(time1))
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

		  if(globals.filter=="none")
			{
			  // Cannot draw time with filter none
			  // if time is not exact.

			  ok = isexact;

			}
		  else if(globals.filter=="linear")
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

	  if(!ok)
		continue;

	  // The image is accepted for rendering, but
	  // we might not overwrite an existing one.
	  // Hence we update the counter here already.

	  imagesdone++;

	  // Create the filename

	  // The timestamp as a string

	  NFmiString datatimestr = t.ToStr(kYYYYMMDDHHMM);

	  if(globals.verbose)
		cout << "Time is " << datatimestr.CharPtr() << endl;

	  string filename =
		globals.savepath
		+ "/"
		+ globals.prefix
		+ datatimestr.CharPtr();

	  if(globals.timestampflag)
		{
		  for(qi=0; qi<globals.queryfilenames.size(); qi++)
			{
			  time_t secs = NFmiFileSystem::FileModificationTime(globals.queryfilenames[qi]);
			  NFmiTime tlocal(secs);
			  filename += "_" + tlocal.ToStr(kDDHHMM);
			}
		}

	  filename +=
		globals.suffix
		+ "."
		+ globals.format;

	  // In force-mode we always write, but otherwise
	  // we first check if the output image already
	  // exists. If so, we assume it is up to date
	  // and skip to the next time stamp.

	  if(!globals.force && !NFmiFileSystem::FileEmpty(filename))
		{
		  if(globals.verbose)
			cout << "Not overwriting " << filename << endl;
		  continue;
		}

	  // Initialize the background

	  int imgwidth = static_cast<int>(area->Width()+0.5);
	  int imgheight = static_cast<int>(area->Height()+0.5);

	  NFmiImage image(imgwidth,imgheight);
	  globals.setImageModes(image);

	  NFmiColorTools::Color erasecolor = ColorTools::checkcolor(globals.erase);
	  image.Erase(erasecolor);

	  if(!globals.background.empty())
		image = globals.backgroundimage;

	  // Loop over all parameters

	  list<ContourSpec>::iterator piter;
	  list<ContourSpec>::iterator pbegin = globals.specs.begin();
	  list<ContourSpec>::iterator pend   = globals.specs.end();

	  for(piter=pbegin; piter!=pend; ++piter)
		{
		  // Establish the parameter

		  string name = piter->param();

		  bool ismeta = false;
		  ok = false;
		  FmiParameterName param = FmiParameterName(NFmiEnumConverter().ToEnum(name));

		  if(param==kFmiBadParameter)
			{
			  if(!MetaFunctions::isMeta(name))
				throw runtime_error("Unknown parameter "+name);
			  ismeta = true;
			  ok = true;
			  // We always assume the first querydata is ok
			  qi = 0;
			  globals.queryinfo = globals.querystreams[0];
			}
		  else
			{
			  // Find the proper queryinfo to be used
			  // Note that qi will be used later on for
			  // getting the coordinate matrices

			  for(qi=0; qi<globals.querystreams.size(); qi++)
				{
				  globals.queryinfo = globals.querystreams[qi];
				  globals.queryinfo->Param(param);
				  ok = globals.queryinfo->IsParamUsable();
				  if(ok) break;
				}
			}

		  if(!ok)
			throw runtime_error("The parameter is not usable: " + name);

		  if(globals.verbose)
			{
			  cout << "Param " << name << " from queryfile number "
				   << (qi+1) << endl;
			}

		  // Establish the contour method

		  string interpname = piter->contourInterpolation();
		  NFmiContourTree::NFmiContourInterpolation interp
			= NFmiContourTree::ContourInterpolationValue(interpname);
		  if(interp==NFmiContourTree::kFmiContourMissingInterpolation)
			throw runtime_error("Unknown contour interpolation method " + interpname);

		  // Get the values.

		  if(!ismeta)
			globals.queryinfo->Values(vals);
		  else
			vals = MetaFunctions::values(piter->param(),
										 globals.queryinfo);

		  // Replace values if so requested

		  if(piter->replace())
			vals.Replace(piter->replaceSourceValue(),piter->replaceTargetValue());

		  if(globals.filter=="none")
			{
			  // The time is known to be exact
			}
		  else if(globals.filter=="linear")
			{
			  NFmiTime utc = globals.queryinfo->ValidTime();
			  NFmiTime tnow = TimeTools::ConvertZone(utc,globals.timestampzone);
			  bool isexact = t.IsEqual(tnow);

			  if(!isexact)
				{
				  NFmiDataMatrix<float> tmpvals;
				  NFmiTime t2utc = globals.queryinfo->ValidTime();
				  NFmiTime t2 = TimeTools::ConvertZone(t2utc,globals.timestampzone);
				  globals.queryinfo->PreviousTime();
				  NFmiTime t1utc = globals.queryinfo->ValidTime();
				  NFmiTime t1 = TimeTools::ConvertZone(t1utc,globals.timestampzone);
				  if(!ismeta)
					globals.queryinfo->Values(tmpvals);
				  else
					tmpvals = MetaFunctions::values(piter->param(), globals.queryinfo);
				  if(piter->replace())
					tmpvals.Replace(piter->replaceSourceValue(),
									piter->replaceTargetValue());

				  // Data from t1,t2, we want t

				  long offset = t.DifferenceInMinutes(t1);
				  long range = t2.DifferenceInMinutes(t1);

				  float weight = (static_cast<float>(offset))/range;

				  vals.LinearCombination(tmpvals,weight,1-weight);

				}
			}
		  else
			{
			  NFmiTime tprev = t;
			  tprev.ChangeByMinutes(-globals.timeinterval);

			  NFmiDataMatrix<float> tmpvals;
			  int steps = 1;
			  for(;;)
				{
				  globals.queryinfo->PreviousTime();
				  NFmiTime utc = globals.queryinfo->ValidTime();
				  NFmiTime tnow = TimeTools::ConvertZone(utc,globals.timestampzone);
				  if(tnow.IsLessThan(tprev))
					break;

				  steps++;
				  if(!ismeta)
					globals.queryinfo->Values(tmpvals);
				  else
					tmpvals = MetaFunctions::values(piter->param(), globals.queryinfo);
				  if(piter->replace())
					tmpvals.Replace(piter->replaceSourceValue(),
									piter->replaceTargetValue());

				  if(globals.filter=="min")
					vals.Min(tmpvals);
				  else if(globals.filter=="max")
					vals.Max(tmpvals);
				  else if(globals.filter=="mean")
					vals += tmpvals;
				  else if(globals.filter=="sum")
					vals += tmpvals;
				}

			  if(globals.filter=="mean")
				vals /= steps;
			}


		  // Smoothen the values

		  NFmiSmoother smoother(piter->smoother(),
								piter->smootherFactor(),
								piter->smootherRadius());

		  shared_ptr<NFmiDataMatrix<NFmiPoint> > worldpts = globals.queryinfo->LocationsWorldXY(*area);
		  vals = smoother.Smoothen(*worldpts,vals);

		  // Find the minimum and maximum

		  float valmin = kFloatMissing;
		  float valmax = kFloatMissing;
		  for(unsigned int j=0; j<vals.NY(); j++)
			for(unsigned int i=0; i<vals.NX(); i++)
			  if(vals[i][j]!=kFloatMissing)
				{
				  if(valmin==kFloatMissing || vals[i][j]<valmin)
					valmin = vals[i][j];
				  if(valmax==kFloatMissing || vals[i][j]>valmax)
					valmax = vals[i][j];
				}

		  if(globals.verbose)
			cout << "Data range for " << name << " is " << valmin << "," << valmax << endl;

		  // Setup the contourer with the values

		  globals.calculator.data(vals);

		  // Save the data values at desired points for later
		  // use, this lets us avoid using InterpolatedValue()
		  // which does not use smoothened values.

		  // First, however, if this is the first image, we add
		  // the grid points to the set of points, if so requested

		  if(!labeldxdydone && piter->labelDX() > 0 && piter->labelDY() > 0)
			{
			  for(unsigned int j=0; j<worldpts->NY(); j+=piter->labelDY())
				for(unsigned int i=0; i<worldpts->NX(); i+=piter->labelDX())
				  piter->add(area->WorldXYToLatLon((*worldpts)[i][j]));
			}

		  piter->clearLabelValues();
		  if((piter->labelFormat() != "") &&
			 !piter->labelPoints().empty() )
			{
			  list<pair<NFmiPoint,NFmiPoint> >::const_iterator iter;

			  for(iter=piter->labelPoints().begin();
				  iter!=piter->labelPoints().end();
				  ++iter)
				{
				  NFmiPoint latlon = iter->first;
				  NFmiPoint ij = globals.queryinfo->LatLonToGrid(latlon);

				  float value;

				  if(fabs(ij.X()-FmiRound(ij.X()))<0.00001 &&
					 fabs(ij.Y()-FmiRound(ij.Y()))<0.00001)
					{
					  value = vals[FmiRound(ij.X())][FmiRound(ij.Y())];
					}
				  else
					{
					  int i = static_cast<int>(ij.X()); // rounds down
					  int j = static_cast<int>(ij.Y());
					  float v00 = vals.At(i,j,kFloatMissing);
					  float v10 = vals.At(i+1,j,kFloatMissing);
					  float v01 = vals.At(i,j+1,kFloatMissing);
					  float v11 = vals.At(i+1,j+1,kFloatMissing);
					  if(!globals.queryinfo->BiLinearInterpolation(ij.X(),
																   ij.Y(),
																   value,
																   v00,v10,
																   v01,v11))
						value = kFloatMissing;

					}
				  piter->addLabelValue(value);
				}
			}

		  // Fill the contours

		  list<ContourRange>::const_iterator citer;
		  list<ContourRange>::const_iterator cbegin;
		  list<ContourRange>::const_iterator cend;

		  cbegin = piter->contourFills().begin();
		  cend   = piter->contourFills().end();

		  for(citer=cbegin ; citer!=cend; ++citer)
			{
			  // Skip to next contour if this one is outside
			  // the value range. As a special case
			  // min=max=missing is ok, if both the limits
			  // are missing too. That is, when we are
			  // contouring missing values.

			  if(valmin==kFloatMissing || valmax==kFloatMissing)
				{
				  if(citer->lolimit()!=kFloatMissing &&
					 citer->hilimit()!=kFloatMissing)
					continue;
				}
			  else
				{
				  if(citer->lolimit()!=kFloatMissing &&
					 valmax<citer->lolimit())
					continue;
				  if(citer->hilimit()!=kFloatMissing &&
					 valmin>citer->hilimit())
					continue;
				}

			  bool exactlo = true;
			  bool exacthi = (citer->hilimit()!=kFloatMissing &&
							  piter->exactHiLimit()!=kFloatMissing &&
							  citer->hilimit()==piter->exactHiLimit());

			  NFmiPath path =
				globals.calculator.contour(*globals.queryinfo,
										   citer->lolimit(),
										   citer->hilimit(),
										   exactlo,
										   exacthi,
										   piter->dataLoLimit(),
										   piter->dataHiLimit(),
										   piter->contourDepth(),
										   interp,
										   globals.contourtriangles);

			  if(globals.verbose && globals.calculator.wasCached())
				cout << "Using cached "
					 << citer->lolimit() << " - "
					 << citer->hilimit() << endl;

			  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(citer->rule());
			  path.Project(area.get());
			  path.Fill(image,citer->color(),rule);

			}

		  // Fill the contours with patterns

		  list<ContourPattern>::const_iterator patiter;
		  list<ContourPattern>::const_iterator patbegin;
		  list<ContourPattern>::const_iterator patend;

		  patbegin = piter->contourPatterns().begin();
		  patend   = piter->contourPatterns().end();

		  for(patiter=patbegin ; patiter!=patend; ++patiter)
			{
			  // Skip to next contour if this one is outside
			  // the value range. As a special case
			  // min=max=missing is ok, if both the limits
			  // are missing too. That is, when we are
			  // contouring missing values.

			  if(valmin==kFloatMissing || valmax==kFloatMissing)
				{
				  if(patiter->lolimit()!=kFloatMissing &&
					 patiter->hilimit()!=kFloatMissing)
					continue;
				}
			  else
				{
				  if(patiter->lolimit()!=kFloatMissing &&
					 valmax<patiter->lolimit())
					continue;
				  if(patiter->hilimit()!=kFloatMissing &&
					 valmin>patiter->hilimit())
					continue;
				}

			  bool exactlo = true;
			  bool exacthi = (patiter->hilimit()!=kFloatMissing &&
							  piter->exactHiLimit()!=kFloatMissing &&
							  patiter->hilimit()==piter->exactHiLimit());

			  NFmiPath path =
				globals.calculator.contour(*globals.queryinfo,
										   patiter->lolimit(),
										   patiter->hilimit(),
										   exactlo, exacthi,
										   piter->dataLoLimit(),
										   piter->dataHiLimit(),
										   piter->contourDepth(),
										   interp,
										   globals.contourtriangles);

			  if(globals.verbose && globals.calculator.wasCached())
				cout << "Using cached "
					 << patiter->lolimit() << " - "
					 << patiter->hilimit() << endl;

			  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(patiter->rule());
			  NFmiImage pattern(patiter->pattern());

			  path.Project(area.get());
			  path.Fill(image,pattern,rule,patiter->factor());

			}

		  // Stroke the contours

		  list<ContourValue>::const_iterator liter;
		  list<ContourValue>::const_iterator lbegin;
		  list<ContourValue>::const_iterator lend;

		  lbegin = piter->contourValues().begin();
		  lend   = piter->contourValues().end();

		  for(liter=lbegin ; liter!=lend; ++liter)
			{
			  // Skip to next contour if this one is outside
			  // the value range.

			  if(valmin!=kFloatMissing && valmax!=kFloatMissing)
				{
				  if(liter->value()!=kFloatMissing &&
					 valmax<liter->value())
					continue;
				  if(liter->value()!=kFloatMissing &&
					 valmin>liter->value())
					continue;
				}

			  NFmiPath path =
				globals.calculator.contour(*globals.queryinfo,
										   liter->value(),
										   kFloatMissing,
										   true, false,
										   piter->dataLoLimit(),
										   piter->dataHiLimit(),
										   piter->contourDepth(),
										   interp,
										   globals.contourtriangles);

			  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(liter->rule());
			  path.Project(area.get());
			  path.SimplifyLines(10);
			  path.Stroke(image,liter->color(),rule);

			}
		}

	  // Bang the foreground

	  if(!globals.foreground.empty())
		{
		  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(globals.foregroundrule);

		  image.Composite(globals.foregroundimage,rule,kFmiAlignNorthWest,0,0,1);

		}

	  // Draw wind arrows if so requested

	  NFmiEnumConverter converter;
	  if((!globals.arrowpoints.empty() ||
		  (globals.windarrowdx!=0 && globals.windarrowdy!=0)) &&
		 (globals.arrowfile!=""))
		{

		  FmiParameterName param = FmiParameterName(NFmiEnumConverter().ToEnum(globals.directionparam));
		  if(param==kFmiBadParameter)
			throw runtime_error("Unknown parameter "+globals.directionparam);

		  // Find the proper queryinfo to be used
		  // Note that qi will be used later on for
		  // getting the coordinate matrices

		  ok = false;
		  for(qi=0; qi<globals.querystreams.size(); qi++)
			{
			  globals.queryinfo = globals.querystreams[qi];
			  globals.queryinfo->Param(param);
			  ok = globals.queryinfo->IsParamUsable();
			  if(ok) break;
			}

		  if(!ok)
			throw runtime_error("Parameter is not usable: " + globals.directionparam);

		  // Read the arrow definition

		  NFmiPath arrowpath;
		  if(globals.arrowfile != "meteorological")
			{
			  ifstream arrow(globals.arrowfile.c_str());
			  if(!arrow)
				throw runtime_error("Could not open " + globals.arrowfile);
			  // Read in the entire file
			  string pathstring = NFmiStringTools::ReadFile(arrow);
			  arrow.close();

			  // Convert to a path

			  arrowpath.Add(pathstring);
			}

		  // Handle all given coordinates

		  list<NFmiPoint>::const_iterator iter;

		  for(iter=globals.arrowpoints.begin();
			  iter!=globals.arrowpoints.end();
			  ++iter)
			{

			  // The start point
			  NFmiPoint xy0 = area->ToXY(*iter);

			  // Skip rendering if the start point is masked

			  if(IsMasked(xy0,globals.mask,globals.maskimage))
				continue;

			  float dir = globals.queryinfo->InterpolatedValue(*iter);
			  if(dir==kFloatMissing)	// ignore missing
				continue;

			  float speed = -1;

			  if(globals.queryinfo->Param(FmiParameterName(converter.ToEnum(globals.speedparam))))
				speed = globals.queryinfo->InterpolatedValue(*iter);
			  globals.queryinfo->Param(FmiParameterName(converter.ToEnum(globals.directionparam)));


			  // Direction calculations

			  const float pi = 3.141592658979323;
			  const float length = 0.1;	// degrees

			  float x1 = iter->X()+sin(dir*pi/180)*length;
			  float y1 = iter->Y()+cos(dir*pi/180)*length;

			  NFmiPoint xy1 = area->ToXY(NFmiPoint(x1,y1));

			  // Calculate the actual angle

			  float alpha = atan2(xy1.X()-xy0.X(),
								  xy1.Y()-xy0.Y());

			  // Create a new path

			  NFmiPath thispath;

			  if(globals.arrowfile == "meteorological")
				thispath.Add(GramTools::metarrow(speed*globals.windarrowscaleC));
			  else
				thispath.Add(arrowpath);

			  if(speed>0 && speed!=kFloatMissing)
				thispath.Scale(globals.windarrowscaleA*log10(globals.windarrowscaleB*speed+1)+globals.windarrowscaleC);
			  thispath.Scale(globals.arrowscale);
			  thispath.Rotate(alpha*180/pi);
			  thispath.Translate(xy0.X(),xy0.Y());

			  // And render it

			  thispath.Fill(image,
							ColorTools::checkcolor(globals.arrowfillcolor),
							ColorTools::checkrule(globals.arrowfillrule));
			  thispath.Stroke(image,
							  ColorTools::checkcolor(globals.arrowstrokecolor),
							  ColorTools::checkrule(globals.arrowstrokerule));
			}

		  // Draw the full grid if so desired

		  if(globals.windarrowdx!=0 && globals.windarrowdy!=0)
			{

			  NFmiDataMatrix<float> speedvalues(vals.NX(),vals.NY(),-1);
			  if(globals.queryinfo->Param(FmiParameterName(converter.ToEnum(globals.speedparam))))
				globals.queryinfo->Values(speedvalues);
			  globals.queryinfo->Param(FmiParameterName(converter.ToEnum(globals.directionparam)));

			  shared_ptr<NFmiDataMatrix<NFmiPoint> > worldpts = globals.queryinfo->LocationsWorldXY(*area);
			  for(unsigned int j=0; j<worldpts->NY(); j+=globals.windarrowdy)
				for(unsigned int i=0; i<worldpts->NX(); i+=globals.windarrowdx)
				  {
					// The start point

					NFmiPoint latlon = area->WorldXYToLatLon((*worldpts)[i][j]);
					NFmiPoint xy0 = area->ToXY(latlon);

					// Skip rendering if the start point is masked
					if(IsMasked(xy0,
								globals.mask,
								globals.maskimage))
					  continue;

					float dir = vals[i][j];
					if(dir==kFloatMissing)	// ignore missing
					  continue;

					float speed = speedvalues[i][j];

					// Direction calculations

					const float pi = 3.141592658979323;
					const float length = 0.1;	// degrees

					float x0 = latlon.X();
					float y0 = latlon.Y();

					float x1 = x0+sin(dir*pi/180)*length;
					float y1 = y0+cos(dir*pi/180)*length;

					NFmiPoint xy1 = area->ToXY(NFmiPoint(x1,y1));

					// Calculate the actual angle

					float alpha = atan2(xy1.X()-xy0.X(),
										xy1.Y()-xy0.Y());

					// Create a new path

					NFmiPath thispath;
					if(globals.arrowfile == "meteorological")
					  thispath.Add(GramTools::metarrow(speed*globals.windarrowscaleC));
					else
					  thispath.Add(arrowpath);
					if(speed>0 && speed != kFloatMissing)
					  thispath.Scale(globals.windarrowscaleA*log10(globals.windarrowscaleB*speed+1)+globals.windarrowscaleC);
					thispath.Scale(globals.arrowscale);
					thispath.Rotate(alpha*180/pi);
					thispath.Translate(xy0.X(),xy0.Y());

					// And render it

					thispath.Fill(image,
								  ColorTools::checkcolor(globals.arrowfillcolor),
								  ColorTools::checkrule(globals.arrowfillrule));
					thispath.Stroke(image,
									ColorTools::checkcolor(globals.arrowstrokecolor),
									ColorTools::checkrule(globals.arrowstrokerule));
				  }
			}
		}

	  // Draw labels

	  for(piter=pbegin; piter!=pend; ++piter)
		{

		  // Draw label markers first

		  if(!piter->labelMarker().empty())
			{
			  // Establish that something is to be done

			  if(piter->labelPoints().empty())
				continue;

			  // Establish the marker specs

			  NFmiImage marker;
			  marker.Read(piter->labelMarker());

			  NFmiColorTools::NFmiBlendRule markerrule = ColorTools::checkrule(piter->labelMarkerRule());

			  float markeralpha = piter->labelMarkerAlphaFactor();

			  // Draw individual points

			  unsigned int pointnumber = 0;
			  list<pair<NFmiPoint,NFmiPoint> >::const_iterator iter;
			  for(iter=piter->labelPoints().begin();
				  iter!=piter->labelPoints().end();
				  ++iter)
				{
				  // The point in question

				  NFmiPoint xy = area->ToXY(iter->first);

				  // Skip rendering if the start point is masked

				  if(IsMasked(xy,
							  globals.mask,
							  globals.maskimage))
					continue;

				  // Skip rendering if LabelMissing is "" and value is missing
				  if(piter->labelMissing().empty())
					{
					  float value = piter->labelValues()[pointnumber++];
					  if(value == kFloatMissing)
						continue;
					}

				  image.Composite(marker,
								  markerrule,
								  kFmiAlignCenter,
								  FmiRound(xy.X()),
								  FmiRound(xy.Y()),
								  markeralpha);
				}

			}

		  // Label markers now drawn, only label texts remain

		  // Quick exit from loop if no labels are
		  // desired for this parameter

		  if(piter->labelFormat() == "")
			continue;

		  // Create the font object to be used

		  NFmiFontHershey font(piter->labelFont());

		  // Create the text object to be used

		  NFmiText text("",
						font,
						piter->labelSize(),
						0.0,	// x
						0.0,	// y
						AlignmentValue(piter->labelAlignment()),
						piter->labelAngle());


		  NFmiText caption(piter->labelCaption(),
						   font,
						   piter->labelSize(),
						   0.0,
						   0.0,
						   AlignmentValue(piter->labelCaptionAlignment()),
						   piter->labelAngle());

		  // The rules

		  NFmiColorTools::NFmiBlendRule fillrule
			= ColorTools::checkrule(piter->labelFillRule());

		  NFmiColorTools::NFmiBlendRule strokerule
			= ColorTools::checkrule(piter->labelStrokeRule());

		  // Draw labels at specifing latlon points if requested

		  list<pair<NFmiPoint,NFmiPoint> >::const_iterator iter;

		  int pointnumber = 0;
		  for(iter=piter->labelPoints().begin();
			  iter!=piter->labelPoints().end();
			  ++iter)
			{

			  // The point in question

			  float x,y;
			  if(iter->second.X() == kFloatMissing)
				{
				  NFmiPoint xy = area->ToXY(iter->first);
				  x = xy.X();
				  y = xy.Y();
				}
			  else
				{
				  x = iter->second.X();
				  y = iter->second.Y();
				}

			  // Skip rendering if the start point is masked

			  if(IsMasked(NFmiPoint(x,y),
						  globals.mask,
						  globals.maskimage))
				continue;

			  float value = piter->labelValues()[pointnumber++];

			  // Convert value to string
			  string strvalue = piter->labelMissing();

			  if(value!=kFloatMissing)
				{
				  char tmp[20];
				  sprintf(tmp,piter->labelFormat().c_str(),value);
				  strvalue = tmp;
				}

			  // Don't bother drawing empty strings
			  if(strvalue.empty())
				continue;

			  // Set new text properties

			  text.Text(strvalue);
			  text.X(x + piter->labelOffsetX());
			  text.Y(y + piter->labelOffsetY());

			  // And render the text

			  text.Fill(image,piter->labelFillColor(),fillrule);
			  text.Stroke(image,piter->labelStrokeColor(),strokerule);

			  // Then the label caption

			  if(!piter->labelCaption().empty())
				{
				  caption.X(text.X() + piter->labelCaptionDX());
				  caption.Y(text.Y() + piter->labelCaptionDY());
				  caption.Fill(image,piter->labelFillColor(),fillrule);
				  caption.Stroke(image,piter->labelStrokeColor(),strokerule);
				}

			}

		}



	  // Bang the combine image (legend, logo, whatever)

	  if(!globals.combine.empty())
		{
		  NFmiColorTools::NFmiBlendRule rule = ColorTools::checkrule(globals.combinerule);

		  image.Composite(globals.combineimage,
						  rule,
						  kFmiAlignNorthWest,
						  globals.combinex,
						  globals.combiney,
						  globals.combinefactor);

		}

	  // Finally, draw a time stamp on the image if so
	  // requested

	  string thestamp = "";

	  {
		int obsyy = t.GetYear();
		int obsmm = t.GetMonth();
		int obsdd = t.GetDay();
		int obshh = t.GetHour();
		int obsmi = t.GetMin();

		// Interpretation: The age of the forecast is the age
		// of the oldest forecast

		NFmiTime tfor;
		for(qi=0; qi<globals.querystreams.size(); qi++)
		  {
			globals.queryinfo = globals.querystreams[qi];
			NFmiTime futctime = globals.queryinfo->OriginTime();
			NFmiTime tlocal = TimeTools::ConvertZone(futctime,globals.timestampzone);
			if(qi==0 || tlocal.IsLessThan(tfor))
			  tfor = tlocal;
		  }

		int foryy = tfor.GetYear();
		int formm = tfor.GetMonth();
		int fordd = tfor.GetDay();
		int forhh = tfor.GetHour();
		int formi = tfor.GetMin();

		char buffer[100];

		if(globals.timestampimage == "obs")
		  {
			// hh:mi dd.mm.yyyy
			sprintf(buffer,"%02d:%02d %02d.%02d.%04d",
					obshh,obsmi,obsdd,obsmm,obsyy);
			thestamp = buffer;
		  }
		else if(globals.timestampimage == "for")
		  {
			// hh:mi dd.mm.yyyy
			sprintf(buffer,"%02d:%02d %02d.%02d.%04d",
					forhh,formi,fordd,formm,foryy);
			thestamp = buffer;
		  }
		else if(globals.timestampimage == "forobs")
		  {
			// hh:mi dd.mm.yyyy +hh
			long diff = t.DifferenceInMinutes(tfor);
			if(diff%60==0 && globals.timestep%60==0)
			  sprintf(buffer,"%02d.%02d.%04d %02d:%02d %s%ldh",
					  fordd,formm,foryy,forhh,formi,
					  (diff<0 ? "" : "+"), diff/60);
			else
			  sprintf(buffer,"%02d.%02d.%04d %02d:%02d %s%ldm",
					  fordd,formm,foryy,forhh,formi,
					  (diff<0 ? "" : "+"), diff);
			thestamp = buffer;
		  }
	  }

	  if(!thestamp.empty())
		{
		  NFmiFontHershey font("TimesRoman-Bold");

		  int x = globals.timestampimagex;
		  int y = globals.timestampimagey;

		  if(x<0) x+= image.Width();
		  if(y<0) y+= image.Height();

		  NFmiText text(thestamp,font,14,x,y,kFmiAlignNorthWest,0.0);

		  // And render the text

		  NFmiPath path = text.Path();

		  NFmiEsriBox box = path.BoundingBox();

		  NFmiPath rect;
		  int w = 4;
		  rect.MoveTo(box.Xmin()-w,box.Ymin()-w);
		  rect.LineTo(box.Xmax()+w,box.Ymin()-w);
		  rect.LineTo(box.Xmax()+w,box.Ymax()+w);
		  rect.LineTo(box.Xmin()-w,box.Ymax()+w);
		  rect.CloseLineTo();

		  rect.Fill(image,
					NFmiColorTools::MakeColor(180,180,180,32),
					NFmiColorTools::kFmiColorOver);

		  path.Stroke(image,
					  NFmiColorTools::Black,
					  NFmiColorTools::kFmiColorCopy);

		}

	  // dx and dy labels have now been extracted into a list,
	  // disable adding them again and again and again..

	  labeldxdydone = true;

	  // Save

	  if(globals.verbose)
		cout << "Writing " << filename << endl;
	  if(globals.format=="png")
		image.WritePng(filename);
	  else if(globals.format=="jpg" || globals.format=="jpeg")
		image.WriteJpeg(filename);
	  else if(globals.format=="gif")
		image.WriteGif(filename);
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

  parse_command_line(argc,argv);

  // Process all command files
  // ~~~~~~~~~~~~~~~~~~~~~~~~~

  list<string>::const_iterator fileiter = globals.cmdline_files.begin();
  for( ; fileiter!=globals.cmdline_files.end(); ++fileiter)
    {
      // Get the script to be executed

      if(globals.verbose)
		cout << "Processing file: " << *fileiter << endl;

	  string text = read_script(*fileiter);
	  text = preprocess_script(text);

      // Process the commands

	  istringstream input(text);
      string command;
      while( input >> command)
		{
		  // Handle comments

		  if(command == "#")						do_comment(input);
		  else if(command[0] == '#')				do_comment(input);
		  else if(command == "//")					do_comment(input);
		  else if(command == "cache")				do_cache(input);
		  else if(command == "querydata")			do_querydata(input);
		  else if(command == "querydatalevel")		do_querydatalevel(input);
		  else if(command == "filter")				do_filter(input);
		  else if(command == "timestepskip")		do_timestepskip(input);
		  else if(command == "timestep")			do_timestep(input);
		  else if(command == "timeinterval")		do_timeinterval(input);
		  else if(command == "timesteps")			do_timesteps(input);
		  else if(command == "timestamp")			do_timestamp(input);
		  else if(command == "timestampzone")		do_timestampzone(input);
		  else if(command == "timesteprounding")	do_timesteprounding(input);
		  else if(command == "timestampimage")		do_timestampimage(input);
		  else if(command == "timestampimagexy")	do_timestampimagexy(input);
		  else if(command == "projection")			do_projection(input);
		  else if(command == "erase")				do_erase(input);
		  else if(command == "fillrule")			do_fillrule(input);
		  else if(command == "strokerule")			do_strokerule(input);
		  else if(command == "directionparam")		do_directionparam(input);
		  else if(command == "speedparam")			do_speedparam(input);
		  else if(command == "arrowscale")			do_arrowscale(input);
		  else if(command == "windarrowscale")		do_windarrowscale(input);
		  else if(command == "arrowfill")			do_arrowfill(input);
		  else if(command == "arrowstroke")			do_arrowstroke(input);
		  else if(command == "arrowpath")			do_arrowpath(input);
		  else if(command == "windarrow")			do_windarrow(input);
		  else if(command == "windarrows")			do_windarrows(input);
		  else if(command == "background")			do_background(input);
		  else if(command == "foreground")			do_foreground(input);
		  else if(command == "mask")				do_mask(input);
		  else if(command == "combine")				do_combine(input);
		  else if(command == "foregroundrule")		do_foregroundrule(input);
		  else if(command == "savepath")			do_savepath(input);
		  else if(command == "prefix")				do_prefix(input);
		  else if(command == "suffix")				do_suffix(input);
		  else if(command == "format")				do_format(input);
		  else if(command == "gamma")				do_gamma(input);
		  else if(command == "intent")				do_intent(input);
		  else if(command == "pngquality")			do_pngquality(input);
		  else if(command == "jpegquality")			do_jpegquality(input);
		  else if(command == "savealpha")			do_savealpha(input);
		  else if(command == "wantpalette")			do_wantpalette(input);
		  else if(command == "forcepalette")		do_forcepalette(input);
		  else if(command == "alphalimit")			do_alphalimit(input);
		  else if(command == "hilimit")				do_hilimit(input);
		  else if(command == "datalolimit")			do_datalolimit(input);
		  else if(command == "datahilimit")			do_datahilimit(input);
		  else if(command == "datareplace")			do_datareplace(input);
		  else if(command == "contourdepth")		do_contourdepth(input);
		  else if(command == "contourinterpolation") do_contourinterpolation(input);
		  else if(command == "contourtriangles")	do_contourtriangles(input);
		  else if(command == "smoother")			do_smoother(input);
		  else if(command == "smootherradius")		do_smootherradius(input);
		  else if(command == "smootherfactor")		do_smootherfactor(input);
		  else if(command == "param")				do_param(input);
		  else if(command == "shape")				do_shape(input);
		  else if(command == "contourfill")			do_contourfill(input);
		  else if(command == "contourpattern")		do_contourpattern(input);
		  else if(command == "contourline")			do_contourline(input);
		  else if(command == "contourfills")		do_contourfills(input);
		  else if(command == "contourlines")		do_contourlines(input);
		  else if(command == "labelmarker")			do_labelmarker(input);
		  else if(command == "labelfont")			do_labelfont(input);
		  else if(command == "labelsize")			do_labelsize(input);
		  else if(command == "labelstroke")			do_labelstroke(input);
		  else if(command == "labelfill")			do_labelfill(input);
		  else if(command == "labelalign")			do_labelalign(input);
		  else if(command == "labelformat")			do_labelformat(input);
		  else if(command == "labelmissing")		do_labelmissing(input);
		  else if(command == "labelangle")			do_labelangle(input);
		  else if(command == "labeloffset")			do_labeloffset(input);
		  else if(command == "labelcaption")		do_labelcaption(input);
		  else if(command == "label")				do_label(input);
		  else if(command == "labelxy")				do_labelxy(input);
		  else if(command == "labels")				do_labels(input);
		  else if(command == "labelfile")			do_labelfile(input);
		  else if(command == "clear")				do_clear(input);

		  else if(command == "draw")
			{
			  input >> command;

			  if(command == "shapes")				do_draw_shapes(input);
			  else if(command == "imagemap")		do_draw_imagemap(input);
			  else if(command == "contours")		do_draw_contours(input);
			  else
				throw runtime_error("draw " + command + " not implemented");
			}
		  else
			throw runtime_error("Unknown command " + command);
		}
    }
  return 0;
}

// ----------------------------------------------------------------------
// Main program.
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
	{
	  return domain(argc, argv);
	}
  catch(const runtime_error & e)
	{
	  cerr << "Error: qdcontour failed due to" << endl
		   << "--> " << e.what() << endl;
	  return 1;
	}
}

// ======================================================================
