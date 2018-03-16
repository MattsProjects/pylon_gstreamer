/*  twocameras_compositor.cpp: Sample application using CInstantCameraAppSrc class.
	This will grab and display images from two cameras.
	
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
#include <thread>

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

		gchar  *debug;
		GError *error;

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
		CInstantCameraAppSrc camera1("21949158");
		CInstantCameraAppSrc camera2("21734321");

		// rescale both cameras' images to 320x240 for demo purposes
		int rescaleWidth = 320;
		int rescaleHeight = 240;

		// Initialize the cameras and driver for use with GStreamer
		// use maximum possible width and height, and maximum possible framerate under current settings.
		cout << "Initializing camera and driver..." << endl;
		camera1.InitCamera(-1, -1, -1, false, false, rescaleWidth, rescaleHeight);
		camera2.InitCamera(-1, -1, -1, false, false, rescaleWidth, rescaleHeight);

		// Apply some additional settings you may like
		cout << "Applying additional user settings..." << endl;
		// Set the same exposure time for both cameras
		GenApi::CEnumerationPtr(camera1.GetNodeMap().GetNode("ExposureAuto"))->FromString("Off");
		GenApi::CEnumerationPtr(camera2.GetNodeMap().GetNode("ExposureAuto"))->FromString("Off");
		GenApi::CFloatPtr(camera1.GetNodeMap().GetNode("ExposureTime"))->SetValue(3000);
		GenApi::CFloatPtr(camera2.GetNodeMap().GetNode("ExposureTime"))->SetValue(3000);

		// Split the bandwidth between both cameras. This is critical for multi-camera operation.
		// example: if we are using two usb cameras
		if (camera1.GetDeviceInfo().GetDeviceClass() == Pylon::BaslerUsbDeviceClass && camera1.GetDeviceInfo().GetDeviceClass() == Pylon::BaslerUsbDeviceClass)
		{
			// Turn on the bandwidth limiter
			GenApi::CEnumerationPtr(camera1.GetNodeMap().GetNode("DeviceLinkThroughputLimitMode"))->FromString("On");
			GenApi::CEnumerationPtr(camera2.GetNodeMap().GetNode("DeviceLinkThroughputLimitMode"))->FromString("On");

			// Set each camera's limit to 150 MB/sec. Typically USB 3 can support ~350-400 MB/sec, but performance varies based on the host system's chipset.
			// So, assuming a maximum of 300 MB/sec per chipset is usually safe.
			GenApi::CIntegerPtr(camera1.GetNodeMap().GetNode("DeviceLinkThroughputLimit"))->SetValue(150000000);
			GenApi::CIntegerPtr(camera2.GetNodeMap().GetNode("DeviceLinkThroughputLimit"))->SetValue(150000000);
		}

		// If we change settings like exposuretime, bandwidth, width, height, etc.
		// Then the camera's framerate possibilities have probably changed...
		// Since we are using multiple cameras, we probably want them to be "in sync", so let's use the maximum common framerate between them
		double frameRateCamera1 = camera1.GetFrameRate(); // get the max framerate based on current settings
		double frameRateCamera2 = camera2.GetFrameRate();
		double maxCommonFrameRate = 0;

		if (frameRateCamera1 <= frameRateCamera2)
			maxCommonFrameRate = frameRateCamera1;
		else
			maxCommonFrameRate = frameRateCamera2;

		camera1.SetFrameRate(maxCommonFrameRate);
		camera2.SetFrameRate(maxCommonFrameRate);
		
		cout << "Maximum common framerate: " << maxCommonFrameRate << endl;		
		
		cout << "Using Camera             : " << camera1.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Camera Area Of Interest  : " << camera1.GetWidth() << "x" << camera1.GetHeight() << endl;
		cout << "Camera Speed             : " << camera1.GetFrameRate() << " fps" << endl;
		cout << endl;
		cout << "Using Camera             : " << camera2.GetDeviceInfo().GetFriendlyName() << endl;
		cout << "Camera Area Of Interest  : " << camera2.GetWidth() << "x" << camera2.GetHeight() << endl;
		cout << "Camera Speed             : " << camera2.GetFrameRate() << " fps" << endl;
		cout << endl;

		cout << "Creating pipeline to display two cameras side-by-side..." << endl;
		cout << endl;

		// create a new pipeline to add elements too
		pipeline = gst_pipeline_new("pipeline");

		// prepare a handler (watcher) for messages from the pipeline
		bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// Create the other needed gstreamer pipeline elements
		GstElement *source1;
		GstElement *source2;
		GstElement *videoconvert1;
		GstElement *videoconvert2;
		GstElement *compositor;
		GstPad *compositor_sink_pad1;
		GstPad *compositor_sink_pad2;
		GstElement *capsfilter;
		GstElement *sink;

		// Use the cameras as the source of the pipeline.
		source1 = camera1.GetSource();
		source2 = camera2.GetSource();

		// some video format converters may be needed
		videoconvert1 = gst_element_factory_make("videoconvert", "videoconvert1");
		videoconvert2 = gst_element_factory_make("videoconvert", "videoconvert2");
			
		// The compositor element will take care for mixing the two camera streams into one window for display
		cout << "Creating compositor..." << endl;
		compositor = gst_element_factory_make("compositor", "compositor");
			
		// The compositor's sink pads will decide where to place the videos (eg: next to each other)
		GstPadTemplate* compositor_sink_pad_t = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(compositor), "sink_%u");
		compositor_sink_pad1 = gst_element_request_pad(compositor, compositor_sink_pad_t, NULL, NULL);
		compositor_sink_pad2 = gst_element_request_pad(compositor, compositor_sink_pad_t, NULL, NULL);
		g_object_set(compositor_sink_pad1, "xpos", 0, "ypos", 0, NULL);
		g_object_set(compositor_sink_pad2, "xpos", rescaleWidth, "ypos", 0, NULL);

		// The capsfilter element will tell the sink what framerate to support (if possible)
		capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
		GstCaps *caps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, (int)maxCommonFrameRate, 1, NULL);
		g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
		gst_caps_unref(caps);

		// The sink element for this sample will be whatever videosink the system prefers for display.
		sink = gst_element_factory_make("autovideosink", "videosink");
		g_object_set(sink, "sync", false, NULL);
		g_object_set(sink, "message-forward", true, NULL);

		// Add all the elements to the pipeline.
		gst_bin_add_many(GST_BIN(pipeline), source1, videoconvert1, compositor, sink, source2, videoconvert2, capsfilter, NULL);
		
		// Link the elements together
		gst_element_link_many(source1, videoconvert1, compositor, capsfilter, sink, NULL);
		gst_element_link_many(source2, videoconvert2, compositor, capsfilter, sink, NULL);
		gst_element_link_many(compositor, sink, NULL);
		
		// Start the camera and grab engine.
		if (camera1.StartCamera() == false || camera2.StartCamera() == false)
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

		camera1.StopCamera();
		camera1.CloseCamera();

		camera2.StopCamera();
		camera2.CloseCamera();
		
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
