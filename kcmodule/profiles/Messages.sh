#! /bin/sh
$EXTRACTRC `find -name \*.ui -o -name \*.rc -o -name \*.kcfg` >> rc.cpp || exit 11
$XGETTEXT `find -name \*.cpp -o -name \*.h -o -name \*.qml` -o $podir/kcm_powerdevilprofilesconfig.pot
rm -f rc.cpp
