%define DIRNAME spine
%define LIBNAME smartmet-spine
%define SPECNAME smartmet-library-%{DIRNAME}
Summary: SmartMet Server core helper classes
Name: %{SPECNAME}
Version: 21.11.23
Release: 1%{?dist}.fmi
License: MIT
Group: BrainStorm/Development
URL: https://github.com/fmidev/smartmet-library-spine
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{rhel} >= 8
Requires: mariadb-connector-c
#TestRequires: mariadb-connector-c
%else
Requires: mariadb-libs
#TestRequires: mariadb-devel
%endif

BuildRequires: boost169-chrono
BuildRequires: boost169-devel
BuildRequires: boost169-timer
BuildRequires: ctpp2-devel
BuildRequires: dtl
BuildRequires: fmt-devel >= 7.1.3
BuildRequires: gcc-c++
BuildRequires: gdal32-devel
BuildRequires: glibc-devel
BuildRequires: jsoncpp-devel >= 1.8.4
BuildRequires: libconfig17 >= 1.7.3
BuildRequires: libconfig17-devel >= 1.7.3
BuildRequires: double-conversion-devel
BuildRequires: libicu-devel
BuildRequires: make
BuildRequires: mariadb-devel
BuildRequires: rpm-build
BuildRequires: smartmet-library-gis-devel >= 21.9.24
BuildRequires: smartmet-library-macgyver-devel >= 21.10.4
BuildRequires: smartmet-library-newbase-devel >= 21.10.13
BuildRequires: smartmet-utils-devel >= 21.11.4
Requires: boost169-chrono
Requires: boost169-date-time
Requires: boost169-filesystem
Requires: boost169-iostreams
Requires: boost169-program-options
Requires: boost169-regex
Requires: boost169-system
Requires: boost169-thread
Requires: boost169-timer
Requires: ctpp2
Requires: fmt >= 7.1.3
Requires: gdal32-libs
Requires: hdf5
Requires: jsoncpp >= 1.8.4
Requires: libconfig17 >= 1.7.3
Requires: libicu
Requires: double-conversion
Requires: smartmet-library-gis >= 21.9.24
Requires: smartmet-library-macgyver >= 21.10.4
Requires: smartmet-library-newbase >= 21.10.13
Requires: smartmet-timezones >= 21.10.29
#TestRequires: bzip2-devel
#TestRequires: gcc-c++
#TestRequires: gdal32-devel
#TestRequires: jsoncpp-devel >= 1.8.4
#TestRequires: make
#TestRequires: smartmet-library-regression
#TestRequires: zlib-devel
#TestRequires: smartmet-utils-devel >= 21.11.4
Obsoletes: libsmartmet-brainstorm-spine < 16.11.1
Obsoletes: libsmartmet-brainstorm-spine-debuginfo < 16.11.1

%description
FMI BrainStorm Spinal Cord Library

%package -n %{SPECNAME}-devel
Summary: SmartMet Spine development files
Group: SmartMet/Development
Requires: boost169-devel
Requires: dtl
Requires: smartmet-library-macgyver-devel
Requires: smartmet-library-gis-devel
Requires: smartmet-library-newbase-devel
Requires: libconfig17-devel
Requires: %{SPECNAME} = %{version}-%{release}
# Require for compatibility: earlier smartmet-plugin-test was part of smartmet-library-spine-devel
Obsoletes: libsmartmet-brainstorm-spine-devel < 16.11.1
%description -n %{SPECNAME}-devel
SmartMet Spine development files

%package -n %{SPECNAME}-plugin-test
Summary: SmartMet Spine development files: plugin test program
Group: SmartMet/Development
Requires: libconfig17
Requires: %{SPECNAME} = %{version}-%{release}
%description -n %{SPECNAME}-plugin-test
SmartMet Spine development files: plugin test program

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}

%build
make %{_smp_mflags}

%install
%makeinstall

%clean
# rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755,root,root,0755)
%{_libdir}/lib%{LIBNAME}.so

%files -n %{SPECNAME}-devel
%defattr(0644,root,root,0755)
%{_includedir}/smartmet/%{DIRNAME}

%files -n %{SPECNAME}-plugin-test
%{_bindir}/smartmet-plugin-test

%changelog

* Tue Nov 23 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.23-1.fmi
- Added support for x and y query parameters (BRAINSTORM-2091)

* Wed Nov 10 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.10-1.fmi
- Use double-conversion library in ValueFormatter (BRAINSTORM-2166)

* Wed Nov 3 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.3-1.fmi
- Fixed search order of file-path starting with '/' (BRAINSTORM-2193)

* Mon Oct 18 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.18-1.fmi
- Use resources directory for common resources (BRAINSTORM-2164)

* Tue Oct 12 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.12-1.fmi
- DEM-parameter added to location parameters (BRAINSTORM-2169)

* Mon Oct 11 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.11-1.fmi
- New time aggregation function added

* Fri Sep 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.17-1.fmi
- Repackaged to resolve CircleCI issues. Updated RPM dependencies.

* Mon Sep 13 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.13-1.fmi
- Repackaged due to Fmi::Cache statistics fixes

* Fri Sep 10 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.10-1.fmi
- Plugin tests now detect WGS84 branch mode
- Move smartmet-plugin-test to a separate RPM package

* Tue Sep  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.7-1.fmi
- Use libconfig17 ja libconfig17-devel

* Mon Aug 30 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.8.30-1.fmi
- Cache counters added (BRAINSTORM-1005)

* Sat Aug 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.21-1.fmi
- Faster LocalTimePool with unordered_map instead of map

* Fri Aug 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.20-1.fmi
- Faster TableFormatter code

* Thu Aug 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.19-1.fmi
- Start using local time pool to avoid unnecessary allocations of local_date_time objects (BRAINSTORM-2122)

* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.fmi
- Simplified shutdown mechanism

* Wed Jul 28 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.7.28-1.fmi
- Silenced several compiler warnings
- Initialize locations to NaN coordinates by default

* Mon Jul  5 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.7.5-1.fmi
- Move DataFilter from obsengine to spine

* Tue Jun 22 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.22-1.fmi
- Merged WGS84 branch to master

* Tue Jun 15 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.6.15-1.fmi
- Avoid thread interruption in destructors

* Mon May 31 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.31-1.fmi
- Deprecated Hash.h, use macgyver library Hash.h instead

* Fri May 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.21-1.fmi
- Fixed TimeSeriesGeneratorOptions hash_value generation

* Thu May 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.20-3.fmi
- Repackaged with improved hashing functions

* Thu May 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.20-2.fmi
- Added Parameter::hashValue()

* Thu May 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.20-1.fmi
- Use Fmi hash functions, boost::hash_combine produces too many collisions

* Wed May 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.19-1.fmi
- Added Reactor::isInitializing() to be used by the backend plugin

* Tue May 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.11-1.fmi
- Changed meaning of precision -1 in ValueFormatter to mean shortest possible presentation by {fmt}

* Fri Apr 16 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.16-1.fmi
- Handle errors when running the configured alert script

* Thu Apr 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.15-1.fmi
- Fixed active requests log cleaning code to handle exceptions

* Sat Mar 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.27-1.fmi
- Fixed alert print of active requests to count rows correctly

* Tue Mar  9 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.9-1.fmi
- Print active requests when the alert script is triggered if verbose mode is on

* Mon Mar  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.1-1.fmi
- Prefer emplace_back over push_back for speed

* Sat Feb 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.27-1.fmi
- Added support for metaparameter enumerations

* Fri Feb  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.5-1.fmi
- Improved padding of PluginTest output for clarity

* Fri Jan 29 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.29-1.fmi
- Always run test scripts by default if the script exists

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Tue Jan  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.5-2.fmi
- Upgrade to fmt 7.1.3

* Tue Jan  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.5-1.fmi
- Repackaged due to geos39 upgrade

* Wed Dec 30 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.30-3.fmi
- Install /usr/bin/smartmet-plugin-test with smartmet-library-spine instead of separate RPM

* Wed Dec 30 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.30-2.fmi
- Move smartmet-plugin-test to a separate RPM package (smartmet-plugin-test)
- Fix dependency problem from previous version 20.12.30-1.fmi

* Wed Dec 30 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.30-1.fmi
- Rebuild due to jsoncpp upgrade for RHEL7 (also RHEL8 to avoid broken RPM dependencies)

* Tue Dec 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.15-1.fmi
- Upgrade to pgdg12

* Thu Dec 10 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.10-1.fmi
- Adapt to changes in makefile.inc

* Fri Dec  4 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.4-3.fmi
- Import PluginTest from WFS plugin, modify for use with all plugins
- install as part of smartmet-library-spine-devel package

* Fri Dec  4 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.4-2.fmi
- PluginTest.h updates (support comments in ignore list, warn about ignores not found)

* Thu Dec  3 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.3-1.fmi
- Additional manual cleanup in Reactor destructor

* Mon Nov 23 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.11.23-1.fmi
- Small PluginTest.h update (supports specifying ignore lists)

* Wed Oct 28 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.28-2.fmi
- Rebuild due to fmt upgrade

* Wed Oct 28 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.28-1.fmi
- Optimized JsonCache for speed

* Mon Oct 26 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.26-1.fmi
- Enabled .testignore files in plugin tests

* Tue Oct 20 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.20-1.fmi
- Rebuild to API incompatible libconfig replacement (libconfig update to 1.7.2)

* Thu Oct 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.15-1.fmi
- libmft internal alignment has been deprecated, use zero fill automatically instead

* Wed Oct 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.14-1.fmi
- Do not use stringstreams in TableFormatter API due to GNU std::locale locks

* Fri Oct  9 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.9-1.fmi
- Use Fmi::trim for speed

* Thu Oct  8 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.8-1.fmi
- Fix SmartMetPlugin::initPlugin (cleanup in case of C++ exception)

* Wed Oct  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.7-3.fmi
- Added throttle.alert_limit for controlling when to run the alert script

* Wed Oct  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.7-2.fmi
- Prevent unnecessary coredump if engine shutdown hooks throw during the shutdown

* Wed Oct  7 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.7-1.fmi
- Rebuilt due to smartmet-library-macgyver makefile.inc changes
- Fail build if libsmartmet-spine.so has unresolved external references

* Tue Oct  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.6-1.fmi
- Enable include paths relative to main configuration file

* Mon Oct  5 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.5-1.fmi
- Use makefile.inc from smartmet-library-macgyver

* Wed Sep 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.23-1.fmi
- Removed Spine::Exception, using Fmi::Exception instead
- Made Value destructor non-virtual

* Mon Sep 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.14-3.fmi
- Removed obsolete warning messages on log cleaning

* Mon Sep 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.14-2.fmi
- Do not run the alert system command when ramping up to full throttle

* Mon Sep 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.14-1.fmi
- Added possibility to run an alert system command when active requests limit is broken

* Tue Sep  8 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.9.8-1.fmi
- Reactor shutdown support update

* Fri Sep  4 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.4-1.fmi
- Forward declare GDAL classes in CRSRegistry header to avoid dependency escalation

* Tue Sep  1 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.1-1.fmi
- PluginTest now calls Reactor::init as required by the new API

* Wed Aug 26 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.08.26-1.fmi
- Use std::unique_ptr<Fmi::AsyncTaskGroup> in Reactor class
- Move loading engines and plugins to init() method
- Reactor: stop other async init tasks if one or more failed

* Tue Aug 25 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.08.25-1.fmi
- Use Fmi::AsyncTaskGroup in Reactor class

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Tue Aug 11 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.11-2.fmi
- Throttling is now lowered only by 200 OK responses

* Tue Aug 11 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.11-1.fmi
- Improved throttling messages
- Fixed command line parsing of throttling restart_limit

* Mon Aug 10 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.10-1.fmi
- Added throttling for slowing down the server during high load

* Fri Jul 31 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.31-1.fmi
- Repackaged due to libpqxx upgrade

* Tue Jul 28 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.28-2.fmi
- Log also response size and ETag (or "-" if not set)

* Tue Jul 28 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.28-1.fmi
- Exceptions now print nothing if stack traces are disabled

* Mon Jun  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.8-1.fmi
- Upgraded libpqxx

* Wed May 27 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.27-1.fmi
- Fixed bug in parameter parsing when square brackets are present
- For example 'percentage_t[0:60](TotalCloudCover)'

* Tue May 26 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.26-1.fmi
- Accept altenative interval separators in time-aggragation functions (BRAINSTORM-1828)
- In addition to ':'-character also '/'- and ';'-characters are supported

* Tue May 12 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.12-1.fmi
- Ported to use GDAL 3.0
- Support for sensors, TaggedFMISIDs (BRAINSTORM-1678)

* Thu May  7 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.5.7-1.fmi
- Import CRSRegistry from GIS engine

* Tue May  5 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.5-1.fmi
- Do not print a stack trace for errorneous time strings given by the user

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgrade to Boost 1.69

* Mon Apr  6 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.4.6-1.fmi
- New template method Reactor::getEngine

* Mon Mar  9 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.3.9-1.fmi
- Update ParameterTools.(cpp|h): add missing import of is_time_parameter()

* Thu Mar  5 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.3.5-1.fmi
- Import ParameterTools.(cpp|h) from timeseries plugin
- Import special parameter handling part from obsengine

* Wed Feb 26 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.26-1.fmi
- Improved messages on plugin registration
- Fixed verbose mode output alignment

* Thu Feb 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.13-1.fmi
- Added support for private request handlers which frontends are not informed about

* Tue Feb 11 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.2.11-1.fmi
- Aggregation function must not fail if aggregation vector contains only Spine::TimeSeries::None() objects (BRAINSTORM-1756)

* Mon Feb 10 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.2.10-1.fmi
- Import class MultiLanguageString from WFS plugin
- Import class MultiLanguageStringArray from WFS plugin

* Fri Feb  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.7-1.fmi
- Initialize Station POD members to zero to avoid random station numbers etc

* Wed Feb 5 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.2.5-1.fmi
- Interpolate- and nearest-functions added (BRAINSTORM-1504)
- Aggregation-related bugs fixed (BRAINSTORM-1755)

* Wed Jan 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.15-1.fmi
- Added active request throttling

* Fri Dec 13 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.13-1.fmi
- Removed obsolete GDAL dependency

* Tue Dec 10 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.10-1.fmi
- Upgrade to gdal30

* Wed Dec  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.4-1.fmi
- Fixed dependency to be on gdal-libs instead of gdal

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-2.fmi
- Bind tests to port 0, since any available port will do. This enables parallel plugin tests.

* Wed Nov 20 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.11.20-1.fmi
- New data member added in Parameter class (original case sensitive name). Bugfix: don't try to parse nonexistent parameter functions (BRAINSTORM-1726)

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuilt due to newbase API/ABI changes

* Tue Oct 29 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.29-1.fmi
- Plugin tests are now run by a class with multiple controllable settings

* Tue Oct  8 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.8-1.fmi
- Fixed dependency to be mariadb-libs instead of mariadb-embedded

* Tue Oct  1 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.1-1.fmi
- Added option --stacktrace to enable a stack trace printout on crashes (default is from now on is false)

* Fri Sep 27 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.27-1.fmi
- Fixed PluginTest to print diff results after the actual tests, parallel runs made output overlap

* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.fmi
- Use std::string and fmt::format in Exception instead of std::ostringstream to avoid locks
- Added ASAN and TSAN builds
- Removed support for RHEL6
- Added object.c_str() calls needed for RHEL8
- Use Fmi::to_simple_string instead of Boost version to avoid locale locks (TSAN)
- Use Fmi::looks_signed_int instead of regexes to avoid locale locks (TSAN)
- SmartMet Server plugin tests can now be run in parallel for the benefit of TSAN
- Use atomics when logging (TSAN)
- Reactor now uses atomic status variables (TSAN)
- Use std::shared_ptr instead of pointer in SmartMet::Spine::Exception for linking to previous exception
- Configuration entry redirect support in SmartMet::Spine::ConfigBase

* Tue Sep 17 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.17-1.fmi
- Fixed ActiveBackends class to destroy gracefully by moving its mutex into the cpp file

* Wed Aug 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.28-1.fmi
- Added optional fmisid information to Location objects

* Wed Aug 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.14-1.fmi
- Rewrote log requests to avoid large copies and thread exhaustion

* Wed Aug  7 2019 Andris Pavenis <andris.pavenis@fmi.fi> - 19.8.7-1.fmi
- SmartMet::Spine::Exception: exception handling improvement

* Fri Aug  2 2019 Andris Pavenis <andris.pavenis@fmi.fi> - 19.8.2-1.fmi
- SmartMet::Spine::Exception: only look for exception type when there is any

* Fri Jun 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.28-1.fmi
- Fixed WxmlFormatter to skip origin times which do not look like ISO timestamps

* Tue Jun  4 2019 Andris Pavenis <andris.pavenis@fmi.fi> - 19.6.4-1.fmi
- SmartMet::Spine::HTTP::Authentification: logging improvements

* Wed May  8 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.5.8-1.fmi
- Json::expand case insensitivity now applies to the qid too, not just the querystring parameters

* Tue May  7 2019 Andris Pavenis <andris.pavenis@fmi.fi> - 19.5.7-1.fmi
- SmartMet::Spine::HTTP::Authentification: new methods clearUsers and removeUser

* Mon Apr 29 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.4.29-1.fmi
- Fixed bug in filtering: filter data even if there is no aggregation invloved

* Thu Apr 25 2019 Andris Pavenis <andris.pavenis@fmi.fi> - 19.4.25-1.fmi
- Fixed typos in method names in SmartMet::Spine::HTTP::Authentification

* Tue Apr 23 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.23-1.fmi
- Filter data in aggregation functions (BRAINSTORM-1546)

* Fri Apr 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.12-1.fmi
- Added generic HTTP authentication class

* Thu Apr  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.4-1.fmi
- Added possibility to remove content handlers

* Thu Mar 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.3.14-1.fmi
- Fixed RadarPrecipitation1h name conversion

* Thu Feb 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.28-1.fmi
- ActiveRequests object now stores the entire request for improved reports

* Thu Dec 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.12.13-2.fmi
- Improved error reporting if host specific configpath file is not found

* Thu Dec 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.12.13-1.fmi
- Improved error reporting when stack traces have been disabled

* Tue Dec  4 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.4-1.fmi
- Added paging to Table (BS-1430)

* Tue Nov 27 2018 Heikki Pernu <heinkki.pernu@fmi.fi> - 18.11.27-1.fmi
- Set PluginTest.h directory list ordering to alphabetical

* Fri Nov 23 2018 Heikki Pernu <heikki.pernu@fmi.fi> - 18.11.23-1.fmi
- Added boost-devel to devel package requirements due to needed headers

* Fri Nov  9 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.11.9-1.fmi
- Support for data_source-field added (BRAINSTORM-1233)

* Mon Nov  5 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.5-1.fmi
- Added tracking of the number active requests to backends to enable faster load balancing

* Sun Nov  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.4-1.fmi
- Added handling of shutdown (3210) and high_load (4321) response codes

* Fri Oct 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.26-3.fmi
- Use portable BOOST_CURRENT_FUNCTION instead of gcc METHOD_NAME

* Fri Oct 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.26-2.fmi
- configfile settings can now be overridden in the overrides-section

* Fri Oct 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.26-1.fmi
- Check for shutdown requests more often

* Thu Oct 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.1-1.fmi
- Improved error messages for configuration file errors

* Sat Sep 29 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.29-1.fmi
- Upgraded to latest fmt

* Mon Sep 24 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.24-1.fmi
- Removed an extra std::move

* Sun Sep 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.23-1.fmi
- Silenced CodeChecker warnings

* Thu Sep 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.13-1.fmi
- Options::report now prints the OOM handler

* Wed Sep 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.12-1.fmi
- Added new_handler to Options

* Tue Sep 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.11-2.fmi
- Silenced several CodeChecker warnings

* Tue Sep 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.11-1.fmi
- Added parameter 'weathernumber' to ParameterFactory

* Wed Sep 5 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.5-1.fmi
- Count-function supports now also string fields (BRAINSTORM-1131)

* Thu Aug 30 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.30-2.fmi
- Silenced CodeChecker warnings

* Thu Aug 30 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.30-1.fmi
- Options now checks there are no positional arguments

* Mon Aug 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.20-1.fmi
- Added optional_size and required_size, optional_unsigned_int and required_unsigned_int should be avoided

* Wed Aug 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.15-1.fmi
- Try to avoid reallocations while building request strings

* Mon Aug 13 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.13-1.fmi
- Support for 'wkt' parameter added
- Fixed formatting of LonLat-data in OStreamVisitor

* Thu Aug  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.9-1.fmi
- Silenced more CodeChecker warnings

* Tue Aug  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.7-1.fmi
- Silenced several CodeChecker warnings

* Fri Aug  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.3-1.fmi
- Cast __PRETTY_FUNCTION__ to char * to avoid code analysis warnings

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-2.fmi
- Fixed ValueFormatter to handle unknown floatfield values

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-1.fmi
- Added default values for Point and BoundingBox double fields

* Wed Aug  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.1-1.fmi
- Use C++11 for-loops instead of BOOST_FOREACH

* Thu Jul 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.26-1.fmi
- Added JsonCache to store parsed JSON files with a modification time check

* Mon Jul 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.23-2.fmi
- Silenced CodeChecker warnings
- Fixed accumulation initial value to be double instead of int

* Mon Jul 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.23-1.fmi
- Use libfmt instead of string streams in ValueFormatter to avoid global locale locks

* Wed Jul  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.4-1.fmi
- Avoid using ostringstream in HTTP code to avoid global locale locks

* Mon Jun 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.18-1.fmi
- Fixed shutdown message not to be Frontend specific

* Wed Jun  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.6-1.fmi
- Added configpath setting which enables easy host name specific names for config files

* Sun May 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.27-1.fmi
- Added request start time to default file logs

* Wed May 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.23-1.fmi
- format=debug and format=html now escape special HTML characters

* Mon May 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.21-1.fmi
- Fixed slow pool max requeue size initialization from command line

* Tue May 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.15-1.fmi
- Added option --maxrequestsize with default value 131072 (the previously hard coded value)

* Mon May 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.14-2.fmi
- Fixed --maxactiverequests not to have alias -q which is already in use

* Mon May 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.14-1.fmi
- Added possibility to limit the number of active requests

* Fri May 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.11-1.fmi
- Create access log directory if it does not exist already

* Wed May  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.9-2.fmi
- Frontends now log active requests

* Wed May  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.9-1.fmi
- Added class ActiveRequests

* Fri May  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.4-1.fmi
- Changed latitude and longitude to be data derived variables

* Wed May  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.2-1.fmi
- Repackaged since newbase NFmiEnumConverter ABI changed

* Wed Apr 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.11-1.fmi
- Assign a known parameter number to meta parameters too if possible (WindUMS and WindVMS)

* Tue Apr 10 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.10-1.fmi
- WindUMS and WindVMS are now meta parametrs to enable rotation away from grid orientation

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-2.fmi
- Upgrade to boost 1.66

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Added base64 decoding

* Mon Mar 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.26-1.fmi
- Store request start time in SmartMetPlugin::callRequestHandler to ease debugging

* Thu Mar 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.22-1.fmi
- Added parameter GridNorth to ParameterFactory

* Wed Mar 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.21-1.fmi
- SmartMetCache no longer tries to initialize FileCache if its size is set to zero

* Wed Mar  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.7-1.fmi
- Improved error message for invalid configuration files parsed by class Options

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-1.fmi
- Meta parameters names are now case independent

* Mon Feb 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.26-1.fmi
- Added timestamps to engine and plugin initialization messages

* Thu Feb 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.22-1.fmi
- Symbol and SymbolText changed to SmartSymbol and SmartSymbolText

* Wed Feb 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.14-1.fmi
- Added data derived parameters 'symbol' and 'symboltext'
- Cleaned up exception handling

* Fri Feb  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.9-1.fmi
- SmartMetEngine now has a reactor datamember to provide reactor services to engines

* Mon Jan 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.15-2.fmi
- Automatically trim parameter definitions to avoid trivial user errors

* Mon Jan 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.15-1.fmi
- Recompiled with latest postgresql and libpqxx

* Thu Nov 30 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.30-1.fmi
- PostGISDataSource class removed (BRAINSTORM-722)

* Wed Nov 22 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.23-1.fmi
- Improved progress reports on engine and plugin initialization

* Mon Nov 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.13-1.fmi
- Fixed resolving of relative paths in the main configuration file

* Fri Nov 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.10-1.fmi
- Added host dependent configuration setting lookup

* Thu Nov  2 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.2-1.fmi
- Added reporting on when all plugins or engines have been initialized

* Wed Nov  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.1-1.fmi
- Rebuilt due to GIS-library API change

* Mon Oct 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.23-1.fmi
- Added timestamps to error logging

* Wed Oct 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.18-1.fmi
- Added error checking for missing/unreadable configuration files

* Thu Oct 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.12-1.fmi
- Fixed ConfigBase to allow paths to be relative to files in current directory

* Wed Sep 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.13-1.fmi
- Added Exception::Trace as a simpler way to trace exceptions

* Fri Sep  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.8-1.fmi
- Fixed Json expansion to insert missing objects along the variable path

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65
- Exception::disable -methods now return the exception itself to enable command chaining

* Sun Aug 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.20-1.fmi
- Removed throwing from ~DynamicPlugin for causing std::terminate

* Thu Aug  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.3-1.fmi
- Added HTTP::urlencode and HTTP::urldecode

* Mon Jul 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.10-1.fmi
- Print stack trace if PluginTest fails to run

* Wed May 31 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.31-1.fmi
- Added a separate method for substituting JSON references from query strings

* Fri May  5 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.5-1.fmi
- Added HTTP method getProtocol for getting protocol from header

* Thu Apr 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.27-1.fmi
- Allow empty paths in get_optional_path

* Wed Apr 26 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.26-1.fmi
- Added ConfigBase::get_mandatory_path_array method

* Mon Apr 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.10-1.fmi
- Added ConfigBase methods get_optional_path and get_mandatory_path
- Modified code to allow relative paths for libfile and configfile

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-2.fmi
- Exception::addParameter, addDetail and addDetails now return this to enable chaining

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-1.fmi
- Added Exception::printError to simplify printing error messages with time stamps, stack traces etc

* Thu Mar 16 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.16-1.fmi
- Fixed JsonFormatter to escape the characters required by the RFC

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-1.fmi
- Added logging and stack trace modes to Exception class

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-1.fmi
- Switched to use macgyver StringConversion tools 

* Mon Mar 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.13-1.fmi
- Recompiled with latest newbase and macgyver

* Fri Feb  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.3-1.fmi
- Fixed plugin base to flush error messages before exit

* Wed Feb  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.1-1.fmi
- FmiApiKey implementation refactored

* Tue Jan 24 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.24-1.fmi
- Added flushing of stdout to make sure the stack trace is printed if an engine is terminated

* Fri Jan 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.12-1.fmi
- Improved debug-mode html output
- Valid html5 output in debug-mode

* Tue Jan 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.10-1.fmi
- Added header FmiApiKey.h (function FmiApiKey::getFmiApiKey()) for extracting request apikey

* Wed Jan  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.4-1.fmi
- Updated newbase and macgyver dependencies

* Wed Nov 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.30-1.fmi
- Added stack trace print if init() fails

* Tue Nov 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.29-1.fmi
- Fixed the default crs value of BoundingBox.

* Tue Nov  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.1-1.fmi
- Namespace changed.
- Derive Exception from std::exception

* Thu Sep 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.29-1.fmi
- HandlerView no longer rethrows exceptions, only an error is printed

* Tue Sep  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- Added a new exeception handling mechanism. The idea is that the methods
- cannot let any exception passing through without a control. So, the methods
- must catch all exception and most of the cases they just throw their own
- "BrainStorm::Exception" instead. In this way get an exception hierarchy that works 
- like a stack trace. Notice that it is very easy to add additional details and
- parameters related to the exception when using BrainStorm::Exception class.

* Tue Aug 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.30-1.fmi
- Made response counters atomic

* Tue Aug 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.23-1.fmi
- Added a new response code for shutdown (3210), using instead of 503 from now on

* Mon Aug 15 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.15-1.fmi
- The init(),shutdown() and requestHandler() methods are now protected methods
- in the BrainStormPlugin class. These methods are called from the public methods
- of the same class. This gives us better overall control what comes to the init,
- the shutdown and the request handling.
- Fixed a potential segmentation fault from using an unopened log file

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Full recompile due to newbase API changes

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-1.fmi
- Full recompile

* Wed Jun  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.1-1.fmi
- Added graceful server shutdown

* Mon May 16 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.16-1.fmi
- Use TimeZones instead of TimeZoneFactory

* Wed Apr 20 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.20-1.fmi
- Added possibility for plugin lazy linking
- Timeseries generator now handles climatological data tools

* Tue Apr 12 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.4.12-1.fmi
- Fixed rounding in scientific mode

* Fri Mar  4 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.4-1.fmi
- Checking correct rounding of double precision values that last digit is 5.

* Mon Feb 22 2016  Santeri Oksman <oksman@dev.dev.weatherproof.fi> - 16.2.22-1.fmi
- Fixes to timestep handling
- Add method all() which can be used to determine whether all time steps should be returned

* Tue Feb  9 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.9-1.fmi
- Using TimeSeries::None as a default type in TimeSeries::Value;

* Tue Feb  2 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.2-1.fmi
- Timeseries value has now None-type to signify missing data

* Sat Jan 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.23-1.fmi
- Reimplemented ParameterFactory singleton without using Fmi::Singleton

* Fri Jan 22 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.22-1.fmi
- Avoid locale locks when formatting access log time stamps

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-1.fmi
- newbase API changed, full recompile

* Mon Dec 14 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.12.14-1.fmi
- Added last_modified method to FileCache

* Thu Nov 26 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.26-1.fmi
- Now ignoring explicitly set parameters when serializing POST-requests
  Body must be now set explicitly when constructing POST-requests.
  This prevents confusion if both content and GET parameters are used.

* Wed Nov 18 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.18-1.fmi
- BrainStormPlugin now receives a const HTTP Request
- More precise constness handling in HTTP classes

* Tue Nov 10 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.10-1.fmi
- Avoid using string streams to avoid global std::locale locks
- Use a global const regex instead of building one in loops to avoid locks

* Mon Nov  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.9-1.fmi
- Using fast case conversion without locale locks when possible

* Wed Nov  4 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.4-1.fmi
- Removed use of stringstream from HTTP Response accessor
- Avoid substr if possible when processing JSON expansion for speed

* Thu Oct 22 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.10.22-1.fmi
- Added setter for WXML formatter type

* Tue Sep 29 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.9.29-1.fmi
- Added covertype parameter. Returns GlobCover enumerated value for the given coordinate
- Fixed date time table formatting

* Thu Aug 27 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.27-1.fmi
- Added TimeSeriesGeneratorCache

* Tue Aug 25 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.25-1.fmi
- Use unique_ptr instead of shared_ptr when possible for speed

* Mon Aug 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.24-1.fmi
- Fixed optional_unsigned_long and required_unsigned_long APIs

* Tue Aug 18 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.18-1.fmi
- Use time formatters from macgyver to avoid global locks from sstreams
- Use ASCII insensitive comparisons for HTTP parameters instead of locale dependent ones

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-1.fmi
- Use -fno-omit-frame-pointer to improve perf use

* Fri Aug 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.14-2.fmi
- Allow spaces when parsing bounding box strings

* Fri Aug 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.14-1.fmi
- Avoid boost::lexical_cast, Fmi::number_cast and std::ostringstream for speed

* Tue Aug  4 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.4-1.fmi
- Moved TaggedLocation from Geoengine
- Added integer type to timeseries variant
- Added way to control lonlat-type formatting to TableFeeder

* Fri Jun 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.26-1.fmi
- Added weathersymbol metaparameter (= WeatherSymbol3 + night information)

* Thu Jun 25 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.25-1.fmi
- JSON::expand can now be case insensitive to query string options

* Tue Jun 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.23-1.fmi
- Location datamembers are no longer const to enable modification of details
- LocationPtr now points to a const Location
- Added copy constructor and assignment operators for Location

* Mon May 25 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.5.25-1.fmi
- Imported Obsengine-related parameters to ParameterFactory
- Added nan-ignoring aggregations

* Wed Apr 29 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.29-1.fmi
- Spine now handles parameter option parsing
- Added covertype to Location object

* Tue Apr 14 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.14-1.fmi
- New release to enable observation cache

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Wed Apr  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.8-1.fmi
- Added dem-field to location object
- Added new dem-based constructor for location object
- Dynamic linking of smartmet libraries

* Tue Mar 24 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.24-1.fmi
- Fixed bug in response (and request) parsing (too relaxed header parsing)

* Mon Mar 23 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.23-1.fmi
- Fixed HTTP response parsing
- Added dem-parameter
- Added tdem-parameter (temperature with topography correction)

* Wed Feb 25 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.25-1.fmi
- Fixed bug in HTTP request string serialization

* Tue Feb 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.24-1.fmi
- Recompiled due to newbase changes

* Mon Feb 16 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.16-1.fmi
- Added hybrid memory-filesystem cache object to Spine
- Added FileCache and Json from Dali
- Added HTTP request string overrides to Json objects

* Mon Dec 15 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.15-1.fmi
- Modified Table not to use a static for empty array values (core dump suspect)

* Thu Nov 13 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.13-1.fmi
- Recompiled due to newbase API change in NewArea methods

* Fri Oct 24 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.24-1.fmi
- removeParameter function added in HTTP::Request

* Tue Sep  9 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.9.9-1.fmi
- Serialize more station parameters.

* Fri Aug 29 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.29-1.fmi
- Added calculation of hashes for various boost datatypes

* Tue Aug 26 2014 Anssi Reponen <anssi.reponen@fmi.fi> - 14.8.26-2.fmi
- Corrected handling of missing values during aggregation
- Corrected handling of timesteps in the beginning and in the end of timeseries

* Tue Aug 26 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.8.26-1.fmi
- Incorporated Anssi's fix to timeseries NaN handling

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-2.fmi
- Use the latest GDAL library
- Catching thrown messages when access log file is not possible to open.

* Mon Jun 30 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.30-1.fmi
- Server no longer attempts to open log files if defaultloggin=false
- PluginTest no longer stripp POST requests

* Wed May 28 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.5.28-1.fmi
- Recompile due new WeatherSymbol parameter

* Wed May 21 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.5.21-1.fmi
- RadarPrecipitation1h is now an alias of Precipitation1h. Because Ilmanet.

* Wed May 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.14-1.fmi
- Used shared macgyver and locus libraries

* Tue May  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.7-1.fmi
- Timeparser hotfix

* Mon Apr 28 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.4.28-1.fmi
- Time series aggregation functionality moved here from QEngine
- Time series output functionality added: inserting and formatting time series into Table or stdout
- Paramater class re-factored: Parameter functions moved to own file

* Thu Apr 17 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.4.17-1.fmi
- Removed features related to log rotation, logrotate handles them

* Wed Apr 16 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.4.16-1.fmi
- Added request log flushing to disk

* Thu Feb 27 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.27-1.fmi
- Recompile due to new time parsing in macgyver

* Mon Feb 17 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.17-2.fmi
- Fixed bugs in parameter name conversions

* Mon Feb 17 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.17-1.fmi
- Meta parameter names are now case insensitive too

* Mon Feb 3 2014 Mikko Visa <mikko.visa@fmi.fi> - 14.2.3-3.fmi
- Open data release 2014-02-03
- HTTP Request parser now accepts parameters with missing equals-sign
- Added time offset forwards for aggregation
- Added Integ function

* Mon Jan 20 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.1.20-1.fmi
- HTTP parser now accepts (and ignores) stray '&' - characters after the resource

* Tue Jan 14 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.1.14-2.fmi
- Fixed bug regarding url parameter decoding

* Tue Jan 14 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.1.14-1.fmi
- HTTP parser is now more permissive

* Mon Jan 13 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.1.13-2.fmi
- Rebuild with newer macgyver.

* Mon Jan 13 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.1.13-1.fmi
- HTTP Request and Response parsing is implemented in Boost Spirit
- Added water to snow conversion parameters Snow1h, Snow1hUpper, Snow1hLower
- Fixed time series generator to handle starttime=data and endtime=data

* Thu Dec 12 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.12.12-1.fmi
- Latitude and longitude are now metaparameters to enable aggregations

* Mon Nov 25 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.25-1.fmi
- Added parameters modtime and mtime, which are equivalent

* Tue Nov 19 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.11.19-1.fmi
- Recompiled due to new parameters in newbase

* Tue Nov  5 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.5-1.fmi
- Added isSYKEStation field to Station
- Parameters for moonrise and moonset
- Created quality code conversion class.
- HTTP getMethod now returns an enum, getMethodString returns string representation
- Added some colour to log messages
- Moved TemplateFormatter and TypeName to macgyver

* Wed Oct 9 2013 Tuomo Lauri  <tuomo.lauri@fmi.fi> - 13.10.9-1.fmi
- Parallelized Reactor construction process
- Removed threaded server type as unused
- HTTP Response gateway status is no longer a public member
- Added daylength to ParameterFactory
- Added Thread.h header for common mutex and lock defintions

* Wed Sep 18 2013 Tuomo Lauri  <tuomo.lauri@fmi.fi> - 13.9.18-1.fmi
- Improved streamer status handling

* Fri Sep 6 2013 Tuomo Lauri   <tuomo.lauri@fmi.fi> - 13.9.6-1.fmi
- Fixed Wxml formatter timestring for good

* Tue Sep 3 2013 Anssi Reponen <anssi.reponen@fmi.fi> - 13.9.3-1.fmi
- setMissingText function added to ValueFormatter

* Wed Aug 28 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.8.28-1.fmi
- Aggregation-related improvements

* Fri Aug 23 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.8.23-1.fmi
- Fixed HourlyMaximumWindSpeed parameter aliasing

* Thu Aug 15 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.8.15-1.fmi
- Added server callback hooks to Reactor
- Added backend stream status checking functionality to Response

* Mon Aug 12 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.8.12-1.fmi
- Added status checks for ContentStreamer

* Thu Aug  8 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.8.8-1.fmi
- Added hash functions to Parameter and Station for caching purposes

* Tue Jul 23 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.7.23-1.fmi
- Recompiled due to thread safety fixes in newbase & macgyver

* Wed Jul  3 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54
- Refactored options parser so that command line arguments again override the configuration file

* Sun Jun 23 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.6.23-1.fmi
- Fixed constructors to use input parameters instead of member variables

* Mon Jun 17 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi> - 13.6.17-1.fmi
- Station.h fix
- Fixed WXML formatting to detect locations with the same name

* Tue Jun  4 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.6.4-1.fmi
- Fixed WXML formatter to group by geoid instead of the name

* Mon Jun  3 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.6.3-1.fmi
- Various fixes

* Fri May 31 2013 Roope Tervo <roope.tervo@fmi.fi> - 13.5.31-1.fmi
- Try to missing thunder strikes by doing new package

* Fri May 24 2013 <mika.heiskanen@fmi.fi> - 13.5.24-1.fmi
- Fixes to time series generator defaults on timezones and start times

* Wed May 22 2013 <mika.heiskanen@fmi.fi> - 13.5.22-3.fmi
- Fixed Reactor not to require configuration variables

* Wed May 22 2013 <mika.heiskanen@fmi.fi> - 13.5.22-2.fmi
- PluginTest can now hande chunked responses too

* Wed May 22 2013 <andris.pavenis@fmi.fi> - 13.5.22-1.fmi
- Use ConfigBase in Reactor to avoid unnecessary C++ exceptions
- Time series generator now works more intelligently when generating time series starting from now

* Fri May 17 2013 <andris.pavenis@fmi.fi> - 13.5.17-1.fmi
- Fixed PluginTest

* Thu May 16 2013 <andris.pavenis@fmi.fi> - 13.5.16-1.fmi
- Table class hotfix

* Wed May 15 2013 lauri  <tuomo.lauri@fmi.fi> - 13.5.15-1.fmi
- Fixed logging functionality

* Tue May 14 2013 tervo <roope.tervo@fmi.fi> - 13.5.14-2.fmi
- BBox handling fixes

* Tue May 14 2013 oksman <santeri.oksman@fmi.fi> - 13.5.14-1.fmi
- Added serialization capability to Station.h

* Sat May 11 2013 oksman <santeri.oksman@fmi.fi> - 13.5.11-1.fmi
- Added type indicators to Station

* Fri May 10 2013 lauri <tuomo.lauri@fmi.fi> - 13.5.10-1.fmi
- Added try-catch-clause to content-length header interpretation

* Wed May 08 2013 lauri <tuomo.lauri@fmi.fi> - 13.5.8-1.fmi
- Slightly safer parseRequest-function

* Tue May 07 2013 oksman <santeri.oksman@fmi.fi> - 13.5.7-1.fmi
- Rebuild to get master and develop to the same stage.

* Mon Apr 22 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.22-1.fmi
- Reactor now uses two thread pools, one for slow queries and one for fast ones

* Fri Apr 12 2013 lauri <tuomo.lauri@fmi.fi> - 13.4.12-1.fmi
- Added IP whitelist filtering to Reactor

* Mon Apr 8 2013 oksman <santeri.oksman@fmi.fi> - 13.4.8-1.fmi
- New beta build.

* Mon Mar 25 2013 mheiskan  <mika.heiskanen@fmi.fi> - 13.3.25-2.fmi
- Using macgyver to generate local date times gracefully

* Mon Mar 25 2013 lauri     <tuomo.lauri@fmi.fi> - 13.3.25-1.fmi
- Modification to IPFilter constructor

* Mon Mar 18 2013 lauri     <tuomo.lauri@fmi.fi> - 13.3.18-1.fmi
- HTTP Request parser now accepts GET parameters in a POST request

* Mon Mar 11 2013 lauri     <tuomo.lauri@fmi.fi> - 13.3.11-1.fmi
- Added IP Filter

* Thu Mar  7 2013 pavenis   <andris.pavenis@fmi.fi>
- Depend on dtl (http://code.google.com/p/dtl-cpp/)
- Fix timesteps option to work when timestep=data

* Tue Mar  5 2013 lauri     <tuomo.lauri@fmi.fi> - 13.3.5-1.fmi
- HTTP Request toString-method did not url-encode, now it does

* Wed Feb 27 2013 lauri     <tuomo.lauri@fmi.fi> - 13.2.27-1.fmi
- Made HTTP query parameters case-insensitive

* Fri Feb 22 2013 lauri     <tuomo.lauri@fmi.fi> - 13.2.22-1.fmi
- HTTP header keys are now used in case-insensitive manner

* Wed Feb 20 2013 lauri     <tuomo.lauri@fmi.fi> - 13.2.20.1-fmi
- Endtime is now included in the times generated by the TimeSeriesGenerator

* Mon Feb 11 2013 lauri     <tuomo.lauri@fmi.fi> - 13.2.11-1.fmi

* Fixed bug in TimeSeriesGeneratorOptions

* Wed Feb  6 2013 lauri     <tuomo.lauri@fmi.fi> - 13.2.6-1.fmi
- Added HTTP handling library for Synapse server
- Added ConfigBase for better configuration handling
- Added Value generic value type

* Tue Nov 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.13-2.fmi
- Fixed DST handling in TimeSeriesGenerator

* Tue Nov 13 2012 lauri    <tuomo.lauri@fmi.fi> - 12.11.13-1.el6.fmi
- New boost::thread behaviour introduced in the newest Boost release, fixed in Reactor logging.
- Added convenience functions from WFS branch

* Wed Nov  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.7-1.el6.fmi
- Upgrade to boost 1.52
- Dynamic library instead of a static one
- Added devel package for headers

* Mon Oct  8 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.10.8-1.el6.fmi
- Added HtmlFormatter

* Tue Aug 14 2012 lauri    <tuomo.lauri@fmi.fi> - 12.8.14-1.el6.fmi
- XmlFormatter now escapes quotes

* Tue Aug  7 2012 lauri    <tuomo.lauri@fmi.fi> - 12.8.7-1.el6.fmi
- Merged LocationFormatter into Location
- Added <feature> to ParameterFactory
- Added <country> to Location

* Thu Aug 2 2012 lauri    <tuomo.lauri@fmi.fi> - 12.8.2-2.el6.fmi
- LocationFormatter now returns name if region is undefined

* Thu Aug 2 2012 lauri    <tuomo.lauri@fmi.fi> - 12.8.2-1.el6.fmi
- Renamed area to region in LocationFormatter to comply with ParameterFactory

* Wed Aug 1 2012 lauri    <tuomo.lauri@fmi.fi> - 12.8.1-1.el6.fmi
- Renamed timezone to tz in LocationFormatter

* Mon Jul 30 2012 lauri    <tuomo.lauri@fmi.fi> - 12.7.30-1.el6.fmi
- Removed unneeded features from LocationFormatter

* Mon Jul 23 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.23-1.el6.fmi
- Added ApparentTemperature

* Fri Jul 20 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.10-1.el6.fmi
- Implemented Table::empty(), which had only been declared

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi
- Migration to boost 1.50

* Mon Jun 25 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.6.25-1.el6.fmi
- Added LocationFormatter, removed LocationAccessor
- Fixed include path and linked libraries
- Changed some parts of the API to use double instead of float

* Thu Jun 21 2012 lauri <tuomo.lauri@fmi.fi> - 12.6.21-1.el6.fmi
- Added LocationAccessor for easier access to Location - objects.

* Fri Jun 8 2012 oksman <santeri.oksman@fmi.fi> - 12.6.8-1.el6.fmi
- Always enclose json with array braces ([]), even with one entry.

* Tue Apr 10 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.10-1.el5.fmi
- Using boost::math::isfinite to check that "nan" does not slip through to XML

* Wed Apr  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.4-1.el6.fmi
- Added SummerSimmerIndex, SSI and FeelsLike

* Mon Apr  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.2-1.el6.fmi
- Upgraded to latest macgyver

* Sat Mar 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.31-1.el5.fmi
- Upgraded to boost 1.49

* Tue Jan 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.1.31-2.el5.fmi
- WXML now outputs -INF, INF and NaN as xs:float types as per standard
- ValueFormatter now ingores missingtext option for the WXML format

* Thu Jan 12 2012 oksman <santeri.oksman@fmi.fi> - 12.1.12-1.el5.fmi
- Added rwsid field to Station.

* Tue Dec 27 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.27-2.el5.fmi
- columns() and rows() now work for empty tables too

* Wed Dec 21 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.21-1.el6.fmi
- Added Table class, SparseTable is too slow

* Tue Aug 16 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.8.16-1.el5.fmi
- Upgrade to boost 1.47 and latest newbase

* Tue Jul 26 2011 oksman <santeri.oksman@fmi.fi> - 11.7.26-1.el5.fmi
- Added stationDirection field to Station.

* Wed Jul 20 2011 oksman <santeri.oksman@fmi.fi> - 11.7.20-1.el5.fmi
- Added region and iso2 fields to Station.

* Thu Mar 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.24-2.el5.fmi
- Upgrade to boost 1.46

* Thu Mar 24 2011 oksman <santeri.oksman@fmi.fi> - 11.3.24-1.el5.fmi
- Added field timezone to Station.

* Wed Mar 23 2011 oksman <santeri.oksman@fmi.fi> - 11.3.23-1.el5.fmi
- WxmlFormatter handles also observations now.

* Tue Jan 25 2011 oksman <santeri.oksman@fmi.fi> - 11.1.25-1.el5.fmi
- Added Station struct for observation station handling

* Tue Jan 18 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.1.18-1.el5.fmi
- Refactored query string parsing

* Thu Oct 28 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.28-1.el5.fmi
- Updated macgyver API forces recompile

* Tue Sep 14 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.9.14-1.el5.fmi
- Upgrade to boost 1.44

* Mon Aug  9 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.8.9-1.el5.fmi
- Switched to use Fmi::number_cast

* Wed Jul 21 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.21-1.el5.fmi
- Added configuration value xml.tag for XmlFormatter

* Tue Jul  6 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.6-1.el5.fmi
- Wxml format now expects input to contain latlon info too

* Thu Apr 29 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.4.29-1.el5.fmi
- Added SparseTable and related formatting tools

* Fri Jan 15 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.15-1.el5.fmi
- Upgrade to boost 1.41

* Tue Jul 14 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.14-1.el5.fmi
- Upgrade to boost 1.39

* Wed May 27 2009 westerba <antti.westerberg@fmi.fi> - 9.3.27-1.el5.fmi
- Full rebuild release of Brainstorm

* Wed Mar 25 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.3.25-1.el5.fmi
- Full rebuild release of Brainstorm

* Fri Dec 19 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.19-1.el5.fmi
- Moved time tools to macgyver library for more common use

* Wed Nov 19 2008 westerba <antti.westerberg@fmi.fi> - 8.11.19-1.el5.fmi
- New API version and epoch time tools removed as unnecessary

* Mon Oct  6 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.10.6-1.el5.fmi
- Initial build
