///////////////////////////////////////////////////////////////////////////////
// FILE:          VS14M.h
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

#ifndef _VS14M_H_
#define _VS14M_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include <string>
#include <map>
#include <algorithm>
//#include "../../3rdparty/ArtemisVS14M/ArtemisSciAPI.h"
#include "C:\\Users\\dk1109\\repositories\\umanager\\micromanager\\DeviceAdapters\\Artermis\\ArtemisHscAPI.h"

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
// CVS14M class
// Simulation of the Camera device
//////////////////////////////////////////////////////////////////////////////

class MySequenceThread;

class CVS14M : public CCameraBase<CVS14M>  
{
public:
	CVS14M();
	~CVS14M();

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
	double GetNominalPixelSizeUm() const {return nominalPixelSizeUm_;}
	double GetPixelSizeUm() const {return nominalPixelSizeUm_ * GetBinning();}
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
	int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCameraCCDXSize(MM::PropertyBase* , MM::ActionType );
	int OnCameraCCDYSize(MM::PropertyBase* , MM::ActionType );
	int OnTriggerDevice(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCCDTemp(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFlipUD(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFlipLR(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRotate(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSubframeX(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSubframeY(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSubframeW(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSubframeH(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPriority(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnAsymmBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnYBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPrecharge(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnArtemisLinearise(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnArtemisVenetian(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnOverlappedExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPreviewMode(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	int SetAllowedBinning();
	int SetAllowedYBinning();
	void TestResourceLocking(const bool);
	void GenerateEmptyImage(ImgBuffer& img);
	int ResizeImageBuffer();

	int GetCurrentTemperature();
	int TemperatureContol();
	double CVS14M::roundUp(double numToRound, double toMultipleOf);
	int setPriority(bool highpriority);
	int findFactors(int input, std::vector<int> factors);
	int toggleAsymmBinning();
	bool isCoolingPresent(ArtemisHandle hCam);
	int setPrechargeMode(int mode);
	int setArtemisProcessing(bool linearise, bool VBE);

	int reportCamErr(int err);

	void rotate90(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr);
	void rotate270(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr);
	void rotate180(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr);
	void mirrorY(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr);
	void mirrorX(int original_xsize, int original_ysize, unsigned short *in_arr, unsigned short *out_arr);

	static const double nominalPixelSizeUm_;

	double exposureMaximum_;
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
	double ccdT_;
	std::string triggerDevice_;

	ArtemisHandle hCam_;
	float currentTemp_;
	float ambientTemp_;

	bool stopOnOverflow_;

	bool asymmBinning_;
	//int yBinning_;


	bool flipUD_;
	bool flipLR_;
	long imageRotationAngle_;

	bool highPriority_;
	int prechargeMode_;
	bool processVBE_;
	bool processLinearise_;
	bool overlapExposure_;
	bool previewMode_;

	MMThreadLock imgPixelsLock_;
	friend class MySequenceThread;
	int nComponents_;
	MySequenceThread * thd_;
};

class MySequenceThread : public MMDeviceThreadBase
{
	friend class CVS14M;
	enum { default_numImages=1, default_intervalMS = 100 };
public:
	MySequenceThread(CVS14M* pCam);
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
	CVS14M* camera_;                                                     
	MM::MMTime startTime_;                                                    
	MM::MMTime actualDuration_;                                               
	MM::MMTime lastFrameTime_;                                                
	MMThreadLock stopLock_;                                                   
	MMThreadLock suspendLock_;                                                
}; 



//////////////////////////////////////////////////////////////////////////////
// TransposeProcessor class
// transpose an image
// K.H.
//////////////////////////////////////////////////////////////////////////////
class TransposeProcessor : public CImageProcessorBase<TransposeProcessor>
{
public:
	TransposeProcessor () : inPlace_ (false), pTemp_(NULL), tempSize_(0), busy_(false)
	{
		// parent ID display
		//CreateHubIDProperty();
	}
	~TransposeProcessor () {if( NULL!= pTemp_) free(pTemp_); tempSize_=0;  }

	int Shutdown() {return DEVICE_OK;}
	void GetName(char* name) const {strcpy(name,"TransposeProcessor");}

	int Initialize();

	bool Busy(void) { return busy_;};

	// really primative image transpose algorithm which will work fine for non-square images... 
	template <typename PixelType>
	int TransposeRectangleOutOfPlace(PixelType* pI, unsigned int width, unsigned int height)
	{
		int ret = DEVICE_OK;
		unsigned long tsize = width*height*sizeof(PixelType);
		if( this->tempSize_ != tsize)
		{
			if( NULL != this->pTemp_)
			{
				free(pTemp_);
				pTemp_ = NULL;
			}
			pTemp_ = (PixelType *)malloc(tsize);
		}
		if( NULL != pTemp_)
		{
			PixelType* pTmpImage = (PixelType *) pTemp_;
			tempSize_ = tsize;
			for( unsigned long ix = 0; ix < width; ++ix)
			{
				for( unsigned long iy = 0; iy < height; ++iy)
				{
					pTmpImage[iy + ix*width] = pI[ ix + iy*height];
				}
			}
			memcpy( pI, pTmpImage, tsize);
		}
		else
		{
			ret = DEVICE_ERR;
		}

		return ret;
	}


	template <typename PixelType>
	void TransposeSquareInPlace(PixelType* pI, unsigned int dim)
	{ 
		PixelType tmp;
		for( unsigned long ix = 0; ix < dim; ++ix)
		{
			for( unsigned long iy = ix; iy < dim; ++iy)
			{
				tmp = pI[iy*dim + ix];
				pI[iy*dim +ix] = pI[ix*dim + iy];
				pI[ix*dim +iy] = tmp; 
			}
		}

		return;
	}

	int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

	// action interface
	// ----------------
	int OnInPlaceAlgorithm(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	bool inPlace_;
	void* pTemp_;
	unsigned long tempSize_;
	bool busy_;
};



//////////////////////////////////////////////////////////////////////////////
// ImageFlipX class
// flip an image
// K.H.
//////////////////////////////////////////////////////////////////////////////
class ImageFlipX : public CImageProcessorBase<ImageFlipX>
{
public:
	ImageFlipX () :  busy_(false) {}
	~ImageFlipX () {  }

	int Shutdown() {return DEVICE_OK;}
	void GetName(char* name) const {strcpy(name,"ImageFlipX");}

	int Initialize();
	bool Busy(void) { return busy_;};

	template <typename PixelType>
	int Flip(PixelType* pI, unsigned int width, unsigned int height)
	{
		PixelType tmp;
		int ret = DEVICE_OK;
		for( unsigned long iy = 0; iy < height; ++iy)
		{
			for( unsigned long ix = 0; ix <  (width>>1) ; ++ix)
			{
				tmp = pI[ ix + iy*width];
				pI[ ix + iy*width] = pI[ width - 1 - ix + iy*width];
				pI[ width -1 - ix + iy*width] = tmp;
			}
		}
		return ret;
	}

	int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

	int OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	bool busy_;
	MM::MMTime performanceTiming_;
};


//////////////////////////////////////////////////////////////////////////////
// ImageFlipY class
// flip an image
// K.H.
//////////////////////////////////////////////////////////////////////////////
class ImageFlipY : public CImageProcessorBase<ImageFlipY>
{
public:
	ImageFlipY () : busy_(false), performanceTiming_(0.) {}
	~ImageFlipY () {  }

	int Shutdown() {return DEVICE_OK;}
	void GetName(char* name) const {strcpy(name,"ImageFlipY");}

	int Initialize();
	bool Busy(void) { return busy_;};

	template <typename PixelType>
	int Flip(PixelType* pI, unsigned int width, unsigned int height)
	{
		PixelType tmp;
		int ret = DEVICE_OK;
		for( unsigned long ix = 0; ix < width ; ++ix)
		{
			for( unsigned long iy = 0; iy < (height>>1); ++iy)
			{
				tmp = pI[ ix + iy*width];
				pI[ ix + iy*width] = pI[ ix + (height - 1 - iy)*width];
				pI[ ix + (height - 1 - iy)*width] = tmp;
			}
		}
		return ret;
	}


	int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

	// action interface
	// ----------------
	int OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	bool busy_;
	MM::MMTime performanceTiming_;

};



//////////////////////////////////////////////////////////////////////////////
// MedianFilter class
// apply Median filter an image
// K.H.
//////////////////////////////////////////////////////////////////////////////
class MedianFilter : public CImageProcessorBase<MedianFilter>
{
public:
	MedianFilter () : busy_(false), performanceTiming_(0.),pSmoothedIm_(0), sizeOfSmoothedIm_(0)
	{
		// parent ID display
		//CreateHubIDProperty();
	};
	~MedianFilter () { if(0!=pSmoothedIm_) free(pSmoothedIm_); };

	int Shutdown() {return DEVICE_OK;}
	void GetName(char* name) const {strcpy(name,"MedianFilter");}

	int Initialize();
	bool Busy(void) { return busy_;};

	// NOTE: this utility MODIFIES the argument, make a copy yourself if you want the original data preserved
	template <class U> U FindMedian(std::vector<U>& values ) {
		std::sort(values.begin(), values.end());
		return values[(values.size())>>1];
	};


	template <typename PixelType>
	int Filter(PixelType* pI, unsigned int width, unsigned int height)
	{
		int ret = DEVICE_OK;
		int x[9];
		int y[9];

		const unsigned long thisSize = sizeof(*pI)*width*height;
		if( thisSize != sizeOfSmoothedIm_)
		{
			if(NULL!=pSmoothedIm_)
			{
				sizeOfSmoothedIm_ = 0;
				free(pSmoothedIm_);
			}
			// malloc is faster than new...
			pSmoothedIm_ = (PixelType*)malloc(thisSize);
			if(NULL!=pSmoothedIm_)
			{
				sizeOfSmoothedIm_ = thisSize;
			}
		}

		PixelType* pSmooth = (PixelType*) pSmoothedIm_;

		if(NULL != pSmooth)
		{
			/*Apply 3x3 median filter to reduce shot noise*/
			for (unsigned int i=0; i<width; i++) {
				for (unsigned int j=0; j<height; j++) {
					x[0]=i-1;
					y[0]=(j-1);
					x[1]=i;
					y[1]=(j-1);
					x[2]=i+1;
					y[2]=(j-1);
					x[3]=i-1;
					y[3]=(j);
					x[4]=i;
					y[4]=(j);
					x[5]=i+1;
					y[5]=(j);
					x[6]=i-1;
					y[6]=(j+1);
					x[7]=i;
					y[7]=(j+1);
					x[8]=i+1;
					y[8]=(j+1);
					// truncate the median filter window  -- duplicate edge points
					// this could be more efficient, we could fill in the interior image [1,w0-1]x[1,h0-1] then explicitly fill in the edge pixels.
					// also the temporary image could be as small as 2 rasters of the image
					for(int ij =0; ij < 9; ++ij)
					{
						if( x[ij] < 0)
							x[ij] = 0;
						else if( int(width-1) < x[ij])
							x[ij] = int(width-1);
						if( y[ij] < 0)
							y[ij] = 0;
						else if( int(height-1) < y[ij])
							y[ij] = (int)(height-1);
					}
					std::vector<PixelType> windo;
					for(int ij = 0; ij < 9; ++ij)
					{
						windo.push_back(pI[ x[ij] + width*y[ij]]);
					}
					pSmooth[i + j*width] = FindMedian(windo);
				}
			}

			memcpy( pI, pSmoothedIm_, thisSize);
		}
		else
			ret = DEVICE_ERR;

		return ret;
	}
	int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

	// action interface
	// ----------------
	int OnPerformanceTiming(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	bool busy_;
	MM::MMTime performanceTiming_;
	void*  pSmoothedIm_;
	unsigned long sizeOfSmoothedIm_;



};

#endif //_VS14M_H_
