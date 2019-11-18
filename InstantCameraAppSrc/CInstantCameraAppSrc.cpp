/*  CInstantCameraAppSrc.cpp: Definition file for CInstantCameraAppSrc Class.
	This will extend the Basler Pylon::CInstantCamera Class to make it more convinient to use with GstAppSrc.

	Copyright 2017, 2018, 2019 Matthew Breit <matt.breit@gmail.com>

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

/*
	A Typical Image Streaming Pipeline would look as follows:
	|<--------------- Camera Acquisition & Pylon Grabbing ------------------->|<---------------------------------------- GStreamer Pipeline ------------------------->|
	|<------------------------------------------------------------- CInstantCameraAppSrc --------------------------------------->|
	+-------------------------------------------------------------------------+--------------------------------------------------+    +----------+    +---------------+
	|                                                       GetGstAppSrc()---------> sourceBin element                           |    |  other   |    | sink element  |
	|                                                                         |                                                  |    | elements |    |               |
	|                                    RetrieveImage()<---cb_need_data()<---------"need-data" signal                           |    |          |    |               |
	|            ------------------> --> 1. RetrieveResult()                  |                                                  |    |          |    |               |
	|            | LatestImageOnly |     2. Convert to RGB if color           |                                                  |    |          |    |               |
	|            <------------------     3. Put into a [pylonimage]           | +-------------+ +------------+ +------------+    |    |          |    |               |
	| [Camera]-->[Pylon Grab Engine]     4. Wrap in a gst buffer              | |             | |            | |            |    |    |          |    |               |
	| -------->                          5. "push-buffer" signal-------------------->AppSrc-------->Rescale------->Rotate------>src--sink       src--sink             |
	| |freerun|                                                               | |             | |            | |            |    |    |          |    |               |
	| <--------                                                               | +-------------+ +------------+ +------------+    |    |          |    |               |
	+-------------------------------------------------------------------------+--------------------------------------------------+    +----------+    +---------------+
	CInstantCameraAppSrc::
	InitCamera()
	StartCamera()
	StopCamera()
	CloseCamera()
	GetWidth()
	GetHeight()
	GetFrameRate()

	1. The camera and grab engine in this case are always freerunning (unless ondemand is used, then it sits idle and sends a trigger when an image is needed)
	2. LatestImageOnly strategy means the Grab Engine keeps the latest image received ready for retrieval.
	3. When AppSrc needs data, it sends the "need-data" signal.
	4. This fires cb_need_data which calls RetrieveImage().
	5. RetrieveImage() retrieves the image from the Grab Engine, converts it to RGB, and places it in a PylonImage container.
	6. The memory of the PylonImage container is then wrapped and pushed to AppSrc's src pad by sending the "push-buffer" signal.
	7. AppSrc provides the image to the rescaler element, which then pushes it to image rotation element.
	8. AppSrc, rescaler, and rotator elements are binned together into sourceBin.
	9. The output of sourceBin (it's src pad) is then the input to the rest of the pipeline
	*/

#include "CInstantCameraAppSrc.h"

using namespace Pylon;
using namespace GenApi;
using namespace std;

// Here we extend the Pylon CInstantCamera class with a few things to make it easier to integrate with Appsrc.
CInstantCameraAppSrc::CInstantCameraAppSrc(string serialnumber)
{
	// initialize Pylon runtime
	Pylon::PylonInitialize();

	m_serialNumber = serialnumber;
	m_isOpen = false;

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
		OpenCamera();
		m_isOpen = true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in CInstantCameraAppSrc(): " << endl << e.GetDescription() << e.GetSourceFileName() << endl;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in CInstantCameraAppSrc(): " << endl << e.what() << endl;
	}
}

CInstantCameraAppSrc::~CInstantCameraAppSrc()
{
	CloseCamera();
	// free resources allocated by pylon runtime.
	Pylon::PylonTerminate();
}

int CInstantCameraAppSrc::GetWidth()
{
	if (GenApi::IsReadable(GetNodeMap().GetNode("Width")))
		return CIntegerPtr(GetNodeMap().GetNode("Width"))->GetValue();
	else
		return -1;
}

int CInstantCameraAppSrc::GetHeight()
{
	if (GenApi::IsReadable(GetNodeMap().GetNode("Height")))
		return CIntegerPtr(GetNodeMap().GetNode("Height"))->GetValue();
	else
		return -1;
}

double CInstantCameraAppSrc::GetFrameRate()
{
	if (GenApi::IsReadable(GetNodeMap().GetNode("ResultingFrameRateAbs")))
		return CFloatPtr(GetNodeMap().GetNode("ResultingFrameRateAbs"))->GetValue();
	else if (GenApi::IsReadable(GetNodeMap().GetNode("ResultingFrameRate")))
		return CFloatPtr(GetNodeMap().GetNode("ResultingFrameRate"))->GetValue(); // BCON LVDS and USB use SFNC3 names
	else if (GenApi::IsReadable(GetNodeMap().GetNode("AcquisitionFrameRate")))
		return CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRate"))->GetValue(); // MIPI
	else
		return -1;

	// default
	//return 0;
}

bool CInstantCameraAppSrc::SetFrameRate(double framesPerSecond)
{
	try
	{
		if (GenApi::IsWritable(GetNodeMap().GetNode("AcquisitionFrameRateEnable")))
			CBooleanPtr(GetNodeMap().GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
		if (GenApi::IsWritable(GetNodeMap().GetNode("AcquisitionFrameRate")))
			CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRate"))->SetValue(framesPerSecond);
		if (GenApi::IsWritable(GetNodeMap().GetNode("AcquisitionFrameRateAbs")))
			CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRateAbs"))->SetValue(framesPerSecond);
		
		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in SetFrameRate(): " << endl << e.GetDescription() << e.GetSourceFileName() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in SetFrameRate(): " << endl << e.what() << endl;
		return false;
	}
}

// Open the camera and adjust some settings
bool CInstantCameraAppSrc::InitCamera(int width, int height, int framesPerSecond, bool useOnDemand, bool useTrigger, int scaledWidth, int scaledHeight, int rotation, int numFramesToGrab)
{
	try
	{
		m_isInitialized = false;
		m_width = width;
		m_height = height;
		m_frameRate = framesPerSecond;
		m_isOnDemand = useOnDemand;
		m_isTriggered = useTrigger;
		m_scaledWidth = scaledWidth;
		m_scaledHeight = scaledHeight;
		m_rotation = rotation;
		m_numFramesToGrab = numFramesToGrab;

		// since Image On Demand uses software trigger, it cannot be used with isTriggered
		if (m_isOnDemand == true && m_isTriggered == true)
		{
			cout << "Cannot use both Image-on-Demand and Triggered mode. Using only Triggered Mode." << endl;
			m_isOnDemand = false;
		}

		// setup the camera. Here we use the GenICam GenAPI method so we can support multiple interfaces like usb and gige
		// Note: Get the "Node names" from pylon viewer

		// some features are unique to only Usb or only GigE cameras (like ip address).
		// Also, some features may have different names (AcquisitionFrameRate vs. AcquisitionFrameRateAbs) due to different versions of the GeniCam SFNC standard.
		// Here we manage this.

		// You can enable migration mode in usb cameras so that we can solve the 'same feature has different name' topic.
		// Migration mode lets us access features by their old names too.
		// We skip this here because the BCON interface also uses SFNC3 names, but does not support migration mode.
		// Since we want to support BCON also in this program, we will manage each feature with a different name manually.
		//if (GetDeviceInfo().GetDeviceClass() == "BaslerUsb")
		//	GenApi::CBooleanPtr(GetTLNodeMap().GetNode("MigrationModeEnable"))->SetValue(true);

		OpenCamera();

		if (m_width == -1)
		{
			if (IsReadable(GetNodeMap().GetNode("Width")))
				m_width = GenApi::CIntegerPtr(GetNodeMap().GetNode("Width"))->GetMax();
		}
		else
		{
			if (IsWritable(GetNodeMap().GetNode("Width")))
				GenApi::CIntegerPtr(GetNodeMap().GetNode("Width"))->SetValue(m_width);
		}
		if (m_height == -1)
		{
			if (IsReadable(GetNodeMap().GetNode("Height")))
				m_height = GenApi::CIntegerPtr(GetNodeMap().GetNode("Height"))->GetMax();
		}
		else
		{
			if (IsWritable(GetNodeMap().GetNode("Height")))
				GenApi::CIntegerPtr(GetNodeMap().GetNode("Height"))->SetValue(m_height);
		}

		if (IsWritable(GetNodeMap().GetNode("CenterX")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("CenterX"))->SetValue(true);
		if (IsWritable(GetNodeMap().GetNode("CenterY")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("CenterY"))->SetValue(true);

		if (m_isOnDemand == true || m_isTriggered == true)
		{
			if (IsWritable(GetNodeMap().GetNode("TriggerSelector")))
			{
				GenApi::CEnumerationPtr ptrTriggerSelector = GetNodeMap().GetNode("TriggerSelector");
				if (IsWritable(ptrTriggerSelector->GetEntryByName("AcquisitionStart")))
				{
					ptrTriggerSelector->FromString("AcquisitionStart");
					GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerMode"))->FromString("Off");
				}
				if (IsWritable(ptrTriggerSelector->GetEntryByName("FrameBurstStart"))) // BCON and USB use SFNC3 names
				{
					ptrTriggerSelector->FromString("FrameBurstStart");
					GenApi::CEnumerationPtr(GetNodeMap().GetNode("TriggerMode"))->FromString("Off");
				}
				if (IsWritable(ptrTriggerSelector->GetEntryByName("FrameStart")))
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

		// Configure some Pylon driver settings
		//MaxNumBuffer.SetValue(20); // in case we use a grab strategy besides 'latest image only'

		// Configure the Pylon image format converter
		// We're going to use GStreamer's RGB format in pipelines, so we may need to use Pylon to convert the camera's image to RGB (depending on the camera used)
		EPixelType pixelType = Pylon::EPixelType::PixelType_RGB8packed;
		m_FormatConverter.OutputPixelFormat.SetValue(pixelType);

		// setup some settings common to most cameras (it's always best to check if a feature is available before setting it)
		if (m_isTriggered == false)
		{
			if (m_frameRate == -1)
			{
				m_frameRate = this->GetFrameRate();
			}

			if (IsWritable(GetNodeMap().GetNode("AcquisitionFrameRateEnable")))
				GenApi::CBooleanPtr(GetNodeMap().GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
			if (IsWritable(GetNodeMap().GetNode("AcquisitionFrameRateAbs")))
				GenApi::CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRateAbs"))->SetValue(m_frameRate); // this is called "AcquisitionFrameRate" (not abs) in usb cameras. Migration mode lets us use the old name though.
			if (IsWritable(GetNodeMap().GetNode("AcquisitionFrameRate")))
				GenApi::CFloatPtr(GetNodeMap().GetNode("AcquisitionFrameRate"))->SetValue(m_frameRate); // BCON and USB use SFNC3 names.
		}

		// Initialize the Pylon image to a blank image on the off chance that the very first m_Image can't be supplied by the instant camera (ie: missing trigger signal)
		m_Image.Reset(pixelType, m_width, m_height);

		m_isInitialized = true;

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in InitCamera(): " << endl << e.GetDescription() << e.GetSourceFileName() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in InitCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Start the image grabbing of camera and driver
bool CInstantCameraAppSrc::StartCamera()
{
	try
	{
		if (m_isInitialized == false)
		{
			cout << "Camera not initialized. Run InitCamera() first." << endl;
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
		cerr << "An exception occured in StartCamera(): " << endl << e.GetDescription() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in StartCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Retrieve an image from the driver and place it into an image container
bool CInstantCameraAppSrc::retrieve_image()
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
		{
			ExecuteSoftwareTrigger(); // TODO: Check for bug or broken camera on 21949158. It would "Grab Timeout" when used in the twocameras_compositor sample. Other camera combinations worked just fine.
		}
		// Retrieve a Grab Result from the Grab Engine's Output Queue. If nothing comes to the output queue in 5 seconds, throw a timeout exception.
		RetrieveResult(5000, ptrGrabResult, Pylon::ETimeoutHandling::TimeoutHandling_ThrowException);
		// if the Grab Result indicates success, then we have a good image within the result.
		if (ptrGrabResult->GrabSucceeded())
		{
			// if we have a color image, and the image is not RGB, convert it to RGB and place it into the CInstantCameraAppSrc::image for GStreamer
			if (m_isColor == true && m_FormatConverter.ImageHasDestinationFormat(ptrGrabResult) == false)
			{
				m_FormatConverter.Convert(m_Image, ptrGrabResult);
			}
			// else if we have an RGB image or a Mono image, simply copy the image to CInstantCameraAppSrc::image
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
			cout << "Will push last good image instead..." << endl;
		}


		// create a gst buffer wrapping the image container's buffer
		m_gstBuffer = gst_buffer_new_wrapped_full(
			(GstMemoryFlags)GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS,
			(gpointer)m_Image.GetBuffer(),
			m_Image.GetImageSize(),
			0,
			m_Image.GetImageSize(),
			NULL,
			NULL);

		// Push the gst buffer wrapping the image buffer to the source pads of the AppSrc element, where it's picked up by the rest of the pipeline
		GstFlowReturn ret;
		g_signal_emit_by_name(m_appsrc, "push-buffer", m_gstBuffer, &ret);

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in retrieve_image(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in retrieve_image(): " << endl << e.what() << endl;
		return false;
	}
}

// Stop the image grabbing of camera and driver
bool CInstantCameraAppSrc::StopCamera()
{
	try
	{
		// send an EOS event to effectively stop need-data signals. Otherwise the clearing of grab engine buffers by stopgrabbing()
		// may occur during a subsequent retrieve_image(), which could lead to a null grabresult pointer ("no grab result data referenced error")
		cout << "Sending EOS event..." << endl;
		gst_element_send_event(m_appsrc, gst_event_new_eos());

		cout << "Stopping Camera image acquistion and Pylon image grabbing..." << endl;
		StopGrabbing();

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in StopCamera(): " << endl << e.GetDescription() << endl;
		return false;

	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in StopCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Close the camera and do any other cleanup needed
bool CInstantCameraAppSrc::OpenCamera()
{
	try
	{
		Open();
		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in CloseCamera(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in CloseCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Close the camera and do any other cleanup needed
bool CInstantCameraAppSrc::CloseCamera()
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
		cerr << "An exception occured in CloseCamera(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in CloseCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Reset the camera to defaults
bool CInstantCameraAppSrc::ResetCamera()
{
	try
	{
		if (GenApi::IsWritable(GetNodeMap().GetNode("UserSetSelector")))
		{
			GenApi::CEnumerationPtr(GetNodeMap().GetNode("UserSetSelector"))->FromString("Default");
			GenApi::CCommandPtr(GetNodeMap().GetNode("UserSetLoad"))->Execute();
			return false;
		}
		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in ResetCamera(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in ResetCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// Mimic the Pylon Viewer's Automatic Image Adjustment toolbar button
bool CInstantCameraAppSrc::AutoAdjustImage()
{
	try
	{
		if (GenApi::IsWritable(GetNodeMap().GetNode("ExposureAuto")))
		{
			GenApi::CEnumerationPtr(GetNodeMap().GetNode("ExposureAuto"))->FromString("Once");
		}
		if (GenApi::IsWritable(GetNodeMap().GetNode("GainAuto")))
		{
			GenApi::CEnumerationPtr(GetNodeMap().GetNode("GainAuto"))->FromString("Once");
		}
		if (GenApi::IsWritable(GetNodeMap().GetNode("BalanceWhiteAuto")))
		{
			GenApi::CEnumerationPtr(GetNodeMap().GetNode("BalanceWhiteAuto"))->FromString("Once");
		}
		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in AutoAdjustImage(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in AutoAdjustImage(): " << endl << e.what() << endl;
		return false;
	}
}

// Save current settings to camera, with the option to boot the camera with them
bool CInstantCameraAppSrc::SaveSettingsToCamera(bool BootWithNewSettings)
{
	try
	{
		// USB cameras sometimes have different names for features than GigE cameras, due to SFNC versions.
		// They can be made backwards compatible and accessible by the old names by enabling "migration mode"
		// Just in case some of those features are related to saving settings to the camera, enable migration mode.
		if (GenApi::IsWritable(GetNodeMap().GetNode("MigrationModeEnable")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("MigrationModeEnable"))->SetValue(true);

		// Note: Below we don't check writability of nodes, because we would like to throw an error if things fail to alert the application 
		
		// Select UserSet1
		GenApi::CEnumerationPtr(GetNodeMap().GetNode("UserSetSelector"))->FromString("UserSet1");
		// Save current settings to UserSet1
		GenApi::CCommandPtr(GetNodeMap().GetNode("UserSetSave"))->Execute();
		// Tell the camera to bootup with UserSet1 instead of default settings
		if (BootWithNewSettings == true)
			GenApi::CEnumerationPtr(GetNodeMap().GetNode("UserSetDefault"))->FromString("UserSet1");
		
		// Turn off migration mode (just good programming practice, if you change something, change it back so we don't risk affecting anything else)
		if (GenApi::IsWritable(GetNodeMap().GetNode("MigrationModeEnable")))
			GenApi::CBooleanPtr(GetNodeMap().GetNode("MigrationModeEnable"))->SetValue(false);

		return true;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in SaveSettingsToCamera(): " << endl << e.GetDescription() << endl;
		return false;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in SaveSettingsToCamera(): " << endl << e.what() << endl;
		return false;
	}
}

// we will provide the application a configured gst source element to match the camera.
GstElement* CInstantCameraAppSrc::GetSource()
{
	try
	{
		// create an appsrc element
		// Give this element a unique name by adding the camera's serial number, so that mutiple cameras can be used in the same pipeline.
		string appsrcName = "source";
		appsrcName.append(this->GetDeviceInfo().GetSerialNumber());
		m_appsrc = gst_element_factory_make("appsrc", appsrcName.c_str());

		// setup the appsrc properties
		g_object_set(G_OBJECT(m_appsrc),
			"stream-type", 0, // 0 = GST_APP_STREAM_TYPE_STREAM
			"format", GST_FORMAT_TIME,
			"is-live", TRUE,
			"num-buffers", m_numFramesToGrab,
			"do-timestamp", TRUE, // required for H264 streaming
			NULL);

		// setup the appsrc caps (what kind of video is coming out of the source element?
		string format = "RGB";
		if (m_isColor == false)
			format = "GRAY8";

		g_object_set(G_OBJECT(m_appsrc), "caps",
			gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, format.c_str(),
			"width", G_TYPE_INT, this->GetWidth(), // just in case the camera used a different value than our desired, due to increment constraints
			"height", G_TYPE_INT, this->GetHeight(),
			"framerate", GST_TYPE_FRACTION, (int)this->GetFrameRate(), 1, NULL), NULL); // just in case we desired an u

		// connect the appsrc to the cb_need_data callback function. When appsrc sends the need-data signal, cb_need_data will run.
		g_signal_connect(m_appsrc, "need-data", G_CALLBACK(cb_need_data), this);

		// we can also bin the source with a videoscaler and videoflip element to offer easy rescaling and rotation to the user
		GstElement *rescaler;
		GstElement *rescalerCaps;
		GstElement *rotator;
		GstElement *converter;
		GstElement *finalConverter;
		GstElement *finalFilter;
		GstCaps	   *finalFilter_caps;
		rescaler = gst_element_factory_make("videoscale", "rescaler");
		rescalerCaps = gst_element_factory_make("capsfilter", "rescalerCaps");
		rotator = gst_element_factory_make("videoflip", "rotator");
		converter = gst_element_factory_make("videoconvert", "converter");
		finalConverter = gst_element_factory_make("videoconvert", "finalConverter");
		finalFilter = gst_element_factory_make("capsfilter", "filter");

		// configure the videoscaler and videoscaler caps elements
		if (m_scaledWidth == -1 || m_scaledHeight == -1)
		{
			// don't do any rescaling
			m_scaledWidth = this->GetWidth();
			m_scaledHeight = this->GetHeight();
		}
		else if (m_scaledWidth < 2 || m_scaledHeight < 2)
		{
			// rescaling to widths less that 2 could cause buffer pool errors
			cerr << "Scaling width and height must be greater than 2x2! Will not scale image!" << endl;
			m_scaledWidth = this->GetWidth();
			m_scaledHeight = this->GetHeight();
		}

		// configure the capsfilter after the videoscaler element, so it will apply scaling.
		g_object_set(G_OBJECT(rescalerCaps), "caps",
			gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, format.c_str(),
			"width", G_TYPE_INT, m_scaledWidth,
			"height", G_TYPE_INT, m_scaledHeight,
			"framerate", GST_TYPE_FRACTION, (int)this->GetFrameRate(), 1, NULL), NULL);

		// configure the videoflip element for rotation
		if (m_rotation == -1 || m_rotation == 0)
			m_rotation = 0; // GST_VIDEO_FLIP_METHOD_IDENTITY (none). We offer it as -1 to the user to remain consistent with other options where -1 = no effect
		else if (m_rotation == 90)
			m_rotation = 1; // GST_VIDEO_FLIP_METHOD_90R
		else if (m_rotation == 180)
			m_rotation = 2; // GST_VIDEO_FLIP_METHOD_180
		else if (m_rotation == 270)
			m_rotation = 3; // GST_VIDEO_FLIP_METHOD_90L
		else
		{
			cerr << "Only rotation angles of 90, 180, 270 are supported! Will not rotate image!" << endl;
			m_rotation = 0;
		}

		g_object_set(G_OBJECT(rotator), "method", m_rotation, NULL);
		
		// configure the final filter caps so that we output the common I420 format (if color)
		if (m_isColor == true)
		{
			finalFilter_caps = gst_caps_new_simple("video/x-raw",
				"format", G_TYPE_STRING, "I420",
				NULL);
		}
		else
		{
			finalFilter_caps = gst_caps_new_simple("video/x-raw",
				"format", G_TYPE_STRING, format.c_str(),
				NULL);
		}
			
		g_object_set(G_OBJECT(finalFilter), "caps", finalFilter_caps, NULL);
		gst_caps_unref(finalFilter_caps);
		
		// combine the appsrc, rescaler, and rotator elements into a single binned element
		// Give this "sourceBin" a unique name by adding the camera serial number, so that multiple cameras can be placed in the same pipeline.
		string sourceBinName = "sourcebin";
		sourceBinName.append(this->GetDeviceInfo().GetSerialNumber());
		m_sourceBin = gst_bin_new(sourceBinName.c_str());

		gst_bin_add_many(GST_BIN(m_sourceBin), m_appsrc, converter, rescaler, rescalerCaps, rotator, finalConverter, finalFilter, NULL);
		gst_element_link_many(m_appsrc, converter, rescaler, rescalerCaps, rotator, finalConverter, finalFilter, NULL);

		// setup a ghost pad, so the src output of the last element in the bin attaches to the rest of the pipeline.
		GstPad *binSrc;
		binSrc = gst_element_get_static_pad(finalFilter, "src");
		gst_element_add_pad(m_sourceBin, gst_ghost_pad_new("src", binSrc));
		gst_object_unref(GST_OBJECT(binSrc));

		g_object_set(G_OBJECT(m_sourceBin),
			"async-handling", TRUE,
			"message-forward", TRUE,
			NULL);

		return m_sourceBin;
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in GetSource(): " << endl << e.GetDescription() << endl;
		return m_sourceBin;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in GetSource(): " << endl << e.what() << endl;
		return m_sourceBin;
	}
}

// the callback that's fired when the appsrc element sends the 'need-data' signal.
void CInstantCameraAppSrc::cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
	try
	{
		// remember, the "user data" the signal passes to the callback is really the address of the Instant Camera
		CInstantCameraAppSrc *pCamera = (CInstantCameraAppSrc*)user_data;

		// If we request data, and discover the camera is removed, send the EOS signal.
		if (pCamera->IsCameraDeviceRemoved() == true)
		{
			cout << "Camera Removed!" << endl;
			GstFlowReturn ret;
			g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
		}

		// tell the CInstantCameraAppSrc to Retrieve an Image. It will pull an image from the pylon driver, and place it into it's CInstantCameraAppSrc::image container.
		pCamera->retrieve_image();
	}
	catch (GenICam::GenericException &e)
	{
		cerr << "An exception occured in cb_need_data(): " << endl << e.GetDescription() << endl;
	}
	catch (std::exception &e)
	{
		cerr << "An exception occurred in cb_need_data(): " << endl << e.what() << endl;
	}

}
