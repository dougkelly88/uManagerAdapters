///////////////////////////////////////////////////////////////////////////////
// FILE:          Kentech.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech devices. 
//                
// AUTHOR:        Doug Kelly, dk1109@ic.ac.uk, 01/09/2014
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
//

#ifndef _KENTECH_H_
#define _KENTECH_H_


static const char* g_HDG800DeviceName = "KentechHDG800";
static const char* g_HDGDeviceName = "KentechHDG";
static const char* g_SEDeviceName = "KentechSingleEdgeHRI";
static const char* g_HRIDeviceName = "KentechStandardHRI";
static const char* g_PPDGDeviceName = "KentechSlowDelayBox";

static const char* g_addScanPos = "Add current delay to scan at current position";
static const char* default_calib_path = "C:\\Program Files (x86)\\Micro-Manager-1.4-32 mid-August build\\mmplugins\\Kentech calibration\\HDG800 delay calibration.csv";


//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_POSITION 101
#define ERR_INITIALIZE_FAILED 102
#define ERR_WRITE_FAILED 103
#define ERR_CLOSE_FAILED 104
#define ERR_BOARD_NOT_FOUND 105
#define ERR_PORT_OPEN_FAILED 106
#define ERR_COMMUNICATION 107
#define ERR_NO_PORT_SET 108
#define ERR_VERSION_MISMATCH 109
#define ERR_UNRECOGNIZED_ANSWER 110
#define ERR_PORT_CHANGE_FORBIDDEN 111
#define ERR_OPENFILE_FAILED 112
#define ERR_CALIBRATION_FAILED 113
#define ERR_UNRECOGNISED_PARAM_VALUE 114


#endif //_KENTECH_H_