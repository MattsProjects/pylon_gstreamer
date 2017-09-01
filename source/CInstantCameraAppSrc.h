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
	CInstantCameraAppSrc();
	~CInstantCameraAppSrc();

	int GetWidth();
	int GetHeight();
	bool InitCamera(string serialnumber, int width, int height, int framesPerSecond, bool useOnDemand, bool useTrigger);
	bool StartCamera();
	bool StopCamera();
	bool CloseCamera();
	double GetFrameRate();
	GstElement* GetAppSrc();	
	
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
	GstElement* m_source;
	GstBuffer* m_gstBuffer;
	bool retrieve_image();
	static void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data);
};
