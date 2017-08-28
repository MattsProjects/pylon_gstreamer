/*  CInstantCameraForAppSrc.h: header file for CInstantCameraForAppSrc Class.
    This will extend the Basler Pylon::CInstantCamera Class to make it more convinient to use with GstAppSrc.
   
    Copyright (C) 2017 Matthew Breit <matt.breit@gmail.com>

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
	int64_t GetWidth();
	int64_t GetHeight();
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
