///////////////////////////////////////////////////////////////////////////////
// FILE:          Flea2.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   PGR Flea2 camera interface for micromanager
//                
// AUTHOR:        Doug Kelly, dk1109@ic.ac.uk, 15/08/2014
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

#include "Flea2.h"
//#include "FlyCapture2.h"

#include <cstdio>
#include <string>
#include <math.h>
#include "../MMDevice/ModuleInterface.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <cmath>

using namespace std;


// External names used used by the rest of the system
// to load particular device from the "Flea2.dll" library
const char* g_CameraDeviceName = "Flea2Cam";


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
	RegisterDevice(g_CameraDeviceName, MM::CameraDevice, "PGR camera");


}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	// decide which device class to create based on the deviceName parameter
	if (strcmp(deviceName, g_CameraDeviceName) == 0)
	{
		// create camera
		return new CFlea2();
	}
	
	// ...supplied name not recognized
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// CFlea2 implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~

/**
* CFlea2 constructor.
* Setup default all variables and create device properties required to exist
* before intialization. In this case, no such properties were required. All
* properties will be created in the Initialize() method.
*
* As a general guideline Micro-Manager devices do not access hardware in the
* the constructor. We should do as little as possible in the constructor and
* perform most of the initialization in the Initialize() method.
*/
CFlea2::CFlea2() :
CCameraBase<CFlea2> (),
	//exposureMaximum_(63312.04),
	exposureMaximum_(10000),	//Currently hardcoded
	initialized_(false),
	bitDepth_(8),
	roiX_(0),
	roiY_(0),
	roiW_(1032),
	roiH_(776),
	sequenceStartTime_(0),
	isSequenceable_(false),
	sequenceMaxLength_(100),
	sequenceRunning_(false),
	sequenceIndex_(0),
	binSizeX_(1),
	binSizeY_(1),
	trigMode_("Isochronous"),
	cameraCCDXSize_(1032),
	cameraCCDYSize_(776),
	asymmBinning_(false),
	
	gain_(1),
	stopOnOverflow_(false),
	flipUD_(false),
	flipLR_(false),
	imageRotationAngle_(0),
	
	nComponents_(1)
{
	//memset(testProperty_,0,sizeof(testProperty_));

	// call the base class method to set-up default error codes/messages
	InitializeDefaultErrorMessages();
	readoutStartTime_ = GetCurrentMMTime();
	thd_ = new MySequenceThread(this);

	CreateFloatProperty("MaximumExposureMs", exposureMaximum_, false,
		new CPropertyAction(this, &CFlea2::OnMaxExposure),
		true);

	//CreateIntegerProperty("Physical camera", 1, false, 
	//	new CPropertyAction(this, &CFlea2::OnPhysicalCamera), 
	//	true);

	////
}

/**
* CFlea2 destructor.
* If this device used as intended within the Micro-Manager system,
* Shutdown() will be always called before the destructor. But in any case
* we need to make sure that all resources are properly released even if
* Shutdown() was not called.
*/
CFlea2::~CFlea2()
{
	StopSequenceAcquisition();
	delete thd_;
}

/**
* Obtains device name.
* Required by the MM::Device API.
*/
void CFlea2::GetName(char* name) const
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
int CFlea2::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	// Connect to camera and get camera info
	FlyCapture2::BusManager busMgr;
	FlyCapture2::CameraInfo camInfo;
	FlyCapture2::Error pgrErr;
	FlyCapture2::PGRGuid guid;
    unsigned int numCameras;
    
	pgrErr = busMgr.GetNumOfCameras(&numCameras);
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
		LogMessage( (std::string) pgrErr.GetDescription() );
        return DEVICE_ERR;
    }
	else if (numCameras<1)
		return FlyCapture2::PGRERROR_NOT_FOUND;

	pgrErr = busMgr.GetCameraFromIndex(0, &guid);
	pgrErr = hCam_.Connect(&guid);
	pgrErr = hCam_.GetCameraInfo(&camInfo);

	// set property list
	// -----------------

	// Name
	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_CameraDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Description
	nRet = CreateStringProperty(MM::g_Keyword_Description, "PGR Flea2 Camera Device Adapter", true);
	if (DEVICE_OK != nRet)
		return nRet;

	// CameraName
	nRet = CreateStringProperty(MM::g_Keyword_CameraName, "Flea2", true);
	assert(nRet == DEVICE_OK);

	// CameraID
	nRet = CreateStringProperty(MM::g_Keyword_CameraID, boost::lexical_cast<std::string>(camInfo.serialNumber).c_str(), true);
	assert(nRet == DEVICE_OK);

	// API version
	nRet = CreateStringProperty("PGR firmware version", camInfo.firmwareVersion, true);
	if (nRet != DEVICE_OK)
		return nRet;

	// CCD chip size
	std::string resolution = camInfo.sensorResolution;
	vector<string> tokens;
	boost::split(tokens, resolution, boost::is_any_of("x"));
	//boost::char_separator<char> delim("x");
	//boost::tokenizer<boost::char_separator<char>> tokens(resolution, delim);
	//int xyres[2] = {0, 0};
	//for ( boost::tokenizer<boost::char_separator<char>>::iterator it = tokens.begin();
 //        it != tokens.end();
 //        ++it)
	//{
	//	xyres[it] = *it;
	//}
	

	long fullframex = atol(tokens[0].c_str());
	long fullframey = atol(tokens[1].c_str());

	//Read only pixel size properties
	CreateFloatProperty("PixelSizeX_um", 4.65, true);
	CreateFloatProperty("PixelSizeY_um", 4.65, true);	// Get from camera props??
	CreateStringProperty("Manufacturer", camInfo.vendorName, true);
	CreateStringProperty("DescriptionFromCamera", camInfo.modelName, true);


	// binning
	CPropertyAction *pAct = new CPropertyAction (this, &CFlea2::OnBinning);
	nRet = CreateIntegerProperty(MM::g_Keyword_Binning, 1, false, pAct);
	assert(nRet == DEVICE_OK);

	nRet = SetAllowedBinning();
	if (nRet != DEVICE_OK)
		return nRet;

	// pixel type
	pAct = new CPropertyAction (this, &CFlea2::OnPixelType);
	nRet = CreateStringProperty(MM::g_Keyword_PixelType, g_PixelType_8bit, false, pAct);
	assert(nRet == DEVICE_OK);

	vector<string> pixelTypeValues;
	pixelTypeValues.push_back(g_PixelType_8bit);
	pixelTypeValues.push_back(g_PixelType_16bit); 
	//pixelTypeValues.push_back(g_PixelType_32bitRGB);
	//pixelTypeValues.push_back(g_PixelType_64bitRGB);
	//pixelTypeValues.push_back(::g_PixelType_32bit);

	nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
	if (nRet != DEVICE_OK)
		return nRet;

	// Bit depth
	pAct = new CPropertyAction (this, &CFlea2::OnBitDepth);
	nRet = CreateIntegerProperty("BitDepth", 8, false, pAct);
	assert(nRet == DEVICE_OK);

	vector<string> bitDepths;
	bitDepths.push_back("8");
	//bitDepths.push_back("10");
	//bitDepths.push_back("12");
	//bitDepths.push_back("14");
	bitDepths.push_back("16");
	//bitDepths.push_back("32");
	nRet = SetAllowedValues("BitDepth", bitDepths);
	if (nRet != DEVICE_OK)
		return nRet;

	// exposure
	float exp = 10.0;
	nRet = CreateFloatProperty(MM::g_Keyword_Exposure, exp, false);
	assert(nRet == DEVICE_OK);
	SetPropertyLimits(MM::g_Keyword_Exposure, 0.1, exposureMaximum_);	//Minimum hardcoded based on technical reference


	// CCD size of the camera - get from device? for user info only...
	//long fullframex = 1392;
	//long fullframey = 1040;
	pAct = new CPropertyAction (this, &CFlea2::OnCameraCCDXSize);
	CreateIntegerProperty("OnCameraCCDXSize", fullframex, true, pAct);
	cameraCCDXSize_ = fullframex;
	pAct = new CPropertyAction (this, &CFlea2::OnCameraCCDYSize);
	CreateIntegerProperty("OnCameraCCDYSize", fullframey, true, pAct);
	cameraCCDYSize_ = fullframey;


	// Whether or not to use exposure time sequencing
	pAct = new CPropertyAction (this, &CFlea2::OnIsSequenceable);
	std::string propName = "UseExposureSequences";
	CreateStringProperty(propName.c_str(), "No", false, pAct);
	AddAllowedValue(propName.c_str(), "Yes");
	AddAllowedValue(propName.c_str(), "No");

	// Trigger mode - continuous, software or hardware triggering
	pAct = new CPropertyAction (this, &CFlea2::OnTrigMode);
	CreateStringProperty("TriggerMode", trigMode_.c_str(), false, pAct);
	AddAllowedValue("TriggerMode", "Asynchronous-hardware");
	AddAllowedValue("TriggerMode", "Asynchronous-software");
	AddAllowedValue("TriggerMode", "Isochronous");

	//Gain 
	pAct = new CPropertyAction (this, &CFlea2::OnGain);
	CreateFloatProperty("Gain", 1, false, pAct);
	SetPropertyLimits("Gain", 0.0, 24.0);

	//Flip image UD?
	pAct = new CPropertyAction(this, &CFlea2::OnFlipUD);
	CreateIntegerProperty("FlipImageUD", 0, false, pAct);
	AddAllowedValue("FlipImageUD","0");
	AddAllowedValue("FlipImageUD","1");

	//Flip image LR?
	pAct = new CPropertyAction(this, &CFlea2::OnFlipLR);
	CreateIntegerProperty("FlipImageLR", 0, false, pAct);
	AddAllowedValue("FlipImageLR","0");
	AddAllowedValue("FlipImageLR","1");

    //Rotate image?
    pAct = new CPropertyAction(this, &CFlea2::OnRotate);
    CreateIntegerProperty("RotateImage", 0, false, pAct);
    AddAllowedValue("RotateImage","0");
    //AddAllowedValue("RotateImage","90");
    AddAllowedValue("RotateImage","180");
    //AddAllowedValue("RotateImage","270");

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

		// Turn off frame rate control, go as fast as possible based on exposure (in isochronous mode)
		FlyCapture2::Property prop;
        prop.type = FlyCapture2::FRAME_RATE;
        pgrErr = hCam_.GetProperty( &prop );
        if (pgrErr != FlyCapture2::PGRERROR_OK)
        {
            LogMessage("Error getting camera properties");
            return DEVICE_ERR;
        }

        prop.autoManualMode = false;
        prop.onOff = false;

        pgrErr = hCam_.SetProperty( &prop );
        if (pgrErr != FlyCapture2::PGRERROR_OK)
        {
            LogMessage("Error setting camera properties");
            return DEVICE_ERR;
        }


	//Not obvious why this isn't taken care of with UpdateStatus()...?
	SetProperty("TriggerMode", trigMode_.c_str());
	SetProperty(MM::g_Keyword_Binning, "1");
	SetExposure( (double) exp );
	setGain(gain_);
	
	initialized_ = true;
	hCam_.StartCapture();	// start isochronous capture

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
int CFlea2::Shutdown()
{
	initialized_ = false;
	StopSequenceAcquisition();

	return DEVICE_OK;
}

/**
* Performs exposure and grabs a single image.
* This function should block during the actual exposure and return immediately afterwards 
* (i.e., before readout).  This behavior is needed for proper synchronization with the shutter.
* Required by the MM::Camera API.
*/
int CFlea2::SnapImage()
{
	static int callCounter = 0;
	++callCounter;

	MM::MMTime startTime = GetCurrentMMTime();
	double exp = GetExposure();
	float exp_seconds = ((float) exp)/1000;
	if (sequenceRunning_ && IsCapturing()) 
	{
		//exp = GetSequenceExposure();
	}

	if (trigMode_.compare("Asynchronous-hardware") == 0)
	{
		return DEVICE_NOT_YET_IMPLEMENTED;
	}
	else if (trigMode_.compare("Asynchronous-software") == 0)
	{
		while (!PollForTriggerReady( &hCam_ )) {}	// wait for trigger ready
		int ret = FireSoftwareTrigger( &hCam_);
		if (ret != DEVICE_OK)
			return ret;
	}
	else if (trigMode_.compare("Isochronous") == 0)
	{

	}

	//if (!overlapExposure_)
	//{
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
	//}
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
const unsigned char* CFlea2::GetImageBuffer()
{
	MMThreadGuard g(imgPixelsLock_);
	FlyCapture2::Error pgrErr;

	//if (overlapExposure_)
	//{
	//	if (!PGROverlappedExposureValid(hCam_))
	//		return PGR_OPERATION_FAILED;

	//}
	//while (!PGRImageReady(hCam_)) {}

	//Debug
	int dw = img_.Width();
	int dh = img_.Height();
	int dd = img_.Depth();

	unsigned int dataSize = img_.Height()*img_.Width()*img_.Depth();
	unsigned char *pBuf;
	unsigned char *nBuf = new unsigned char [dataSize];
	
	FlyCapture2::Image rawImage;
	FlyCapture2::Image convertedImage(nBuf, dataSize);
	pgrErr = hCam_.RetrieveBuffer( &rawImage );

	if (pgrErr != FlyCapture2::PGRERROR_OK)
	{
		LogMessage("Error retrieving image from camera");
	}
	
	if (bitDepth_ == 8)
		pgrErr = rawImage.Convert( FlyCapture2::PIXEL_FORMAT_MONO8, &convertedImage );
	else if (bitDepth_ == 16)
		pgrErr = rawImage.Convert( FlyCapture2::PIXEL_FORMAT_MONO16, &convertedImage );
	else
		convertedImage = rawImage;


	pBuf = const_cast<unsigned char*>(img_.GetPixelsRW());
	//nBuf = (unsigned short*) nBuf;
	
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

	delete[] nBuf;
	return img_.GetPixels();
}

/**
* Returns image buffer X-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CFlea2::GetImageWidth() const
{
	return img_.Width();
}

/**
* Returns image buffer Y-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CFlea2::GetImageHeight() const
{
	return img_.Height();
}

/**
* Returns image buffer pixel depth in bytes.
* Required by the MM::Camera API.
*/
unsigned CFlea2::GetImageBytesPerPixel() const
{
	return img_.Depth();
} 

/**
* Returns the bit depth (dynamic range) of the pixel.
* This does not affect the buffer size, it just gives the client application
* a guideline on how to interpret pixel values.
* Required by the MM::Camera API.
*/
unsigned CFlea2::GetBitDepth() const
{
	return bitDepth_;
}

/**
* Returns the size in bytes of the image buffer.
* Required by the MM::Camera API.
*/
long CFlea2::GetImageBufferSize() const
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
int CFlea2::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
	if (xSize == 0 && ySize == 0)
	{
		// effectively clear ROI

		roiW_ = cameraCCDXSize_/binSizeX_;
		roiH_ = cameraCCDYSize_/binSizeY_;
		int roi[4] = {0, 0, roiW_, roiH_};
		int ret = applyFormat7Commands(binSizeX_, bitDepth_, roi);
		if (ret != DEVICE_OK)
			return ret;
		ResizeImageBuffer();
		roiX_ = 0;
		roiY_ = 0;
	}
	else
	{
		// apply ROI
		//int roi[4] = {x*binSizeX_, y*binSizeY_, xSize*binSizeX_, ySize*binSizeY_};
		int roi[4] = {x, y, xSize, ySize};
		int ret = applyFormat7Commands(binSizeX_, bitDepth_, roi);
		if (ret != DEVICE_OK)
			return ret;

		ResizeImageBuffer();
	}
	return DEVICE_OK;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
int CFlea2::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{

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
int CFlea2::ClearROI()
{

	roiW_ = cameraCCDXSize_/binSizeX_;
	roiH_ = cameraCCDYSize_/binSizeY_;
	int roi[4] = {0, 0, roiW_, roiH_};
	int ret = applyFormat7Commands(binSizeX_, bitDepth_, roi);
	if (ret != DEVICE_OK)
			return ret;
	ResizeImageBuffer();
	roiX_ = 0;
	roiY_ = 0;

	return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double CFlea2::GetExposure() const
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
double CFlea2::GetSequenceExposure() 
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
void CFlea2::SetExposure(double exp)
{

	SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp));
	GetCoreCallback()->OnExposureChanged(this, exp);

	FlyCapture2::Error pgrErr;
	FlyCapture2::Property prop;
    prop.type = FlyCapture2::SHUTTER;
    pgrErr = hCam_.GetProperty( &prop );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage("Error getting shutter property");
    }

    prop.autoManualMode = false;
    prop.absControl = true;

    prop.absValue = (float) exp;

    pgrErr = hCam_.SetProperty( &prop );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage("Error setting shutter property");
    }

	// Set timeout for retrieve buffer to be exposure time + 5 seconds:
	// Get the camera configuration
    FlyCapture2::FC2Config config;
    pgrErr = hCam_.GetConfiguration( &config );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage( "Error getting camera configuration" );
    } 
    
    // Set the grab timeout to 5 seconds
    config.grabTimeout = 5000 + ((int) 1000 * exp);

    // Set the camera configuration
    pgrErr = hCam_.SetConfiguration( &config );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage( "Error setting camera configuration" );
    } 

}

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int CFlea2::GetBinning() const
{
	//Again, assume that binning cannot be changed externally so there will 
	//be no clash between hardware and software, and avoid calling 
	//PGRGetBin(., ., .) every time...
	
	//char buf[MM::MaxStrLength];
	//int x = -1;
	//int y = -1;
	//PGRGetBin(hCam_, &x, &y);
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
int CFlea2::SetBinning(int binFX)
{
	int roi [4] = {roiX_, roiY_, roiW_, roiH_};
	applyFormat7Commands(binSizeX_, bitDepth_, roi); 
	
	return DEVICE_OK;
}

int CFlea2::IsExposureSequenceable(bool& isSequenceable) const
{
	isSequenceable = isSequenceable_;
	return DEVICE_OK;
}

int CFlea2::GetExposureSequenceMaxLength(long& nrEvents) const
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	nrEvents = sequenceMaxLength_;
	return DEVICE_OK;
}

int CFlea2::StartExposureSequence()
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	// may need thread lock
	sequenceRunning_ = true;
	return DEVICE_OK;
}

int CFlea2::StopExposureSequence()
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
int CFlea2::ClearExposureSequence()
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
int CFlea2::AddToExposureSequence(double exposureTime_ms) 
{
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	exposureSequence_.push_back(exposureTime_ms);
	return DEVICE_OK;
}

int CFlea2::SendExposureSequence() const {
	if (!isSequenceable_) {
		return DEVICE_UNSUPPORTED_COMMAND;
	}

	return DEVICE_OK;
}

int CFlea2::SetAllowedBinning() 
{
	//For now, hardcode values from Flea2 technical reference (Format7)
	vector<string> binValues;
	int x = 2;
	int y = 2;	//Hardcoded lims

	for (int i = 0; i < x; ++i){
		if ( ( fmod((double) cameraCCDXSize_, (double) (i+1)) == 0 ) && ( fmod((double) cameraCCDYSize_, (double) (i+1)) == 0) )
			binValues.push_back(""+boost::lexical_cast<std::string>(i + 1));
	}
	
	LogMessage("Setting Allowed Binning settings", false);
	return SetAllowedValues(MM::g_Keyword_Binning, binValues);
}

int CFlea2::SetAllowedYBinning() 
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
int CFlea2::StartSequenceAcquisition(double interval)
{
	return StartSequenceAcquisition(LONG_MAX, interval, false);            
}

/**                                                                       
* Stop and wait for the Sequence thread finished                                   
*/                                                                        
int CFlea2::StopSequenceAcquisition()                                     
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
int CFlea2::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
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
int CFlea2::InsertImage()
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
int CFlea2::RunSequenceOnThread(MM::MMTime startTime)
{
	int ret=DEVICE_ERR;

	if (trigMode_.compare("Asynchronous-hardware") == 0)
	{
		return DEVICE_NOT_YET_IMPLEMENTED;
	}
	else if (trigMode_.compare("Asynchronous-software") == 0)
	{
		while (!PollForTriggerReady( &hCam_ )) {}	// wait for trigger ready
		int ret = FireSoftwareTrigger( &hCam_);
		if (ret != DEVICE_OK)
			return ret;
	}
	else if (trigMode_.compare("Isochronous") == 0)
	{

	} 
	

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

bool CFlea2::IsCapturing() {
	return !thd_->IsStopped();
}

/*
* called from the thread function before exit 
*/
void CFlea2::OnThreadExiting() throw()
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

MySequenceThread::MySequenceThread(CFlea2* pCam)
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
// CFlea2 Action handlers
///////////////////////////////////////////////////////////////////////////////

int CFlea2::OnMaxExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
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
int CFlea2::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
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
				roiX_ = oldbinX*roiX_/binSizeX_;
				roiY_ = oldbinX*roiY_/binSizeY_;
				roiW_ = oldbinX*roiW_/binSizeX_;
				roiH_ = oldbinX*roiH_/binSizeY_;

				SetBinning(binSizeX_);	//Better getting properties than using globals - probably OK for now...
				ResizeImageBuffer();
				
				//SetROI(oldbinX*roiX_/binSizeX_, oldbinX*roiY_/binSizeY_, oldbinX*roiW_/binSizeX_, oldbinX*roiH_/binSizeY_);

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
int CFlea2::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
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
				pProp->Set(g_PixelType_8bit);
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
int CFlea2::OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct)
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

			int roi[4] = {roiX_, roiY_, roiW_, roiH_};
			applyFormat7Commands(binSizeX_, bitDepth_, roi);

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

int CFlea2::OnCameraCCDXSize(MM::PropertyBase* pProp , MM::ActionType eAct)
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

int CFlea2::OnCameraCCDYSize(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int CFlea2::OnTrigMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string val = "Isochronous";
	if (eAct == MM::BeforeGet)
	{
		val = trigMode_;
		pProp->Set(val.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		isSequenceable_ = false;
		pProp->Get(val);
		trigMode_ = val;
		setTrigMode(val);
	}

	return DEVICE_OK;
}

int CFlea2::OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int CFlea2::OnFlipUD(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int CFlea2::OnFlipLR(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int CFlea2::OnRotate(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int CFlea2::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	double gain = 1.0;

	if (eAct == MM::BeforeGet)
	{
		gain = gain_;
		pProp->Set(gain);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(gain);
		gain_ = gain;
		setGain(gain);
	}

	return DEVICE_OK;
}


int CFlea2::ResizeImageBuffer()
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

void CFlea2::GenerateEmptyImage(ImgBuffer& img)
{
	MMThreadGuard g(imgPixelsLock_);
	if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
		return;
	unsigned char* pBuf = const_cast<unsigned char*>(img.GetPixels());
	memset(pBuf, 0, img.Height()*img.Width()*img.Depth());
}

void CFlea2::TestResourceLocking(const bool recurse)
{
	if(recurse)
		TestResourceLocking(false);
}

void CFlea2::rotate90(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr){

	int xsize = original_ysize;
	int ysize = original_xsize;	//swapped here for clairty...
	for (int y = 0; y < ysize; ++y){
		for (int x = 0, destx = xsize-1; x<xsize; ++x, --destx){
			out_arr[y * xsize + destx] = in_arr[x * ysize + y];
		}
	}
}

void CFlea2::rotate180(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr){

	for (int i = 0; i < original_xsize*original_ysize; ++i){
		out_arr[original_xsize*original_ysize - 1 -i] = in_arr[i];
	}
}

void CFlea2::rotate270(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr){

	int xsize = original_ysize;
	int ysize = original_xsize;	//swapped here for clairty...
	for (int x = 0; x < xsize; ++x){
		for (int y = 0, desty = ysize-1; y<ysize; ++y, --desty){
			out_arr[x + desty*xsize] = in_arr[x * ysize + y];
		}
	}
}

void CFlea2::mirrorY(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr){

	for (int x = 0; x < original_xsize; ++x){
		for (int y = 0, desty = original_ysize - 1; y < original_ysize; ++y, --desty){
			out_arr[desty * original_xsize + x] = in_arr[y * original_xsize + x];
		}
	}
}

void CFlea2::mirrorX(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr){

	for (int y = 0; y < original_ysize; ++y){
		for (int x = 0, destx = original_xsize - 1; x < original_xsize; ++x, --destx){
			out_arr[y * original_xsize + destx] = in_arr[y * original_xsize + x];
		}
	}
}


double CFlea2::roundUp(double numToRound, double toMultipleOf) 
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


int CFlea2::findFactors(int input, std::vector<int> factors){
	
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

int CFlea2::applyFormat7Commands(int binning, int bitdepth, int roi[4]) //and array for ROI?
{
			hCam_.StopCapture();
	
			FlyCapture2::Mode k_fmt7Mode;
			FlyCapture2::PixelFormat k_fmt7PixFmt;
			FlyCapture2::Error pgrErr;

			if (binning == 2)
				k_fmt7Mode = FlyCapture2::MODE_1;
			else
				k_fmt7Mode = FlyCapture2::MODE_0;

			if (bitdepth == 8)
				k_fmt7PixFmt = FlyCapture2::PIXEL_FORMAT_MONO8;
			else
				k_fmt7PixFmt = FlyCapture2::PIXEL_FORMAT_MONO16;

			FlyCapture2::Format7Info fmt7Info;
			bool supported;
			fmt7Info.mode = k_fmt7Mode;
			pgrErr = hCam_.GetFormat7Info( &fmt7Info, &supported );
			

			if ((!supported) || ( (k_fmt7PixFmt & fmt7Info.pixelFormatBitField) == 0 ))
			{
				LogMessage("Pixel format not supported");
				return DEVICE_INVALID_PROPERTY_VALUE;
			}

			FlyCapture2::Format7ImageSettings fmt7ImageSettings;
			roiW_ = roundUp(roi[2], fmt7Info.imageHStepSize);
			roiH_ = roundUp(roi[3], fmt7Info.imageVStepSize);
			roiX_ = roundUp(roi[0], fmt7Info.offsetHStepSize);
			roiY_ = roundUp(roi[1], fmt7Info.offsetVStepSize);
			//roiH_ = ySize;
			//roiX_ = x;
			//roiY_ = y;
			fmt7ImageSettings.mode = k_fmt7Mode;
			fmt7ImageSettings.offsetX = roiX_;
			fmt7ImageSettings.offsetY = roiY_;
			fmt7ImageSettings.width = roiW_;
			fmt7ImageSettings.height = roiH_;
			fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

			bool valid;
			FlyCapture2::Format7PacketInfo fmt7PacketInfo;
			
			// Validate the settings to make sure that they are valid
			pgrErr = hCam_.ValidateFormat7Settings(
				&fmt7ImageSettings,
				&valid,
				&fmt7PacketInfo );
			int rbpp = fmt7PacketInfo.recommendedBytesPerPacket;

			if (!valid)
			{
				LogMessage("Format7 settings are not valid");
				return DEVICE_INVALID_PROPERTY_VALUE;
			}

			// Set the settings to the camera
			pgrErr = hCam_.SetFormat7Configuration(
				&fmt7ImageSettings,
				fmt7PacketInfo.recommendedBytesPerPacket );
			if (pgrErr != FlyCapture2::PGRERROR_OK)
			{
				LogMessage( "Error sending Format7 commands to camera" );
				return DEVICE_ERR;
			}

			hCam_.StartCapture();

			return DEVICE_OK;
}

int CFlea2::setGain(double gain)
{
	FlyCapture2::Error pgrErr;
	FlyCapture2::Property prop;
    prop.type = FlyCapture2::GAIN;
    pgrErr = hCam_.GetProperty( &prop );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage("Error getting gain property");
		return DEVICE_ERR;
    }

    prop.autoManualMode = false;
    prop.absControl = true;

    prop.absValue = gain;

    pgrErr = hCam_.SetProperty( &prop );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage("Error setting gain property");
		return DEVICE_ERR;
    }

	return DEVICE_OK;
}

int CFlea2::setTrigMode(std::string trigMode)
{
	FlyCapture2::Error pgrErr;
	FlyCapture2::TriggerMode triggerMode;

	hCam_.StopCapture();

	if (trigMode.compare("Asynchronous-hardware") == 0)
	{
		//Check that hardware option is supported
		FlyCapture2::TriggerModeInfo triggerModeInfo;
		pgrErr = hCam_.GetTriggerModeInfo( &triggerModeInfo );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in getting trigger mode info");
			return DEVICE_ERR;
		}

		if ( triggerModeInfo.present != true )
		{
			LogMessage( "Camera does not support external trigger. Switching to isochronous trigger. " );
			SetProperty("TriggerMode", "Isochronous");
			return DEVICE_NOT_SUPPORTED;
		}

		pgrErr = hCam_.GetTriggerMode( &triggerMode );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in getting trigger mode");
			return DEVICE_ERR;
		}

		// Set camera to trigger mode 0
		triggerMode.onOff = true;
		triggerMode.mode = 0;
		triggerMode.parameter = 0;
		triggerMode.source = 0;

		pgrErr = hCam_.SetTriggerMode( &triggerMode );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in setting trigger mode");
			return DEVICE_ERR;
		}

	}
	else if (trigMode.compare("Asynchronous-software") == 0)
	{
		const unsigned int k_triggerInq = 0x530;
		unsigned int regVal = 0;
		pgrErr = hCam_.ReadRegister( k_triggerInq, &regVal );

		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in checking sw trigger mode presence");
			return DEVICE_ERR;
		}

		if( ( regVal & 0x10000 ) != 0x10000 )
		{
			LogMessage( "Camera does not support software trigger. Switching to isochronous trigger. " );
			SetProperty("TriggerMode", "Isochronous");
			return DEVICE_NOT_SUPPORTED;
		}

		pgrErr = hCam_.GetTriggerMode( &triggerMode );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in getting trigger mode");
			return DEVICE_ERR;
		}

		triggerMode.onOff = true;
		triggerMode.mode = 0;
		triggerMode.parameter = 0;
		triggerMode.source = 7;	//Software trigger

		pgrErr = hCam_.SetTriggerMode( &triggerMode );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in setting trigger mode");
			return DEVICE_ERR;
		}
	}
	else //default, isochronous
	{
		triggerMode.onOff = false;    
		pgrErr = hCam_.SetTriggerMode( &triggerMode );
		if (pgrErr != FlyCapture2::PGRERROR_OK)
		{
			LogMessage("Error in setting trigger mode");
			return DEVICE_ERR;
		}
		//hCam_.StartCapture();
	}
	hCam_.StartCapture();
	
	return DEVICE_OK;
}

int CFlea2::FireSoftwareTrigger( FlyCapture2::Camera* pCam )
{
    const unsigned int k_softwareTrigger = 0x62C;
    const unsigned int k_fireVal = 0x80000000;
    FlyCapture2::Error pgrErr;    

    pgrErr = pCam->WriteRegister( k_softwareTrigger, k_fireVal );
    if (pgrErr != FlyCapture2::PGRERROR_OK)
    {
        LogMessage("Error in setting trigger mode");
		return DEVICE_ERR;
    }

    return DEVICE_OK;
}

bool CFlea2::PollForTriggerReady( FlyCapture2::Camera* pCam )
{
    const unsigned int k_softwareTrigger = 0x62C;
    FlyCapture2::Error pgrErr;
    unsigned int regVal = 0;

    do 
    {
        pgrErr = pCam->ReadRegister( k_softwareTrigger, &regVal );
        if (pgrErr != FlyCapture2::PGRERROR_OK)
        {
            LogMessage("Error in polling for trigger readiness");
			return false;
        }

    } while ( (regVal >> 31) != 0 );

	return true;
}