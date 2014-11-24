///////////////////////////////////////////////////////////////////////////////
// FILE:          Kentech.cpp
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

#include "HDG800.h"
#include "SingleEdge.h"
#include "Kentech.h"
#include "HDG.h"
#include "StandardHRI.h"
#include "SlowDelayBox.h"


///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_HDG800DeviceName, MM::GenericDevice, "Kentech HDG800 Delay Generator");
	RegisterDevice(g_SEDeviceName, MM::GenericDevice, "Kentech Single Edge High Rate Imager");
	RegisterDevice(g_HDGDeviceName, MM::GenericDevice, "Kentech HDG Delay Generator");
	RegisterDevice(g_HRIDeviceName, MM::GenericDevice, "Kentech Standard High Rate Imager");
	RegisterDevice(g_PPDGDeviceName, MM::GenericDevice, "Kentech Precision Programmable Delay Generator");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_HDG800DeviceName) == 0)
	{
		return new KHDG800();
	}
	if (strcmp(deviceName, g_SEDeviceName) == 0)
	{
		return new KSE();
	}
	if (strcmp(deviceName, g_HDGDeviceName) == 0)
	{
		return new KHDG();
	}
	if (strcmp(deviceName, g_HRIDeviceName) == 0)
	{
		return new KHRI();
	}
	if (strcmp(deviceName, g_PPDGDeviceName) == 0)
	{
		return new KSDB();
	}
	// ...supplied name not recognized
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}
