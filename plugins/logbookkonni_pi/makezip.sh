#!/bin/sh

#
# This is needed because creating the zip files with cmake does not work.
# See the comments in cmake/PluginLocalization.cmake
#

mkdir -p build

#
# Languages zip contains the language resource files.  Build the plugin
# first using cmake before creating this file.  We build this file anyway
# even though the languages are included in the packages just in case
# there are changes needed.
#
rm -f build/LogbookKonni_Languages.zip
(
    cd build/Resources
    zip -r ../LogbookKonni_Languages *
)

#
# Layouts zip contains the layouts used when printing or saving the logbook
# to HTML or ODT file format. It also contains the help.
#
rm -f build/LogbookKonni_Layouts.zip
(
    cd Layouts
    zip -r ../build/LogbookKonni_Layouts.zip HTMLLayouts ODTLayouts Clouds

)
