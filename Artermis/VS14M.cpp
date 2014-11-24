///////////////////////////////////////////////////////////////////////////////
// FILE:          VS14M.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Artemis VS14M camera interface for micromanager
//                
// AUTHOR:        Doug Kelly, dk1109@ic.ac.uk, 26/07/2014
//
// COPYRIGHT:     
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

#include "VS14M.h"
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include "../../3rdparty/ArtemisVS14M/ArtemisHscAPI.h"
//#include "C:\\Users\\dk1109\\repositories\\umanager\\micromanager\\DeviceAdapters\\Artermis\\ArtemisHscAPI.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <cmath>



using namespace std;
const double CVS14M::nominalPixelSizeUm_ = 1.0;
double g_IntensityFactor_ = 1.0;

// External names used used by the rest of the system
// to load particular device from the "VS14M.dll" library
const char* g_CameraDeviceName = "VS14MCam";


// constants for naming pixel types (allowed values of the "PixelType" property)
const char* g_PixelType_8bit = "8bit";
const char* g_PixelType_16bit = "16bit";
const char* g_PixelType_32bitRGB = "32bitRGB";
const char* g_PixelType_64bitRGB = "64bitRGB";
const char* g_PixelType_32bit = "32bit";  // floating point greyscale


///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_CameraDeviceName, MM::CameraDevice, "Artemis camera");
	RegisterDevice("TransposeProcessor", MM::ImageProcessorDevice, "TransposeProcessor");
	RegisterDevice("ImageFlipX", MM::ImageProcessorDevice, "ImageFlipX");
	RegisterDevice("ImageFlipY", MM::ImageProcessorDevice, "ImageFlipY");
	RegisterDevice("MedianFilter", MM::ImageProcessorDevice, "MedianFilter");

}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	// decide which device class to create based on the deviceName parameter
	if (strcmp(deviceName, g_CameraDeviceName) == 0)
	{
		// create camera
		return new CVS14M();
	}
	

	else if(strcmp(deviceName, "TransposeProcessor") == 0)
	{
		return new TransposeProcessor();
	}
	else if(strcmp(deviceName, "ImageFlipX") == 0)
	{
		return new ImageFlipX();
	}
	else if(strcmp(deviceName, "ImageFlipY") == 0)
	{
		return new ImageFlipY();
	}
	else if(strcmp(deviceName, "MedianFilter") == 0)
	{
		return new MedianFilter();
	}


	// ...supplied name not recognized
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// CVS14M implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~

/**
* CVS14M constructor.
* Setup default all variables and create device properties required to exist
* before intialization. In this case, no such properties were required. All
* properties will be created in the Initialize() method.
*
* As a general guideline Micro-Manager devices do not access hardware in the
* the constructor. We should do as little as possible in the constructor and
* perform most of the initialization in the Initialize() method.
*/
CVS14M::CVS14M() :
CCameraBase<CVS14M> (),
	exposureMaximum_(86400000.0),		//24 hours in ms
	dPhase_(0),
	initialized_(false),
	bitDepth_(16),
	roiX_(0),
	roiY_(0),
	roiW_(1392),
	roiH_(1040),
	sequenceStartTime_(0),
	isSequenceable_(false),
	sequenceMaxLength_(100),
	sequenceRunning_(false),
	sequenceIndex_(0),
	binSizeX_(1),
	binSizeY_(1),
	asymmBinning_(false),
	cameraCCDXSize_(1392),
	cameraCCDYSize_(1040),
	ccdT_ (0.0),
	triggerDevice_(""),
	currentTemp_(-1.0),
	stopOnOverflow_(false),
	flipUD_(false),
	flipLR_(false),
	imageRotationAngle_(0),
	highPriority_(false),
	prechargeMode_(PRECHARGE_NONE),
	processVBE_(true),
	processLinearise_(true),
	overlapExposure_(false), 
	previewMode_(false),
	nComponents_(1)
{
	//memset(testProperty_,0,sizeof(testProperty_));

	// call the base class method to set-up default error codes/messages
	InitializeDefaultErrorMessages();
	readoutStartTime_ = GetCurrentMMTime();
	thd_ = new MySequenceThread(this);

	// parent ID display
	//CreateHubIDProperty();

	CreateFloatProperty("MaximumExposureMs", exposureMaximum_, false,
		new CPropertyAction(this, &CVS14M::OnMaxExposure),
		true);
}

/**
* CVS14M destructor.
* If this device used as intended within the Micro-Manager system,
* Shutdown() will be always called before the destructor. But in any case
* we need to make sure that all resources are properly released even if
* Shutdown() was not called.
*/
CVS14M::~CVS14M()
{
	//StopSequenceAcquisition();
	//ArtemisDisconnectAll();
	ArtemisUnLoadDLL();
	LogMessage("DLL unloaded OK");
	delete thd_;
}

/**
* Obtains device name.
* Required by the MM::Device API.
*/
void CVS14M::GetName(char* name) const
{
	// Return the name used to referr to this device adapte
	CDeviceUtils::CopyLimitedString(name, g_CameraDeviceName);
}

/**
* Intializes the hardware.
* Required by the MM::Device API.
* Typically we access and initialize hardware at this point.
* Device properties are typically created here as well, except
* the ones we need to use for defining initialization parameters.
* Such pre-initialization properties are created in the constructor.
* (This device does not have any pre-initialization properties)
*/
int CVS14M::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	// set property list
	// -----------------

	// Name
	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_CameraDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Description
	nRet = CreateStringProperty(MM::g_Keyword_Description, "Artemis VS14M Camera Device Adapter", true);
	if (DEVICE_OK != nRet)
		return nRet;

	// CameraName
	nRet = CreateStringProperty(MM::g_Keyword_CameraName, "VS14M", true);
	assert(nRet == DEVICE_OK);

	// CameraID
	nRet = CreateStringProperty(MM::g_Keyword_CameraID, "V1.0", true);
	assert(nRet == DEVICE_OK);

	//Load DLL and connect camera before setting up any properties that need hardware-based limits. 
	//bool dllOK = ArtemisLoadDLL("C:\\Windows\\SysWOW64\\ArtemisSci.dll");
	bool dllOK = ArtemisLoadDLL("C:\\Windows\\SysWOW64\\ArtemisHsc.dll"); //change to relative path???

	if (dllOK)
		LogMessage("DLL loaded OK!");
	else
		return DEVICE_ERR;

	hCam_ = ArtemisConnect(0);	//Connect first camera found. 

	//// Deal with connecting cameras
	//char devname[64];
	//int ncams = 0;
	//for (int i = 0; i < 10; i++)
	//{
	//	if (ArtemisDeviceIsCamera(i))
	//	{
	//		ArtemisDeviceName(i,devname);
	//		std::cout << "Device name: " << devname << ", number " << i <<std::endl;
	//	}
	//}

	// API version
	int APIVer = ArtemisAPIVersion();
	nRet = CreateStringProperty("Artemis API version", ("V" + 
		(boost::lexical_cast<std::string>(APIVer))).c_str(), true);
	if (nRet != DEVICE_OK)
		return nRet;

	struct ARTEMISPROPERTIES pProp;
	ArtemisProperties(hCam_, &pProp);
	long fullframex = pProp.nPixelsX;
	long fullframey = pProp.nPixelsY;

	//Read only pixel size properties
	CreateFloatProperty("PixelSizeX_um", pProp.PixelMicronsX, true);
	CreateFloatProperty("PixelSizeY_um", pProp.PixelMicronsY, true);
	CreateStringProperty("Manufacturer", pProp.Manufacturer, true);
	CreateStringProperty("DescriptionFromCamera", pProp.Description, true);


	// binning
	CPropertyAction *pAct = new CPropertyAction (this, &CVS14M::OnBinning);
	nRet = CreateIntegerProperty(MM::g_Keyword_Binning, 1, false, pAct);
	assert(nRet == DEVICE_OK);

	nRet = SetAllowedBinning();
	if (nRet != DEVICE_OK)
		return nRet;

	// pixel type
	pAct = new CPropertyAction (this, &CVS14M::OnPixelType);
	nRet = CreateStringProperty(MM::g_Keyword_PixelType, g_PixelType_16bit, false, pAct);
	assert(nRet == DEVICE_OK);

	vector<string> pixelTypeValues;
	//pixelTypeValues.push_back(g_PixelType_8bit);
	pixelTypeValues.push_back(g_PixelType_16bit); 
	//pixelTypeValues.push_back(g_PixelType_32bitRGB);
	//pixelTypeValues.push_back(g_PixelType_64bitRGB);
	//pixelTypeValues.push_back(::g_PixelType_32bit);

	nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
	if (nRet != DEVICE_OK)
		return nRet;

	// Bit depth
	pAct = new CPropertyAction (this, &CVS14M::OnBitDepth);
	nRet = CreateIntegerProperty("BitDepth", 16, false, pAct);
	assert(nRet == DEVICE_OK);

	vector<string> bitDepths;
	//bitDepths.push_back("8");
	//bitDepths.push_back("10");
	//bitDepths.push_back("12");
	//bitDepths.push_back("14");
	bitDepths.push_back("16");
	//bitDepths.push_back("32");
	nRet = SetAllowedValues("BitDepth", bitDepths);
	if (nRet != DEVICE_OK)
		return nRet;

	// exposure
	nRet = CreateFloatProperty(MM::g_Keyword_Exposure, 10.0, false);
	assert(nRet == DEVICE_OK);
	SetPropertyLimits(MM::g_Keyword_Exposure, 0.0, exposureMaximum_);

	//temperature control only if present on camera:

	if (isCoolingPresent(hCam_)){
		
		// camera temperature RO
		GetCurrentTemperature();
		ambientTemp_ = currentTemp_;
		ccdT_ = ambientTemp_;
		nRet = CreateFloatProperty("CCDTemperature Readout", currentTemp_, false);
		assert(nRet == DEVICE_OK);

		// camera temperature
		pAct = new CPropertyAction (this, &CVS14M::OnCCDTemp);
		nRet = CreateFloatProperty(MM::g_Keyword_CCDTemperature, ambientTemp_, false, pAct);
		assert(nRet == DEVICE_OK);
		float minTemp = max((ambientTemp_ - 35), -20.0); //Attempt to account for case where camera has been pre-cooled. 
		SetPropertyLimits(MM::g_Keyword_CCDTemperature, minTemp, ambientTemp_);
	}

	// CCD size of the camera - get from device? for user info only...
	//long fullframex = 1392;
	//long fullframey = 1040;
	pAct = new CPropertyAction (this, &CVS14M::OnCameraCCDXSize);
	CreateIntegerProperty("OnCameraCCDXSize", fullframex, true, pAct);
	cameraCCDXSize_ = fullframex;
	pAct = new CPropertyAction (this, &CVS14M::OnCameraCCDYSize);
	CreateIntegerProperty("OnCameraCCDYSize", fullframey, true, pAct);
	cameraCCDYSize_ = fullframey;

	// Trigger device - include only if camera supports triggering
	if (pProp.cameraflags & ARTEMIS_PROPERTIES_CAMERAFLAGS_EXT_TRIGGER){
		pAct = new CPropertyAction (this, &CVS14M::OnTriggerDevice);
		CreateStringProperty("TriggerDevice", "", false, pAct);
	}

	// Whether or not to use exposure time sequencing
	pAct = new CPropertyAction (this, &CVS14M::OnIsSequenceable);
	std::string propName = "UseExposureSequences";
	CreateStringProperty(propName.c_str(), "No", false, pAct);
	AddAllowedValue(propName.c_str(), "Yes");
	AddAllowedValue(propName.c_str(), "No");

	//Flip image UD?
	pAct = new CPropertyAction(this, &CVS14M::OnFlipUD);
	CreateIntegerProperty("FlipImageUD", 0, false, pAct);
	AddAllowedValue("FlipImageUD","0");
	AddAllowedValue("FlipImageUD","1");

	//Flip image LR?
	pAct = new CPropertyAction(this, &CVS14M::OnFlipLR);
	CreateIntegerProperty("FlipImageLR", 0, false, pAct);
	AddAllowedValue("FlipImageLR","0");
	AddAllowedValue("FlipImageLR","1");

    //Rotate image?
    pAct = new CPropertyAction(this, &CVS14M::OnRotate);
    CreateIntegerProperty("RotateImage", 0, false, pAct);
    AddAllowedValue("RotateImage","0");
    //AddAllowedValue("RotateImage","90");
    AddAllowedValue("RotateImage","180");
    //AddAllowedValue("RotateImage","270");

	//////Subframe controls
 //   pAct = new CPropertyAction(this, &CVS14M::OnSubframeX);
 //   CreateIntegerProperty("FrameXStart", 0, false, pAct);
	//pAct = new CPropertyAction(this, &CVS14M::OnSubframeY);
 //   CreateIntegerProperty("FrameYStart", 0, false, pAct);
	//pAct = new CPropertyAction(this, &CVS14M::OnSubframeW);
 //   CreateIntegerProperty("FrameXWidth", fullframex, false, pAct);
	//pAct = new CPropertyAction(this, &CVS14M::OnSubframeH);
 //   CreateIntegerProperty("FrameYHeight", fullframey, false, pAct);
 //   SetPropertyLimits("FrameXStart", 0, fullframex-1);
 //   SetPropertyLimits("FrameYStart", 0, fullframey - 1);
 //   SetPropertyLimits("FrameXWidth", 1, fullframex);
 //   SetPropertyLimits("FrameYHeight", 1, fullframey);

	//Priority?
	pAct = new CPropertyAction(this, &CVS14M::OnPriority);
	CreateIntegerProperty("DownloadThreadPriority", 0, false, pAct);
	AddAllowedValue("DownloadThreadPriority","0");
	AddAllowedValue("DownloadThreadPriority","1");

	//asymmmetrical binning?
	pAct = new CPropertyAction(this, &CVS14M::OnAsymmBinning);
	CreateIntegerProperty("AsymmetricalBinning", 0, false, pAct);
	AddAllowedValue("AsymmetricalBinning","0");
	AddAllowedValue("AsymmetricalBinning","1");

	pAct = new CPropertyAction(this, &CVS14M::OnYBinning);
	nRet = CreateIntegerProperty("YBinning", 1, false, pAct);
	assert(nRet == DEVICE_OK);

	nRet = SetAllowedYBinning();
	if (nRet != DEVICE_OK)
		return nRet;

	//Precharge mode
	pAct = new CPropertyAction(this, &CVS14M::OnPrecharge);
	CreateStringProperty("PrechargeMode", "None", false, pAct);
	AddAllowedValue("PrechargeMode", "None");
	AddAllowedValue("PrechargeMode", "On camera");
	//AddAllowedValue("PrechargeMode", "In software");

	//Camera processing properties
	pAct = new CPropertyAction(this, &CVS14M::OnArtemisLinearise);
	CreateIntegerProperty("ArtemisJFETLinearise", 1, false, pAct);
	AddAllowedValue("ArtemisJFETLinearise","0");
	AddAllowedValue("ArtemisJFETLinearise","1");

	pAct = new CPropertyAction(this, &CVS14M::OnArtemisVenetian);
	CreateIntegerProperty("ArtemisFixVenetianBlindEffect", 1, false, pAct);
	AddAllowedValue("ArtemisFixVenetianBlindEffect","0");
	AddAllowedValue("ArtemisFixVenetianBlindEffect","1");


	// Camera modes to increase speed 

	//if (ArtemisContinuousExposingModeSupported(hCam_))	//what is difference to ArtemisCanOverlapExposures?
	//if (ArtemisCanOverlapExposures(hCam_))
	if (pProp.cameraflags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_OVERLAP_MODE)
	{
		pAct = new CPropertyAction(this, &CVS14M::OnOverlappedExposure);
		CreateIntegerProperty("OverlappedExposure", 0, false, pAct);
		AddAllowedValue("OverlappedExposure","0");
		AddAllowedValue("OverlappedExposure","1");
	}

	if (pProp.cameraflags & ARTEMIS_PROPERTIES_CAMERAFLAGS_PREVIEW)
	{
		pAct = new CPropertyAction(this, &CVS14M::OnPreviewMode);
		CreateIntegerProperty("PreviewMode", 0, false, pAct);
		AddAllowedValue("PreviewMode","0");
		AddAllowedValue("PreviewMode","1");
	}



	// synchronize all properties
	// --------------------------
	nRet = UpdateStatus();
	if (nRet != DEVICE_OK)
		return nRet;


	// setup the buffer
	// ----------------
	nRet = ResizeImageBuffer();
	if (nRet != DEVICE_OK)
		return nRet;

#ifdef TESTRESOURCELOCKING
	TestResourceLocking(true);
	LogMessage("TestResourceLocking OK",true);
#endif




	initialized_ = true;




	// initialize image buffer
	GenerateEmptyImage(img_);
	return DEVICE_OK;


}

/**
* Shuts down (unloads) the device.
* Required by the MM::Device API.
* Ideally this method will completely unload the device and release all resources.
* Shutdown() may be called multiple times in a row.
* After Shutdown() we should be allowed to call Initialize() again to load the device
* without causing problems.
*/
int CVS14M::Shutdown()
{
	initialized_ = false;
	StopSequenceAcquisition();
	ArtemisCoolerWarmUp(hCam_);
	ArtemisDisconnectAll();
	return DEVICE_OK;
}

/**
* Performs exposure and grabs a single image.
* This function should block during the actual exposure and return immediately afterwards 
* (i.e., before readout).  This behavior is needed for proper synchronization with the shutter.
* Required by the MM::Camera API.
*/
int CVS14M::SnapImage()
{
	static int callCounter = 0;
	++callCounter;

	MM::MMTime startTime = GetCurrentMMTime();
	double exp = GetExposure();
	float exp_seconds = ((float) exp)/1000;
	if (sequenceRunning_ && IsCapturing()) 
	{
		exp = GetSequenceExposure();
	}

	if (triggerDevice_.length() > 0) {
		
		int err = ArtemisTriggeredExposure(hCam_, true);
		//send trigger signal.

	}
	else if  (overlapExposure_)
	{
		ArtemisStartOverlappedExposure(hCam_);
	}
	else
		ArtemisStartExposure(hCam_, exp_seconds);

	if (!overlapExposure_)
	{
		MM::MMTime s0(0,0);
		if( s0 < startTime )
		{
			while (exp > (GetCurrentMMTime() - startTime).getMsec())
			{
				CDeviceUtils::SleepMs(1);
			}		
		}
		else
		{
			std::cerr << "You are operating this device adapter without setting the core callback, timing functions aren't yet available" << std::endl;
			// called without the core callback probably in off line test program
			// need way to build the core in the test program

		}
	}
	readoutStartTime_ = GetCurrentMMTime();



	return DEVICE_OK;
}

/**
* Returns pixel data.
* Required by the MM::Camera API.
* The calling program will assume the size of the buffer based on the values
* obtained from GetImageBufferSize(), which in turn should be consistent with
* values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
* The calling program allso assumes that camera never changes the size of
* the pixel buffer on its own. In other words, the buffer can change only if
* appropriate properties are set (such as binning, pixel type, etc.)
*/
const unsigned char* CVS14M::GetImageBuffer()
{
	MMThreadGuard g(imgPixelsLock_);

	//if (overlapExposure_)
	//{
	//	if (!ArtemisOverlappedExposureValid(hCam_))
	//		return ARTEMIS_OPERATION_FAILED;

	//}
	while (!ArtemisImageReady(hCam_)) {}
	if (overlapExposure_)
		bool overlappedOK = ArtemisOverlappedExposureValid(hCam_);

	////Debug
	//int dw = img_.Width();
	//int dh = img_.Height();
	//int dd = img_.Depth();

	unsigned short *pBuf;
	unsigned short *nBuf;
	pBuf = (unsigned short*) const_cast<unsigned char*>(img_.GetPixelsRW());
	nBuf = (unsigned short*)ArtemisImageBuffer(hCam_);
	
	if (flipUD_)
		mirrorY(img_.Width(), img_.Height(), nBuf, pBuf);
	else if (flipLR_)
		mirrorX(img_.Width(), img_.Height(), nBuf, pBuf);
    else if (!(imageRotationAngle_ == 0)){
        switch (imageRotationAngle_){
        case 90:
            rotate90(img_.Width(), img_.Height(), nBuf, pBuf);
            break;
        case 180:
            rotate180(img_.Width(), img_.Height(), nBuf, pBuf);
            break;
        case 270:
            rotate270(img_.Width(), img_.Height(), nBuf, pBuf);
            break;
        }
    }

	else
		memcpy(pBuf, nBuf, img_.Width()*img_.Height()*img_.Depth());

	return img_.GetPixels();
}

/**
* Returns image buffer X-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CVS14M::GetImageWidth() const
{
	return img_.Width();
}

/**
* Returns image buffer Y-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CVS14M::GetImageHeight() const
{
	return img_.Height();
}

/**
* Returns image buffer pixel depth in bytes.
* Required by the MM::Camera API.
*/
unsigned CVS14M::GetImageBytesPerPixel() const
{
	return img_.Depth();
} 

/**
* Returns the bit depth (dynamic range) of the pixel.
* This does not affect the buffer size, it just gives the client application
* a guideline on how to interpret pixel values.
* Required by the MM::Camera API.
*/
unsigned CVS14M::GetBitDepth() const
{
	return bitDepth_;
}

/**
* Returns the size in bytes of the image buffer.
* Required by the MM::Camera API.
*/
long CVS14M::GetImageBufferSize() const
{
	return img_.Width() * img_.Height() * GetImageBytesPerPixel();
}

/**
* Sets the camera Region Of Interest.
* Required by the MM::Camera API.
* This command will change the dimensions of the image.
* Depending on the hardware capabilities the camera may not be able to configure the
* exact dimensions requested - but should try do as close as possible.
* If the hardware does not have this capability the software should simulate the ROI by
* appropriately cropping each frame.
* This demo implementation ignores the position coordinates and just crops the buffer.
* @param x - top-left corner coordinate
* @param y - top-left corner coordinate
* @param xSize - width
* @param ySize - height
*/
int CVS14M::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
	if (xSize == 0 && ySize == 0)
	{
		// effectively clear ROI
		ArtemisSubframePos(hCam_, 0, 0);
		ArtemisSubframeSize(hCam_, cameraCCDXSize_, cameraCCDYSize_);
		roiW_ = cameraCCDXSize_/binSizeX_;
		roiH_ = cameraCCDYSize_/binSizeY_;
		ResizeImageBuffer();
		roiX_ = 0;
		roiY_ = 0;
	}
	else
	{
		// apply ROI
		ArtemisSubframePos(hCam_, x*binSizeX_, y*binSizeY_);
		ArtemisSubframeSize(hCam_, xSize*binSizeX_, ySize*binSizeY_);
		//ResizeImageBuffer();

		//img_.Resize(xSize, ySize);
		roiW_ = xSize;
		roiH_ = ySize;
		roiX_ = x;
		roiY_ = y;
		ResizeImageBuffer();
	}
	return DEVICE_OK;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
int CVS14M::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
	//Assume that ROI will not be externally modified, so don't need to talk to hardware
	//(via ArtemisGetSubframe) repeatedly. 
	//ArtemisGetSubframe(hCam_, x, y, xSize, ySize);
	x = roiX_;
	y = roiY_;
	xSize = img_.Width();
	ySize = img_.Height();

	img_.Resize(xSize,ySize);
	//roiX_ = x;
	//roiY_ = y;

	return DEVICE_OK;
}

/**
* Resets the Region of Interest to full frame.
* Required by the MM::Camera API.
*/
int CVS14M::ClearROI()
{
	ArtemisSubframePos(hCam_, 0, 0);
	ArtemisSubframeSize(hCam_, cameraCCDXSize_, cameraCCDYSize_);
	roiW_ = cameraCCDXSize_/binSizeX_;
	roiH_ = cameraCCDYSize_/binSizeY_;
	ResizeImageBuffer();
	roiX_ = 0;
	roiY_ = 0;

	return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double CVS14M::GetExposure() const
{
	char buf[MM::MaxStrLength];
	int ret = GetProperty(MM::g_Keyword_Exposure, buf);
	if (ret != DEVICE_OK)
		return 0.0;
	return atof(buf);
}

/**
* Returns the current exposure from a sequence and increases the sequence counter
* Used for exposure sequences
*/
double CVS14M::GetSequenceExposure() 
{
	if (exposureSequence_.size() == 0) 
		return this->GetExposure();

	double exposure = exposureSequence_[sequenceIndex_];

	sequenceIndex_++;
	if (sequenceIndex_ >= exposureSequence_.size())
		sequenceIndex_ = 0;

	return exposure;
}

/**
* Sets exposure in milliseconds.
* Required by the MM::Camera API.
*/
void CVS14M::SetExposure(double exp)
{

	SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp));
	GetCoreCallback()->OnExposureChanged(this, exp);
	if (overlapExposure_)
		ArtemisSetOverlappedExposureTime(hCam_, (float) exp/1000);
}

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int CVS14M::GetBinning() const
{
	//Again, assume that binning cannot be changed externally so there will 
	//be no clash between hardware and software, and avoid calling 
	//ArtemisGetBin(., ., .) every time...
	
	//char buf[MM::MaxStrLength];
	//int x = -1;
	//int y = -1;
	//ArtemisGetBin(hCam_, &x, &y);
	//int ret = GetProperty(MM::g_Keyword_Binning, buf);
	//if (ret != DEVICE_OK)
	//	return 1;
	//if (atoi(buf) != x){
	//	LogMessage("Current camera binning doesn't match current property value", false);
	//	return 1;
	//}
	//return x;

	char buf[MM::MaxStrLength];
	int ret = GetProperty(MM::g_Keyword_Binning, buf);
	if (ret != DEVICE_OK)
		return 1;
	return atoi(buf);
}

/**
* Sets binning factor.
* Required by the MM::Camera API.
* N.B. since this method is inherited, can only take one argument...
*/
int CVS14M::SetBinning(int binFX)
{
	//int dummy = ArtemisGetProcessing(hCam_);
	ArtemisBin(hCam_, binFX, binSizeY_);

	return DEVICE_OK;
}

int CVS14M::IsExposureSequenceable(bool& isSequenceable) const
{
	isSequenceable = isSequenceable_;
	return DEVICE_OK;
}

int CVS14M::GetExposureSequenceMaxLength(long& nrEvents) const
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	nrEvents = sequenceMaxLength_;
	return DEVICE_OK;
}

int CVS14M::StartExposureSequence()
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	// may need thread lock
	sequenceRunning_ = true;
	return DEVICE_OK;
}

int CVS14M::StopExposureSequence()
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	// may need thread lock
	sequenceRunning_ = false;
	sequenceIndex_ = 0;
	return DEVICE_OK;
}

/**
* Clears the list of exposures used in sequences
*/
int CVS14M::ClearExposureSequence()
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	exposureSequence_.clear();
	return DEVICE_OK;
}

/**
* Adds an exposure to a list of exposures used in sequences
*/
int CVS14M::AddToExposureSequence(double exposureTime_ms) 
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	exposureSequence_.push_back(exposureTime_ms);
	return DEVICE_OK;
}

int CVS14M::SendExposureSequence() const {
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	return DEVICE_OK;
}

int CVS14M::SetAllowedBinning() 
{
	//Note that binning > 8 seems to break things... 
	//For now, only allow binning in case in which integer number of pixels falls out directly. 
	//For more complicated cases, need to invoke subframe?
	vector<string> binValues;
	int x = 1;
	int y = 1;
	ArtemisGetMaxBin(hCam_, &x, &y);

	//for (int i = 0; i < 8; ++i){
	for (int i = 0; i < x; ++i){
		if ( ( fmod((double) cameraCCDXSize_, (double) (i+1)) == 0 ) && ( fmod((double) cameraCCDYSize_, (double) (i+1)) == 0) )
			binValues.push_back(""+boost::lexical_cast<std::string>(i + 1));
	}
	
	LogMessage("Setting Allowed Binning settings", false);
	return SetAllowedValues(MM::g_Keyword_Binning, binValues);
}

int CVS14M::SetAllowedYBinning() 
{
	//For now, only allow binning in case in which integer number of pixels falls out directly. 
	//For more complicated cases, need to invoke subframe?
	vector<string> binValues;

	for (int i = 0; i < cameraCCDYSize_; ++i){
		if  ( fmod((double) cameraCCDYSize_, (double) (i+1)) == 0 ) 
			binValues.push_back(""+boost::lexical_cast<std::string>(i + 1));
	}
	
	LogMessage("Setting Allowed Binning settings", false);
	return SetAllowedValues("YBinning", binValues);
}

/**
* Required by the MM::Camera API
* Please implement this yourself and do not rely on the base class implementation
* The Base class implementation is deprecated and will be removed shortly
*/
int CVS14M::StartSequenceAcquisition(double interval)
{
	return StartSequenceAcquisition(LONG_MAX, interval, false);            
}

/**                                                                       
* Stop and wait for the Sequence thread finished                                   
*/                                                                        
int CVS14M::StopSequenceAcquisition()                                     
{
	if (!thd_->IsStopped()) {
		thd_->Stop();                                                       
		thd_->wait();                                                       
	}                                                                      

	return DEVICE_OK;                                                      
} 

/**
* Simple implementation of Sequence Acquisition
* A sequence acquisition should run on its own thread and transport new images
* coming of the camera into the MMCore circular buffer.
*/
int CVS14M::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	if (IsCapturing())
		return DEVICE_CAMERA_BUSY_ACQUIRING;

	int ret = GetCoreCallback()->PrepareForAcq(this);
	if (ret != DEVICE_OK)
		return ret;
	sequenceStartTime_ = GetCurrentMMTime();
	imageCounter_ = 0;
	thd_->Start(numImages,interval_ms);
	stopOnOverflow_ = stopOnOverflow;
	return DEVICE_OK;
}

/*
* Inserts Image and MetaData into MMCore circular Buffer
*/
int CVS14M::InsertImage()
{
	MM::MMTime timeStamp = this->GetCurrentMMTime();
	char label[MM::MaxStrLength];
	this->GetLabel(label);

	// Important:  metadata about the image are generated here:
	Metadata md;
	md.put("Camera", label);
	md.put(MM::g_Keyword_Metadata_StartTime, CDeviceUtils::ConvertToString(sequenceStartTime_.getMsec()));
	md.put(MM::g_Keyword_Elapsed_Time_ms, CDeviceUtils::ConvertToString((timeStamp - sequenceStartTime_).getMsec()));
	md.put(MM::g_Keyword_Metadata_ROI_X, CDeviceUtils::ConvertToString( (long) roiX_)); 
	md.put(MM::g_Keyword_Metadata_ROI_Y, CDeviceUtils::ConvertToString( (long) roiY_)); 

	imageCounter_++;

	char buf[MM::MaxStrLength];
	GetProperty(MM::g_Keyword_Binning, buf);
	md.put(MM::g_Keyword_Binning, buf);

	MMThreadGuard g(imgPixelsLock_);

	const unsigned char* pI;
	pI = GetImageBuffer();

	unsigned int w = GetImageWidth();
	unsigned int h = GetImageHeight();
	unsigned int b = GetImageBytesPerPixel();

	int ret = GetCoreCallback()->InsertImage(this, pI, w, h, b, md.Serialize().c_str());
	if (!stopOnOverflow_ && ret == DEVICE_BUFFER_OVERFLOW)
	{
		// do not stop on overflow - just reset the buffer
		GetCoreCallback()->ClearImageBuffer(this);
		// don't process this same image again...
		return GetCoreCallback()->InsertImage(this, pI, w, h, b, md.Serialize().c_str(), false);
	} else
		return ret;
}

/*
* Do actual capturing
* Called from inside the thread  
*/
int CVS14M::RunSequenceOnThread(MM::MMTime startTime)
{
	int ret=DEVICE_ERR;

	double exp = GetExposure();
	float exp_seconds = ((float) exp)/1000;		//seems cumbersome to do every time...

	// Trigger
	if (triggerDevice_.length() > 0) {
		MM::Device* triggerDev = GetDevice(triggerDevice_.c_str());
		if (triggerDev != 0) {
			LogMessage("trigger requested");
			
			std::string dummy = triggerDevice_;
			triggerDev->SetProperty("Trigger","+");
		}
	//send trigger signal
	}
	else
		ArtemisStartExposure(hCam_, exp_seconds);
	

	ret = InsertImage();


	while (((double) (this->GetCurrentMMTime() - startTime).getMsec() / imageCounter_) < this->GetSequenceExposure())
	{
		CDeviceUtils::SleepMs(1);
	}

	if (ret != DEVICE_OK)
	{
		return ret;
	}
	return ret;
};

bool CVS14M::IsCapturing() {
	return !thd_->IsStopped();
}

/*
* called from the thread function before exit 
*/
void CVS14M::OnThreadExiting() throw()
{
	try
	{
		LogMessage(g_Msg_SEQUENCE_ACQUISITION_THREAD_EXITING);
		GetCoreCallback()?GetCoreCallback()->AcqFinished(this,0):DEVICE_OK;
	}
	catch(...)
	{
		LogMessage(g_Msg_EXCEPTION_IN_ON_THREAD_EXITING, false);
	}
}

MySequenceThread::MySequenceThread(CVS14M* pCam)
	:intervalMs_(default_intervalMS)
	,numImages_(default_numImages)
	,imageCounter_(0)
	,stop_(true)
	,suspend_(false)
	,camera_(pCam)
	,startTime_(0)
	,actualDuration_(0)
	,lastFrameTime_(0)
{};

MySequenceThread::~MySequenceThread() {};

void MySequenceThread::Stop() {
	MMThreadGuard g(this->stopLock_);
	stop_=true;
}

void MySequenceThread::Start(long numImages, double intervalMs)
{
	MMThreadGuard g1(this->stopLock_);
	MMThreadGuard g2(this->suspendLock_);
	numImages_=numImages;
	intervalMs_=intervalMs;
	imageCounter_=0;
	stop_ = false;
	suspend_=false;
	activate();
	actualDuration_ = 0;
	startTime_= camera_->GetCurrentMMTime();
	lastFrameTime_ = 0;
}

bool MySequenceThread::IsStopped(){
	MMThreadGuard g(this->stopLock_);
	return stop_;
}

void MySequenceThread::Suspend() {
	MMThreadGuard g(this->suspendLock_);
	suspend_ = true;
}

bool MySequenceThread::IsSuspended() {
	MMThreadGuard g(this->suspendLock_);
	return suspend_;
}

void MySequenceThread::Resume() {
	MMThreadGuard g(this->suspendLock_);
	suspend_ = false;
}

int MySequenceThread::svc(void) throw()
{
	int ret=DEVICE_ERR;
	try 
	{
		do
		{  
			ret = camera_->RunSequenceOnThread(startTime_);
		} while (DEVICE_OK == ret && !IsStopped() && imageCounter_++ < numImages_-1);
		if (IsStopped())
			camera_->LogMessage("SeqAcquisition interrupted by the user\n");
	}catch(...){
		camera_->LogMessage(g_Msg_EXCEPTION_IN_THREAD, false);
	}
	stop_=true;
	actualDuration_ = camera_->GetCurrentMMTime() - startTime_;
	camera_->OnThreadExiting();
	return ret;
}


///////////////////////////////////////////////////////////////////////////////
// CVS14M Action handlers
///////////////////////////////////////////////////////////////////////////////

int CVS14M::OnMaxExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(exposureMaximum_);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(exposureMaximum_);
	}
	return DEVICE_OK;
}

/**
* Handles "Binning" property.
*/
int CVS14M::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	int ret = DEVICE_ERR;
	switch(eAct)
	{
	case MM::AfterSet:
		{
			if(IsCapturing())
				return DEVICE_CAMERA_BUSY_ACQUIRING;

			// the user just set the new value for the property, so we have to
			// apply this value to the 'hardware'.
			long binFactor;
			pProp->Get(binFactor);
			char val[MM::MaxStrLength];
			int no_vals = GetNumberOfPropertyValues(MM::g_Keyword_Binning);
			std::vector<int> binning_vec;
			for (int i = 0; i < no_vals; ++i){
				GetPropertyValueAt(MM::g_Keyword_Binning, i, val);
				binning_vec.push_back(atoi(val));
			}

			if(binFactor > 0 && binFactor < (*max_element(binning_vec.begin(), binning_vec.end()) + 1))
			{
				int oldbinX;
				oldbinX = binSizeX_;
				binSizeX_ = binFactor;

				if (!asymmBinning_)
					binSizeY_ = binSizeX_;

				//ClearROI();
				//N.B. since SetBinning is inherited, can only take one argument...
				SetBinning(binSizeX_);	//Better getting properties than using globals - probably OK for now...
				
				SetROI(oldbinX*roiX_/binSizeX_, oldbinX*roiY_/binSizeY_, oldbinX*roiW_/binSizeX_, oldbinX*roiH_/binSizeY_);

				std::ostringstream os;
				os << binSizeX_;
				OnPropertyChanged("Binning", os.str().c_str());
				ret=DEVICE_OK;
			}
		}break;
	case MM::BeforeGet:
		{
			ret=DEVICE_OK;
			pProp->Set(binSizeX_);
		}break;
	default:
		break;
	}
	return ret; 
}

/**
* Handles "PixelType" property.
*/
int CVS14M::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	int ret = DEVICE_ERR;
	switch(eAct)
	{
	case MM::AfterSet:
		{
			if(IsCapturing())
				return DEVICE_CAMERA_BUSY_ACQUIRING;

			string pixelType;
			pProp->Get(pixelType);

			if (pixelType.compare(g_PixelType_8bit) == 0)
			{
				nComponents_ = 1;
				img_.Resize(img_.Width(), img_.Height(), 1);
				bitDepth_ = 8;
				ret=DEVICE_OK;
			}
			else if (pixelType.compare(g_PixelType_16bit) == 0)
			{
				nComponents_ = 1;
				img_.Resize(img_.Width(), img_.Height(), 2);
				ret=DEVICE_OK;
			}
			else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
			{
				nComponents_ = 4;
				img_.Resize(img_.Width(), img_.Height(), 4);
				ret=DEVICE_OK;
			}
			else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
			{
				nComponents_ = 4;
				img_.Resize(img_.Width(), img_.Height(), 8);
				ret=DEVICE_OK;
			}
			else if ( pixelType.compare(g_PixelType_32bit) == 0)
			{
				nComponents_ = 1;
				img_.Resize(img_.Width(), img_.Height(), 4);
				ret=DEVICE_OK;
			}
			else
			{
				// on error switch to default pixel type
				nComponents_ = 1;
				img_.Resize(img_.Width(), img_.Height(), 1);
				pProp->Set(g_PixelType_16bit);
				ret = ERR_UNKNOWN_MODE;
			}
		} break;
	case MM::BeforeGet:
		{
			long bytesPerPixel = GetImageBytesPerPixel();
			if (bytesPerPixel == 1)
				pProp->Set(g_PixelType_8bit);
			else if (bytesPerPixel == 2)
				pProp->Set(g_PixelType_16bit);
			else if (bytesPerPixel == 4)
			{
				if(4 == this->nComponents_) // todo SEPARATE bitdepth from #components
					pProp->Set(g_PixelType_32bitRGB);
				else if( 1 == nComponents_)
					pProp->Set(::g_PixelType_32bit);
			}
			else if (bytesPerPixel == 8) // todo SEPARATE bitdepth from #components
				pProp->Set(g_PixelType_64bitRGB);
			else
				pProp->Set(g_PixelType_16bit);
			ret=DEVICE_OK;
		} break;
	default:
		break;
	}
	return ret; 
}

/**
* Handles "BitDepth" property.
*/
int CVS14M::OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	int ret = DEVICE_ERR;
	switch(eAct)
	{
	case MM::AfterSet:
		{
			if(IsCapturing())
				return DEVICE_CAMERA_BUSY_ACQUIRING;

			long bitDepth;
			pProp->Get(bitDepth);

			unsigned int bytesPerComponent;

			switch (bitDepth) {
			case 8:
				bytesPerComponent = 1;
				bitDepth_ = 8;
				ret=DEVICE_OK;
				break;
			case 10:
				bytesPerComponent = 2;
				bitDepth_ = 10;
				ret=DEVICE_OK;
				break;
			case 12:
				bytesPerComponent = 2;
				bitDepth_ = 12;
				ret=DEVICE_OK;
				break;
			case 14:
				bytesPerComponent = 2;
				bitDepth_ = 14;
				ret=DEVICE_OK;
				break;
			case 16:
				bytesPerComponent = 2;
				bitDepth_ = 16;
				ret=DEVICE_OK;
				break;
			case 32:
				bytesPerComponent = 4;
				bitDepth_ = 32; 
				ret=DEVICE_OK;
				break;
			default: 
				// on error switch to default pixel type
				bytesPerComponent = 1;

				pProp->Set((long)8);
				bitDepth_ = 8;
				ret = ERR_UNKNOWN_MODE;
				break;
			}
			char buf[MM::MaxStrLength];
			GetProperty(MM::g_Keyword_PixelType, buf);
			std::string pixelType(buf);
			unsigned int bytesPerPixel = 1;


			// automagickally change pixel type when bit depth exceeds possible value
			if (pixelType.compare(g_PixelType_8bit) == 0)
			{
				if( 2 == bytesPerComponent)
				{
					SetProperty(MM::g_Keyword_PixelType, g_PixelType_16bit);
					bytesPerPixel = 2;
				}
				else if ( 4 == bytesPerComponent)
				{
					SetProperty(MM::g_Keyword_PixelType, g_PixelType_32bit);
					bytesPerPixel = 4;

				}else
				{
					bytesPerPixel = 1;
				}
			}
			else if (pixelType.compare(g_PixelType_16bit) == 0)
			{
				bytesPerPixel = 2;
			}
			else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
			{
				bytesPerPixel = 4;
			}
			else if ( pixelType.compare(g_PixelType_32bit) == 0)
			{
				bytesPerPixel = 4;
			}
			else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
			{
				bytesPerPixel = 8;
			}
			img_.Resize(img_.Width(), img_.Height(), bytesPerPixel);

		} break;
	case MM::BeforeGet:
		{
			pProp->Set((long)bitDepth_);
			ret=DEVICE_OK;
		} break;
	default:
		break;
	}
	return ret; 
}

int CVS14M::OnCameraCCDXSize(MM::PropertyBase* pProp , MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(cameraCCDXSize_);
	}
	else if (eAct == MM::AfterSet)
	{
		long value;
		pProp->Get(value);
		if ( (value < 16) || (33000 < value))
			return DEVICE_ERR;  // invalid image size
		if( value != cameraCCDXSize_)
		{
			cameraCCDXSize_ = value;
			img_.Resize(cameraCCDXSize_/binSizeX_, cameraCCDYSize_/binSizeY_);
		}
	}
	return DEVICE_OK;

}

int CVS14M::OnCameraCCDYSize(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(cameraCCDYSize_);
	}
	else if (eAct == MM::AfterSet)
	{
		long value;
		pProp->Get(value);
		if ( (value < 16) || (33000 < value))
			return DEVICE_ERR;  // invalid image size
		if( value != cameraCCDYSize_)
		{
			cameraCCDYSize_ = value;
			img_.Resize(cameraCCDXSize_/binSizeX_, cameraCCDYSize_/binSizeY_);
		}
	}
	return DEVICE_OK;

}

int CVS14M::OnTriggerDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(triggerDevice_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(triggerDevice_);
		if (triggerDevice_.length() == 0)
			int err = ArtemisTriggeredExposure(hCam_, false);
	}
	return DEVICE_OK;
}

int CVS14M::OnCCDTemp(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{

		GetCurrentTemperature();
		pProp->Set(ccdT_);

	}
	else if (eAct == MM::AfterSet)
	{
		GetCurrentTemperature();
		TemperatureContol();
		//if (ccdT_ < ambientTemp_){
		//	
		//	if (ccdT_ < (ambientTemp_ - 35))
		//		ArtemisSetCooling(hCam_, (ambientTemp_ - 35)*100);	//Setpoint is oC*100
		//	else
		//		ArtemisSetCooling(hCam_, 100*ccdT_);

		//}

		//else
		//	ArtemisCoolerWarmUp(hCam_);
		
		pProp->Get(ccdT_);
	}
	return DEVICE_OK;
}

int CVS14M::OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string val = "Yes";
	if (eAct == MM::BeforeGet)
	{
		if (!isSequenceable_) 
		{
			val = "No";
		}
		pProp->Set(val.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		isSequenceable_ = false;
		pProp->Get(val);
		if (val == "Yes") 
		{
			isSequenceable_ = true;
		}
	}

	return DEVICE_OK;
}

int CVS14M::OnFlipUD(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
		flipUD_ = (0==tvalue)?false:true;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(flipUD_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnFlipLR(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
		flipLR_ = (0==tvalue)?false:true;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(flipLR_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnRotate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
    long angle;
    pProp->Get(angle);
        imageRotationAngle_ = angle;
        SetProperty("FlipImageLR","0");
        SetProperty("FlipImageUD","0");
        ResizeImageBuffer();
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long) imageRotationAngle_);
   }

   return DEVICE_OK;
}

//int CVS14M::OnSubframeX(MM::PropertyBase* pProp, MM::ActionType eAct){
//	
//	double x;
//
//	if (eAct == MM::BeforeGet)
//	{
//		x = roiX_;
//		pProp->Set(x);
//	}
//	else if (eAct == MM::AfterSet)
//	{
//		pProp->Get(x);
//		roiX_ = roundUp(x, binSizeX_);
//		
//		// DO NOT change value of width based on x position unless greater than property 
//		// limit (automatically handled) - means that changing x shifts ROI, rather than 
//		// stretching. Seems sensible, but can be modified to essentially make W a 
//		// dependent variable and allow user control of position of lower right hand
//		// corner. 
//
//		SetPropertyLimits("FrameXWidth", binSizeX_, (cameraCCDXSize_ - roiX_));
//		if ((cameraCCDXSize_ - roiX_) < roiW_)
//			roiW_ = (cameraCCDXSize_ - roiX_);
//		SetROI(roiX_, roiY_, roiW_, roiH_);
//	}
//
//	return DEVICE_OK;
//}
//
//int CVS14M::OnSubframeY(MM::PropertyBase* pProp, MM::ActionType eAct){
//	
//	double y;
//
//	if (eAct == MM::BeforeGet)
//	{
//		y = roiY_;
//		pProp->Set(y);
//	}
//	else if (eAct == MM::AfterSet)
//	{
//		pProp->Get(y);
//		roiY_ = roundUp(y, (int) binSizeY_);
//		
//		// DO NOT change value of width based on y position unless greater than property 
//		// limit (automatically handled) - means that changing y shifts ROI, rather than 
//		// stretching. Seems sensible, but can be modified to essentially make H a 
//		// dependent variable and allow user control of position of lower right hand
//		// corner. 
//
//		SetPropertyLimits("FrameYHeight", binSizeY_, (cameraCCDYSize_ - roiY_));
//		if ((cameraCCDYSize_ - roiY_) < roiH_)
//			roiH_ = (cameraCCDYSize_ - roiY_);
//		SetROI(roiX_, roiY_, roiW_, roiH_);
//	}
//
//	return DEVICE_OK;
//}
//
//int CVS14M::OnSubframeW(MM::PropertyBase* pProp, MM::ActionType eAct){
//	
//	double w;
//
//	if (eAct == MM::BeforeGet)
//	{
//		w = roiW_;
//		pProp->Set(w);
//	}
//	else if (eAct == MM::AfterSet)
//	{
//		pProp->Get(w);
//		roiW_ = roundUp(w, (int) binSizeX_);
//		SetROI(roiX_, roiY_, roiW_, roiH_);
//	}
//
//	return DEVICE_OK;
//}
//
//int CVS14M::OnSubframeH(MM::PropertyBase* pProp, MM::ActionType eAct){
//	
//	double h;
//
//	if (eAct == MM::BeforeGet)
//	{
//		h = roiH_;
//		pProp->Set(h);
//	}
//	else if (eAct == MM::AfterSet)
//	{
//		pProp->Get(h);
//		roiH_ = roundUp(h, (int) binSizeY_);
//		SetROI(roiX_, roiY_, roiW_, roiH_);
//	}
//
//	return DEVICE_OK;
//}

int CVS14M::OnPriority(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
		highPriority_ = (0==tvalue)?false:true;
		setPriority(highPriority_);
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(highPriority_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnAsymmBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
		asymmBinning_ = (0==tvalue)?false:true;
   
		toggleAsymmBinning();
 
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(asymmBinning_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnYBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	int ret = DEVICE_ERR;
	switch(eAct)
	{
	case MM::AfterSet:
		{
			if(IsCapturing())
				return DEVICE_CAMERA_BUSY_ACQUIRING;

			// the user just set the new value for the property, so we have to
			// apply this value to the 'hardware'.
			//ONLY APPLY CHANGE IF ASYMMETRICAL BINNING IS TOGGLED ON, ELSE REVERT TO SAME VAL AS XBIN
			if (asymmBinning_){
				long binFactor;
				pProp->Get(binFactor);
				char val[MM::MaxStrLength];
				int no_vals = GetNumberOfPropertyValues("YBinning");
				std::vector<int> binning_vec;
				for (int i = 0; i < no_vals; ++i){
					GetPropertyValueAt("YBinning", i, val);
					binning_vec.push_back(atoi(val));
				}

				if(binFactor > 0 && binFactor < (*max_element(binning_vec.begin(), binning_vec.end()) + 1))
				{
					binSizeY_ = binFactor;

					//N.B. since SetBinning is inherited, can only take one argument...
					SetBinning(binSizeX_);	//Better getting properties than using globals? - probably OK for now...
					img_.Resize(roiW_/binSizeX_, roiH_/binSizeY_);
					//binSizeX_ = binFactor;
					std::ostringstream os;
					os << binSizeX_;
					//OnPropertyChanged("Binning", os.str().c_str());
					ret=DEVICE_OK;
				}
			}
			else
			{
				binSizeY_ = binSizeX_;
				ret = DEVICE_OK;
			}
		}break;
	case MM::BeforeGet:
		{
			ret=DEVICE_OK;
			pProp->Set(binSizeY_);
		}break;
	default:
		break;
	}

	return ret; 
}

int CVS14M::OnPrecharge(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	
	std::string val = "None";
	if (eAct == MM::BeforeGet)
	{
		switch (prechargeMode_)
		{
		case PRECHARGE_NONE:
			val = "None";
			break;
		case PRECHARGE_ICPS:
			val = "On camera";
			break;
		//case PRECHARGE_FULL:
		//	val = "In software";
		//	break;
		default:
			val = "None";
			break;
		}
	 
		pProp->Set(val.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		prechargeMode_ = PRECHARGE_NONE;
		pProp->Get(val);
		if (val == "None") 
		{
			prechargeMode_ = PRECHARGE_NONE;
		}
		else if (val == "On camera")
		{
			prechargeMode_ = PRECHARGE_ICPS;
		}
		else if (val == "In software")
		{
			prechargeMode_ = PRECHARGE_FULL;
		}

		int camErr = setPrechargeMode(prechargeMode_);
		if (camErr > 0)
			return camErr;

	}

	return DEVICE_OK;
}

int CVS14M::OnArtemisLinearise(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
	
		processLinearise_ = (0==tvalue)?false:true;
   
		setArtemisProcessing(processLinearise_, processVBE_);
 
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(processLinearise_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnArtemisVenetian(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
	
		processVBE_ = (0==tvalue)?false:true;
   
		setArtemisProcessing(processLinearise_, processVBE_);
 
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(processVBE_?1L:0L);
   }

   return DEVICE_OK;
}

int CVS14M::OnOverlappedExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
	
		overlapExposure_ = (0==tvalue)?false:true;
   
		ArtemisSetContinuousExposingMode(hCam_, overlapExposure_);
		double exp = GetExposure();
		SetExposure(exp);
 
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(overlapExposure_?1L:0L);
   }

   return DEVICE_OK;

}

int CVS14M::OnPreviewMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
	
		previewMode_ = (0==tvalue)?false:true;
   
		ArtemisSetPreview(hCam_, previewMode_);
 
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(previewMode_?1L:0L);
   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Private CVS14M methods
///////////////////////////////////////////////////////////////////////////////

/**
* Sync internal image buffer size to the chosen property values.
*/
int CVS14M::ResizeImageBuffer()
{
	char buf[MM::MaxStrLength];
	//int ret = GetProperty(MM::g_Keyword_Binning, buf);
	//if (ret != DEVICE_OK)
	//   return ret;
	//binSizeX_ = atol(buf);

	int ret = GetProperty(MM::g_Keyword_PixelType, buf);
	if (ret != DEVICE_OK)
		return ret;

	std::string pixelType(buf);
	int byteDepth = 0;

	if (pixelType.compare(g_PixelType_8bit) == 0)
	{
		byteDepth = 1;
	}
	else if (pixelType.compare(g_PixelType_16bit) == 0)
	{
		byteDepth = 2;
	}
	else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
	{
		byteDepth = 4;
	}
	else if ( pixelType.compare(g_PixelType_32bit) == 0)
	{
		byteDepth = 4;
	}
	else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
	{
		byteDepth = 8;
	}

    if ((imageRotationAngle_ == 90) || (imageRotationAngle_ == 270))
        img_.Resize(cameraCCDYSize_/binSizeY_, cameraCCDXSize_/binSizeX_, byteDepth);
    else
		//img_.Resize(roiW_/binSizeX_, roiH_/binSizeY_, byteDepth);
		img_.Resize(roiW_, roiH_, byteDepth);


	return DEVICE_OK;
}

void CVS14M::GenerateEmptyImage(ImgBuffer& img)
{
	MMThreadGuard g(imgPixelsLock_);
	if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
		return;
	unsigned char* pBuf = const_cast<unsigned char*>(img.GetPixels());
	memset(pBuf, 0, img.Height()*img.Width()*img.Depth());
}

void CVS14M::TestResourceLocking(const bool recurse)
{
	if(recurse)
		TestResourceLocking(false);
}

void CVS14M::rotate90(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr){

	int xsize = original_ysize;
	int ysize = original_xsize;	//swapped here for clairty...
	for (int y = 0; y < ysize; ++y){
		for (int x = 0, destx = xsize-1; x<xsize; ++x, --destx){
			out_arr[y * xsize + destx] = in_arr[x * ysize + y];
		}
	}
}

void CVS14M::rotate180(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr){

	for (int i = 0; i < original_xsize*original_ysize; ++i){
		out_arr[original_xsize*original_ysize - 1 -i] = in_arr[i];
	}
}

void CVS14M::rotate270(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr){

	int xsize = original_ysize;
	int ysize = original_xsize;	//swapped here for clairty...
	for (int x = 0; x < xsize; ++x){
		for (int y = 0, desty = ysize-1; y<ysize; ++y, --desty){
			out_arr[x + desty*xsize] = in_arr[x * ysize + y];
		}
	}
}

void CVS14M::mirrorY(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr){

	for (int x = 0; x < original_xsize; ++x){
		for (int y = 0, desty = original_ysize - 1; y < original_ysize; ++y, --desty){
			out_arr[desty * original_xsize + x] = in_arr[y * original_xsize + x];
		}
	}
}

void CVS14M::mirrorX(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr){

	for (int y = 0; y < original_ysize; ++y){
		for (int x = 0, destx = original_xsize - 1; x < original_xsize; ++x, --destx){
			out_arr[y * original_xsize + destx] = in_arr[y * original_xsize + x];
		}
	}
}


int TransposeProcessor::Initialize()
{


	if( NULL != this->pTemp_)
	{
		free(pTemp_);
		pTemp_ = NULL;
		this->tempSize_ = 0;
	}
	CPropertyAction* pAct = new CPropertyAction (this, &TransposeProcessor::OnInPlaceAlgorithm);
	(void)CreateIntegerProperty("InPlaceAlgorithm", 0, false, pAct);
	return DEVICE_OK;
}

// action interface
// ----------------
int TransposeProcessor::OnInPlaceAlgorithm(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(this->inPlace_?1L:0L);
	}
	else if (eAct == MM::AfterSet)
	{
		long ltmp;
		pProp->Get(ltmp);
		inPlace_ = (0==ltmp?false:true);
	}

	return DEVICE_OK;
}


int TransposeProcessor::Process(unsigned char *pBuffer, unsigned int width, unsigned int height, unsigned int byteDepth)
{
	int ret = DEVICE_OK;
	// 
	if( width != height)
		return DEVICE_NOT_SUPPORTED; // problem with tranposing non-square images is that the image buffer
	// will need to be modified by the image processor.
	if(busy_)
		return DEVICE_ERR;

	busy_ = true;

	if( inPlace_)
	{
		if(  sizeof(unsigned char) == byteDepth)
		{
			TransposeSquareInPlace( (unsigned char*)pBuffer, width);
		}
		else if( sizeof(unsigned short) == byteDepth)
		{
			TransposeSquareInPlace( (unsigned short*)pBuffer, width);
		}
		else if( sizeof(unsigned long) == byteDepth)
		{
			TransposeSquareInPlace( (unsigned long*)pBuffer, width);
		}
		else if( sizeof(unsigned long long) == byteDepth)
		{
			TransposeSquareInPlace( (unsigned long long*)pBuffer, width);
		}
		else 
		{
			ret = DEVICE_NOT_SUPPORTED;
		}
	}
	else
	{
		if( sizeof(unsigned char) == byteDepth)
		{
			ret = TransposeRectangleOutOfPlace( (unsigned char*)pBuffer, width, height);
		}
		else if( sizeof(unsigned short) == byteDepth)
		{
			ret = TransposeRectangleOutOfPlace( (unsigned short*)pBuffer, width, height);
		}
		else if( sizeof(unsigned long) == byteDepth)
		{
			ret = TransposeRectangleOutOfPlace( (unsigned long*)pBuffer, width, height);
		}
		else if( sizeof(unsigned long long) == byteDepth)
		{
			ret =  TransposeRectangleOutOfPlace( (unsigned long long*)pBuffer, width, height);
		}
		else
		{
			ret =  DEVICE_NOT_SUPPORTED;
		}
	}
	busy_ = false;

	return ret;
}




int ImageFlipY::Initialize()
{
	CPropertyAction* pAct = new CPropertyAction (this, &ImageFlipY::OnPerformanceTiming);
	(void)CreateFloatProperty("PeformanceTiming (microseconds)", 0, true, pAct);
	return DEVICE_OK;
}

// action interface
// ----------------
int ImageFlipY::OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct)
{

	if (eAct == MM::BeforeGet)
	{
		pProp->Set( performanceTiming_.getUsec());
	}
	else if (eAct == MM::AfterSet)
	{
		// -- it's ready only!
	}

	return DEVICE_OK;
}


int ImageFlipY::Process(unsigned char *pBuffer, unsigned int width, unsigned int height, unsigned int byteDepth)
{
	if(busy_)
		return DEVICE_ERR;

	int ret = DEVICE_OK;

	busy_ = true;
	performanceTiming_ = MM::MMTime(0.);
	MM::MMTime  s0 = GetCurrentMMTime();


	if( sizeof(unsigned char) == byteDepth)
	{
		ret = Flip( (unsigned char*)pBuffer, width, height);
	}
	else if( sizeof(unsigned short) == byteDepth)
	{
		ret = Flip( (unsigned short*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long) == byteDepth)
	{
		ret = Flip( (unsigned long*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long long) == byteDepth)
	{
		ret =  Flip( (unsigned long long*)pBuffer, width, height);
	}
	else
	{
		ret =  DEVICE_NOT_SUPPORTED;
	}

	performanceTiming_ = GetCurrentMMTime() - s0;
	busy_ = false;

	return ret;
}

///
int ImageFlipX::Initialize()
{
	CPropertyAction* pAct = new CPropertyAction (this, &ImageFlipX::OnPerformanceTiming);
	(void)CreateFloatProperty("PeformanceTiming (microseconds)", 0, true, pAct);
	return DEVICE_OK;
}

// action interface
// ----------------
int ImageFlipX::OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct)
{

	if (eAct == MM::BeforeGet)
	{
		pProp->Set( performanceTiming_.getUsec());
	}
	else if (eAct == MM::AfterSet)
	{
		// -- it's ready only!
	}

	return DEVICE_OK;
}


int ImageFlipX::Process(unsigned char *pBuffer, unsigned int width, unsigned int height, unsigned int byteDepth)
{
	if(busy_)
		return DEVICE_ERR;

	int ret = DEVICE_OK;

	busy_ = true;
	performanceTiming_ = MM::MMTime(0.);
	MM::MMTime  s0 = GetCurrentMMTime();


	if( sizeof(unsigned char) == byteDepth)
	{
		ret = Flip( (unsigned char*)pBuffer, width, height);
	}
	else if( sizeof(unsigned short) == byteDepth)
	{
		ret = Flip( (unsigned short*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long) == byteDepth)
	{
		ret = Flip( (unsigned long*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long long) == byteDepth)
	{
		ret =  Flip( (unsigned long long*)pBuffer, width, height);
	}
	else
	{
		ret =  DEVICE_NOT_SUPPORTED;
	}

	performanceTiming_ = GetCurrentMMTime() - s0;
	busy_ = false;

	return ret;
}

///
int MedianFilter::Initialize()
{
	CPropertyAction* pAct = new CPropertyAction (this, &MedianFilter::OnPerformanceTiming);
	(void)CreateFloatProperty("PeformanceTiming (microseconds)", 0, true, pAct);
	(void)CreateStringProperty("BEWARE", "THIS FILTER MODIFIES DATA, EACH PIXEL IS REPLACED BY 3X3 NEIGHBORHOOD MEDIAN", true);
	return DEVICE_OK;
}

// action interface
// ----------------
int MedianFilter::OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct)
{

	if (eAct == MM::BeforeGet)
	{
		pProp->Set( performanceTiming_.getUsec());
	}
	else if (eAct == MM::AfterSet)
	{
		// -- it's ready only!
	}

	return DEVICE_OK;
}


int MedianFilter::Process(unsigned char *pBuffer, unsigned int width, unsigned int height, unsigned int byteDepth)
{
	if(busy_)
		return DEVICE_ERR;

	int ret = DEVICE_OK;

	busy_ = true;
	performanceTiming_ = MM::MMTime(0.);
	MM::MMTime  s0 = GetCurrentMMTime();


	if( sizeof(unsigned char) == byteDepth)
	{
		ret = Filter( (unsigned char*)pBuffer, width, height);
	}
	else if( sizeof(unsigned short) == byteDepth)
	{
		ret = Filter( (unsigned short*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long) == byteDepth)
	{
		ret = Filter( (unsigned long*)pBuffer, width, height);
	}
	else if( sizeof(unsigned long long) == byteDepth)
	{
		ret =  Filter( (unsigned long long*)pBuffer, width, height);
	}
	else
	{
		ret =  DEVICE_NOT_SUPPORTED;
	}

	performanceTiming_ = GetCurrentMMTime() - s0;
	busy_ = false;

	return ret;
}



int CVS14M::GetCurrentTemperature(){

	int dummyTemp = 1;
	int ret = ArtemisTemperatureSensorInfo(hCam_, 1, &dummyTemp);
	currentTemp_ = ((float) dummyTemp)/100;
	if (ret != DEVICE_OK)
		return ret;
	
	ret = SetProperty("CCDTemperature Readout", boost::lexical_cast<std::string>(currentTemp_).c_str());
	LogMessage("Getting current temp", false);

	return DEVICE_OK;
}

int CVS14M::TemperatureContol(){
	
	if (ccdT_ < ambientTemp_){
			
			if (ccdT_ < (ambientTemp_ - 35))
				ArtemisSetCooling(hCam_, (ambientTemp_ - 35)*100);	//Setpoint is oC*100
			else
				ArtemisSetCooling(hCam_, 100*ccdT_);

		}

		else
			ArtemisCoolerWarmUp(hCam_);

	return DEVICE_OK;
}

double CVS14M::roundUp(double numToRound, double toMultipleOf) 
{ 
	if(toMultipleOf == 0) 
	{ 
		return numToRound; 
	} 

	//double remainder = numToRound % toMultipleOf;
	double remainder = fmod(numToRound, toMultipleOf);
	if (remainder == 0)	{
		return numToRound;
	}
	return (numToRound + toMultipleOf - remainder);
}

int CVS14M::setPriority(bool highpriority){

	ArtemisHighPriority(hCam_, highpriority);

	return DEVICE_OK;

}

int CVS14M::findFactors(int input, std::vector<int> factors){
	
	//clear vector
	if (!(factors.size() == 0)){
		factors.clear();
	}

	for (int i = 1; i <= input; ++i){
		if (input % i == 0)
		{
			factors.push_back(i);
		}
	}
	
	return DEVICE_OK;
}

int CVS14M::toggleAsymmBinning()
{
	if (!asymmBinning_)
	{
		binSizeY_ = binSizeX_;
		SetBinning(binSizeX_);	//Better getting properties than using globals - probably OK for now...
		img_.Resize(roiW_/binSizeX_, roiH_/binSizeY_);
	}

	return DEVICE_OK;
}

//TODO: implement hardware error reporting. 
//int CVS14M::reportCamErr(int err){
//	return DEVICE_OK;
//}

bool CVS14M::isCoolingPresent(ArtemisHandle hCam)
{
	int flags;
	int level;
	int minlvl;
	int maxlvl;
	int setpoint;
	int camErr;

	camErr = ArtemisCoolingInfo(hCam, &flags, &level, &minlvl, &maxlvl, &setpoint);

	return (flags & 1);	//where first bit is flag for cooling being present

}

int CVS14M::setPrechargeMode(int mode)
{

	int camErr = ArtemisPrechargeMode(hCam_, mode);

	return camErr;
}

int CVS14M::setArtemisProcessing(bool linearise, bool VBE)
{

	int options = 0;

	if (linearise)
		options += ARTEMIS_PROCESS_LINEARISE;
	if (VBE)
		options += ARTEMIS_PROCESS_VBE;

	int camErr = ArtemisSetProcessing(hCam_, options);

	return camErr;

}