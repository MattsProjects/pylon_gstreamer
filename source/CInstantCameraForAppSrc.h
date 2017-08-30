/*  CInstantCameraForAppSrc.h: header file for CInstantCameraForAppSrc Class.
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
