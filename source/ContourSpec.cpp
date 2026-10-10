// ======================================================================
/*!
 * \file
 * \brief Implementation of class ContourSpec
 */
// ======================================================================

#include "ContourSpec.h"
#include "NoiseTools.h"

#include <imagine/NFmiColorTools.h>

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 *
 * \param theParam The parameter
 * \param theInterpolation The interpolation method
 * \param theSmoother The smoothing method
 * \param theLevel The level
 * \param theSmootherRadius The smoother search radius
 * \param theSmootherFactor The smoother sharpness factor
 * \param theHiLimit The absolute high limit of the data
 */
// ----------------------------------------------------------------------

ContourSpec::ContourSpec(const std::string &theParam,
                         const std::string &theInterpolation,
                         const std::string &theSmoother,
                         int theLevel,
                         float theSmootherRadius,
                         int theSmootherFactor,
                         float theHiLimit)
    : itsParam(theParam),
      itsLevel(theLevel),
      itsContourInterpolation(theInterpolation),
      itsSmoother(theSmoother),
      itsSmootherRadius(theSmootherRadius),
      itsSmootherFactor(theSmootherFactor),
      itsExactHiLimit(theHiLimit),
      itsDataLoLimit(kFloatMissing),
      itsDataHiLimit(kFloatMissing),
      itHasReplace(false),
      itHasDespeckle(false),
      itsLabelMarker(""),
      itsLabelMarkerRule("Copy"),
      itsLabelMarkerAlphaFactor(1.0),
      itsLabelFont("misc/6x13B.pcf.gz:6x13"),
      itsLabelColor(Imagine::NFmiColorTools::Black),
      itsLabelRule("OnOpaque"),
      itsLabelAlignment("Center"),
      itsLabelFormat("%.1f"),
      itsLabelMissing("-"),
      itsLabelOffsetX(0),
      itsLabelOffsetY(0),
      itsLabelDX(0),
      itsLabelDY(0),
      itsLabelXyX0(0),
      itsLabelXyY0(0),
      itsLabelXyDX(0),
      itsLabelXyDY(0),
      itsLabelCaption(""),
      itsLabelCaptionDX(0),
      itsLabelCaptionDY(0),
      itsLabelCaptionAlignment("West"),
      itsContourLabelFont("misc/5x8.pcf.gz:5x8"),
      itsContourLabelColor(Imagine::NFmiColorTools::Black),
      itsContourLabelBackgroundColor(Imagine::NFmiColorTools::MakeColor(180, 180, 180, 32)),
      itsContourLabelBackgroundXMargin(2),
      itsContourLabelBackgroundYMargin(2),
      itsContourLabelTexts(),
      itsOverlay()
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Return ContourRange specifications
 *
 * \return Reference to the internal list of ContourRange objects
 */
// ----------------------------------------------------------------------

const std::list<ContourRange>& ContourSpec::contourFills() const
{
  return itsContourFills;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return ContourPattern specifications
 *
 * \return Reference to the internal list of ContourPattern objects
 */
// ----------------------------------------------------------------------

const std::list<ContourPattern>& ContourSpec::contourPatterns() const
{
  return itsContourPatterns;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return ContourValue specifications
 *
 * \return Reference to the internal list of ContourValue objects
 */
// ----------------------------------------------------------------------

const std::list<ContourValue>& ContourSpec::contourValues() const
{
  return itsContourValues;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return ContourSymbol specifications
 *
 * \return Reference to the internal list of ContourSymbol objects
 */
// ----------------------------------------------------------------------

const std::list<ContourSymbol>& ContourSpec::contourSymbols() const
{
  return itsContourSymbols;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return ContourFont specifications
 *
 * \return Reference to the internal list of ContourFont objects
 */
// ----------------------------------------------------------------------

const std::list<ContourFont>& ContourSpec::contourFonts() const
{
  return itsContourFonts;
}
// ----------------------------------------------------------------------
/*!
 * \brief Add a contour label text mapping
 */
// ----------------------------------------------------------------------

void ContourSpec::addContourLabelText(float theValue, const std::string &theText)
{
  itsContourLabelTexts[theValue] = theText;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return ContourLabelText map
 *
 * \return Reference to the internal map
 */
// ----------------------------------------------------------------------

const std::map<float, std::string> &ContourSpec::contourLabelTexts() const
{
  return itsContourLabelTexts;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return ContourLabel specifications
 *
 * \return Reference to the internal list of ContourLabel objects
 */
// ----------------------------------------------------------------------

const std::list<ContourLabel>& ContourSpec::contourLabels() const
{
  return itsContourLabels;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the parameter name
 *
 * \return The parameter name
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::param() const
{
  return itsParam;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the level
 *
 * \return The level value
 */
// ----------------------------------------------------------------------

int ContourSpec::level() const
{
  return itsLevel;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the interpolation method
 *
 * \return The interpolation method
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::contourInterpolation() const
{
  return itsContourInterpolation;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the smoother name
 *
 * \return The smoother
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::smoother() const
{
  return itsSmoother;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the smoother radius
 *
 * \return The radius
 */
// ----------------------------------------------------------------------

float ContourSpec::smootherRadius() const
{
  return itsSmootherRadius;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the smoother sharpness factor
 *
 * \return The factor
 */
// ----------------------------------------------------------------------

int ContourSpec::smootherFactor() const
{
  return itsSmootherFactor;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the upper limit for data values
 *
 * \return The upper limit
 */
// ----------------------------------------------------------------------

float ContourSpec::exactHiLimit() const
{
  return itsExactHiLimit;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the data upper limit
 *
 * \return The limit
 */
// ----------------------------------------------------------------------

float ContourSpec::dataHiLimit() const
{
  return itsDataHiLimit;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the data lower limit
 *
 * \return The limit
 */
// ----------------------------------------------------------------------

float ContourSpec::dataLoLimit() const
{
  return itsDataLoLimit;
}
// ----------------------------------------------------------------------
/*!
 * \brief Set the level
 *
 * \param theValue The level value
 */
// ----------------------------------------------------------------------

void ContourSpec::level(int theValue) { itsLevel = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the interpolation method
 *
 * \param theValue The method
 */
// ----------------------------------------------------------------------

void ContourSpec::contourInterpolation(const std::string &theValue)
{
  itsContourInterpolation = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the smoother
 *
 * \param theValue The method
 */
// ----------------------------------------------------------------------

void ContourSpec::smoother(const std::string &theValue) { itsSmoother = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the smoother radius
 *
 * \param theRadius The radius
 */
// ----------------------------------------------------------------------

void ContourSpec::smootherRadius(float theRadius) { itsSmootherRadius = theRadius; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the smoother sharpness factor
 *
 * \param theFactor The factor
 */
// ----------------------------------------------------------------------

void ContourSpec::smootherFactor(int theFactor) { itsSmootherFactor = theFactor; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the data upper limit
 *
 * \param theLimit The upper limit
 */
// ----------------------------------------------------------------------

void ContourSpec::exactHiLimit(float theLimit) { itsExactHiLimit = theLimit; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the data lower limit
 *
 * \param theLimit The lower limit
 */
// ----------------------------------------------------------------------

void ContourSpec::dataLoLimit(float theLimit) { itsDataLoLimit = theLimit; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the data upper limit
 *
 * \param theLimit The upper limit
 */
// ----------------------------------------------------------------------

void ContourSpec::dataHiLimit(float theLimit) { itsDataHiLimit = theLimit; }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourRange specification
 *
 * \param theRange The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourRange &theRange) { itsContourFills.push_back(theRange); }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourValue specification
 *
 * \param theValue The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourValue &theValue) { itsContourValues.push_back(theValue); }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourPattern specification
 *
 * \param theValue The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourPattern &theValue) { itsContourPatterns.push_back(theValue); }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourSymbol specification
 *
 * \param theValue The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourSymbol &theValue) { itsContourSymbols.push_back(theValue); }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourFont specification
 *
 * \param theValue The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourFont &theValue) { itsContourFonts.push_back(theValue); }
// ----------------------------------------------------------------------
/*!
 * \brief Add a new ContourLabel specification
 *
 * \param theLabel The specification to add
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const ContourLabel &theLabel) { itsContourLabels.push_back(theLabel); }
// ----------------------------------------------------------------------
/*!
 * \brief Return whether a data replacement command is set or not
 *
 * \return True if a replacement has been set
 */
// ----------------------------------------------------------------------

bool ContourSpec::replace() const
{
  return itHasReplace;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the replacement source value
 *
 * \return The value being replaced
 */
// ----------------------------------------------------------------------

float ContourSpec::replaceSourceValue() const
{
  return itsReplaceSourceValue;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the replacement destination value
 *
 * \return The new value
 */
// ----------------------------------------------------------------------

float ContourSpec::replaceTargetValue() const
{
  return itsReplaceTargetValue;
}
// ----------------------------------------------------------------------
/*!
 * \brief Set a replacement
 *
 * \param theSrcValue The old value
 * \param theDstValue The new value
 */
// ----------------------------------------------------------------------

void ContourSpec::replace(float theSrcValue, float theDstValue)
{
  itHasReplace = true;
  itsReplaceSourceValue = theSrcValue;
  itsReplaceTargetValue = theDstValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return label points
 *
 * \return A reference to the internal list of points
 */
// ----------------------------------------------------------------------

const std::list<std::pair<NFmiPoint, NFmiPoint>>& ContourSpec::labelPoints() const
{
  return itsLabelPoints;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return pixel label points
 *
 * \return A reference to the internal list of points
 */
// ----------------------------------------------------------------------

const std::list<std::pair<NFmiPoint, float>>& ContourSpec::pixelLabels() const
{
  return itsPixelLabels;
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new label point
 *
 * \param thePoint The geographic coordinates
 * \param theXY The xy-adjustment on the pixmap
 */
// ----------------------------------------------------------------------

void ContourSpec::add(const NFmiPoint &thePoint, const NFmiPoint theXY)
{
  itsLabelPoints.emplace_back(thePoint, theXY);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new pixel label
 *
 * \param thePoint The pixel coordinate
 * \param theValue The value
 */
// ----------------------------------------------------------------------

void ContourSpec::addPixelLabel(const NFmiPoint &thePoint, float theValue)
{
  itsPixelLabels.emplace_back(thePoint, theValue);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the label values
 *
 * \return The label values
 */
// ----------------------------------------------------------------------

const std::vector<float>& ContourSpec::labelValues() const
{
  return itsLabelValues;
}
// ----------------------------------------------------------------------
/*!
 * \brief Add a new label value
 *
 * \param theValue The new value
 */
// ----------------------------------------------------------------------

void ContourSpec::addLabelValue(float theValue) { itsLabelValues.push_back(theValue); }
// ----------------------------------------------------------------------
/*!
 * \brief Clear all label values
 *
 */
// ----------------------------------------------------------------------

void ContourSpec::clearLabelValues()
{
  itsLabelValues.clear();
}
// ----------------------------------------------------------------------
/*!
 * \brief Clear all pixel labels
 *
 */
// ----------------------------------------------------------------------

void ContourSpec::clearPixelLabels()
{
  itsPixelLabels.clear();
}
// ----------------------------------------------------------------------
/*!
 * \brief Clear all labels
 *
 */
// ----------------------------------------------------------------------

void ContourSpec::clearLabels()
{
  itsLabelPoints.clear();
  itsLabelValues.clear();
  itsPixelLabels.clear();
  itsLabelDX = 0;
  itsLabelDY = 0;
  itsLabelXyX0 = 0;
  itsLabelXyY0 = 0;
  itsLabelXyDX = 0;
  itsLabelXyDY = 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the label marker image
 *
 * \return The marker image filename
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelMarker() const
{
  return itsLabelMarker;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label marker blending rule
 *
 * \return The blending rule
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelMarkerRule() const
{
  return itsLabelMarkerRule;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label marker alpha blending factor
 *
 * \return The blending factor
 */
// ----------------------------------------------------------------------

float ContourSpec::labelMarkerAlphaFactor() const
{
  return itsLabelMarkerAlphaFactor;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label font
 *
 * \return The font name
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelFont() const
{
  return itsLabelFont;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label color
 *
 * \return The color
 */
// ----------------------------------------------------------------------

int ContourSpec::labelColor() const
{
  return itsLabelColor;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label rule
 *
 * \return The rule
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelRule() const
{
  return itsLabelRule;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label alignment
 *
 * \return The alignment
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelAlignment() const
{
  return itsLabelAlignment;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label number format
 *
 * \return The number format
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelFormat() const
{
  return itsLabelFormat;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label for missing values
 *
 * \return The missing value text
 */
// ----------------------------------------------------------------------

const std::string& ContourSpec::labelMissing() const
{
  return itsLabelMissing;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label X-offset
 *
 * \return The offset
 */
// ----------------------------------------------------------------------

float ContourSpec::labelOffsetX() const
{
  return itsLabelOffsetX;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label Y-offset
 *
 * \return The offset
 */
// ----------------------------------------------------------------------

float ContourSpec::labelOffsetY() const
{
  return itsLabelOffsetY;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label X-step
 *
 * \return The step
 */
// ----------------------------------------------------------------------

float ContourSpec::labelDX() const
{
  return itsLabelDX;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label Y-step
 *
 * \return The step
 */
// ----------------------------------------------------------------------

float ContourSpec::labelDY() const
{
  return itsLabelDY;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the pixellabel X-origin
 *
 * \return The origin X-coordinate
 */
// ----------------------------------------------------------------------

float ContourSpec::labelXyX0() const
{
  return itsLabelXyX0;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the pixellabel Y-origin
 *
 * \return The origin Y-coordinate
 */
// ----------------------------------------------------------------------

float ContourSpec::labelXyY0() const
{
  return itsLabelXyY0;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the pixellabel X-step
 *
 * \return The X-step
 */
// ----------------------------------------------------------------------

float ContourSpec::labelXyDX() const
{
  return itsLabelXyDX;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the pixellabel Y-step
 *
 * \return The Y-step
 */
// ----------------------------------------------------------------------

float ContourSpec::labelXyDY() const
{
  return itsLabelXyDY;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label caption
 *
 * \return The caption
 */
// ----------------------------------------------------------------------

std::string ContourSpec::labelCaption() const
{
  return itsLabelCaption;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label caption X-offset
 *
 * \return The offset
 */
// ----------------------------------------------------------------------

float ContourSpec::labelCaptionDX() const
{
  return itsLabelCaptionDX;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label caption Y-offset
 *
 * \return The offset
 */
// ----------------------------------------------------------------------

float ContourSpec::labelCaptionDY() const
{
  return itsLabelCaptionDY;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the label caption alignment
 *
 * \return The alignment
 */
// ----------------------------------------------------------------------

std::string ContourSpec::labelCaptionAlignment() const
{
  return itsLabelCaptionAlignment;
}
// ----------------------------------------------------------------------
/*!
 * \brief Set the label marker filename
 *
 * \param theValue The filename
 */
// ----------------------------------------------------------------------

void ContourSpec::labelMarker(const std::string &theValue) { itsLabelMarker = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label marker blending rule
 *
 * \param theValue The rule
 */
// ----------------------------------------------------------------------

void ContourSpec::labelMarkerRule(const std::string &theValue) { itsLabelMarkerRule = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label marker alpha blending factor
 *
 * \param theValue The factor
 */
// ----------------------------------------------------------------------

void ContourSpec::labelMarkerAlphaFactor(float theValue) { itsLabelMarkerAlphaFactor = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label font
 *
 * \param theValue The font name
 */
// ----------------------------------------------------------------------

void ContourSpec::labelFont(const std::string &theValue) { itsLabelFont = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label color
 *
 * \param theValue The color
 */
// ----------------------------------------------------------------------

void ContourSpec::labelColor(int theValue) { itsLabelColor = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label rule
 *
 * \param theValue The rule
 */
// ----------------------------------------------------------------------

void ContourSpec::labelRule(const std::string &theValue) { itsLabelRule = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label alignment
 *
 * \param theValue The alignment
 */
// ----------------------------------------------------------------------

void ContourSpec::labelAlignment(const std::string &theValue) { itsLabelAlignment = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label number format
 *
 * \param theValue The format
 */
// ----------------------------------------------------------------------

void ContourSpec::labelFormat(const std::string &theValue) { itsLabelFormat = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label for missing values
 *
 * \param theValue The label
 */
// ----------------------------------------------------------------------

void ContourSpec::labelMissing(const std::string &theValue) { itsLabelMissing = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label X-offset
 *
 * \param theValue The offset
 */
// ----------------------------------------------------------------------

void ContourSpec::labelOffsetX(float theValue) { itsLabelOffsetX = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label Y-offset
 *
 * \param theValue The offset
 */
// ----------------------------------------------------------------------

void ContourSpec::labelOffsetY(float theValue) { itsLabelOffsetY = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label X-step
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelDX(float theValue) { itsLabelDX = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label Y-step
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelDY(float theValue) { itsLabelDY = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the pixellabel origin X-coordinate
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelXyX0(float theValue) { itsLabelXyX0 = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the pixellabel origin Y-coordinate
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelXyY0(float theValue) { itsLabelXyY0 = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the pixellabel X-step
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelXyDX(float theValue) { itsLabelXyDX = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the pixellabel Y-step
 *
 * \param theValue The step
 */
// ----------------------------------------------------------------------

void ContourSpec::labelXyDY(float theValue) { itsLabelXyDY = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label caption
 *
 * \param theValue The caption
 */
// ----------------------------------------------------------------------

void ContourSpec::labelCaption(const std::string &theValue) { itsLabelCaption = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label caption X-offset
 *
 * \param theValue The offset
 */
// ----------------------------------------------------------------------

void ContourSpec::labelCaptionDX(float theValue) { itsLabelCaptionDX = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label caption Y-offset
 *
 * \param theValue The offset
 */
// ----------------------------------------------------------------------

void ContourSpec::labelCaptionDY(float theValue) { itsLabelCaptionDY = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the label caption alignment
 *
 * \param theValue The alignment
 */
// ----------------------------------------------------------------------

void ContourSpec::labelCaptionAlignment(const std::string &theValue)
{
  itsLabelCaptionAlignment = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the contour label font
 *
 * \param theValue The font description
 */
// ----------------------------------------------------------------------

void ContourSpec::contourLabelFont(const std::string &theValue) { itsContourLabelFont = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the contour label color
 *
 * \param theValue The color
 */
// ----------------------------------------------------------------------

void ContourSpec::contourLabelColor(int theValue) { itsContourLabelColor = theValue; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the contour label background color
 *
 * \param theValue The color
 */
// ----------------------------------------------------------------------

void ContourSpec::contourLabelBackgroundColor(int theValue)
{
  itsContourLabelBackgroundColor = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the contour label background x-margin
 *
 * \param theValue The x-margin
 */
// ----------------------------------------------------------------------

void ContourSpec::contourLabelBackgroundXMargin(int theValue)
{
  itsContourLabelBackgroundXMargin = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the contour label background y-margin
 *
 * \param theValue The y-margin
 */
// ----------------------------------------------------------------------

void ContourSpec::contourLabelBackgroundYMargin(int theValue)
{
  itsContourLabelBackgroundYMargin = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the contour label font
 *
 * \return The font description
 */
// ----------------------------------------------------------------------

const std::string &ContourSpec::contourLabelFont() const { return itsContourLabelFont; }
// ----------------------------------------------------------------------
/*!
 * \brief Get the contour label color
 *
 * \return The color
 */
// ----------------------------------------------------------------------

int ContourSpec::contourLabelColor() const { return itsContourLabelColor; }
// ----------------------------------------------------------------------
/*!
 * \brief Get the contour label background color
 *
 * \return The color
 */
// ----------------------------------------------------------------------

int ContourSpec::contourLabelBackgroundColor() const { return itsContourLabelBackgroundColor; }
// ----------------------------------------------------------------------
/*!
 * \brief Get the contour label background x-margin
 *
 * \return The x-margin
 */
// ----------------------------------------------------------------------

int ContourSpec::contourLabelBackgroundXMargin() const { return itsContourLabelBackgroundXMargin; }
// ----------------------------------------------------------------------
/*!
 * \brief Get the contour label background y-margin
 *
 * \return The y-margin
 */
// ----------------------------------------------------------------------

int ContourSpec::contourLabelBackgroundYMargin() const { return itsContourLabelBackgroundYMargin; }
// ----------------------------------------------------------------------
/*!
 * \brief Enable despeckling
 */
// ----------------------------------------------------------------------

void ContourSpec::despeckle(
    float theLoLimit, float theHiLimit, int theRadius, float theWeight, int theIterations)
{
  itHasDespeckle = true;
  itsDespeckleLoLimit = theLoLimit;
  itsDespeckleHiLimit = theHiLimit;
  itsDespeckleRadius = theRadius;
  itsDespeckleWeight = theWeight;
  itsDespeckleIterations = theIterations;
}

// ----------------------------------------------------------------------
/*!
 * \brief Despeckle the data
 */
// ----------------------------------------------------------------------

void ContourSpec::despeckle(NFmiDataMatrix<float> &theValues) const
{
  if (!itHasDespeckle) return;

  NoiseTools::despeckle(theValues,
                        itsDespeckleLoLimit,
                        itsDespeckleHiLimit,
                        itsDespeckleRadius,
                        itsDespeckleWeight,
                        itsDespeckleIterations);
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the overlay
 */
// ----------------------------------------------------------------------

const std::string &ContourSpec::overlay() const { return itsOverlay; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the overlay
 */
// ----------------------------------------------------------------------

void ContourSpec::overlay(const std::string &theOverlay) { itsOverlay = theOverlay; }
// ======================================================================
