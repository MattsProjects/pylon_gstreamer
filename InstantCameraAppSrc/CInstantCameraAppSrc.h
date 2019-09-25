/*  CInstantCameraAppSrc.h: header file for CInstantCameraAppSrc Class.
    This will extend the Basler Pylon::CInstantCamera Class to make it more convinient to use with GstAppSrc.

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

*/

#include <pylon/PylonIncludes.h>
#include <gst/gst.h>

using namespace Pylon;
using namespace GenApi;
using namespace std;

// ******* CInstantCameraAppSrc *******
// Here we extend the Pylon CInstantCamera class with a few things to make it easier to integrate with Appsrc.
class CInstantCameraAppSrc : public CInstantCamera
{
public:
	CInstantCameraAppSrc(string serialnumber = "");
	~CInstantCameraAppSrc();

	int GetWidth();
	int GetHeight();
	bool InitCamera(
		int width,
		int height,
		int framesPerSecond,
		bool useOnDemand,
		bool useTrigger,
		int scaledWidth = -1,
		int scaledHeight = -1,
		int rotation = -1,
		int numFramesToGrab = -1);
	bool StartCamera();
	bool StopCamera();
	bool OpenCamera();
	bool CloseCamera();
	bool ResetCamera();
	bool SetFrameRate(double framesPerSecond);
	bool AutoAdjustImage();
	bool SaveSettingsToCamera(bool BootWithNewSettings = false);
	double GetFrameRate();
	GstElement* GetSource();	
	
private:
	int m_width;
	int m_height;
	int m_frameRate;
	int m_scaledWidth;
	int m_scaledHeight;
	int m_rotation;
	int m_numFramesToGrab;
	bool m_isInitialized;
	bool m_isColor;
	bool m_isOnDemand;
	bool m_isTriggered;
	bool m_isOpen;
	string m_serialNumber;
	Pylon::CPylonImage m_Image;
	Pylon::CImageFormatConverter m_FormatConverter;
	GstElement* m_appsrc;
	GstElement* m_sourceBin;
	GstBuffer* m_gstBuffer;
	bool retrieve_image();
	static void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data);
};
