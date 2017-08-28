# pylongstreamer
Using Basler's Pylon API with GStreamer's GstAppSrc element.

Usage:
pylongstreamer -option1 -option2 -pipeline

Example:
pylongstreamer -camera 21234567 -width 640 -heigh 480 -framerate 30 -usetrigger -h264stream 192.168.1.2

Run without any arguments to see a list.

/source: Full source code and linux Makefile
/vs: Visual Studio 2013 solution and project

Tested on Linux (x86/x64/armhf/aarch64 & Windows 7 x64).
Built with Pylon 5.0.9 (linux) and 5.0.10 (windows), and GStreamer 1.0.

(will expand readme later...)
