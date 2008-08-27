#! /bin/sh
powerdevil_subdirs="kcmodule daemon plasma/runner plasma/applet"
$EXTRACTRC `find $powerdevil_subdirs -name \*.ui` >> rc.cpp || exit 11
$EXTRACTRC `find $powerdevil_subdirs -name \*.rc` >> rc.cpp || exit 11
$XGETTEXT `find $powerdevil_subdirs -name \*.cpp -o -name \*.h` *.cpp *.h -o $podir/powerdevil.pot
rm -f rc.cpp

