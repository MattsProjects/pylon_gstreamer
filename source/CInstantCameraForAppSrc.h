/*  CInstantCameraForAppSrc.h: header file for CInstantCameraForAppSrc Class.
    This will extend the Basler Pylon::CInstantCamera Class to make it more convinient to use with GstAppSrc.
	
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

#include <pylon/PylonIncludes.h>

using namespace Pylon;
using namespace GenApi;
using namespace std;

// ******* CInstantCameraForAppSrc *******
// Here we extend the Pylon CInstantCamera class with a few things to make it easier to integrate with Appsrc.
class CInstantCameraForAppSrc : public CInstantCamera
{
public:
	CInstantCameraForAppSrc(string serialnumber, int width, int height, int framesPerSecond, bool useOnDemand, bool useTrigger);
	~CInstantCameraForAppSrc();

	bool InitCamera();
	bool StartCamera();
	bool StopCamera();
	bool CloseCamera();
	bool RetrieveImage();
	bool IsColor();
	bool IsOnDemand();
	bool IsTriggered();
	void *GetImageBuffer();
	size_t GetImageSize();
	int GetWidth();
	int GetHeight();
	double GetFrameRate();

private:
	int m_width;
	int m_height;
	int m_frameRate;
	bool m_isInitialized;
	bool m_isColor;
	bool m_isOnDemand;
	bool m_isTriggered;
	string m_serialNumber;
	Pylon::CPylonImage m_Image;
	Pylon::CImageFormatConverter m_FormatConverter;
};
