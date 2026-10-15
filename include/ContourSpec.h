// ----------------------------------------------------------------------
// Yksittäisen parametrin piirto-ohjeet
// ----------------------------------------------------------------------

#ifndef CONTOURSPEC_H
#define CONTOURSPEC_H

#include "ContourFont.h"
#include "ContourLabel.h"
#include "ContourPattern.h"
#include "ContourRange.h"
#include "ContourSymbol.h"
#include "ContourValue.h"

#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiPoint.h>

#include <list>
#include <string>
#include <vector>

class ContourSpec
{
 public:
#ifdef COMPILER_GENERATED
  ~ContourSpec();
  ContourSpec(const ContourSpec &theValue);
  ContourSpec &operator=(const ContourSpec &theValue);
#endif

  ContourSpec(const std::string &theParam,
              const std::string &theInterpolation,
              const std::string &theSmoother,
              int theLevel = -1,
              float theSmootherRadius = 1.0,
              int theSmootherFactor = 1,
              float theHiLimit = kFloatMissing);

  const std::list<ContourRange>& contourFills() const;
  const std::list<ContourPattern>& contourPatterns() const;
  const std::list<ContourValue>& contourValues() const;
  const std::list<ContourSymbol>& contourSymbols() const;
  const std::list<ContourLabel>& contourLabels() const;
  const std::list<ContourFont>& contourFonts() const;
  const std::map<float, std::string> &contourLabelTexts() const;

  const std::string& param() const;
  int level() const;
  const std::string& contourInterpolation() const;
  const std::string& smoother() const;
  float smootherRadius() const;
  int smootherFactor() const;
  float exactHiLimit() const;
  float dataHiLimit() const;
  float dataLoLimit() const;

  const std::string& overlay() const;

  void level(int theValue);
  void contourInterpolation(const std::string &theValue);
  void smoother(const std::string &theValue);
  void smootherRadius(float theRadius);
  void smootherFactor(int theFactor);
  void exactHiLimit(float theLimit);
  void overlay(const std::string &theOverlay);

  void dataLoLimit(float theLimit);
  void dataHiLimit(float theLimit);

  void add(const ContourRange &theRange);
  void add(const ContourValue &theValue);
  void add(const ContourPattern &theValue);
  void add(const ContourSymbol &theValue);
  void add(const ContourLabel &theValue);
  void add(const ContourFont &theValue);

  // This was done to replace 32700 with -1 in PrecipitationForm

  bool replace() const;
  float replaceSourceValue() const;
  float replaceTargetValue() const;

  void replace(float theSrcValue, float theDstValue);

  // Despeckling

  void despeckle(
      float theLoLimit, float theHiLimit, int theRadius, float theWeight, int theIterations);

  void despeckle(NFmiDataMatrix<float> &theValues) const;

  // Label specific methods

  const std::list<std::pair<NFmiPoint, NFmiPoint>>& labelPoints() const;

  void add(const NFmiPoint &thePoint,
           const NFmiPoint theXY = NFmiPoint(kFloatMissing, kFloatMissing));

  const std::vector<float>& labelValues() const;

  // labelsxy command methods
  const std::list<std::pair<NFmiPoint, float>>& pixelLabels() const;
  void addPixelLabel(const NFmiPoint &thePoint, float value);
  void clearPixelLabels();

  void addLabelValue(float theValue);
  void clearLabelValues();
  void clearLabels();

  const std::string& labelMarker() const;
  const std::string& labelMarkerRule() const;
  float labelMarkerAlphaFactor() const;
  const std::string& labelFont() const;
  int labelColor() const;
  const std::string& labelRule() const;

  const std::string& labelAlignment() const;
  const std::string& labelFormat() const;
  const std::string& labelMissing() const;
  float labelOffsetX() const;
  float labelOffsetY() const;
  float labelDX() const;
  float labelDY() const;

  float labelXyX0() const;
  float labelXyY0() const;
  float labelXyDX() const;
  float labelXyDY() const;

  std::string labelCaption() const;
  float labelCaptionDX() const;
  float labelCaptionDY() const;
  std::string labelCaptionAlignment() const;

  void labelMarker(const std::string &theValue);
  void labelMarkerRule(const std::string &theValue);
  void labelMarkerAlphaFactor(float theValue);
  void labelFont(const std::string &theValue);
  void labelColor(int theValue);
  void labelRule(const std::string &theValue);
  void labelFillRule(const std::string &theValue);
  void labelAlignment(const std::string &theValue);
  void labelFormat(const std::string &theValue);
  void labelMissing(const std::string &theValue);
  void labelOffsetX(float theValue);
  void labelOffsetY(float theValue);
  void labelDX(float theValue);
  void labelDY(float theValue);
  void labelXyX0(float theValue);
  void labelXyY0(float theValue);
  void labelXyDX(float theValue);
  void labelXyDY(float theValue);

  void labelCaption(const std::string &theValue);
  void labelCaptionDX(float theValue);
  void labelCaptionDY(float theValue);
  void labelCaptionAlignment(const std::string &theValue);

  const std::string &contourLabelFont() const;
  int contourLabelColor() const;
  int contourLabelBackgroundColor() const;
  int contourLabelBackgroundXMargin() const;
  int contourLabelBackgroundYMargin() const;

  void contourLabelFont(const std::string &theValue);
  void contourLabelColor(int theValue);
  void contourLabelBackgroundColor(int theValue);
  void contourLabelBackgroundXMargin(int theValue);
  void contourLabelBackgroundYMargin(int theValue);

  void addContourLabelText(float theValue, const std::string &theText);

 private:
  ContourSpec();

  std::string itsParam;
  int itsLevel;
  std::string itsContourInterpolation;
  std::string itsSmoother;
  float itsSmootherRadius;
  int itsSmootherFactor;

  std::list<ContourRange> itsContourFills;
  std::list<ContourValue> itsContourValues;
  std::list<ContourPattern> itsContourPatterns;
  std::list<ContourSymbol> itsContourSymbols;
  std::list<ContourLabel> itsContourLabels;
  std::list<ContourFont> itsContourFonts;

  float itsExactHiLimit;
  float itsDataLoLimit;
  float itsDataHiLimit;

  bool itHasReplace;
  float itsReplaceSourceValue = 0.0F;
  float itsReplaceTargetValue = 0.0F;

  bool itHasDespeckle;
  float itsDespeckleLoLimit = 0.0F;
  float itsDespeckleHiLimit = 0.0F;
  int itsDespeckleRadius = 0;
  float itsDespeckleWeight = 0.0F;
  int itsDespeckleIterations = 0;

  // LatLon, optional label location pairs in pixels
  std::list<std::pair<NFmiPoint, NFmiPoint>> itsLabelPoints;
  // Respective values calculated while contouring
  std::vector<float> itsLabelValues;

  // And then labels for specific XY-coordinates
  std::list<std::pair<NFmiPoint, float>> itsPixelLabels;

  std::string itsLabelMarker;
  std::string itsLabelMarkerRule;
  float itsLabelMarkerAlphaFactor;
  std::string itsLabelFont;
  int itsLabelColor;
  std::string itsLabelRule;
  std::string itsLabelAlignment;
  std::string itsLabelFormat;
  std::string itsLabelMissing;
  float itsLabelOffsetX;
  float itsLabelOffsetY;
  float itsLabelDX;
  float itsLabelDY;
  float itsLabelXyX0;
  float itsLabelXyY0;
  float itsLabelXyDX;
  float itsLabelXyDY;

  std::string itsLabelCaption;
  float itsLabelCaptionDX;
  float itsLabelCaptionDY;
  std::string itsLabelCaptionAlignment;

  std::string itsContourLabelFont;
  int itsContourLabelColor;
  int itsContourLabelBackgroundColor;
  int itsContourLabelBackgroundXMargin;
  int itsContourLabelBackgroundYMargin;
  std::map<float, std::string> itsContourLabelTexts;

  std::string itsOverlay;
};

#endif  // CONTOURSPEC_H

// ======================================================================
