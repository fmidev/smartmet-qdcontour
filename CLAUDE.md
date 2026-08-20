# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is qdcontour

A legacy command-line tool for rendering contour images from QueryData (FMI's native gridded weather data format) and maps from shapefiles. It is driven by a control file that specifies projections, data sources, contour parameters, styling, and output format. Largely superseded by the SmartMet Server WMS plugin, but still maintained.

## Build commands

```bash
make                # Build the qdcontour binary
make test           # Run image-comparison regression tests
make format         # clang-format (Google style, Allman braces, 100-col)
make clean          # Clean build artifacts
make rpm            # Build RPM package
```

Build uses the shared SmartMet build config (`makefile.inc` from `smartbuildcfg`). Requires pkg-config packages: `geos`, `gdal`. Links against: `smartmet-macgyver`, `smartmet-newbase`, `smartmet-imagine`, `smartmet-gis`, `smartmet-trax`.

## Running a single test

```bash
# From the test/ directory:
make _check TEST=contourline          # Run one named test
make _check TEST=windarrow_grid_normal
```

Each test runs `qdcontour -f conf/<TEST>.conf`, then compares the output PNG against a baseline in `results_ok/` using `smartpngdiff`. Diff images go to `results_diff/`.

## Architecture

The entire program is a single binary built from `main/qdcontour.cpp` (~5700 lines) plus ~24 support source files.

**Rendering pipeline**: Control file parsing (`process_cmd()`) → data loading via `LazyQueryData` → contour computation via `ContourCalculator` → image composition → output (PNG/JPEG/PDF).

**Key types**:
- **`Globals`** — singleton holding all global state: active query data, rendering config, caches (`ImageCache`, `ContourCache`, `ArrowCache`), shape/contour specs, locators
- **`LazyQueryData`** — lazy-loading wrapper around newbase's `NFmiFastQueryInfo`/`NFmiQueryData`; reads headers immediately, defers data loading
- **`ContourSpec`** — aggregates all rendering instructions for one parameter: fill ranges, line values, patterns, symbols, labels, fonts, despeckle settings
- **`ContourCalculator`** — computes isolines/isobands from data grids with caching (pimpl pattern)
- **`ShapeSpec`** — fill/stroke/marker configuration for shapefile rendering

**Main rendering entry points** in `qdcontour.cpp`:
- `do_draw_contours()` — full pipeline: time loop × parameter loop → fills → patterns → lines → overlays → arrows → labels → timestamps → save
- `do_draw_shapes()` — shapefile-only rendering (no time loop)

**Control file** is a line-oriented command language with 60+ directives (projection, querydata, param, contourfill, contourline, windarrow, shape, etc.). Parsed by `process_cmd()` which is a large if/else-if chain.

## CI

CircleCI with parallel RHEL8 and RHEL10 builds. Workflow: `ci-build deps` → RPM build → `ci-build test` → S3 upload.
