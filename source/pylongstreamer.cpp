/*  pylongstreamer.cpp: Sample application using CInstantCameraAppSrc class.
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
	

Usage:
pylongstreamer -camera <serialnumber> -width <columns> -height <rows> -framerate <fps> -ondemand -usetrigger -<pipeline> <options>

Example:
pylongstreamer -camera 12345678 -width 320 -height 240 -framerate 15 -h264file mymovie.h264

Quick-Start Example (use first camera found, display in window, 640x480, 30fps):
pylongstreamer -display

Note:
-camera: If not used, we will use first detected camera.
-ondemand: Instead of freerunning, camera will be software triggered with each need-data signal. May lower CPU load, but may be less 'real-time'.
-usetrigger: Camera will expect to be hardware triggered by user via IO ports (cannot be used with -ondemand).
-framebuffer <fbdevice> (directs raw image stream to Linux framebuffer, e.g. /dev/fb0). Useful when using additional displays

Pipeline Examples (pick one):
-h264stream <ipaddress> (Encodes images as h264 and transmits stream to another PC running a GStreamer receiving pipeline.)
-h264file <filename> <number of images> (Encodes images as h264 and saves stream to local file.)
-display (displays the raw image stream in a window on the local machine.)
-framebuffer <fbdevice> (directs raw image stream to Linux framebuffer, e.g. /dev/fb0)

Note:
Some GStreamer elements (plugins) used in the pipeline examples may not be available on all systems. Consult GStreamer for more information:
https://gstreamer.freedesktop.org/
*/


#include "CInstantCameraAppSrc.h"
#include "CPipelineHelper.h"
#include <gst/gst.h>

using namespace std;

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
		//! Emit the EOS signal which tells all the elements to shut down properly:
		cout << endl;
		cout << "Sending EOS signal to shutdown pipeline cleanly..." << endl;
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
// quick-start settings for AOI and framerate
int width = 640;
int height = 480;
int frameRate = 30;

int numImagesToRecord = -1; // capture indefinitley unless otherwise specified.
bool h264stream = false;
bool h264file = false;
bool display = false;
bool framebuffer = false;
bool onDemand = false;
bool useTrigger = false;
string serialNumber = "";
string ipaddress = "";
string filename = "";
string fbdev = "";

int ParseCommandLine(gint argc, gchar *argv[])
{
	try
	{
		if (argc < 2)
		{
			cout << endl;
			cout << "PylonGStreamer: " << endl;
			cout << " Example of using Basler's Pylon API with GStreamer's GstAppSrc element." << endl;
			cout << endl;
			cout << "Pipeline Example:" << endl;
			cout << " +--------------------------------+                   +---------------------+    +---------- +    +----------------+" << endl;
			cout << " | CInstantCameraAppSrc        |                   | AppSrc              |    | convert   |    | autovideosink  |" << endl;
			cout << " | (Camera & Pylon Grab Engine)   |<--- need-data <---|                     |    |           |    |                |" << endl;
			cout << " |                                |--> push-buffer -->|                    src--sink        src--sink              |" << endl;
			cout << " +--------------------------------+                   +---------------------+    +-----------+    +----------------+" << endl;
			cout << endl;
			cout << "Usage:" << endl;
			cout << " pylongstreamer -camera <serialnumber> -width <columns> -height <rows> -framerate <fps> -ondemand -usetrigger -<pipeline> <options>" << endl;
			cout << endl;
			cout << "Example: " << endl;
			cout << " pylongstreamer -camera 12345678 -width 320 -height 240 -framerate 15 -h264file mymovie.h264" << endl;
			cout << endl;
			cout << "Quick-Start Example (use first camera found, display in window, 640x480, 30fps):" << endl;
			cout << " pylongstreamer -display" << endl;
			cout << endl;
			cout << "Notes:" << endl;
			cout << " -camera: If not used, we will use first detected camera." << endl;
			cout << " -ondemand: Instead of freerunning, camera will be software triggered with each need-data signal. May lower CPU load, but may be less 'real-time'." << endl;
			cout << " -usetrigger: Camera will expect to be hardware triggered by user via IO ports (cannot be used with -ondemand)." << endl;
			cout << endl;
			cout << "Pipeline Examples (pick one):" << endl;
			cout << " -h264stream <ipaddress> (Encodes images as h264 and transmits stream to another PC running a GStreamer receiving pipeline.)" << endl;
			cout << " -h264file <filename> <number of images> (Encodes images as h264 and saves stream to local file.)" << endl;
			cout << " -display (displays the raw image stream in a window on the local machine.)" << endl;
			cout << " -framebuffer <fbdevice> (directs raw image stream to Linux framebuffer, e.g. /dev/fb0). Useful when using additional displays" << endl;
			cout << endl;
			cout << "Note:" << endl;
			cout << " Some GStreamer elements (plugins) used in the pipeline examples may not be available on all systems. Consult GStreamer for more information:" << endl;
			cout << " https://gstreamer.freedesktop.org/" << endl;
			cout << endl;

			return -1;
		}

		for (int i = 1; i < argc; i++)
		{
			if (string(argv[i]) == "-h264stream")
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
			else if (string(argv[i]) == "-display")
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
			else if (string(argv[i]) == "-width")
			{
				if (argv[i + 1] != NULL)
					width = atoi(argv[i + 1]);
				else
					return -1;
			}
			else if (string(argv[i]) == "-height")
			{
				if (argv[i + 1] != NULL)
					height = atoi(argv[i + 1]);
				else
					return -1;
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
			else if (string(argv[i]) == "-camera")
			{
				if (argv[i + 1] != NULL)
					serialNumber = string(argv[i + 1]);
				else
				{
					cout << "Serial number not specified. eg: -camera 21045367" << endl;
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
		}

		if (display == false && framebuffer == false && h264file == false && h264stream == false)
		{
			cout << "No pipeline specified." << endl;
			return -1;
		}

		return 0;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in ParseCommandLine(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in ParseCommandLine(): " << endl << e.what() << endl;
		return false;
	}
}

// *********** END Command line argument variables and parser **************

gint main(gint argc, gchar *argv[])
{

	try
	{

		if (ParseCommandLine(argc, argv) == -1)
			return -1;

		// signal handler for ctrl+C
		signal(SIGINT, IntHandler);

		cout << "Press CTRL+C at any time to quit." << endl;

		// initialize GStreamer 
		gst_init(NULL, NULL);

		// create the mainloop
		loop = g_main_loop_new(NULL, FALSE);

		// The InstantCameraForAppSrc will manage the camera and driver
		// and provide a source element to the GStreamer pipeline.
		CInstantCameraAppSrc camera;

		// Initialize the camera and driver
		cout << "Initializing camera and driver..." << endl;
		if (camera.InitCamera(serialNumber, width, height, frameRate, onDemand, useTrigger) == false)
			return -1;

		cout << "Using Camera        : " << camera.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Image Dimensions    : " << camera.GetWidth() << "x" << camera.GetHeight() << endl;
		cout << "Resulting FrameRate : " << camera.GetFrameRate() << endl;

		// create a new pipeline to add elements too
		pipeline = gst_pipeline_new("pipeline");

		// prepare a handler (watcher) for messages from the pipeline
		bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// A pipeline needs a source element. The InstantCameraForAppSrc will create, configure, and provide an AppSrc which fits the camera.
		GstElement *source = camera.GetAppSrc();
		
		// Build the rest of the pipeline based on the sample chosen.
		// The PipelineHelper will manage the configuration of GStreamer pipelines.  
		// The pipeline helper can be expanded to create several kinds of pipelines
		// as these can depend heavily on the application and host capabilities.
		CPipelineHelper myPipelineHelper(pipeline, source);

		bool pipelineBuilt = false;
		if (display == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_display();
		else if (h264stream == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_h264stream(ipaddress.c_str());
		else if (h264file == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_h264file(filename.c_str(), numImagesToRecord);
		else if (framebuffer == true)
			pipelineBuilt = myPipelineHelper.build_pipeline_framebuffer(fbdev.c_str());

		if (pipelineBuilt == false)
		{
			cout << "Pipeline building failed!" << endl;
			return -1;
		}

		// Start the camera and grab engine.
		if (camera.StartCamera() == false)
			return -1;

		// Start the pipeline.
		cout << "Starting pipeline..." << endl;
		gst_element_set_state(pipeline, GST_STATE_PLAYING);

		// run the main loop. When Ctrl+C is pressed, an EOS signal
		// which will shutdown the pipeline in intHandler(), which will in turn quit the main loop.
		g_main_loop_run(loop);

		// clean up
		camera.StopCamera();
		camera.CloseCamera();

		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(pipeline));
		g_main_loop_unref(loop);

		return 0;

	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << endl;
		return -1;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return -1;
	}

	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press Enter to exit." << endl;
	while (cin.get() != '\n');
}
