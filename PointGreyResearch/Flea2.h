///////////////////////////////////////////////////////////////////////////////
// FILE:          Flea2.h
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

#pragma once
#ifndef _Flea2_H_
#define _Flea2_H_

#include "../MMDevice/DeviceBase.h"
#include "../MMDevice/ImgBuffer.h"
#include "../MMDevice/DeviceThreads.h"
#include <string>
#include <map>
#include <algorithm>

// Copied from global inlude file FlyCapture2.h, but with relative paths:
//=============================================================================
// Platform-specific definitions
//=============================================================================
#include "../../3rdparty/PGR/include/FlyCapture2Platform.h"

//=============================================================================
// Global definitions
//=============================================================================
#include "../../3rdparty/PGR/include/FlyCapture2Defs.h"

//=============================================================================
// PGR Error class
//=============================================================================
#include "../../3rdparty/PGR/include/Error.h"

//=============================================================================
// FlyCapture2 classes
//=============================================================================
#include "../../3rdparty/PGR/include/BusManager.h"
#include "../../3rdparty/PGR/include/Camera.h"
#include "../../3rdparty/PGR/include/GigECamera.h"
#include "../../3rdparty/PGR/include/Image.h"

//=============================================================================
// Utility classes
//=============================================================================
#include "../../3rdparty/PGR/include/Utilities.h"
#include "../../3rdparty/PGR/include/AVIRecorder.h"
#include "../../3rdparty/PGR/include/TopologyNode.h"
#include "../../3rdparty/PGR/include/ImageStatistics.h"


//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102
#define ERR_UNKNOWN_POSITION     103
#define ERR_IN_SEQUENCE          104
#define ERR_SEQUENCE_INACTIVE    105
#define ERR_STAGE_MOVING         106
#define HUB_NOT_AVAILABLE        107

const char* NoHubError = "Parent Hub not defined.";


//////////////////////////////////////////////////////////////////////////////
// CFlea2 class
// Simulation of the Camera device
//////////////////////////////////////////////////////////////////////////////

class MySequenceThread;

class CFlea2 : public CCameraBase<CFlea2>  
{
public:
	CFlea2();
	~CFlea2();

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* name) const;      

	// MMCamera API
	// ------------
	int SnapImage();
	const unsigned char* GetImageBuffer();
	unsigned GetImageWidth() const;
	unsigned GetImageHeight() const;
	unsigned GetImageBytesPerPixel() const;
	unsigned GetBitDepth() const;
	long GetImageBufferSize() const;
	double GetExposure() const;
	void SetExposure(double exp);
	int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize); 
	int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize); 
	int ClearROI();
	int PrepareSequenceAcqusition() { return DEVICE_OK; }
	int StartSequenceAcquisition(double interval);
	int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
	int StopSequenceAcquisition();
	int InsertImage();
	int RunSequenceOnThread(MM::MMTime startTime);
	bool IsCapturing();
	void OnThreadExiting() throw(); 
	//double GetNominalPixelSizeUm() const {return nominalPixelSizeUm_;}
	//double GetPixelSizeUm() const {return nominalPixelSizeUm_ * GetBinning();}
	int GetBinning() const;
	int SetBinning(int bS);

	int IsExposureSequenceable(bool& isSequenceable) const;
	int GetExposureSequenceMaxLength(long& nrEvents) const;
	int StartExposureSequence();
	int StopExposureSequence();
	int ClearExposureSequence();
	int AddToExposureSequence(double exposureTime_ms);
	int SendExposureSequence() const;

	unsigned  GetNumberOfComponents() const { return nComponents_;};

	// action interface
	// ----------------
	int OnMaxExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
	//int OnPhysicalCamera(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCameraCCDXSize(MM::PropertyBase* , MM::ActionType );
	int OnCameraCCDYSize(MM::PropertyBase* , MM::ActionType );
	
	
	int OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFlipUD(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFlipLR(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRotate(MM::PropertyBase* pProp, MM::ActionType eAct);
	
	int OnTrigMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);


private:
	int SetAllowedBinning();
	int SetAllowedYBinning();
	void TestResourceLocking(const bool);
	void GenerateEmptyImage(ImgBuffer& img);
	int ResizeImageBuffer();

	double roundUp(double numToRound, double toMultipleOf);
	int findFactors(int input, std::vector<int> factors);

	void rotate90(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr);
	void rotate270(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr);
	void rotate180(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr);
	void mirrorY(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr);
	void mirrorX(int original_xsize, int original_ysize, unsigned char *in_arr, unsigned char *out_arr);

	int applyFormat7Commands(int binning, int bitdepth, int roi[4]);
	int setGain(double gain);
	int setTrigMode(std::string trigMode);
	int FireSoftwareTrigger( FlyCapture2::Camera* pCam );
	bool PollForTriggerReady( FlyCapture2::Camera* pCam );

	FlyCapture2::Camera hCam_;
	double gain_;

	double exposureMaximum_;
	std::string trigMode_;
	double dPhase_;
	ImgBuffer img_;
	bool busy_;
	bool stopOnOverFlow_;
	bool initialized_;
	MM::MMTime readoutStartTime_;
	int bitDepth_;
	unsigned roiX_;
	unsigned roiY_;
	unsigned roiW_;
	unsigned roiH_;
	MM::MMTime sequenceStartTime_;
	bool isSequenceable_;
	long sequenceMaxLength_;
	bool sequenceRunning_;
	unsigned long sequenceIndex_;
	double GetSequenceExposure();
	std::vector<double> exposureSequence_;
	long imageCounter_;
	long binSizeX_;
	long binSizeY_;
	long cameraCCDXSize_;
	long cameraCCDYSize_;

	bool stopOnOverflow_;

	bool asymmBinning_;
	//int yBinning_;


	bool flipUD_;
	bool flipLR_;
	long imageRotationAngle_;

	MMThreadLock imgPixelsLock_;
	friend class MySequenceThread;
	int nComponents_;
	MySequenceThread * thd_;
};

class MySequenceThread : public MMDeviceThreadBase
{
	friend class CFlea2;
	enum { default_numImages=1, default_intervalMS = 100 };
public:
	MySequenceThread(CFlea2* pCam);
	~MySequenceThread();
	void Stop();
	void Start(long numImages, double intervalMs);
	bool IsStopped();
	void Suspend();
	bool IsSuspended();
	void Resume();
	double GetIntervalMs(){return intervalMs_;}                               
	void SetLength(long images) {numImages_ = images;}                        
	long GetLength() const {return numImages_;}
	long GetImageCounter(){return imageCounter_;}                             
	MM::MMTime GetStartTime(){return startTime_;}                             
	MM::MMTime GetActualDuration(){return actualDuration_;}
private:                                                                     
	int svc(void) throw();
	double intervalMs_;                                                       
	long numImages_;                                                          
	long imageCounter_;                                                       
	bool stop_;                                                               
	bool suspend_;                                                            
	CFlea2* camera_;                                                     
	MM::MMTime startTime_;                                                    
	MM::MMTime actualDuration_;                                             
	MM::MMTime lastFrameTime_;                                                
	MMThreadLock stopLock_;                                                   
	MMThreadLock suspendLock_;                                                
}; 





#endif //_Flea2_H_
