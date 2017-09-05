# pylongstreamer (using the InstantCameraAppSrc class)
- The InstantCameraAppSrc class extends the Basler Pylon InstantCamera class for easy integration into GStreamer's GstAppSrc source element. InstantCameraAppSrc can easily be modified to access camera features as needed. Or, you can access features directly in the implementation, e.g. CInstantCameraAppSrc myCamera; myCamera.GetDeviceInfo().GetFriendlyName())
- The PipelineHelper class helps to finish creation of pipelines. The PipelineHelper can also easily be modified to add additional pipelines.
- pylongstreamer.cpp is a full example application using both classes.

Usage:
pylongstreamer -setting1 value1 -setting2 value2 -pipeline parameter

Example:
pylongstreamer -camera 21234567 -width 640 -height 480 -framerate 30 -usetrigger -h264stream 192.168.1.2

Run without any arguments to see a list.

"source" folder: Full source code and linux Makefile
"vs" folder: Visual Studio 2013 solution and project

Tested on Linux (x86/x64/armhf/aarch64 & Windows 7 x64).
Built with Pylon 5.0.9 (linux) and 5.0.10 (windows), and GStreamer 1.0.

(will expand readme more later... :))
