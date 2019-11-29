# Using Pylon with GStreamer: The InstantCameraAppSrc class
- The InstantCameraAppSrc class presents the Basler camera as a source element for GStreamer pipelines.
- It combines Pylon's InstantCamera class with GStreamer's AppSrc plugin.
- The InstantCamera class offers access to physical camera functions and Pylon driver functions.
- The AppSrc plugin offers an API to bring user-defined images, data, etc. into GStreamer pipelines.
- InstantCameraAppSrc can be extended via the GenApi to access any camera and driver feature (eg: GetFrameRate()).
- InstantCameraAppSrc cab be extended via GStreamer "bins" to include any other plugins within the source element (eg: Rescale, Rotate, etc.)

# Architecture
```
|<--------------- Camera Acquisition & Pylon Grabbing ------------------->|<---------------------------------------- GStreamer Pipeline ------------------------->|
|<------------------------------------------------------------- CInstantCameraAppSrc --------------------------------------->|    
+-------------------------------------------------------------------------+--------------------------------------------------+    +----------+    +---------------+
|                                                       GetGstAppSrc()---------> sourceBin element                           |    |  other   |    | sink element  |
|                                                                         |                                                  |    | elements |    |               |
|                                    RetrieveImage()<---cb_need_data()<---------"need-data" signal                           |    |          |    |               |
|            ------------------> --> 1. RetrieveResult()                  |                                                  |    |          |    |               |
|            | LatestImageOnly |     2. Convert to RGB if color           |                                                  |    |          |    |               |
|            <------------------     3. Put into a [pylonimage]           | +-------------+ +------------+ +------------+    |    |          |    |               |
| [Camera]-->[Pylon Grab Engine]     4. Wrap in a gst buffer              | |             | |            | |            |    |    |          |    |               |
| -------->                          5. "push-buffer" signal-------------------->AppSrc-------->Rescale------->Rotate------>src--sink       src--sink             |
| |freerun|                                                               | |             | |            | |            |    |    |          |    |               |
| <--------                                                               | +-------------+ +------------+ +------------+    |    |          |    |               |
+-------------------------------------------------------------------------+--------------------------------------------------+    +----------+    +---------------+
```

# Sample Programs
- Sample programs based on the InstantCameraAppSrc class are found in the Samples folder.
- "DemoPylonGStreamer" is a rich demonstration of possibilities, including a "PipelineHelper" class to assist in making pipelines.
- "SimpleGrab" is an example of the bare minimum code needed to create a GStreamer application.
- Linux makefiles are included for each sample application.
- Windows Visual Studio project files are included for each sample application in the respective "vs" folder.

# Requirements
- Linux x86/x64/ARM or Windows 7/10. (OSX has not been tested.)
- Pylon 5.0.9 or higher on Linux. Pylon 5.0.10 or higher on Windows. (Older versions down to Pylon 3.0 may work, but are untested.)
- GStreamer 1.0. (GStreamer 0.1 may work, but is untested.)
- Note for Linux users: You may need to install the following libraries:
  gstreamer1.0
  gstreamer1.0-dev
  gstreamer1.0-libav
  gstreamer1.0-plugins-bad
  libgstreamer-plugins-base1.0-dev
  (on Ubuntu systems, all can be isntalled using apt-get install)

# Note about GStreamer Plugins (elements)
- All systems are different. Some GStreamer pipeline plugins used in the Sample Programs may not exist on your system!
- As a result, you may see errors like "Not Negotiated", etc. when running the samples.
- Consult GStreamer directly for more information: https://gstreamer.freedesktop.org/
