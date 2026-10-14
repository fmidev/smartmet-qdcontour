// ======================================================================
/*!
 * \file
 * \brief Implementation of class ShapeSpec
 */
// ======================================================================

#include "ShapeSpec.h"

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 *
 */
// ----------------------------------------------------------------------

ShapeSpec::ShapeSpec(const std::string& theShapeFile,
                     Imagine::NFmiColorTools::Color theFill,
                     Imagine::NFmiColorTools::Color theStroke,
                     const std::string& theFillRule,
                     const std::string& theStrokeRule)
    : itsShapeFileName(theShapeFile),
      itsFillRule(theFillRule),
      itsStrokeRule(theStrokeRule),
      itsFillColor(theFill),
      itsStrokeColor(theStroke),
      itsMarkerRule("Over"),
      itsMarkerAlpha(1.0)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the filename of the shape file
 *
 * \return The shapefile
 */
// ----------------------------------------------------------------------

const std::string& ShapeSpec::filename() const
{
  return itsShapeFileName;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the fill rule for the shape
 *
 * \return The fill rule
 */
// ----------------------------------------------------------------------

const std::string& ShapeSpec::fillrule() const
{
  return itsFillRule;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the stroke rule for the shape
 *
 * \return The stroke rule
 */
// ----------------------------------------------------------------------

const std::string& ShapeSpec::strokerule() const
{
  return itsStrokeRule;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the fill color for the shape
 *
 * \return The fill color
 */
// ----------------------------------------------------------------------

Imagine::NFmiColorTools::Color ShapeSpec::fillcolor() const
{
  return itsFillColor;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the stroke color for the shape
 *
 * \return The stroke color
 */
// ----------------------------------------------------------------------

Imagine::NFmiColorTools::Color ShapeSpec::strokecolor() const
{
  return itsStrokeColor;
}
// ----------------------------------------------------------------------
/*!
 * \brief Set the fill rule for the shape
 *
 * \param theRule The fill rule
 */
// ----------------------------------------------------------------------

void ShapeSpec::fillrule(const std::string &theRule) { itsFillRule = theRule; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the stroke rule for the shape
 *
 * \param theRule the stroke rule
 */
// ----------------------------------------------------------------------

void ShapeSpec::strokerule(const std::string &theRule) { itsStrokeRule = theRule; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the fill color for the shape
 *
 * \param theColor The fill color
 */
// ----------------------------------------------------------------------

void ShapeSpec::fillcolor(Imagine::NFmiColorTools::Color theColor) { itsFillColor = theColor; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the stroke color for the shape
 *
 * \param theColor The stroke color
 */
// ----------------------------------------------------------------------

void ShapeSpec::strokecolor(Imagine::NFmiColorTools::Color theColor) { itsStrokeColor = theColor; }
// ----------------------------------------------------------------------
/*!
 * \brief Set the marker for the shape
 *
 * \param theMarker The marker image filename
 * \param theRule The marker blending rule
 * \param theAlpha The alpha blending factor in range 0-1
 */
// ----------------------------------------------------------------------

void ShapeSpec::marker(const std::string &theMarker, const std::string &theRule, float theAlpha)
{
  itsMarker = theMarker;
  itsMarkerRule = theRule;
  itsMarkerAlpha = theAlpha;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the marker image filename
 *
 * \return The marker
 */
// ----------------------------------------------------------------------

const std::string& ShapeSpec::marker() const
{
  return itsMarker;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the marker image blending rule
 *
 * \return The blending rule
 */
// ----------------------------------------------------------------------

const std::string& ShapeSpec::markerrule() const
{
  return itsMarkerRule;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the marker image alpha blending factor
 *
 * \return The blending factor
 */
// ----------------------------------------------------------------------

float ShapeSpec::markeralpha() const
{
  return itsMarkerAlpha;
}
// ======================================================================
