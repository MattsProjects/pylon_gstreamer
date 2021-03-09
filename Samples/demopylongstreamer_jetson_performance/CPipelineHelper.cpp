/*  CPipelineHelper.cpp: Definition file for CPipelineHeader Class.
    Given a GStreamer pipeline and source, this will finish building a pipeline.

	Copyright 2017-2019 Matthew Breit <matt.breit@gmail.com>

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
*/

#include "CPipelineHelper.h"

#include <stdio.h>
#include <iostream>

using namespace std;

// ****************************************************************************
// For Debugging, the functions below can print the caps of elements
// These come from GStreamer Documentation.
// They are defined at the end of this file.
// example usage:
// print_pad_capabilities(convert, "src");
// print_pad_capabilities(encoder, "sink");

static gboolean print_field (GQuark field, const GValue * value, gpointer pfx);
static void print_caps (const GstCaps * caps, const gchar * pfx);
static void print_pad_templates_information (GstElementFactory * factory);
static void print_pad_capabilities (GstElement *element, gchar *pad_name);
// ****************************************************************************

CPipelineHelper::CPipelineHelper(GstElement *pipeline, GstElement *source)
{
	m_pipelineBuilt = false;
	m_pipeline = pipeline;
	m_source = source;
}

CPipelineHelper::~CPipelineHelper()
{
}

// example of how to create a pipeline for display in a window
bool CPipelineHelper::build_pipeline_display()
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -display. Another pipeline has already been built." << endl;
			return false;
		}
		

		GstElement *convert;
		GstElement *sink;

		cout << "Creating Pipeline for displaying images in local window..." << endl;
		// Create gstreamer elements
		convert = gst_element_factory_make("videoconvert", "converter");
		sink = gst_element_factory_make("autovideosink", "videosink"); // depending on your platform, you may have to use some alternative here, like ("autovideosink", "sink")

		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }
		
		// if you are using nvidia tx1/tx2, the built-in video sink that is found by autovideosink does not advertise it needs conversion (it does not support RGB).
		// so we must use a filter such that the converter knows to convert the image format.
		// if you are using a video sink that supports RGB, then you do not need to convert to i420 and you can remove this filter and save some cpu load.
		GstElement *filter;
		GstCaps *filter_caps;
		filter = gst_element_factory_make("capsfilter", "filter");
		filter_caps = gst_caps_new_simple("video/x-raw",
		  "format", G_TYPE_STRING, "I420",
		  NULL);
		
		g_object_set(G_OBJECT(filter), "caps", filter_caps, NULL);
		gst_caps_unref(filter_caps);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, convert, filter, sink, NULL);
		gst_element_link_many(m_source, convert, filter, sink, NULL);
		
		
		cout << "Pipeline Made." << endl;
		
		m_pipelineBuilt = true;

		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_display(): " << endl << e.what() << endl;
		return false;
	}
}

// example of how to create a pipeline for piping images to a linux framebuffer
bool CPipelineHelper::build_pipeline_framebuffer(string fbDevice)
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -framebuffer. Another pipeline has already been built." << endl;
			return false;
		}

		GstElement *convert;
		GstElement *sink;

		cout << "Creating Pipeline for sending images to framebuffer " << fbDevice << "..." << endl;

		/* Create gstreamer elements */
		convert = gst_element_factory_make("videoconvert", "converter");
		sink = gst_element_factory_make("fbdevsink", "fbsink");

		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }

		g_object_set(G_OBJECT(sink), "device", fbDevice.c_str(), NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, convert, sink, NULL);
		gst_element_link_many(m_source, convert, sink, NULL);
		
		cout << "Pipeline Made." << endl;

		m_pipelineBuilt = true;
		
		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_framebuffer(): " << endl << e.what() << endl;
		return false;
	}
}

// example of how to create a pipeline for encoding images in h264 format and streaming across a network
bool CPipelineHelper::build_pipeline_h264stream(string ipAddress)
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -h264stream. Another pipeline has already been built." << endl;
			return false;
		}

		GstElement *convert;
		GstElement *encoder;
		GstElement *filter2;
		GstElement *rtp264;
		GstElement *sink;
		GstCaps *filter2_caps;
		int port = 554;

		cout << "Creating Pipeline for streaming images as h264 video across network to: " << ipAddress << ":" << port << "..." << endl;
		cout << "Start the receiver PC first with this command: " << endl;
		cout << "gst-launch-1.0 udpsrc port=" << port << " ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! autovideosink sync=false async=false -e" << endl;
		cout << "Then press enter to continue..." << endl;
		cin.get();

		// Create gstreamer elements
		convert = gst_element_factory_make("videoconvert", "converter");

		// depending on your platform, you may have to use some alternative encoder here.
		cout << "Trying omxh264enc encoder..." << endl;
		encoder = gst_element_factory_make("omxh264enc", "omxh264enc");  // omxh264enc works good on Raspberry Pi and Jetson Nano
		if (!encoder)
		{
			cout << "Could not make omxh264enc encoder. Trying imxvpuenc_h264..." << endl;
			encoder = gst_element_factory_make("imxvpuenc_h264", "imxvpuenc_h264"); // for i.MX devices.
			if (!encoder)
			{
				cout << "Could not make imxvpuenc_h264 encoder. Trying v4l2h264enc..." << endl;
				encoder = gst_element_factory_make("v4l2h264enc", "v4l2h264enc");  // for Snapdragon 820 devices
				if (!encoder)
				{
					cout << "Could not make v4l2h264enc encoder. Trying x264enc..." << endl;
					encoder = gst_element_factory_make("x264enc", "x264enc"); // for other devices
					if (!encoder)
					{
						cout << "Could not make x264enc encoder. Giving up..." << endl;
						return false; // give up
					}
				}
			}
		}
		filter2 = gst_element_factory_make("capsfilter", "filter2");
		rtp264 = gst_element_factory_make("rtph264pay", "rtp264");
		sink = gst_element_factory_make("udpsink", "udpsink");

		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!filter2){ cout << "Could not make filter2" << endl; return false; }
		if (!rtp264){ cout << "Could not make rtp264" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }

		// specify some settings on the elements
		// Different encoders have different features you can set.
		if (encoder->object.name == "x264enc")
		{
			// for compatibility on resource-limited systems, set the encoding preset "ultrafast". Lowest quality video, but lowest lag.
			g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);
		}

		if (encoder->object.name == "omxh264enc")
		{
			// 1 = baseline, 2 = main, 3 = high
			g_object_set(G_OBJECT(encoder), "profile", 8, NULL);

		}		

		// filter2 capabilities
		filter2_caps = gst_caps_new_simple("video/x-h264",
			"stream-format", G_TYPE_STRING, "byte-stream",
			NULL);

		g_object_set(G_OBJECT(filter2), "caps", filter2_caps, NULL);
		gst_caps_unref(filter2_caps);

		// sink
		g_object_set(G_OBJECT(sink), "host", ipAddress.c_str(), "port", port, "sync", FALSE, "async", FALSE, NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, convert, encoder, filter2, rtp264, sink, NULL);
		gst_element_link_many(m_source, convert, encoder, filter2, rtp264, sink, NULL);

		cout << "Pipeline Made." << endl;

		m_pipelineBuilt = true;
		
		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_h264stream(): " << endl << e.what() << endl;
		return false;
	}

}

// example of how to create a pipeline for encoding images in h264 format and streaming across a network
bool CPipelineHelper::build_pipeline_h264multicast(string ipAddress)
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -h264stream. Another pipeline has already been built." << endl;
			return false;
		}

		GstElement *convert;
		GstElement *encoder;
		GstElement *filter2;
		GstElement *rtp264;
		GstElement *sink;
		GstCaps *filter2_caps;
		int port = 3500;

		cout << "Creating Pipeline for multicast streaming images as h264 video across network to group: " << ipAddress << ":" << port << "..." << endl;
		cout << "Start the receiver PC first with this command: " << endl;
		cout << "gst-launch-1.0 udpsrc multicast-group=" << ipAddress << " auto-multicast=true port=" << port << " ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! autovideosink sync=false async=false -e" << endl;
		cout << "Then press enter to continue..." << endl;
		cin.get();

		// Create gstreamer elements
		convert = gst_element_factory_make("videoconvert", "converter");

		// depending on your platform, you may have to use some alternative encoder here.
		cout << "Trying omxh264enc encoder..." << endl;
		encoder = gst_element_factory_make("omxh264enc", "omxh264enc");  // omxh264enc works good on Raspberry Pi and Jetson Nano
		if (!encoder)
		{
			cout << "Could not make omxh264enc encoder. Trying imxvpuenc_h264..." << endl;
			encoder = gst_element_factory_make("imxvpuenc_h264", "imxvpuenc_h264"); // for i.MX devices.
			if (!encoder)
			{
				cout << "Could not make imxvpuenc_h264 encoder. Trying v4l2h264enc..." << endl;
				encoder = gst_element_factory_make("v4l2h264enc", "v4l2h264enc");  // for Snapdragon 820 devices
				if (!encoder)
				{
					cout << "Could not make v4l2h264enc encoder. Trying x264enc..." << endl;
					encoder = gst_element_factory_make("x264enc", "x264enc"); // for other devices
					if (!encoder)
					{
						cout << "Could not make x264enc encoder. Giving up..." << endl;
						return false; // give up
					}
				}
			}
		}
		filter2 = gst_element_factory_make("capsfilter", "filter2");
		rtp264 = gst_element_factory_make("rtph264pay", "rtp264");
		sink = gst_element_factory_make("udpsink", "udpsink");

		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!filter2){ cout << "Could not make filter2" << endl; return false; }
		if (!rtp264){ cout << "Could not make rtp264" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }

		// specify some settings on the elements
		// Different encoders have different features you can set.
		if (encoder->object.name == "x264enc")
		{
			// for compatibility on resource-limited systems, set the encoding preset "ultrafast". Lowest quality video, but lowest lag.
			g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);
		}

		if (encoder->object.name == "omxh264enc")
		{
			// 1 = baseline, 2 = main, 3 = high
			g_object_set(G_OBJECT(encoder), "profile", 8, NULL);

		}

		// filter2 capabilities
		filter2_caps = gst_caps_new_simple("video/x-h264",
			"stream-format", G_TYPE_STRING, "byte-stream",
			NULL);

		g_object_set(G_OBJECT(filter2), "caps", filter2_caps, NULL);
		gst_caps_unref(filter2_caps);

		// sink
		g_object_set(G_OBJECT(sink), "host", ipAddress.c_str(), "port", port, "sync", FALSE, "async", FALSE, "auto-multicast", TRUE, NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, convert, encoder, filter2, rtp264, sink, NULL);
		gst_element_link_many(m_source, convert, encoder, filter2, rtp264, sink, NULL);

		cout << "Pipeline Made." << endl;

		m_pipelineBuilt = true;

		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_h264stream(): " << endl << e.what() << endl;
		return false;
	}

}

// example of how to create a pipeline for encoding images in h264 format and streaming to local video file
bool CPipelineHelper::build_pipeline_h264file(string fileName)
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -h264file. Another pipeline has already been built." << endl;
			return false;
		}
	
		GstElement *convert;
		GstElement *encoder;
		GstElement *sink;

		cout << "Creating Pipeline for saving images as h264 video on local host: " << fileName << "..." << endl;

		// Create gstreamer elements
		convert = gst_element_factory_make("videoconvert", "converter");

		// depending on your platform, you may have to use some alternative encoder here.
		encoder = gst_element_factory_make("omxh264enc", "omxh264enc"); // omxh264enc works good on Raspberry Pi
		if (!encoder)
		{
			cout << "Could not make omxh264enc encoder. Trying imxvpuenc_h264..." << endl;
			encoder = gst_element_factory_make("imxvpuenc_h264", "imxvpuenc_h264"); // for i.MX devices.
			if (!encoder)
			{
				cout << "Could not make imxvpuenc_h264 encoder. Trying x264enc..." << endl;
				encoder = gst_element_factory_make("x264enc", "x264enc"); // for other devices
				if (!encoder)
				{
					cout << "Could not make x264enc encoder." << endl;
					return false;
				}
			}
		}
		sink = gst_element_factory_make("filesink", "filesink");


		if (!convert){ cout << "Could not make convert" << endl; return false; }
		if (!encoder){ cout << "Could not make encoder" << endl; return false; }
		if (!sink){ cout << "Could not make sink" << endl; return false; }

		// Set up elements

		// Different encoders have different features you can set.
		if (encoder->object.name == "x264enc")
		{
			// for compatibility on resource-limited systems, set the encoding preset "ultrafast". Lowest quality video, but lowest lag.
			g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);
		}

		g_object_set(G_OBJECT(sink), "location", fileName.c_str(), NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, convert, encoder, sink, NULL);
		gst_element_link_many(m_source, convert, encoder, sink, NULL);
		
		cout << "Pipeline Made." << endl;
		
		m_pipelineBuilt = true;

		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_h264file(): " << endl << e.what() << endl;
		return false;
	}
}

// example of how to create a pipeline from a string that you would use with gst-launch-1.0
bool CPipelineHelper::build_pipeline_parsestring(string pipelineString)
{
	try
	{
		if (m_pipelineBuilt == true)
		{
			cout << "Cancelling -parsestring. Another pipeline has already been built." << endl;
			return false;
		}

		cout << "Applying this Pipeline to the CInstantCameraAppsc: " << pipelineString << "..." << endl;
		
		string strPipeline = "";
		if (pipelineString.find("gst-launch") != std::string::npos)
		{
			std::size_t start = pipelineString.find("!");
			strPipeline = pipelineString.substr(start);
		}
		else
			strPipeline = pipelineString;

		GstElement *userPipeline;
		userPipeline = gst_parse_bin_from_description(strPipeline.c_str(), true, NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, userPipeline, NULL);
		gst_element_link_many(m_source, userPipeline, NULL);

		cout << "Pipeline Made." << endl;

		m_pipelineBuilt = true;

		return true;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_pipeline_parsestring(): " << endl << e.what() << endl;
		return false;
	}
}


// ****************************************************************************
// debugging functions

static gboolean print_field (GQuark field, const GValue * value, gpointer pfx) {
  gchar *str = gst_value_serialize (value);

  g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}

static void print_caps (const GstCaps * caps, const gchar * pfx) {
  guint i;

  g_return_if_fail (caps != NULL);

  if (gst_caps_is_any (caps)) {
    g_print ("%sANY\n", pfx);
    return;
  }
  if (gst_caps_is_empty (caps)) {
    g_print ("%sEMPTY\n", pfx);
    return;
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);

    g_print ("%s%s\n", pfx, gst_structure_get_name (structure));
    gst_structure_foreach (structure, print_field, (gpointer) pfx);
  }
}

/* Prints information about a Pad Template, including its Capabilities */
static void print_pad_templates_information (GstElementFactory * factory) {
  const GList *pads;
  GstStaticPadTemplate *padtemplate;

  g_print ("Pad Templates for %s:\n", gst_element_factory_get_longname (factory));
  if (!gst_element_factory_get_num_pad_templates (factory)) {
    g_print ("  none\n");
    return;
  }

  pads = gst_element_factory_get_static_pad_templates (factory);
  while (pads) {
    padtemplate = (GstStaticPadTemplate*)pads->data;
    pads = g_list_next (pads);

    if (padtemplate->direction == GST_PAD_SRC)
      g_print ("  SRC template: '%s'\n", padtemplate->name_template);
    else if (padtemplate->direction == GST_PAD_SINK)
      g_print ("  SINK template: '%s'\n", padtemplate->name_template);
    else
      g_print ("  UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

    if (padtemplate->presence == GST_PAD_ALWAYS)
      g_print ("    Availability: Always\n");
    else if (padtemplate->presence == GST_PAD_SOMETIMES)
      g_print ("    Availability: Sometimes\n");
    else if (padtemplate->presence == GST_PAD_REQUEST)
      g_print ("    Availability: On request\n");
    else
      g_print ("    Availability: UNKNOWN!!!\n");

    if (padtemplate->static_caps.string) {
      GstCaps *caps;
      g_print ("    Capabilities:\n");
      caps = gst_static_caps_get (&padtemplate->static_caps);
      print_caps (caps, "      ");
      gst_caps_unref (caps);

    }

    g_print ("\n");
  }
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
static void print_pad_capabilities (GstElement *element, gchar *pad_name) {
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  /* Retrieve pad */
  pad = gst_element_get_static_pad (element, pad_name);
  if (!pad) {
    g_printerr ("Could not retrieve pad '%s'\n", pad_name);
    return;
  }

  /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
  caps = gst_pad_get_current_caps (pad);
  if (!caps)
    caps = gst_pad_query_caps (pad, NULL);

  /* Print and free */
  g_print ("Caps for the %s pad:\n", pad_name);
  print_caps (caps, "      ");
  gst_caps_unref (caps);
  gst_object_unref (pad);
}
// ****************************************************************************