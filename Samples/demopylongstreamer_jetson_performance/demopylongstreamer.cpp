/*  demopylongstreamer.cpp: Sample application using CInstantCameraAppSrc class.
    This will stream image data from a Basler camera to a GStreamer pipeline.
	CPipelineHelper is included to help create sample pipelines
	CInstantCameraAppSrc and CPipelineHelper are intended to be used as modules, so that functionality can be added over time.

	Copyright 2017 Matthew Breit <matt.breit@gmail.com>

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

	THIS SOFTWARE REQUIRES ADDITIONAL SOFTWARE (IE: LIBRARIES) IN ORDER TO COMPILE
	INTO BINARY FORM AND TO FUNCTION IN BINARY FORM. ANY SUCH ADDITIONAL SOFTWARE
	IS OUTSIDE THE SCOPE OF THIS LICENSE.
	

	DemoPylonGStreamer:
	Demo of InstantCameraAppSrc class (and PipelineHelper).

	Concept Overview:
	<--------------- InstantCameraAppSrc -------------->    <------------ PipelineHelper ----------->
	+--------------------------------------------------+    +---------+    +---------+    +---------+
	| source                                           |    | element |    | element |    | sink    |
	| (camera + driver + GstAppSrc + rescale + rotate) |    |         |    |         |    |         |
	|                                                 src--sink      src--sink      src--sink       |
	+--------------------------------------------------+    +---------+    +---------+    +---------+

	Usage:
	demopylongstreamer -options -pipeline

	Options:
	-camera <serialnumber> (Use a specific camera. If not specified, will use first camera found.)
	-aoi <width> <height> (Camera's Area Of Interest. If not specified, will use camera's maximum.)
	-rescale <width> <height> (Will rescale the image for the pipeline if desired.)
	-rotate <degrees clockwise> (Will rotate 90, 180, 270 degrees clockwise)
	-framerate <fps> (If not specified, will use camera's maximum under current settings.)
	-ondemand (Will software trigger the camera when needed instead of using continuous free run. May lower CPU load.)
	-usetrigger (Will configure the camera to expect a hardware trigger on IO Line 1. eg: TTL signal.)

	Pipeline Examples (pick one):
	-h264stream <ipaddress> (Encodes images as h264 and transmits stream to another PC running a GStreamer receiving pipeline.)
	-h264file <filename> <number of images> (Encodes images as h264 and records stream to local file.)
	-window (displays the raw image stream in a window on the local machine.)
	-framebuffer <fbdevice> (directs raw image stream to Linux framebuffer. eg: /dev/fb0)
	-parse <string> (try your existing gst-launch-1.0 pipeline string. We will replace the original pipeline source with the Basler camera.)

	Examples:
	demopylongstreamer -window
	demopylongstreamer -camera 12345678 -aoi 640 480 -framerate 15 -rescale 320 240 -h264file mymovie.h264
	demopylongstreamer -rescale 320 240 -parse "gst-launch-1.0 videotestsrc ! videoflip method=vertical-flip ! videoconvert ! autovideosink"

	Quick-Start Example:
	demopylongstreamer -window
	
	NVIDIA TX1/TX2 Note:
	When using autovideosink for display, the system-preferred built-in videosink plugin does advertise the formats it supports. So the image must be converted manually.
	For an example of how to do this, see CPipelineHelper::build_pipeline_display(). 
	If you are using demopylongstreamer with the -parse argument in order to use your own pipeline, add a caps filter after the normal videoconvert and before autovideosink:
	./demopylongstreamer -parse "gst-launch-1.0 videotestsrc ! videoflip method=vertical-flip ! videoconvert ! video/x-raw,format=I420 ! autovideosink"

	Note:
	Some GStreamer elements (plugins) used in the pipeline examples may not be available on all systems. Consult GStreamer for more information:
	https://gstreamer.freedesktop.org/
*/


#include "../../InstantCameraAppSrc/CInstantCameraAppSrc.h"
#include "CPipelineHelper.h"
#include <gst/gst.h>

using namespace std;

int exitCode = 0;

// ******* variables, call-backs, etc. for use with gstreamer ********
// The main event loop manages all the available sources of events for GLib and GTK+ applications
GMainLoop *loop;
// The GstBus is an object responsible for delivering GstMessage packets in a first-in first-out way from the streaming threads
GstBus *bus;
guint bus_watch_id;

// we link elements together in a pipeline, and send messages to/from the pipeline.
GstElement *pipeline;

// handler for bus call messages
gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
	try
	{
		GMainLoop *loop = (GMainLoop *)data;

		switch (GST_MESSAGE_TYPE(msg)) {

		case GST_MESSAGE_EOS:
			g_print("End of stream\n");
			g_main_loop_quit(loop);
			break;

		case GST_MESSAGE_ERROR: {
			gchar  *debug;
			GError *error;

			gst_message_parse_error(msg, &error, &debug);
            g_printerr ("ERROR from element %s: %s\n", GST_OBJECT_NAME (msg->src), error->message);
            g_printerr ("Debugging info: %s\n", (debug) ? debug : "none");
      
			g_error_free(error);
            g_free(debug);
      
			g_main_loop_quit(loop);
			break;
		}

		default:
			break;
		}

		return TRUE;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in bus_call(): " << endl << e.what() << endl;
		return FALSE;
	}
}

static void sigint_restore()
{
	try
	{
#ifndef WIN32
		struct sigaction action;
		memset(&action, 0, sizeof(action));
		action.sa_handler = SIG_DFL;
		sigaction(SIGINT, &action, NULL);
#endif
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in sigint_restore(): " << endl << e.what() << endl;
	}
}

// Signal handler for ctrl+c
void IntHandler(int dummy)
{
	try
	{
		// send End Of Stream event to all pipeline elements
		cout << endl;
		cout << "Sending EOS event to pipeline..." << endl;
		gst_element_send_event(pipeline, gst_event_new_eos());
		sigint_restore();
		return;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in IntHandler(): " << endl << e.what() << endl;
	}
}

// ******* END variables, call-backs, etc. for use with gstreamer ********

// *********** Command line argument variables and parser **************
// quick-start settings for AOI and framerate. Use maximum possible camera resultion and maximum possible framerate @ resolution.
int width = -1;
int height = -1;
int frameRate = -1;

int numImagesToRecord = -1; // capture indefinitley unless otherwise specified.
int scaledWidth = -1; // do not scale by default
int scaledHeight = -1;
int rotation = -1; // do not rotate or flip image by default
bool h264stream = false;
bool h264multicast = false;
bool h264file = false;
bool display = false;
bool framebuffer = false;
bool parsestring = false;
bool onDemand = false;
bool useTrigger = false;
string serialNumber = "";
string ipaddress = "";
string filename = "";
string fbdev = "";
string pipelineString = "";

int ParseCommandLine(gint argc, gchar *argv[])
{
	try
	{
		if (argc < 2)
		{
			cout << endl;
			cout << "DemoPylonGStreamer: " << endl;
			cout << " Demo of InstantCameraAppSrc class (and PipelineHelper)." << endl;
			cout << endl;
			cout << "Concept Overview:" << endl;
			cout << " <--------------- InstantCameraAppSrc -------------->    <------------ PipelineHelper ----------->" << endl;
			cout << " +--------------------------------------------------+    +---------+    +---------+    +---------+" << endl;
			cout << " | source                                           |    | element |    | element |    | sink    |" << endl;
			cout << " | (camera + driver + GstAppSrc + rescale + rotate) |    |         |    |         |    |         |" << endl;
			cout << " |                                                 src--sink      src--sink      src--sink       |" << endl;
			cout << " +--------------------------------------------------+    +---------+    +---------+    +---------+" << endl;
			cout << endl;
			cout << "Usage:" << endl;
			cout << " demopylongstreamer -options -pipeline" << endl;
			cout << endl;
			cout << "Options: " << endl;
			cout << " -camera <serialnumber> (Use a specific camera. If not specified, will use first camera found.)" << endl;
			cout << " -aoi <width> <height> (Camera's Area Of Interest. If not specified, will use camera's maximum.)" << endl;
			cout << " -rescale <width> <height> (Will rescale the image for the pipeline if desired.)" << endl;
			cout << " -rotate <degrees clockwise> (Will rotate 90, 180, 270 degrees clockwise)" << endl;
			cout << " -framerate <fps> (If not specified, will use camera's maximum under current settings.)" << endl;
			cout << " -ondemand (Will software trigger the camera when needed instead of using continuous free run. May lower CPU load.)" << endl;
			cout << " -usetrigger (Will configure the camera to expect a hardware trigger on IO Line 1. eg: TTL signal.)" << endl;
			cout << endl;
			cout << "Pipeline Examples (pick one):" << endl;
			cout << " -h264stream <ipaddress> (Encodes images as h264 and transmits stream to another PC running a GStreamer receiving pipeline.)" << endl;
			cout << " -h264multicast <ipaddress> (Encodes images as h264 and multicasts stream to the network.)" << endl;
			cout << " -h264file <filename> <number of images> (Encodes images as h264 and records stream to local file.)" << endl;
			cout << " -window (displays the raw image stream in a window on the local machine.)" << endl;
			cout << " -framebuffer <fbdevice> (directs raw image stream to Linux framebuffer. eg: /dev/fb0)" << endl;
			cout << " -parse <string> (try your existing gst-launch-1.0 pipeline string. We will replace the original pipeline source with the Basler camera if needed.)" << endl;
			cout << endl;
			cout << "Examples: " << endl;
			cout << " demopylongstreamer -framebuffer /dev/fb0" << endl;
			cout << " demopylongstreamer -rescale 640 480 -h264stream 172.17.1.199" << endl;
			cout << " demopylongstreamer -camera 12345678 -aoi 640 480 -framerate 15 -rescale 320 240 -h264file mymovie.h264" << endl;
			cout << " demopylongstreamer -rescale 320 240 -parse \"gst-launch-1.0 videotestsrc ! videoflip method=vertical-flip ! videoconvert ! autovideosink\"" << endl;
			cout << " demopylongstreamer -rescale 320 240 -parse \"videoflip method=vertical-flip ! videoconvert ! autovideosink\"" << endl;
			cout << endl;
			cout << "Quick-Start Example to display stream:" << endl;
			cout << " demopylongstreamer -window" << endl;
			cout << endl;
			cout << "NVIDIA TX1/TX2 Note:" << endl;
			cout << "When using autovideosink for display, the system-preferred built-in videosink plugin does advertise the formats it supports. So the image must be converted manually." << endl;
			cout << "For an example of how to do this, see CPipelineHelper::build_pipeline_display()." << endl;
			cout << "If you are using demopylongstreamer with the -parse argument in order to use your own pipeline, add a caps filter after the normal videoconvert and before autovideosink:" << endl;
			cout << "./demopylongstreamer -parse \"gst-launch-1.0 videotestsrc ! videoflip method=vertical-flip ! videoconvert ! video/x-raw,format=I420 ! autovideosink\"" << endl;
			cout << endl;
			cout << "Note:" << endl;
			cout << " Some GStreamer elements (plugins) used in the pipeline examples may not be available on all systems. Consult GStreamer for more information:" << endl;
			cout << " https://gstreamer.freedesktop.org/" << endl;
			cout << endl;

			return -1;
		}

		for (int i = 1; i < argc; i++)
		{
			if (string(argv[i]) == "-camera")
			{
				if (argv[i + 1] != NULL)
					serialNumber = string(argv[i + 1]);
				else
				{
					cout << "Serial number not specified. eg: -camera 21045367" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-aoi")
			{
				if (argv[i + 1] != NULL)
					width = atoi(argv[i + 1]);
				else
				{
					cout << "AOI width not specified. eg: -aoi 320 240" << endl;
					return -1;
				}
				if (argv[i + 2] != NULL)
					height = atoi(argv[i + 2]);
				else
				{
					cout << "AOI height not specified. eg: -aoi 320 240" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-rescale")
			{
				if (argv[i + 1] != NULL)
					scaledWidth = atoi(argv[i + 1]);
				else
				{
					cout << "Scaling width not specified. eg: -scaled 320 240" << endl;
					return -1;
				}
				if (argv[i + 2] != NULL)
					scaledHeight = atoi(argv[i + 2]);
				else
				{
					cout << "Scaling height not specified. eg: -scaled 320 240" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-rotate")
			{
				if (argv[i + 1] != NULL)
					rotation = atoi(argv[i + 1]);
				else
				{
					cout << "Rotation not specified. eg: -rotate 90" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-framerate")
			{
				if (argv[i + 1] != NULL)
					frameRate = atoi(argv[i + 1]);
				else
				{
					cout << "Framerate not specified. eg: -framerate 100" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-ondemand")
			{
				onDemand = true;
			}
			else if (string(argv[i]) == "-usetrigger")
			{
				useTrigger = true;
			}
			else if (string(argv[i]) == "-h264stream")
			{
				h264stream = true;
				if (argv[i + 1] != NULL)
					ipaddress = string(argv[i + 1]);
				else
				{
					cout << "IP Address not specified. eg: -h264stream 192.168.2.102" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-h264multicast")
			{
				h264multicast = true;
				if (argv[i + 1] != NULL)
					ipaddress = string(argv[i + 1]);
				else
				{
					cout << "IP Address not specified. eg: -h264multicast 224.1.1.1" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-h264file")
			{
				h264file = true;
				if (argv[i + 1] != NULL)
					filename = string(argv[i + 1]);
				else
				{
					cout << "Filename not specified. eg: -h264file filename 100" << endl;
					return -1;
				}
				if (argv[i + 2] != NULL)
					numImagesToRecord = atoi(argv[i + 2]);
				else
				{
					cout << "Number of images not specified. eg: -h264file filename 100" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-window")
			{
				display = true;
			}
			else if (string(argv[i]) == "-framebuffer")
			{
				framebuffer = true;
				if (argv[i + 1] != NULL)
					fbdev = string(argv[i + 1]);
				else
				{
					cout << "Framebuffer not specified. eg: -framebuffer /dev/fb0" << endl;
					return -1;
				}
			}
			else if (string(argv[i]) == "-parse")
			{
				parsestring = true;
				if (argv[i + 1] != NULL)
					pipelineString = string(argv[i + 1]);
				else
				{
					cout << "pipeline string not specified. Use one of these format with quotes: \"gst-launch-1.0 videotestsrc ! videoflip method=vertical-flip ! videoconvert ! autovideosink\" or \"videoflip method=vertical-flip ! videoconvert ! autovideosink\"" << endl;
					return -1;
				}
			}
			// deprecated
			else if (string(argv[i]) == "-display")
			{
				display = true;
			}
			// deprecated
			else if (string(argv[i]) == "-width")
			{
				if (argv[i + 1] != NULL)
					width = atoi(argv[i + 1]);
				else
				{
					cout << "Width not specified. eg: -width 640" << endl;
					return -1;
				}
			}
			// deprecated
			else if (string(argv[i]) == "-height")
			{
				if (argv[i + 1] != NULL)
					height = atoi(argv[i + 1]);
				else
				{
					cout << "Height not specified. eg: -height 480" << endl;
					return -1;
				}
			}			
		}

		bool pipelinesAvailable[] = { display, framebuffer, h264file, h264stream, h264multicast, parsestring };
		int pipelinesRequested = 0;

		for (int i = 0; i < sizeof(pipelinesAvailable); i++)
			pipelinesRequested += (int)pipelinesAvailable[i];

		if (pipelinesRequested == 0)
		{
			cout << "No Pipeline Specified. Please specifiy one (and only one)." << endl;
			return -1;
		}

		if (pipelinesRequested > 1)
		{
			cout << "Too Many Pipelines Specified. Please use only one." << endl;
			return -1;
		}

		return 0;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in ParseCommandLine(): " << endl << e.GetDescription() << endl;
		return -1;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in ParseCommandLine(): " << endl << e.what() << endl;
		return -1;
	}
}

// *********** END Command line argument variables and parser **************

gint main(gint argc, gchar *argv[])
{

	try
	{

		if (ParseCommandLine(argc, argv) == -1)
		{
			exitCode = -1;
			return exitCode;
		}

		// signal handler for ctrl+C
		signal(SIGINT, IntHandler);

		cout << "Press CTRL+C at any time to quit." << endl;

		// initialize GStreamer 
		gst_init(NULL, NULL);

		// create the mainloop
		loop = g_main_loop_new(NULL, FALSE);

		// The InstantCameraForAppSrc will manage the camera and driver
		// and provide a source element to the GStreamer pipeline.
		CInstantCameraAppSrc camera(serialNumber);

		// reset the camera to defaults if you like
		cout << "Resetting camera to default settings..." << endl;
		camera.ResetCamera();

		// Initialize the camera and driver
		cout << "Initializing camera and driver..." << endl;
		camera.InitCamera(width, height, frameRate, onDemand, useTrigger, scaledWidth, scaledHeight, rotation, numImagesToRecord);		

		cout << "Using Camera             : " << camera.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Camera Area Of Interest  : " << camera.GetWidth() << "x" << camera.GetHeight() << endl;
		cout << "Camera Speed             : " << camera.GetFrameRate() << " fps" << endl;
		if (scaledWidth != -1 && scaledHeight != -1)
			cout << "Images will be scaled to : " << scaledWidth << "x" << scaledHeight << endl;
		if (rotation != -1)
			cout << "Images will be rotated   : " << rotation << " degrees clockwise" << endl;

		// create a new pipeline to add elements too
		pipeline = gst_pipeline_new("pipeline");

		// prepare a handler (watcher) for messages from the pipeline
		bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// A pipeline needs a source element. The InstantCameraForAppSrc will create, configure, and provide an AppSrc which fits the camera.
		GstElement *source = camera.GetSource();
		
		// Build the rest of the pipeline based on the sample chosen.
		// The PipelineHelper will manage the configuration of GStreamer pipelines.  
		// The pipeline helper can be expanded to create several kinds of pipelines
		// as these can depend heavily on the application and host capabilities.
		// Rescaling the image is optional. In this sample we do rescaling and rotation in the InstantCameraAppSrc.
		CPipelineHelper myPipelineHelper(pipeline, source);

		bool pipelineBuilt = false;

		if (display == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_display();
		else if (h264stream == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_h264stream(ipaddress.c_str());
		else if (h264multicast == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_h264multicast(ipaddress.c_str());
		else if (h264file == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_h264file(filename.c_str());
		else if (framebuffer == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_framebuffer(fbdev.c_str());
		else if (parsestring == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_parsestring(pipelineString.c_str());

		if (pipelineBuilt == false)
		{
			exitCode = -1;
			throw std::runtime_error("Pipeline building failed!");
		}

		// Start the camera and grab engine.
		if (camera.StartCamera() == false)
		{
			exitCode = -1;
			throw std::runtime_error("Could not start camera!");
		}
		
		// Start the pipeline.
		cout << "Starting pipeline..." << endl;
		gst_element_set_state(pipeline, GST_STATE_PLAYING);

		// run the main loop. When Ctrl+C is pressed, an EOS event will be sent
		// which will shutdown the pipeline in intHandler(), which will in turn quit the main loop.
		g_main_loop_run(loop);

		// clean up
		cout << "Stopping pipeline..." << endl;
		gst_element_set_state(pipeline, GST_STATE_NULL);

		camera.StopCamera();
		camera.CloseCamera();
		
		gst_object_unref(GST_OBJECT(pipeline));
		g_main_loop_unref(loop);

		exitCode = 0;

	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in main(): " << endl << e.GetDescription() << endl;
		exitCode = -1;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in main(): " << endl << e.what() << endl;
		exitCode = -1;
	}
	
	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press Enter to exit." << endl;
	while (cin.get() != '\n');
	
	return exitCode;
}
