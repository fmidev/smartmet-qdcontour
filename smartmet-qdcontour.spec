%define BINNAME qdcontour
%define RPMNAME smartmet-%{BINNAME}
Summary: qdcontour
Name: %{RPMNAME}
Version: 22.8.19
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Tools
URL: http://www.weatherproof.fi
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

BuildRequires: rpm-build
BuildRequires: gcc-c++
%if %{defined el7}
BuildRequires: devtoolset-7-gcc-c++
%endif
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: freetype-devel
BuildRequires: glibc-devel
BuildRequires: libjpeg-devel
BuildRequires: libpng-devel
BuildRequires: geos310-devel
BuildRequires: gdal34-devel
BuildRequires: smartmet-library-macgyver-devel >= 22.8.9
BuildRequires: smartmet-library-imagine-devel >= 22.6.17
BuildRequires: smartmet-library-newbase-devel >= 22.6.16
BuildRequires: smartmet-library-gis-devel >= 22.7.27
BuildRequires: smartmet-library-tron-devel >= 22.6.17
BuildRequires: zlib-devel
BuildRequires: ImageMagick
BuildRequires: bc
Requires: smartmet-library-imagine >= 22.6.17
Requires: smartmet-library-newbase >= 22.6.16
Requires: smartmet-library-tron >= 22.6.17
Requires: freetype
Requires: libjpeg
Requires: libpng
Requires: zlib
Requires: xorg-x11-fonts-misc
Provides: qdcontour
#TestRequires: gcc-c++
#TestRequires: smartmet-library-macgyver-devel >= 22.8.9
#TestRequires: smartmet-library-tron >= 22.6.17
#TestRequires: smartmet-fonts
#TestRequires: ImageMagick
#TestRequires: bc
#TestRequires: xorg-x11-fonts-misc
#TestRequires: coreutils
#TestRequires: liberation-sans-fonts

%description
qdcontour

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{RPMNAME}
 
%build
make %{_smp_mflags}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0775)
%{_bindir}/qdcontour


%changelog
* Fri Aug 19 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.19-1.fmi
- Test now require smartmet-fonts for the synoptic symbols

* Fri Jun 17 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.17-1.fmi
- Add support for RHEL9. Update libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Tue May 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.24-1.fmi
- Repackaged due to NFmiArea ABI changes

* Fri May 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.20-1.fmi
- Repackaged due to ABI changes to newbase LatLon methods

* Wed May 18 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.18-1.fmi
- Remove obsolete #ifdef WGS84 segments

* Fri Jan 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.1.21-1.fmi
- Repackage due to upgrade of packages from PGDG repo: gdal-3.4, geos-3.10, proj-8.2

* Tue Dec  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.12.7-1.fmi
- Update to postgresql 13 and gdal 3.3

* Thu May  6 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.6-1.fmi
- Repackaged due to ABI changes in NFmiAzimuthalArea

* Fri Apr  9 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.9-1.fmi
- Repackaged with the latest Tron library for faster contouring

* Tue Mar 23 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.23-1.fmi
- Repackaged due to geos39 updates

* Fri Feb 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.19-1.fmi
- Updated to use new SpatialReference/CoordinateMatrix APIs

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Tue Jan  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.5-1.fmi
- Upgrade to GEOS 3.9

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Mon Jun 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.15-1.fmi
- Enable config replacements from environment variables

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgraded to Boost 1.68

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.fmi
- Repackaged due to newbase API changes

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuilt due to newbase API/ABI changes

* Fri Sep 27 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.27-1.fmi
- Repackaged due to ABI changes in SmartMet libraries

* Thu Jul 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.26-1.fmi
- Prefer nullptr over NULL

* Wed May  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.2-1.fmi
- Repackaged since newbase NFmiEnumConverter ABI changed

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Upgrade to boost 1.66

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65

* Mon Feb 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.13-1.fmi
- Recompiled due to newbase API changes

* Tue Feb  7 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.7-1.fmi
- Tron fixes to isoline contouring

* Mon Jan 30 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.30-1.fmi
- Tron now handles self-touching isolines correctly

* Fri Jan 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.27-1.fmi
- Recompiled due to NFmiQueryData object size change

* Thu Jan 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.19-2.fmi
- Tron now builds as long isolines as possible

* Thu Jan 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.19-1.fmi
- Recompiled with more robust contouring from Tron

* Wed Jan 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.18-3.fmi
- Analyze data before contouring isolines to skip unnecessary parts of the data

* Wed Jan 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.18-2.fmi
- Fixed loglinear contouring to build valid geometries

* Wed Jan 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.18-1.fmi
- Recompiled with latest Tron with fixes for self touching contour lines

* Wed Jan 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.11-1.fmi
- Switched to FMI open source naming conventions
- Use latest Tron-library

* Sun Jan 17 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.17-1.fmi
- newbase API changed

* Wed Apr 15 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.15-1.fmi
- newbase API changed

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Mon Mar 30 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.3.30-1.fmi
- Switched to dynamic linkage of smartmet libraries

* Mon Feb 16 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.16-1.fmi
- Fixed contoursymbol command to handle missing values correctly

* Fri Sep 26 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.26-1.fmi
- Fixed the orientation of wind barbs on the southern hemisphere

* Thu Sep 25 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.25-1.fmi
- Improved color reduction algorithm preserves original colors better

* Mon May 12 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.12-1.fmi
- Fixed bug in timestepping when data resolution is less than one hour

* Mon Apr 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.4.28-1.fmi
- Recompile due to additions to SYKE flood parameters

* Thu Apr 10 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.4.10-1.fmi
- Recompiled to be able to draw new pollen parameters

* Mon Mar 31 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.3.31-1.fmi
- Render label markers also at labelsxy positions

* Mon Mar 17 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.3.17-1.fmi
- Fixed contour cache to use interpolated time instead of valid time in cache key

* Wed Jan 22 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.1.22-1.fmi
- Meteorological arrows are now rendered by rounding to nearest integer knots

* Wed Nov 27 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.27-1.fmi
- Recompiled to get support for the gnomonic projection

* Fri Sep 27 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.9.27-1.fmi
- Added windcomponents command for drawing arrows from X/Y or U/V components

* Fri Aug  2 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.2-2.fmi
- Bug fix to forcing Atlantic views of Pacific data

* Fri Aug  2 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.2-1.fmi
- Enabled Pacific views of data

* Wed Jul  3 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54

* Thu Jun 20 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.20-1.fmi
- Added loglinear interpolation

* Fri Jun 14 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.14-1.fmi
- Added timestampformat command for modifying the output filename

* Thu Apr 18 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.18-1.fmi
- Meteorological arrow flags are now filled according to the stroke colour

* Tue Mar 26 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.26-1.fmi
- arrowfill and arrowstroke can now be specified for speed ranges

* Tue Oct  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.10.2-1.fmi
- New parameter names for ice storage from newbase

* Thu Jul  5 2012 Mika Heiskanen <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi
- Migration to boost 1.50

* Wed Jul  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.4-1.el6.fmi
- New newbase parameter names for wind fractiles

* Mon May 28 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.5.28-1.el6.fmi
- Parameters can now be specified by their number

* Mon Mar 19 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.19-1.el6.fmi
- New LAPS parameter names now recognized

* Mon Feb 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.13-3.el5.fmi
- Ignore fill colour of meteorological wind arrows for all rendering methods

* Mon Feb 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.13-2.el5.fmi
- Fixed arrow rendering to check whether the speed value is missing

* Mon Feb 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.13-1.el5.fmi
- datareplace now works for arrow rendering too

* Wed Feb  8 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.8-1.el6.fmi
- New parameternames: IceSpeed, IceDirection

* Tue Feb  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.7-1.el5.fmi
- New parametername Friction
- Fixed wind chill formula

* Fri Nov 25 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.25-1.el5.fmi
- fillarrow has no effect on meteorological arrows anymore

* Thu Nov 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.24-1.el5.fmi
- Upgraded to latest newbase which defines RadarBorder parameter

* Mon Oct 17 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.17-1.el5.fmi
- Upgraded to latest newbase

* Wed Oct  5 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.5-1.el5.fmi
- Upgraded to latest newbase

* Thu Sep 15 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.9.15-1.el5.fmi
- Returned graticule command which was lost in some cvs merge or something

* Wed Jul 20 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.7.20-1.el5.fmi
- Upgrade to boost 1.47

* Fri Jul  8 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.7.8-1.el5.fmi
- Recompiled to get new biomass parameter names into use

* Fri May 20 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.5.20-1.el6.fmi
- RHEL6 release

* Mon Mar 28 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.28-2.el5.fmi
- Flipped metorological arrows to get the flags on the correct side
- Fixed meteorological arrow speed values

* Thu Mar 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.5.20-1.el5.fmi
- Fixed meteorological arrows
- Upgrade to boost 1.46 and new base libraries

* Thu Mar 17 2011 keranen <pekka.keranen@geosaaga.fi> - 11.3.17-1.el6.fmi
- RHEL6 build with compilation fixes

* Wed Feb  2 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.2.2-1.el5.fmi
- Updated to latest newbase with new parameter names

* Tue Oct 12 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.12-1.el5.fmi
- timestampbackground is now by default 185,185,185,185

* Mon Jul  5 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.5-1.el5.fmi
- Upgrade to newbase 10.7.5-1

* Mon Jun  7 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.6.7-1.el5.fmi
- Output directories are now created automatically
- Now supports pkj projection via new newbase

* Wed Jan 20 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.20-1.el5.fmi
- Added the "overlay" command

* Fri Jan 15 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.15-1.el5.fmi
- Upgrade to boost 1.41

* Thu Jan  7 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.7-1.el5.fmi
- Fixed label rendering (imagine lib)

* Tue Jul 14 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.14-1.el5.fmi
- Upgrade to boost 1.39

* Fri Jun 12 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.6.12-1.el5.fmi
- Added datehour timestamp formatting for YLE

* Thu May 28 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.5.28-1.el5.fmi
- Added roundarrow support

* Fri Jan 23 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.1.23-1.el5.fmi
- Recompile for recognizing new newbase parameternames

* Tue Dec 30 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.30-1.el5.fmi
- Added safety checks for polar regions in wind arrow calculations

* Mon Dec 29 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.29-1.el5.fmi
- Fixed wind arrows to calculate north correctly
- Added graticule command

* Wed Oct  8 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.10.8-1.el5.fmi
- Bugfix in tron polygon builder

* Mon Sep 29 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.9.29-1.el5.fmi
- Newbase headers changed

* Wed Sep 24 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.9.24-1.el5.fmi
- Linked statically with boost 1.36
- Improved regression tests

* Mon May 19 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.5.19-1.el5.fmi
- Added contourlinewidth command

* Fri May 16 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.5.16-1.el5.fmi
- Fixed a memory leak caused by rounding errors in creating meteorological arrows

* Tue Mar 11 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.3.11-1.el5.fmi
- Linked with latest newbase including wind interpolation fixes

* Wed Jan 30 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.1.30-1.el5.fmi
- New roadmodel parameter names are now recognized

* Fri Dec 28 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.6-1.el5.fmi
- Linked with fixed newbase

* Fri Nov 30 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.5-1.el5.fmi
- Linked with newest libraries

* Mon Nov 19 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.4-1.el5.fmi
- Linked with newest libraries

* Thu Nov 15 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.3-1.el5.fmi
- Linked with newest libraries

* Fri Oct 19 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.2-1.el5.fmi
- Added "contourlabeltext" command.

* Mon Sep 24 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-4.el5.fmi
- Fixed "make depend".

* Fri Sep 14 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-3.el5.fmi
- Added "make tag" feature.

* Thu Sep 13 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-2.el5.fmi
- Improved make system.

* Thu Jun  7 2007 tervo <tervo@xodin.weatherproof.fi> - 
- Initial build.

