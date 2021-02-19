ifneq "$(wildcard ../qdcontour)" ""
PROGRAM=../qdcontour
else
PROGRAM=qdcontour
endif

GENERATED_CONF_FILES := \
	conf/labelmarker.conf \
	conf/labels_with_ttf.conf \
	conf/roundarrow_grid_labels_ttf.conf

# FIXME: find better way to determine font location
TTF_FONT_FILE := $(shell rpm -ql liberation-sans-fonts | grep LiberationSans-Regular.ttf | perl -pe 's:.*?/fonts/::')
ifeq ($(TTF_FONT_FILE),)
TTF_FONT_FILE=liberation/LiberationSans-Regular.ttf
endif

all: test

clean:
	rm -f *~ */*~ results/* results_diff/*

#---
# New tests, done solely by Make and Imagemagick
#
_CHECK=_check

# Difficult test since the filename changes: timestampformat

# Misc tests that have issues:
#
labelmarker labels_grid_masked missing_values shape_combined shape_fill shape_mark shape_mark_alpha_factor shape_stroke \
contourpattern contoursymbol1 contoursymbol2 \
contourfont1 contourfont2 contourfont_grid \
contourlabeltexts contourlabels contourlabelcolors \
center automatic_width \
despeckle_none \
despeckle_median1_normal \
despeckle_median2_normal \
despeckle_median1_iter2 \
despeckle_median1_upper \
despeckle_median1_lower \
despeckle_median1_lower_range \
windarrow_grid_normal \
windarrow_grid_step \
windarrow_grid_masked \
windarrow_pixelgrid_normal \
windarrow_pixelgrid_masked \
windarrowscale \
roundarrow_grid_normal \
roundarrow_grid_colors \
roundarrow_grid_sizes \
meta_elevation_angle \
meta_wind_chill \
meta_t2m_advection \
meta_thermal_front \
arrows_and_labels \
:
	-@$(MAKE) _check TEST=$@

meta_wind_chill_pdf \
:
	-@$(MAKE) _check_pdf TEST=$(@:_pdf=)

test_pdf:
	-@$(MAKE) test _CHECK=_check_pdf

test:	$(GENERATED_CONF_FILES)
#	-@$(MAKE) --quiet $(_CHECK) TEST=timestampformat
	-@$(MAKE) --quiet $(_CHECK) TEST=meteorological_arrows_world
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_markers_pixelgrid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_grid_coloured
	-@$(MAKE) --quiet $(_CHECK) TEST=meteorological_arrows_coloured
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_tipcolors
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_colors
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_sizes
	-@$(MAKE) --quiet $(_CHECK) TEST=meteorological_arrows_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=meteorological_arrows_and_labels
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_labels_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=roundarrow_grid_labels_ttf
	-@$(MAKE) --quiet $(_CHECK) TEST=windcomponents_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_with_ttf
	-@$(MAKE) --quiet $(_CHECK) TEST=labelmarker
	-@$(MAKE) --quiet $(_CHECK) TEST=timestampimage_forobs
	-@$(MAKE) --quiet $(_CHECK) TEST=timestampimage2
	-@$(MAKE) --quiet $(_CHECK) TEST=timestampimageutc
	-@$(MAKE) --quiet $(_CHECK) TEST=timestampimage_datehour
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_grid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_grid_step
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_grid_masked
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_pixelgrid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrow_pixelgrid_masked
	-@$(MAKE) --quiet $(_CHECK) TEST=windarrowscale
	-@$(MAKE) --quiet $(_CHECK) TEST=shape_fill
	-@$(MAKE) --quiet $(_CHECK) TEST=shape_stroke
	-@$(MAKE) --quiet $(_CHECK) TEST=shape_mark
	-@$(MAKE) --quiet $(_CHECK) TEST=shape_mark_alpha_factor
	-@$(MAKE) --quiet $(_CHECK) TEST=shape_combined
	-@$(MAKE) --quiet $(_CHECK) TEST=automatic_width
	-@$(MAKE) --quiet $(_CHECK) TEST=automatic_height
	-@$(MAKE) --quiet $(_CHECK) TEST=center
	-@$(MAKE) --quiet $(_CHECK) TEST=scale
	-@$(MAKE) --quiet $(_CHECK) TEST=trivial
	-@$(MAKE) --quiet $(_CHECK) TEST=contourline
	-@$(MAKE) --quiet $(_CHECK) TEST=contourlinewidth
	-@$(MAKE) --quiet $(_CHECK) TEST=contourfill
	-@$(MAKE) --quiet $(_CHECK) TEST=contourpattern
	-@$(MAKE) --quiet $(_CHECK) TEST=contoursymbol1
	-@$(MAKE) --quiet $(_CHECK) TEST=contoursymbol2
	-@$(MAKE) --quiet $(_CHECK) TEST=contourfont1
	-@$(MAKE) --quiet $(_CHECK) TEST=contourfont2
	-@$(MAKE) --quiet $(_CHECK) TEST=contourfont_grid
	-@$(MAKE) --quiet $(_CHECK) TEST=contourlabels
	-@$(MAKE) --quiet $(_CHECK) TEST=contourlabeltexts
	-@$(MAKE) --quiet $(_CHECK) TEST=contourlabelcolors
	-@$(MAKE) --quiet $(_CHECK) TEST=directionparam
	-@$(MAKE) --quiet $(_CHECK) TEST=speedparam
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_points
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_grid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_grid_masked
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_grid_step
	-@$(MAKE) --quiet $(_CHECK) TEST=labelformat
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_integers
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_and_arrows
	-@$(MAKE) --quiet $(_CHECK) TEST=arrows_and_labels
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_pixelgrid_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=labels_pixelgrid_masked
	-@$(MAKE) --quiet $(_CHECK) TEST=meta_elevation_angle
	-@$(MAKE) --quiet $(_CHECK) TEST=meta_wind_chill
	-@$(MAKE) --quiet $(_CHECK) TEST=meta_t2m_advection
	-@$(MAKE) --quiet $(_CHECK) TEST=meta_thermal_front
	-@$(MAKE) --quiet $(_CHECK) TEST=missing_values
	-@$(MAKE) --quiet $(_CHECK) TEST=expanddata
	-@$(MAKE) --quiet $(_CHECK) TEST=fahrenheit
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_none
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median1_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median2_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median1_iter2
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median1_upper
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median1_lower_normal
	-@$(MAKE) --quiet $(_CHECK) TEST=despeckle_median1_lower_range

# ImageMagick usage was throw to a separate shell script. It should return 0
# for approvable differences, and non-zero for once that could stop the make
# (currently, all tests are run through anyhow).

PNG=$(notdir $(wildcard results_ok/$(TEST)_*.png)$(wildcard results_ok/$(TEST).png))

_check:
	@echo -n "$(TEST)..........................................." | sed -e 's/^\(.\{40\}\).*/\1/g'
	@-mkdir -p results_diff
	$(PROGRAM) -f conf/$(TEST).conf
	-./pngdiff.sh results_ok/$(PNG) results/$(PNG) results_diff/$(PNG)

_check_pdf:
	@echo
	@echo "*** $(TEST) ***"
	@echo
	$(PROGRAM) -f -c "format pdf" conf/$(TEST).conf

echo:
	@echo $(PNG)

conf/%.conf : conf/%.conf.in ; sed -e "s:@TTF_FONT_FILE@:$(TTF_FONT_FILE):" $< >$@
