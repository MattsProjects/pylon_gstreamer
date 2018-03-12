/*  simplegrab_tx2.cpp: Sample application using CInstantCameraAppSrc class.
    This will grab and display images.
	
	Copyright 2018 Matthew Breit <matt.breit@gmail.com>

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
	

	Concept Overview:
	<--------------- InstantCameraAppSrc -------------->    <------------ GStreamer Pipeline ------->
	+--------------------------------------------------+    +---------+    +---------+    +---------+
	| source                                           |    | element |    | element |    | sink    |
	| (camera + driver + GstAppSrc + rescale + rotate) |    |         |    |         |    |         |
	|                                                 src--sink      src--sink      src--sink       |
	+--------------------------------------------------+    +---------+    +---------+    +---------+

	Note:
	Some GStreamer elements (plugins) used in the pipeline examples may not be available on all systems. Consult GStreamer for more information:
	https://gstreamer.freedesktop.org/
*/


#include "../../InstantCameraAppSrc/CInstantCameraAppSrc.h"
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

gint main(gint argc, gchar *argv[])
{
	try
	{
		// signal handler for ctrl+C
		signal(SIGINT, IntHandler);

		cout << "Press CTRL+C at any time to quit." << endl;

		// initialize GStreamer 
		gst_init(NULL, NULL);

		// create the mainloop
		loop = g_main_loop_new(NULL, FALSE);

		// The InstantCameraForAppSrc will manage the physical camera and pylon driver
		// and provide a source element to the GStreamer pipeline.
		CInstantCameraAppSrc camera;
		
		// Initialize the camera and driver
		cout << "Initializing camera and driver..." << endl;
		
		// use the GenICam API to access features because it supports any interface (USB, GigE, etc.)
		// reset the camera to defaults if you like
		GenApi::CEnumerationPtr(camera.GetNodeMap().GetNode("UserSetSelector"))->FromString("Default");
		GenApi::CCommandPtr(camera.GetNodeMap().GetNode("UserSetLoad"))->Execute();
		// use maximum width and height
		int width = GenApi::CIntegerPtr(camera.GetNodeMap().GetNode("Width"))->GetMax();
		int height = GenApi::CIntegerPtr(camera.GetNodeMap().GetNode("Height"))->GetMax();
		int frameRate = 30; // we will try for this framerate, but the actual camera's capabilities will depend on other settings...
		
		camera.InitCamera(width, height, frameRate, false, false, 320, 240);		

		cout << "Using Camera             : " << camera.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Camera Area Of Interest  : " << camera.GetWidth() << "x" << camera.GetHeight() << endl;
		cout << "Camera Speed             : " << camera.GetFrameRate() << " fps" << endl;

		// create a new pipeline to add elements too
		pipeline = gst_pipeline_new("pipeline");

		// prepare a handler (watcher) for messages from the pipeline
		bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// A pipeline needs a source element. The InstantCameraForAppSrc will create, configure, and provide an AppSrc which fits the camera.
		GstElement *source = camera.GetSource();
		
		// Create the other needed gstreamer pipeline elements
		GstElement *convert;
		GstElement *sink;
		convert = gst_element_factory_make("videoconvert", "converter");
		sink = gst_element_factory_make("autovideosink", "videosink"); // depending on your platform, you may have to use some alternative here, like ("autovideosink", "sink")

		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }
		
		// Nvidia TX1/TX2 Note: The built-in video sink that is found by autovideosink does not advertise that it needs conversion (it does not support RGB).
		// so we must use a filter such that the converter knows to convert the image format.
		// if you are using a video sink that supports RGB, then you do not need to convert to i420 and you can remove this filter and save some cpu load.
		GstElement *filter;
		GstCaps *filter_caps;
		filter = gst_element_factory_make("capsfilter", "filter");
		filter_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", NULL);
		g_object_set(G_OBJECT(filter), "caps", filter_caps, NULL);
		gst_caps_unref(filter_caps);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(pipeline), source, convert, filter, sink, NULL);
		gst_element_link_many(source, convert, filter, sink, NULL);	
		
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
