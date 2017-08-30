/*  pylongstreamer.cpp: Implementation file for pylongstreamer.
    This will use the CInstantCameraForAppSrc and CPipelineHelper classes to create an example application
	which will stream image data from a Basler camera to a GStreamer pipeline.
	CInstantCameraForAppSrc and CPipelineHelper are intended to be used as modules, so that functionality can be added over time.

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
	

A Typical Image Streaming Pipeline would look as follows:

|<----------- Camera Acquisition & Pylon Grabbing ------------->|                               |<---------- GStreamer Pipeline for Display----------------->|
|<----------------- CInstantCameraForAppSrc ------------------->|                                                              |<----- CPipelineHelper ----->|
+---------------------------------------------------------------+                               +-------------------------+    +---------+    +--------------+
|                                                               |                               | AppSrc (source element) |    | element |    | sink element |
|                                                               |                               |                         |    |         |    |              |
|                                    {RetrieveImage}<--------------{cb_need_data}<-----------------("need-data")          |    |         |    |              |
|            ------------------> --> 1. {RetrieveResult}        |                               |                         |    |         |    |              |
|            | LatestImageOnly |     2. Convert to RGB if color |                               |                         |    |         |    |              |
|            <------------------     3. Put into a [pylonimage]--->{cb_need_data}               |                         |    |         |    |              |
| [Camera]-->[Pylon Grab Engine]                                |  1. wrap buffer               |                         |    |         |    |              |
| -------->                                                     |  2. ("push-buffer") [buffer]-------------------------->src--sink      src--sink            |
| |freerun|                                                     |                               |                         |    |         |    |              |
| <--------                                                     |                               |                         |    |         |    |              |
+---------------------------------------------------------------+                               +-------------------------+    +---------+    +--------------+

1. The camera and grab engine in this case are always freerunning (unless ondemand is used, then it sits idle and sends a trigger when an image is needed)
2. LatestImageOnly strategy means the Grab Engine keeps the latest image received ready for retrieval.
3. When AppSrc needs data, it sends the "need-data" signal.
4. This fires cb_need_data which calls RetrieveImage().
5. RetrieveImage() retrieves the image from the Grab Engine, converts it to RGB, and places it in a PylonImage container.
6. The memory of the PylonImage container is then wrapped and pushed to AppSrc's src pad by sending the "push-buffer" signal.
7. AppSrc provides the image to the next element in the pipeline via it's source pad.

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


#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include "CInstantCameraForAppSrc.h"
#include "CPipelineHelper.h"

using namespace std;

// ******* variables, call-backs, etc. for use with gstreamer ********
// The main event loop manages all the available sources of events for GLib and GTK+ applications
GMainLoop *loop;
// The GstBus is an object responsible for delivering GstMessage packets in a first-in first-out way from the streaming threads
GstBus *bus;
guint bus_watch_id;

// we link elements together in a pipeline, and send messages to/from the pipeline.
GstElement *pipeline;

// This call back will run when the AppSrc sends the "need-data" signal (data = image)
void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
	try
	{
		// remember, the "user data" the signal passes to the callback is really the address of the Instant Camera
		CInstantCameraForAppSrc *pCamera = (CInstantCameraForAppSrc*)user_data;
		GstBuffer *buffer;
		GstFlowReturn ret;

		if (pCamera->IsCameraDeviceRemoved() == true)
		{
			cout << "Camera Removed!" << endl;
			g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
		}

		// tell the CInstantCameraForAppSrc to Retrieve an Image. It will pull an image from the pylon driver and place it into it's CInstantCameraForAppSrc::image container.
		if (pCamera->RetrieveImage() == false)
			cout << "Failed to Retrieve new Image. Will push existing image..." << endl;

		// Allocate a new gst buffer that wraps the given memory (CInstantAppSrc::image).
		buffer = gst_buffer_new_wrapped_full(
			(GstMemoryFlags)GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS,
			(gpointer)pCamera->GetImageBuffer(),
			pCamera->GetImageSize(),
			0,
			pCamera->GetImageSize(),
			NULL,
			NULL);

		// Push the image to the source pads of the AppSrc element, where it's picked up by the rest of the pipeline
		g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in cb_need_data(): " << endl << e.GetDescription() << endl;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in cb_need_data(): " << endl << e.what() << endl;
	}

}

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
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in bus_call(): " << endl << e.GetDescription() << endl;
		return FALSE;
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
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in sigint_restore(): " << endl << e.GetDescription() << endl;
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
		cout << "Sending EOS signal to shutdown pipeline cleanly" << endl;
		gst_element_send_event(pipeline, gst_event_new_eos());
		sigint_restore();
		return;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in IntHanlder(): " << endl << e.GetDescription() << endl;
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
			cout << " | CInstantCameraForAppSrc        |                   | AppSrc              |    | convert   |    | autovideosink  |" << endl;
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

		// create the Pylon camera object that we'll link to the AppSrc source element
		CInstantCameraForAppSrc camera(serialNumber, width, height, frameRate, onDemand, useTrigger);

		// initialize GStreamer 
		gst_init(NULL, NULL);

		// create the mainloop
		loop = g_main_loop_new(NULL, FALSE);

		// Initialize the camera (here's where you can change the camera's settings)
		if (camera.InitCamera() == false)
			return -1;

		cout << endl;
		cout << "Using Camera        : " << camera.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Image Dimensions    : " << camera.GetWidth() << "x" << camera.GetHeight() << endl;
		cout << "Resulting FrameRate : " << camera.GetFrameRate() << endl;

		// create a new pipeline to add elements too
		pipeline = gst_pipeline_new("pipeline");

		// prepare a handler (watcher) for messages from the pipeline
		bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// The "source" element in the pipeline will be of type AppSrc
		GstElement *source;
		source = gst_element_factory_make("appsrc", "source");

		// setup the source
		g_object_set(G_OBJECT(source),
			"stream-type", GST_APP_STREAM_TYPE_STREAM,
			"format", GST_FORMAT_TIME,
			"is-live", TRUE,
			"do-timestamp", TRUE, // required for H264 streaming
			"num-buffers", numImagesToRecord, // capture numFramesToRecord, then send End Of Stream (EOS) signal.
			NULL);

		// setup source caps (what kind of video is coming out of the source element?
		string format = "RGB";
		if (camera.IsColor() == false)
			format = "GRAY8";

		g_object_set(G_OBJECT(source), "caps",
			gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, format.c_str(),
			"width", G_TYPE_INT, camera.GetWidth(),
			"height", G_TYPE_INT, camera.GetHeight(),
			"framerate", GST_TYPE_FRACTION, (int)camera.GetFrameRate(), 1, NULL), NULL);

		// ***** THIS IS THE KEY *****
		// When the AppSrc element needs data (image) to push to the rest of the pipeline, it sends the "need-data" signal.
		// Here we link that signal with a callback function (cb_need_data) that will run when the signal is received.
		// In this callback function, we will get the needed data from the pylon camera object.
		g_signal_connect(source, "need-data", G_CALLBACK(cb_need_data), &camera);

		// Build the rest of the pipeline based on the sample chosen.
		bool pipelineBuilt = false;
		
		// now that we have a pipeline and a source, use a PipelineHelper to finish building the pipeline we want
		CPipelineHelper myPipelineHelper(pipeline, source);
		
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

		// run the main loop. When Ctrl+C is pressed, an EOS signal which will shutdown the pipeline in intHandler(), which will also quit the main loop.
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
