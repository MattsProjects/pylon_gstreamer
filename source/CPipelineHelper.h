/*  CPipelineHelper.h: header file for CPipelineHeader Class.
    Given a GStreamer pipeline and source, this will finish building a pipeline.
   
    Copyright (C) 2017 Matthew Breit <matt.breit@gmail.com>
    (Special thanks to Florian Echtler <floe@butterbrot.org>. From his work I learned GStreamer :))

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
   
    Dependent libraries/API's may be licensed under different terms.
    Please consult the respective authors/manufacturer for more information.
*/

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <string>

using namespace std;

// Given a pipeline and source, this class will finish building pipelines of various elements for various purposes.

class CPipelineHelper
{
public:
	CPipelineHelper(GstElement *pipeline, GstElement *source);
	~CPipelineHelper();
	
	// example of how to create a pipeline for display in a window
	bool build_pipeline_display();

	// example of how to create a pipeline for piping images to a linux framebuffer
	bool build_pipeline_framebuffer(string fbDevice);

	// example of how to create a pipeline for encoding images in h264 format and streaming across a network
	bool build_pipeline_h264stream(string ipAddress);

	// example of how to create a pipeline for encoding images in h264 format and streaming to local video file
	bool build_pipeline_h264file(string fileName, int numFramesToRecord);

private:
	bool m_pipelineBuilt;
	GstElement *m_pipeline;
	GstElement *m_source;
};
