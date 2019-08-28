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

CPipelineHelper::CPipelineHelper(GstElement *pipeline, GstElement *source, int scaledWidth, int scaledHeight)
{
	m_pipelineBuilt = false;
	m_scaleVideo = false;
	m_pipeline = pipeline;
	m_source = source;
	m_scaledWidth = scaledWidth;
	m_scaledHeight = scaledHeight;
}

CPipelineHelper::~CPipelineHelper()
{
}

bool CPipelineHelper::build_videoscaler()
{
	try
	{
		// we will always add the scaler element to the pipeline.
		// If there is no caps filter after the element dictating the new size, it simply won't do anything.
		m_videoScaler = gst_element_factory_make("videoscale", "videoscale");
		m_videoScalerCaps = gst_element_factory_make("capsfilter", "videocaps");

		if (m_scaledWidth == -1 || m_scaledHeight == -1)
		{
			// will not apply caps after the videoscaler element, so it will not do any scaling.
			return true;
		}
		else if (m_scaledWidth < 2 || m_scaledHeight < 2)
		{
			// rescaling to widths less that 2 causes 'failed to activate buffer pool' in converter element
			cerr << "Scaling width and height must be greater than 2x2!" << endl;
			return false;
		}
		else
		{
			// configure the capsfilter after the videoscaler element, so it will apply scaling.
			GstCaps *caps = gst_caps_new_simple("video/x-raw",
				"width", G_TYPE_INT, m_scaledWidth,
				"height", G_TYPE_INT, m_scaledHeight,
				NULL);
				
			g_object_set(G_OBJECT(m_videoScalerCaps), "caps", caps, NULL);

			gst_caps_unref(caps);
		
			return true;
		}
		
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in build_videoscaler(): " << endl << e.what() << endl;
		return false;
	}
	
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
		
		// build the videoscaler
		if (build_videoscaler() == false)
			return false;

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
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoScaler, m_videoScalerCaps, convert, filter, sink, NULL);
		gst_element_link_many(m_source, m_videoScaler, m_videoScalerCaps, convert, filter, sink, NULL);
		
		
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
		
		if (build_videoscaler() == false)
			return false;

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
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoScaler, m_videoScalerCaps, convert, sink, NULL);
		gst_element_link_many(m_source, m_videoScaler, m_videoScalerCaps, convert, sink, NULL);
		
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

		if (build_videoscaler() == false)
			return false;
		
		GstElement *convert;
		GstElement *encoder;
		GstElement *filter2;
		GstElement *rtp264;
		GstElement *sink;
		GstCaps *filter2_caps;
		int port = 5000;

		cout << "Creating Pipeline for streaming images as h264 video across network to: " << ipAddress << ":" << port << "..." << endl;
		cout << "Start the receiver PC first with this command: " << endl;
		cout << "gst-launch-1.0 udpsrc port=" << port << " ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! autovideosink sync=false async=false -e" << endl;
		cout << "Then press enter to continue..." << endl;
		cin.get();

		// Create gstreamer elements
		convert = gst_element_factory_make("videoconvert", "converter");

		// depending on your platform, you may have to use some alternative encoder here.
		encoder = gst_element_factory_make("v4l2h264enc", "v4l2h264enc");  // omxh264enc works good on Raspberry Pi
		if (!encoder)
		{
			cout << "Could not make v4l2h264enc encoder. Trying omxh264enc..." << endl;
			encoder = gst_element_factory_make("omxh264enc", "omxh264enc");  // omxh264enc works good on Raspberry Pi
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

		// filter2 capabilities
		filter2_caps = gst_caps_new_simple("video/x-h264",
			"stream-format", G_TYPE_STRING, "byte-stream",
			"profile", G_TYPE_STRING, "high", // when streaming to windows and using x264enc, profile default "high-4:4:4" doesn't work. use "high" instead.
			NULL);

		g_object_set(G_OBJECT(filter2), "caps", filter2_caps, NULL);
		gst_caps_unref(filter2_caps);

		// sink
		g_object_set(G_OBJECT(sink), "host", ipAddress.c_str(), "port", port, "sync", FALSE, "async", FALSE, NULL);

		// add and link the pipeline elements
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoScaler, m_videoScalerCaps, convert, encoder, filter2, rtp264, sink, NULL);
		gst_element_link_many(m_source, m_videoScaler, m_videoScalerCaps, convert, encoder, filter2, rtp264, sink, NULL);
		
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

		if (build_videoscaler() == false)
			return false;
		
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
		gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoScaler, m_videoScalerCaps, convert, encoder, sink, NULL);
		gst_element_link_many(m_source, m_videoScaler, m_videoScalerCaps, convert, encoder, sink, NULL);
		
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

		if (build_videoscaler() == false)
			return false;

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
