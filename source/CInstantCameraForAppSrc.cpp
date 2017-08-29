/*  CInstantCameraForAppSrc.cpp: Definition file for CInstantCameraForAppSrc Class.
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

#include "CInstantCameraForAppSrc.h"

using namespace Pylon;
using namespace GenApi;
using namespace std;

// Here we extend the Pylon CInstantCamera class with a few things to make it easier to integrate with Appsrc.
CInstantCameraForAppSrc::CInstantCameraForAppSrc(string serialnumber, int width, int height, int framesPerSecond, bool useOnDemand, bool useTrigger)
{
	// initialize Pylon runtime
	Pylon::PylonInitialize();

	m_serialNumber = serialnumber;
	m_width = width;
	m_height = height;
	m_frameRate = framesPerSecond;
	m_isOnDemand = useOnDemand;
	m_isTriggered = useTrigger;

	m_isInitialized = false;

	// We're going to use GStreamer's RGB format in pipelines, so we may need to use Pylon to convert the camera's image to RGB (depending on the camera used)
	EPixelType pixelType = Pylon::EPixelType::PixelType_RGB8packed;
	m_FormatConverter.OutputPixelFormat.SetValue(pixelType);

	// on the off chance that the very first m_Image can't be supplied (ie: missing trigger signal), Reset() lets us initialize it to a blank image so we can push at least that.
	m_Image.Reset(pixelType, m_width, m_height);

	// since Image On Demand uses software trigger, it cannot be used with isTriggered
	if (m_isOnDemand == true && m_isTriggered == true)
	{
		cout << "Cannot use both Image-on-Demand and Triggered mode. Using only Triggered Mode." << endl;
		m_isOnDemand = false;
	}
}
CInstantCameraForAppSrc::~CInstantCameraForAppSrc()
{
	CloseCamera();
	// free resources allocated by pylon runtime.
	Pylon::PylonTerminate();
}
bool CInstantCameraForAppSrc::IsColor()
{
	return m_isColor;
}
bool CInstantCameraForAppSrc::IsOnDemand()
{
	return m_isOnDemand;
}
bool CInstantCameraForAppSrc::IsTriggered()
{
	return m_isTriggered;
}
void* CInstantCameraForAppSrc::GetImageBuffer()
{
	return m_Image.GetBuffer();
}
size_t CInstantCameraForAppSrc::GetImageSize()
{
	return m_Image.GetImageSize();
}
int64_t CInstantCameraForAppSrc::GetWidth()
{
	return CIntegerPtr(GetNodeMap().GetNode("Width"))->GetValue();
}
int64_t CInstantCameraForAppSrc::GetHeight()
{
	return CIntegerPtr(GetNodeMap().GetNode("Height"))->GetValue();
}
double CInstantCameraForAppSrc::GetFrameRate()
{
	if (GenApi::IsAvailable(GetNodeMap().GetNode("ResultingFrameRateAbs")))
	  return CFloatPtr(GetNodeMap().GetNode("ResultingFrameRateAbs"))->GetValue();
	else
	  return CFloatPtr(GetNodeMap().GetNode("ResultingFrameRate"))->GetValue(); // BCON and USB use SFNC3 names	
}

// Open the camera and adjust some settings
bool CInstantCameraForAppSrc::InitCamera()
{
	try
	{
		// use the first camera device found. You can also populate a CDeviceInfo object with information like serial number, etc. to choose a specific camera
		if (m_serialNumber == "")
			Attach(CTlFactory::GetInstance().CreateFirstDevice());

		else
		{
			CDeviceInfo info;
			info.SetSerialNumber(m_serialNumber.c_str());
			Attach(CTlFactory::GetInstance().CreateFirstDevice(info));
		}

		// open the camera to access settings
		Open();

		// setup the camera. Here we use the GenICam GenAPI method so we can support multiple interfaces like usb and gige
		// Note: Get the "Node names" from pylon viewer

		// some features are unique to only Usb or only GigE cameras (like ip address).
		// Also, some features may have different names (AcquisitionFrameRate vs. AcquisitionFrameRateAbs) due to different versions of the GeniCam SFNC standard.
		// Here we manage this.

		// First, enable migration mode in usb cameras so that we can solve the 'same feature has different name' topic.
		// Migration mode lets us access features by their old names too.
		// We skip this here because the BCON interface also uses SFNC3 names, but does not support migration mode.
		// Since we want to support BCON also in this program, we will manage each feature with a different name manually.
		//if (GetDeviceInfo().GetDeviceClass() == "BaslerUsb")
		//	GenApi::CBooleanPtr(GetTLNodeMap().GetNode("MigrationModeEnable"))->SetValue(true);

		// setup some settings common to most cameras (it's always best to check if a feature is available before setting it)
		if (m_isTriggered == false)
		{
			if (IsAvailable(GetNodeMap().GetNode("AcquisitionFrameRateEnable")))
				GenApi::CBooleanPtr(GetNodeMap().GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
			if (IsAvailable(GetNodeMap().GetNode("AcquisitionFrameRateAbs")))
				GenApi::CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRateAbs"))->SetValue(m_frameRate); // this is called "AcquisitionFrameRate" (not abs) in usb cameras. Migration mode lets us use the old name though.
			if (IsAvailable(GetNodeMap().GetNode("AcquisitionFrameRate")))
				GenApi::CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRate"))->SetValue(m_frameRate); // BCON and USB use SFNC3 names.
		  
		}
		if (IsAvailable(GetNodeMap().GetNode("Width")))
			GenApi::CIntegerPtr(GetNodeMap().GetNode("Width"))->SetValue(m_width);
		if (IsAvailable(GetNodeMap().GetNode("Height")))
			GenApi::CIntegerPtr(GetNodeMap().GetNode("Height"))->SetValue(m_height);
		if (IsAvailable(GetNodeMap().GetNode("CenterX")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("CenterX"))->SetValue(true);
		if (IsAvailable(GetNodeMap().GetNode("CenterY")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("CenterY"))->SetValue(true);

		if (m_isOnDemand == true || m_isTriggered == true)
		{
			if (IsAvailable(GetNodeMap().GetNode("TriggerSelector")))
			{
				GenApi::CEnumerationPtr ptrTriggerSelector = GetNodeMap().GetNode("TriggerSelector");
				if (IsAvailable(ptrTriggerSelector->GetEntryByName("AcquisitionStart")))
				{
					ptrTriggerSelector->FromString("AcquisitionStart");
					GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerMode"))->FromString("Off");
				}
				if (IsAvailable(ptrTriggerSelector->GetEntryByName("FrameBurstStart"))) // BCON and USB use SFNC3 names
				{
					ptrTriggerSelector->FromString("FrameBurstStart");
					GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerMode"))->FromString("Off");
				}
				if (IsAvailable(ptrTriggerSelector->GetEntryByName("FrameStart")))
				{
					ptrTriggerSelector->FromString("FrameStart");
					GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerMode"))->FromString("On");
					if (m_isOnDemand == true)
						GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerSource"))->FromString("Software");
					if (m_isTriggered == true)
						GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerSource"))->FromString("Line1");
				}
				else
				{
					cout << "FrameStart triggering not available. Continuing in free run mode." << endl;
					m_isOnDemand = false;
					m_isTriggered = false;
				}
			}
			else
			{
				cout << "Triggering not available. Continuing in free run mode." << endl;
				m_isOnDemand = false;
				m_isTriggered = false;
			}
		}


		// now setup some unique-to-usb and unique-to-gige features
		if (GetDeviceInfo().GetDeviceClass() == "BaslerUsb")
		{
			// some usb-specific settings for performance
			GenApi::CIntegerPtr(GetStreamGrabberNodeMap().GetNode("NumMaxQueuedUrbs"))->SetValue(100);

			// if only connected as usb 2, reduce bandwidth to something stable, like 24MB/sec.
			if (GenApi::CEnumerationPtr(GetNodeMap().GetNode("BslUSBSpeedMode"))->ToString() == "HighSpeed")
			{
				GenApi::CEnumerationPtr(GetNodeMap().GetNode("DeviceLinkThroughputLimitMode"))->FromString("On");
				GenApi::CIntegerPtr(GetNodeMap().GetNode("DeviceLinkThroughputLimit"))->SetValue(24000000);
			}
		}
		else if (GetDeviceInfo().GetDeviceClass() == "BaslerGigE")
		{
			// some gige-specific settings for performance
			GenApi::CIntegerPtr(GetNodeMap().GetNode("GevSCPSPacketSize"))->SetValue(1500); // set a usually-known-good gige packet size, like 1500.
		}


		// Check the current pixelFormat of the camera to see if the camera should be treated as color or mono 
		GenApi::CEnumerationPtr PixelFormat = GetNodeMap().GetNode("PixelFormat");
		if (Pylon::IsMonoImage(Pylon::CPixelTypeMapper::GetPylonPixelTypeByName(PixelFormat->ToString())) == true)
			m_isColor = false;
		else
			m_isColor = true;

		// Performance tip: If using a color camera, try using RGB format in the camera. Debayering, conversion to RGB, and PGI enhancement will be all done inside the camera.
		//                  This means the host doesn't have to do anything (in this sample, GStreamer is expecting RGB format for color).
		//                  Using other color formats will mean needing at least a conversion on the host (e.g with the CImageFormatConverter),
		//                  or at most a debayering and a conversion (e.g. BayerRG8).
		//if (colorCamera == true && GenApi::IsAvailable(PixelFormat->GetEntryByName("RGB8")) == true)
		//PixelFormat->FromString("RGB8");

		// setup some driver settings
		MaxNumBuffer.SetValue(20); // in case we use a grab strategy besides 'latest image only'

		m_isInitialized = true;

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << e.GetSourceFileName() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return false;
	}
}

// Start the image grabbing of camera and driver
bool CInstantCameraForAppSrc::StartCamera()
{
	try
	{
		if (m_isInitialized == false)
		{
			cout << "Camera not initialized. Run OpenCamera() first." << endl;
			return false;
		}

		// Start grabbing images with the camera and pylon.
		// Here we use Pylon's GrabStrategy_LatestImageOnly.
		// This is good for display, and for benchmarking (because any "lag" between images is solely due to how fast the application can call app->grabFrame())

		cout << "Starting Camera image acquistion and Pylon driver Grab Engine..." << endl;
		if (m_isTriggered == true)
		{
			cout << "Camera will now expect a hardware trigger on: " << GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerSource"))->ToString() << "..." << endl;
		}
		StartGrabbing(Pylon::EGrabStrategy::GrabStrategy_LatestImageOnly);

		// Note: At this point, the camera is acquiring and transmitting images, and the driver's Grab Engine is grabbing them.
		//       When the Grab Engine has an image, it places it into it's Output Queue for retrieval by CInstantCamera::RetrieveResult().
		//		 When the AppSrc needs an image to push to the GStreamer pipeline, it fires the "need-data" callback, which runs cb_need_data().
		//		 cb_need_data() calls RetrieveImage(), which in turn calls RetrieveResult(), which retrieves an image from the Grab Engine Output Queue
		//       If you like, you can see how many images are waiting for retrieval by checking camera.NumReadyBuffers.GetValue().
		//
		// Note: When using GrabStrategy_LatestImageOnly, there will always be only one image waiting in the Output Queue by design - the latest image to come in from the camera.
		//		 LatestImageOnly is good for display applications and for benchmarking the application/host...
		//		 ...because any "lag" or "stutter" seen in the display is purely dependent on how fast the application is retrieving images (slower application = more stutter).

		// Check that we are actually getting images before proceeding
		//while (true)
		//{
		//	if (NumReadyBuffers.GetValue() > 0)
		//	{
		//		cout << "Images have begun arriving at Grab Engine and are ready to be retrieved and pushed to pipeline!" << endl;
		//		break;
		//	}
		//}

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return false;
	}
}

// Retrieve an image from the driver and place it into an image container
bool CInstantCameraForAppSrc::RetrieveImage()
{
	try
	{
		if (IsGrabbing() == false)
		{
			cout << "Camera is not Grabbing. Run StartCamera() first." << endl;
			return false;
		}

		// Description of "Grabbing" procedure:
		// In this sample, the camera is always free-running and sending images to the Pylon driver's "Grab Engine".
		// The Pylon Grab Engine is thus always spinning. It "Grabs" incoming data, places it into an empty buffer from its "Input Queue", and places the "Grab Result£ into its "Output Queue".
		// Depending on the Pylon "Grab Strategy" used, buffers are recycled in different ways.
		//  In this sample, the LatestImageOnly strategy is used. This means that only one Grab Result is kept in the Output Queue at a time.
		//  If a new image comes from the camera before the previous is retrieved from the output queue, the previous one is overwritten with the newer one.
		// The application retrieves a Grab Result by calling RetrieveResult. If the Grab Result is successful, then a good image is in the buffer. If it is not, there was a problem.

		// The CGrabResultPtr smart pointer contains information about the grab in question, as well as access to the buffer of pixel data.
		Pylon::CGrabResultPtr ptrGrabResult;

		if (m_isOnDemand == true)
			ExecuteSoftwareTrigger();

		// Retrieve a Grab Result from the Grab Engine's Output Queue. If nothing comes to the output queue in 5 seconds, throw a timeout exception.
		RetrieveResult(5000, ptrGrabResult, Pylon::ETimeoutHandling::TimeoutHandling_ThrowException);

		// if the Grab Result indicates success, then we have a good image within the result.
		if (ptrGrabResult->GrabSucceeded())
		{
			// if we have a color image, and the image is not RGB, convert it to RGB and place it into the CInstantCameraForAppSrc::image for GStreamer
			if (m_isColor == true && m_FormatConverter.ImageHasDestinationFormat(ptrGrabResult) == false)
			{
				m_FormatConverter.Convert(m_Image, ptrGrabResult);
			}
			// else if we have an RGB image or a Mono image, simply copy the image to CInstantCameraForAppSrc::image
			// (push a copy of the image to the pipeline instead of a pointer in case we retrieve another image while the first is still going through the pipeline).
			else if (m_FormatConverter.ImageHasDestinationFormat(ptrGrabResult) == true || Pylon::IsMonoImage(ptrGrabResult->GetPixelType()))
			{
				m_Image.CopyImage(ptrGrabResult);
			}
		}
		else
		{
			// If a Grab Failed, the Grab Result is tagged with information about why it failed (technically you could even still access the pixel data to look at the bad image too).
			cout << "Pylon: Grab Result Failed! Error: " << ptrGrabResult->GetErrorDescription() << endl;
			return false;
		}

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return false;
	}
}

// Stop the image grabbing of camera and driver
bool CInstantCameraForAppSrc::StopCamera()
{
	try
	{
		cout << "Stopping Camera image acquistion and Pylon image grabbing..." << endl;
		StopGrabbing();

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return false;
	}
}

// Close the camera and do any other cleanup needed
bool CInstantCameraForAppSrc::CloseCamera()
{
	try
	{
		Close();
		// below is rather redundant. The pylon device is by default attached with the tag 'cleanup delete' which means the device is destroyed when the camera is destroyed.
		DetachDevice();
		DestroyDevice();
		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured: " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred: " << endl << e.what() << endl;
		return false;
	}
}
