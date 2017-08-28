# pylongstreamer
Using Basler's Pylon API with GStreamer's GstAppSrc element.
The CInstantCameraForAppSrc class extends the Basler Pylon CInstantCamera class for easy integration into GStreamer's GstAppSrc source element. Then the CPipelineHelper class helps to finish creation of pipelines.
CInstantCameraForAppSrc can easily be modified to access camera features as needed.
CPipelineHelper can easily be modified to add additional pipelines.

Usage:
pylongstreamer -setting1 value1 -setting2 value2 -pipeline parameter

Example:
pylongstreamer -camera 21234567 -width 640 -height 480 -framerate 30 -usetrigger -h264stream 192.168.1.2

Run without any arguments to see a list.

"source" folder: Full source code and linux Makefile
"vs" folder: Visual Studio 2013 solution and project

Tested on Linux (x86/x64/armhf/aarch64 & Windows 7 x64).
Built with Pylon 5.0.9 (linux) and 5.0.10 (windows), and GStreamer 1.0.

(will expand readme later...)
