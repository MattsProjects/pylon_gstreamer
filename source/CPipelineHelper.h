/*  CPipelineHelper.h: header file for CPipelineHeader Class.
    Given a GStreamer pipeline and source, this will finish building a pipeline.

	Copyright (c) 2017, Matthew Breit <matt.breit@gmail.com>
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.
	* Neither the name of the copyright holder nor the
	names of its contributors may be used to endorse or promote products
	derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	THIS SOFTWARE REQUIRES ADDITIONAL SOFTWARE (IE: LIBRARIES) IN ORDER TO COMPILE
	INTO BINARY FORM AND TO FUNCTION IN BINARY FORM. ANY SUCH ADDITIONAL SOFTWARE
	IS OUTSIDE THE SCOPE OF THIS LICENSE.
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
