#!/bin/bash

SDK_VERSION=$1

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_MacOS_V${SDK_VERSION}/static_lib/libPlayerOneCamera_Static.a static_libs/macOS

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_Linux_V${SDK_VERSION}/static_lib/libPlayerOneCamera_Static.a static_libs/x86_64/

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_RaspberryPI_V${SDK_VERSION}/static_lib/libPlayerOneCamera_Static64.a static_libs/aarch64/libPlayerOneCamera_Static.a

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_RaspberryPI_V${SDK_VERSION}/static_lib/libPlayerOneCamera_Static.a static_libs/armv7l/

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_Windows_V${SDK_VERSION}/static_lib/x86/PlayerOneCamera_Static.lib static_libs/Windows/Win32

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_Windows_V${SDK_VERSION}/static_lib/x64/PlayerOneCamera_Static.lib static_libs/Windows/x64

cp ~/src/Astro/PlayerOne_Camera_SDK/PlayerOne_Camera_SDK_MacOS_V${SDK_VERSION}/include/PlayerOneCamera.h .


