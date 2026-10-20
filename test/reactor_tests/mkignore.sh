#!/bin/sh

# Determine if RPM is available and actually used for package management
use_rpm=false
have_rpm=$(command -v rpm)

if [ "$have_rpm" ] ; then
    # makefile.inc ís assumed to be present as part of the smartmet development environment
    # Therefor do not check its persence here explicitly
    makefile_inc=$(smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc
    # Check if the makefile.inc belongs to the smartmet-utils-devel package
    case $(rpm -qf "$makefile_inc" 2>/dev/null) in
        smartmet-utils-devel*)
            use_rpm=true
            ;;
    esac
fi

if ! "$use_rpm" ; then
    # If RPM is not used, add the changelog_json.get file to the ignore list
    for arg in $@; do
        echo "$arg"
    done
fi
