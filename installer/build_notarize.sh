#!/bin/bash

PACKAGE_NAME="PlayerOne_X2.pkg"
BUNDLE_NAME="org.rti-zone.PlayerOneX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libPlayerOne.dylib
fi


mkdir -p ROOT/tmp/PlayerOne_X2/
cp "../PlayerOneCamera.ui" ROOT/tmp/PlayerOne_X2/
cp "../PlayerOneCamSelect.ui" ROOT/tmp/PlayerOne_X2/
cp "../PlayerOne.png" ROOT/tmp/PlayerOne_X2/
cp "../cameralist PlayerOne.txt" ROOT/tmp/PlayerOne_X2/
cp "../build/Release/libPlayerOne.dylib" ROOT/tmp/PlayerOne_X2/


if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier "$BUNDLE_NAME" --sign "$installer_signature" --scripts Scripts --version 1.0 "$PACKAGE_NAME"
	pkgutil --check-signature "./${PACKAGE_NAME}"
	xcrun notarytool submit "$PACKAGE_NAME" --keychain-profile "$AC_PROFILE" --wait
	xcrun stapler staple $PACKAGE_NAME
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi


rm -rf ROOT
