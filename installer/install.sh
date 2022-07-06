#!/bin/bash

TheSkyX_Install=~/Library/Application\ Support/Software\ Bisque/TheSkyX\ Professional\ Edition/TheSkyXInstallPath.txt
echo "TheSkyX_Install = $TheSkyX_Install"

if [ ! -f "$TheSkyX_Install" ]; then
    echo TheSkyXInstallPath.txt not found
    TheSkyX_Path=`/usr/bin/find ~/ -maxdepth 3 -name TheSkyX`
    if [ -d "$TheSkyX_Path" ]; then
		TheSkyX_Path="${TheSkyX_Path}/Contents"
    else
	   echo TheSkyX application was not found.
    	exit 1
	 fi
else
	TheSkyX_Path=$(<"$TheSkyX_Install")
fi

echo "Installing to $TheSkyX_Path"


if [ ! -d "$TheSkyX_Path" ]; then
    echo TheSkyX Install dir not exist
    exit 1
fi

if [ -d "$TheSkyX_Path/Resources/Common/PlugIns64" ]; then
	PLUGINS_DIR="PlugIns64"
elif [ -d "$TheSkyX_Path/Resources/Common/PlugInsARM32" ]; then
	PLUGINS_DIR="PlugInsARM32"
elif [ -d "$TheSkyX_Path/Resources/Common/PlugInsARM64" ]; then
	PLUGINS_DIR="PlugInsARM64"
else
	PLUGINS_DIR="PlugIns"
fi


cp "./cameralist SVBony.txt" "$TheSkyX_Path/Resources/Common/Miscellaneous Files/"
cp "./SVBonyCamera.ui" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/"
cp "./SVBonyCamSelect.ui" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/"
cp "./libSVBony.so" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/"
sudo cp -f "./90-ckusb.rules" "/etc/udev/rules.d/"

app_owner=`/usr/bin/stat -c "%u" "$TheSkyX_Path" | xargs id -n -u`
if [ ! -z "$app_owner" ]; then
	chown $app_owner "$TheSkyX_Path/Resources/Common/Miscellaneous Files/cameralist SVBony.txt"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/SVBonyCamera.ui"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/SVBonyCamSelect.ui"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/libSVBony.so"
fi
chmod  755 "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/CameraPlugIns/libSVBony.so"
sudo systemctl restart udev

