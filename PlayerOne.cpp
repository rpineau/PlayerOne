//
//  PlayerOne.cpp
//
//  Created by Rodolphe Pineau on 2022/07/06
//  Copyright © 2022 Rodolphe Pineau. All rights reserved.
//


#include "PlayerOne.h"


CPlayerOne::CPlayerOne()
{

	m_sensorModeInfo.clear();
	m_ControlList.clear();
	m_GainList.clear();
	m_sCameraName.clear();
	m_sCameraSerial.clear();
	m_mAvailableFrameRate.clear();

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
	m_sLogfilePath = getenv("HOMEDRIVE");
	m_sLogfilePath += getenv("HOMEPATH");
	m_sLogfilePath += "\\PlayerOneLog.txt";
#elif defined(SB_LINUX_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/PlayerOneLog.txt";
#elif defined(SB_MAC_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/PlayerOneLog.txt";
#endif
	m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

	std::string sSDKVersion;
	getFirmwareVersion(sSDKVersion);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] Version " << std::fixed << std::setprecision(2) << CODE_VERSION << " build " << __DATE__ << " " << __TIME__ << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] " << sSDKVersion << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] Constructor Called." << std::endl;
	m_sLogFile.flush();
#endif

	std::vector<camera_info_t> tCameraIdList;
	listCamera(tCameraIdList);
}

CPlayerOne::~CPlayerOne()
{
	Disconnect();

	if(m_pframeBuffer)
		free(m_pframeBuffer);


}

#pragma mark - Camera access
void CPlayerOne::setUserConf(bool bUserConf)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]" << " [setUserConf]  Set m_bSetUserConf to : " << (bUserConf?"Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif

	m_bSetUserConf = bUserConf;
}

int CPlayerOne::Connect(std::string sSerial)
{
	int nErr = PLUGIN_OK;
	int i;

	POAErrors ret;
	POAConfigAttributes Attribute;
	POASensorModeInfo sensorMode;
	std::string sTmp;

	long nMin, nMax, nValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect]  Tryng to connect to camera with serial : " << m_sCameraSerial << std::endl;
		m_sLogFile.flush();
#endif

	if(!sSerial.size())
		return ERR_NODEVICESELECTED;

	m_sCameraSerial.assign(sSerial);

	nErr = getCameraIdFromSerial(m_nCameraID, m_sCameraSerial);
	if(nErr) {
		if(POAGetCameraCount() >= 1) {
			std::vector<camera_info_t> tCameraIdList;
			listCamera(tCameraIdList);
			if(tCameraIdList.size()) {
				m_nCameraID = tCameraIdList[0].cameraId;
				m_sCameraSerial.assign(tCameraIdList[0].Sn);
			}
			else
				return ERR_NODEVICESELECTED;
		}
		else
			return ERR_NODEVICESELECTED;
	}
	ret = POAOpenCamera(m_nCameraID);
	if (ret != POA_OK) { // we had a camera selected but it's not connected,
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect]  Error connecting to camera ID " << m_nCameraID << " serial " << m_sCameraSerial << " , Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_NODEVICESELECTED;
	}

	ret = POAInitCamera(m_nCameraID);
	if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error inittializing camera , Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	m_bConnected = true;
	getCameraNameFromID(m_nCameraID, m_sCameraName);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected to camera ID  " << m_nCameraID << " Serial " << m_sCameraSerial << std::endl;
	m_sLogFile.flush();
#endif

	ret = POAGetCameraPropertiesByID(m_nCameraID, &m_cameraProperty);
	if (ret != POA_OK)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] POAGetCameraProperties Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERR_CMDFAILED;
	}

	if(m_cameraProperty.bitDepth >8) {
		m_nImageFormat = POA_RAW16;
		m_nNbBitToShift = 0;
	}
	else {
		m_nImageFormat = POA_RAW8;
		m_nNbBitToShift = 8;
	}
	m_dPixelSize = m_cameraProperty.pixelSize;
	m_nNbBin = 0;
	m_nCurrentBin = 0;

	for (i = 0; i < MAX_NB_BIN; i++) {
		m_SupportedBins[i] = m_cameraProperty.bins[i];
		if (m_cameraProperty.bins[i] == 0) {
			break;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_SupportedBins[" << i <<"] : " << m_SupportedBins[i] << std::endl;
		m_sLogFile.flush();
#endif
		if(m_SupportedBins[i] == 1)
			m_nCurrentBin = 1;  // set default bin to 1 if available.
		m_nNbBin++;
	}

	if(!m_nCurrentBin)
		m_nCurrentBin = m_SupportedBins[0]; // if bin 1 was not availble .. use first bin in the array

	m_bHasHardwareBin = m_cameraProperty.isSupportHardBin;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Camera properties:" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.maxWidth        : " << m_cameraProperty.maxWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.maxHeigh        : " << m_cameraProperty.maxHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.bitDepth        : " << m_cameraProperty.bitDepth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.isColorCamera   : " << (m_cameraProperty.isColorCamera?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.isHasST4Port    : " << (m_cameraProperty.isHasST4Port?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.isHasCooler     : " << (m_cameraProperty.isHasCooler?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.isUSB3Speed     : " << (m_cameraProperty.isUSB3Speed?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.bayerPattern    : " << m_cameraProperty.bayerPattern << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.pixelSize       : " << m_cameraProperty.pixelSize << "um" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.SN              : " << m_cameraProperty.SN << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.sensorModelName : " << m_cameraProperty.sensorModelName << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nNbBitToShift                  : " << m_nNbBitToShift << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nNbBin                         : " << m_nNbBin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_cameraProperty.isSupportHardBin: " << (m_cameraProperty.isSupportHardBin?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_dPixelSize                     : " << m_dPixelSize << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Setting ROI to max size." << std::endl;
	m_sLogFile.flush();

#endif
	nErr = setBinSize(1);
	if (nErr)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] setBinSize Error = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERR_CMDFAILED;
	}

	nErr = setROI(0, 0, m_cameraProperty.maxWidth, m_cameraProperty.maxHeight);
	if (nErr)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] setROI Error = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERR_CMDFAILED;
	}

	ret = POAGetConfigsCount(m_nCameraID, &m_nControlNums);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] POAGetCameraProperties Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERR_CMDFAILED;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nControlNums  = " << m_nControlNums << std::endl;
	m_sLogFile.flush();
#endif

	for (i = 0; i < m_nControlNums; i++) {
		ret = POAGetConfigAttributes(m_nCameraID, i, &Attribute);
		if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error getting attribute, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			continue;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Name   = " << Attribute.szConfName << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Description   = " << Attribute.szDescription << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute type   = " << Attribute.valueType << std::endl;
		if(Attribute.valueType == VAL_INT) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Min Value   = " << Attribute.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Max Value   = " << Attribute.maxValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Default Value   = " << Attribute.defaultValue.intValue << std::endl;
		}
		else if (Attribute.valueType == VAL_FLOAT) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute MinValue   = " << Attribute.minValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute MaxValue   = " << Attribute.maxValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Default Value   = " << Attribute.defaultValue.floatValue << std::endl;
		}
		else if (Attribute.valueType == VAL_BOOL) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute MinValue   = " << (Attribute.minValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute MaxValue   = " << (Attribute.maxValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute Default Value   = " << (Attribute.defaultValue.boolValue?"True":"False") << std::endl;
		}

		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute isSupportAuto   = " << (Attribute.isSupportAuto?"Yes":"No") << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute isWritable   = " << Attribute.isWritable << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Attribute isReadable   = " << Attribute.isReadable << std::endl << "-------------------------------" << std::endl;
		m_sLogFile.flush();
#endif
		m_ControlList.push_back(Attribute);
	}

	// get usefull gains and offsets
	ret = POAGetGainsAndOffsets(m_nCameraID, &m_nGainHighestDR, &m_nHCGain, &m_nUnityGain, &m_nGainLowestRN, &m_nOffsetHighestDR, &m_nOffsetHCGain, &m_nOffsetUnityGain, &m_nOffsetLowestRN);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at highest dynamic range        : " << m_nOffsetHighestDR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Gain at HCG Mode(High Conversion Gain) : " << m_nHCGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Gain at unity                          : " << m_nUnityGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Gain at lowest read noise              : " << m_nGainLowestRN << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at highest dynamic range        : " << m_nOffsetHighestDR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at HCG                          : " << m_nOffsetHCGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at unity gain                   : " << m_nOffsetUnityGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at lowest read noise            : " << m_nOffsetLowestRN << std::endl;
	m_sLogFile.flush();
#endif

	POAGetSensorModeCount(m_nCameraID, &m_nSensorModeCount);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Number of sensor mode : " << m_nSensorModeCount << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_bSetUserConf : " << (m_bSetUserConf?"Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif
	m_bHasLensHeater = true;
	nErr = getLensHeaterPowerPerc(nMin, nMax, nValue);
	if(nErr == VAL_NOT_AVAILABLE) {
		m_bHasLensHeater = false;
		nErr = PLUGIN_OK;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_bHasLensHeater : " << (m_bHasLensHeater?"Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif

	if(m_bSetUserConf) {
		// set default values
		setGain(m_nGain);
		setOffset(m_nOffset);
		setWB_R(m_nWbR, m_bR_Auto);
		setWB_G(m_nWbG, m_bG_Auto);
		setWB_B(m_nWbB, m_bB_Auto);
		setFlip(m_nFlip);
		setUSBBandwidth(m_nUSBBandwidth);
		setPixelBinMode(m_bPixelBinMode);
		// color camera
		if(m_cameraProperty.isColorCamera) {
			if(m_bHasHardwareBin && m_bHardwareBinEnabled) {
				setHardwareBinOn(m_bHardwareBinEnabled);
			}
			else if(!m_bHasHardwareBin || !m_bHardwareBinEnabled) {
				if(m_bHasHardwareBin && !m_bHardwareBinEnabled) {
					setHardwareBinOn(m_bHardwareBinEnabled);
				}
				setMonoBin(m_bPixelMonoBin);
				setPixelBinMode(m_bPixelBinMode);
			}
		} // mono camera
		else {
			// hardware bin
			if(m_bHasHardwareBin && m_bHardwareBinEnabled) {
				setHardwareBinOn(m_bHardwareBinEnabled);
			}
			else if(!m_bHasHardwareBin || !m_bHardwareBinEnabled) {
				if(m_bHasHardwareBin && !m_bHardwareBinEnabled) {
					setHardwareBinOn(m_bHardwareBinEnabled);
				}
				setMonoBin(m_bPixelMonoBin);
			}
		}
		setLensHeaterPowerPerc(m_nLensHeaterPowerPerc);

	}
	else {
		setGain(m_nHCGain);
		m_nGain = m_nHCGain;
		setOffset(m_nOffsetHCGain);
		m_nOffset = m_nOffsetHCGain;
		getOffset(nMin, nMax, m_nOffset);
		getWB_R(nMin, nMax, m_nWbR, m_bR_Auto);
		getWB_G(nMin, nMax, m_nWbG, m_bG_Auto);
		getWB_B(nMin, nMax, m_nWbB, m_bB_Auto);
		getFlip(nMin, nMax, m_nFlip);
		getUSBBandwidth(nMin, nMax, m_nUSBBandwidth);
		getMonoBin(m_bPixelMonoBin);
		getPixelBinMode(m_bPixelBinMode);
		if(m_bHasHardwareBin)
			getHardwareBinOn(m_bHardwareBinEnabled);
		else
			m_bHardwareBinEnabled = false;
		getLensHeaterPowerPerc(nMin, nMax, m_nLensHeaterPowerPerc);
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] after m_bSetUserConf, m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif

	rebuildGainList();

	ret = POASetImageFormat(m_nCameraID, m_nImageFormat);


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode count : " << m_nSensorModeCount << std::endl;
	m_sLogFile.flush();
#endif
	// if the camera supports it, in general, there are at least two sensor modes[Normal, LowNoise, ...]
	m_sensorModeInfo.clear();
	for(i=0; i< m_nSensorModeCount; i++) {
		ret = POAGetSensorModeInfo(m_nCameraID, i, &sensorMode);
		if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error getting sensor mode info : "<< ret << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error getting sensor mode info : "<< POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			continue;
		}
		m_sensorModeInfo.push_back(sensorMode);
		sTmp.assign(sensorMode.name);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode " << i << " name : " << sensorMode.name << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode " << i << " desc : " << sensorMode.desc << std::endl;
		m_sLogFile.flush();
#endif
		if(m_nSensorModeIndex == VAL_NOT_AVAILABLE) { // there was no values set
			if(std::string(sensorMode.name).find("Low Noise")!=-1) {
				m_nSensorModeIndex = i;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Setting Low Noise mode as the default as nothing was saved" << std::endl;
				m_sLogFile.flush();
#endif
			}
		}

	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] after building m_sensorModeInfo, m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif

	if(m_nSensorModeCount) {
		if(m_nSensorModeIndex == VAL_NOT_AVAILABLE) {
			getCurentSensorMode(sTmp, m_nSensorModeIndex);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] got sensor mode from camera = " << m_nSensorModeIndex << std::endl;
			m_sLogFile.flush();
#endif
		}
		else {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] setting sensor mode to " << m_nSensorModeIndex << std::endl;
			m_sLogFile.flush();
#endif
			ret = (POAErrors)setSensorMode(m_nSensorModeIndex);
			if(ret) {
				setSensorMode(0);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error setting sensor mode to " << m_nSensorModeIndex<<", Error = " << POAGetErrorString(ret) << std::endl;
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Setting to default mode" << std::endl;
				m_sLogFile.flush();
#endif
			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected, nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	POAStopExposure(m_nCameraID); // make sure nothing is running
	return nErr;
}

void CPlayerOne::Disconnect()
{
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Disconnect."<< std::endl;
	m_sLogFile.flush();
#endif


	nErr = POAStopExposure(m_nCameraID);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] POAStopExposure : " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	nErr = POACloseCamera(m_nCameraID);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] POACloseCamera : " << nErr << std::endl;
	m_sLogFile.flush();
#endif

	if(m_pframeBuffer) {
		free(m_pframeBuffer);
		m_pframeBuffer = NULL;
	}
	m_bConnected = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Disconnect done."<< std::endl;
	m_sLogFile.flush();
#endif

}

int CPlayerOne::getCameraIdFromSerial(int &nCameraId, std::string sSerial)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] sSerial = " << sSerial << std::endl;
	m_sLogFile.flush();

#endif
	nCameraId = -1;

	int cameraNum = POAGetCameraCount();
	for (int i = 0; i < cameraNum; i++)
	{
		ret = POAGetCameraProperties(i, &m_cameraProperty);
		if (ret == POA_OK)
		{
			if(sSerial==m_cameraProperty.SN) {
				nCameraId = m_cameraProperty.cameraID;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] found nCameraId = " << nCameraId << std::endl;
				m_sLogFile.flush();
#endif
				break;
			}
		}
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	if(nCameraId>=0) {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] camera id  = " << nCameraId <<" SN: "<< sSerial << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] camera id  not found for SN: "<< sSerial << std::endl;
	}
	m_sLogFile.flush();
#endif
	if(nCameraId<0)
		nErr = ERR_NODEVICESELECTED;
	return nErr;
}

int CPlayerOne::getCameraSerialFromID(int nCameraId, std::string &sSerial)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] nCameraId = " << nCameraId << std::endl;
	m_sLogFile.flush();
#endif

	if(nCameraId<0)
		return ERR_NODEVICESELECTED;

	sSerial.clear();
	int cameraNum = POAGetCameraCount();
	for (int i = 0; i < cameraNum; i++)
	{
		ret = POAGetCameraProperties(i, &m_cameraProperty);
		if (ret == POA_OK)
		{
			if(nCameraId==m_cameraProperty.cameraID) {
				sSerial = m_cameraProperty.SN;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] sSerial = " << sSerial << std::endl;
				m_sLogFile.flush();
#endif
				break;
			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	if(sSerial.size()) {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] camera id  = " << nCameraId <<" SN: "<< sSerial << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] camera serial  not found for id: "<< nCameraId << std::endl;
	}
	m_sLogFile.flush();
#endif
	return nErr;
}

void CPlayerOne::getCameraNameFromID(int nCameraId, std::string &sName)
{

	POAErrors ret;
	sName.clear();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraNameFromID] nCameraId = " << nCameraId << std::endl;
	m_sLogFile.flush();
#endif

	int cameraNum = POAGetCameraCount();
	for (int i = 0; i < cameraNum; i++)
	{
		ret = POAGetCameraProperties(i, &m_cameraProperty);
		if (ret == POA_OK)
		{
			if(nCameraId==m_cameraProperty.cameraID) {
				sName.assign(m_cameraProperty.cameraModelName);
				break;
			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	if(sName.size()) {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraNameFromID] camera id  = " << nCameraId <<" name : "<< sName << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraNameFromID] camera name  not found for id: "<< nCameraId << std::endl;

	}
	m_sLogFile.flush();
#endif
}

void CPlayerOne::getCameraSerial(std::string &sSerial)
{
	sSerial.assign(m_sCameraSerial);
}

void CPlayerOne::getCameraName(std::string &sName)
{
	sName.assign(m_sCameraName);
}

int CPlayerOne::listCamera(std::vector<camera_info_t>  &cameraIdList)
{
	int nErr = PLUGIN_OK;
	camera_info_t   tCameraInfo;

	cameraIdList.clear();
	m_nCameraNum = POAGetCameraCount();

	POAErrors ret;
	for (int i = 0; i < m_nCameraNum; i++)
	{
		ret = POAGetCameraProperties(i, &m_cameraProperty);
		if (ret == POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Name : " << m_cameraProperty.cameraModelName << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] USB Port type : " << (m_cameraProperty.isUSB3Speed?"USB3":"USB2") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] SN  : " << m_cameraProperty.SN << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Camera ID : " << m_cameraProperty.cameraID << std::endl;
			m_sLogFile.flush();
#endif
			tCameraInfo.cameraId = m_cameraProperty.cameraID;
			tCameraInfo.model.assign(m_cameraProperty.cameraModelName);
			tCameraInfo.Sn.assign(m_cameraProperty.SN);
			cameraIdList.push_back(tCameraInfo);
		}
	}
	return nErr;
}

void CPlayerOne::getFirmwareVersion(std::string &sVersion)
{
	std::stringstream ssTmp;
	ssTmp << " API V" << POAGetAPIVersion() << ", SDK "<<POAGetSDKVersion();
	sVersion.assign(ssTmp.str());
}

void CPlayerOne::isUSB3(bool &bUSB3)
{
	bUSB3 = m_cameraProperty.isUSB3Speed;
}

int CPlayerOne::getNumBins()
{
	return m_nNbBin;
}

int CPlayerOne::getBinFromIndex(int nIndex)
{
	if(!m_bConnected)
		return 1;

	if(nIndex>(m_nNbBin-1))
		return 1;

	return m_SupportedBins[nIndex];
}

int CPlayerOne::getCurentSensorMode(std::string &sSensorMode, int &nModeIndex)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	nModeIndex = 0; // default mode
	sSensorMode.clear();

	if(!m_nSensorModeCount)
		return VAL_NOT_AVAILABLE;

	if(m_sensorModeInfo.size()==0)
		return VAL_NOT_AVAILABLE;

	ret =  POAGetSensorMode(m_nCameraID, &nModeIndex);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCurentSensorMode] Error getting current sensor mode." << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCurentSensorMode] nModeIndex = " << nModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCurentSensorMode] m_sensorModeInfo size = " <<  m_sensorModeInfo.size() << std::endl;
	m_sLogFile.flush();
#endif
	if(nModeIndex>=m_sensorModeInfo.size()) {
		// not good
		sSensorMode.assign("Bad index");
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCurentSensorMode] Error Bad index nModeIndex >= m_sensorModeInfo.size() " << std::endl;
		m_sLogFile.flush();
#endif
	}
	else
		sSensorMode.assign(m_sensorModeInfo[nModeIndex].name);
	return nErr;
}

int CPlayerOne::getSensorModeList(std::vector<std::string> &svModes, int &curentModeIndex)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	svModes.clear();
	curentModeIndex = -1;

	if(!m_nSensorModeCount) // sensor mode no supported by camera
		return VAL_NOT_AVAILABLE;

	if(m_sensorModeInfo.size()==0)
		return VAL_NOT_AVAILABLE;

	ret = POAGetSensorMode(m_nCameraID, &curentModeIndex);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorModeList] POAGetSensorMode Error getting current sensor mode." << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorModeList] curentModeIndex = " << curentModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorModeList] m_sensorModeInfo size = " <<  m_sensorModeInfo.size() << std::endl;
	m_sLogFile.flush();
#endif

	svModes.clear();
	for (POASensorModeInfo mode : m_sensorModeInfo) {
		svModes.push_back(mode.name);
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorModeList] Current Sensor mode is " << svModes.at(curentModeIndex) << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CPlayerOne::setSensorMode(int nModeIndex)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	m_nSensorModeIndex = nModeIndex;

	if(!m_bConnected)
		return ERR_NOLINK;

	if(!m_nSensorModeCount) // sensor mode no supported by camera
		return ERR_COMMANDNOTSUPPORTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setSensorMode] Setting sensor mode to index  = " << nModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setSensorMode] Sensor mode name   = " << m_sensorModeInfo[nModeIndex].name << std::endl;
	m_sLogFile.flush();
#endif

	ret = POASetSensorMode(m_nCameraID, m_nSensorModeIndex);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setSensorMode] Error setting sensor mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERR_CMDFAILED;
	}

	return nErr;
}

int CPlayerOne::getPixelBinMode(bool &bSumMode)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPixelBinMode] called." << std::endl;
	m_sLogFile.flush();
#endif

	ret = getConfigValue(POA_PIXEL_BIN_SUM, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPixelBinMode] Error getting camera pixel mode. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	// POA_TRUE is sum and POA_FALSE is average,
	bSumMode = (confValue.boolValue == POA_TRUE);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPixelBinMode] Pixel bin mode confValue.boolValue =  " << (confValue.boolValue==POA_TRUE?"POA_TRUE":"POA_FALSE") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPixelBinMode] Pixel bin mode set to " << (bSumMode?"Sum":"Average") << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}


int CPlayerOne::setPixelBinMode(bool bSumMode)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;
	m_bPixelBinMode = bSumMode;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPixelBinMode] called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERR_NOLINK;
	// POA_TRUE is sum and POA_FLASE is average
	confValue.boolValue = m_bPixelBinMode?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPixelBinMode] Pixel bin mode set to " << (bSumMode?"Sum":"Average") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_PIXEL_BIN_SUM, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPixelBinMode] Error setting Pixel bin mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	return nErr;
}


int CPlayerOne::getMonoBin(bool &bMonoBin)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMonoBin] called." << std::endl;
	m_sLogFile.flush();
#endif
	bMonoBin = false;
	ret = getConfigValue(POA_MONO_BIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMonoBin] Error getting camera pixel mono bin. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}

	// POA_TRUE is mono bin and POA_FALSE is no mono bin,
	bMonoBin = (confValue.boolValue == POA_TRUE);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMonoBin] Pixel mono bin confValue.boolValue =  " << (confValue.boolValue==POA_TRUE?"POA_TRUE":"POA_FALSE") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMonoBin] Pixel mono bin set to " << (bMonoBin?"Mono":"Color") << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CPlayerOne::setMonoBin(bool bMonoBin)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;
	m_bPixelMonoBin = bMonoBin;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMonoBin] called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERR_NOLINK;
	// POA_TRUE is mono bin and POA_FALSE is no mono bin,
	confValue.boolValue = m_bPixelMonoBin?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMonoBin] Pixel mono bin set to " << (bMonoBin?"Mono":"Color") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_MONO_BIN, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMonoBin] Error setting Pixel mono bin, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	return nErr;
}



void CPlayerOne::getAllUsefulValues(int &nGainHighestDR, int &nHCGain, int &nUnityGain, int &nGainLowestRN,
									int &nOffsetHighestDR, int &nOffsetHCGain, int &nOffsetUnityGain, int &nOffsetLowestRN)
{
	nGainHighestDR = m_nGainHighestDR;
	nHCGain = m_nHCGain;
	nUnityGain = m_nUnityGain;
	nGainLowestRN = m_nGainLowestRN;
	nOffsetHighestDR = m_nOffsetHighestDR;
	nOffsetHCGain = m_nOffsetHCGain;
	nOffsetUnityGain = m_nOffsetUnityGain;
	nOffsetLowestRN = m_nOffsetUnityGain;
}

#pragma mark - Camera capture

int CPlayerOne::startCaputure(double dTime)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	int nTimeout;
	m_bAbort = false;
	POACameraState cameraState;
	POAConfigValue exposure_value;

	nTimeout = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] Start Capture." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] Waiting for camera to be idle." << std::endl;
	m_sLogFile.flush();
#endif
	// POAStopExposure(m_nCameraID);

	ret = POAGetCameraState(m_nCameraID, &cameraState);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] Error getting camera state. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	if(cameraState != STATE_OPENED) {
		return ERR_COMMANDINPROGRESS;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] Starting Capture." << std::endl;
	m_sLogFile.flush();
#endif

	// set exposure time (s -> us)
	exposure_value.intValue = (int)(dTime * 1000000);
	ret = POASetConfig(m_nCameraID, POA_EXPOSURE, exposure_value, POA_FALSE); //set exposure time
	if(ret!=POA_OK)
		return ERR_CMDFAILED;

	ret = POAStartExposure(m_nCameraID, POA_TRUE); // single frame(Snap mode)
	// ret = POAStartExposure(m_nCameraID, POA_FALSE); // continuous exposure mode
	if(ret!=POA_OK)
		nErr =ERR_CMDFAILED;

	m_dCaptureLenght = dTime;
	m_ExposureTimer.Reset();
	return nErr;
}


void CPlayerOne::abortCapture(void)
{
	POAErrors ret;
	m_bAbort = true;

	ret = POAStopExposure(m_nCameraID);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [abortCapture] capture aborted." << std::endl;
	m_sLogFile.flush();
#endif
}

#pragma mark - Camera controls

int CPlayerOne::getTemperture(double &dTemp, double &dPower, double &dSetPoint, bool &bEnabled)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	ret = getConfigValue(POA_TEMPERATURE, confValue, minValue, maxValue, bAuto);

	if(ret == POA_OK) {
		dTemp = confValue.floatValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] dTemp " << dTemp << std::endl;
		m_sLogFile.flush();
#endif
		if(m_cameraProperty.isHasCooler) {
			ret = getConfigValue(POA_TARGET_TEMP, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				dSetPoint = double(confValue.intValue);
			else
				dSetPoint = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] dSetPoint " << dSetPoint << std::endl;
			m_sLogFile.flush();
#endif

			ret = getConfigValue(POA_COOLER_POWER, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				dPower = double(confValue.intValue);
			else
				dPower = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] dPower " << dPower << std::endl;
			m_sLogFile.flush();
#endif

			ret = getConfigValue(POA_COOLER, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				bEnabled = bool(confValue.boolValue);
			else
				bEnabled = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] bEnabled " << (bEnabled?"Yes":"No") << std::endl;
			m_sLogFile.flush();
#endif

		}
		else {
			dPower = 0;
			dSetPoint = dTemp;
			bEnabled = false;
		}
	}
	else {
		dTemp = -100;
		dPower = 0;
		dSetPoint = dTemp;
		bEnabled = false;
	}
	return nErr;
}

int CPlayerOne::setCoolerTemperature(bool bOn, double dTemp)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	if(m_cameraProperty.isHasCooler) {
		confValue.intValue = int(dTemp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Setting temperature to " << int(dTemp) << std::endl;
		m_sLogFile.flush();
#endif

		ret = setConfigValue(POA_TARGET_TEMP, confValue);
		if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Error setting temperature, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}

		confValue.boolValue = (bOn?POA_TRUE:POA_FALSE);
		ret = setConfigValue(POA_COOLER, confValue);
		if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Error setting cooler, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
	}
	return nErr;
}

int CPlayerOne::getWidth()
{
	return m_cameraProperty.maxWidth;
}

int CPlayerOne::getHeight()
{
	return m_cameraProperty.maxHeight;
}

double CPlayerOne::getPixelSize()
{
	return m_dPixelSize;
}

int CPlayerOne::setBinSize(int nBin)
{
	POAErrors ret;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setBinSize] nBin = " << nBin << std::endl;
	m_sLogFile.flush();
#endif

	m_nCurrentBin = nBin;
	ret = POASetImageBin(m_nCameraID, m_nCurrentBin);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setBinSize] Error setting bin size, Error = " << ret << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	return PLUGIN_OK;
}

int CPlayerOne::getBinSize(int &nBin)
{
	POAErrors ret;

	ret = POAGetImageBin(m_nCameraID, &nBin);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setBinSize] Error setting bin size, Error = " << ret << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getBinSize] nBin = " << nBin << std::endl;
	m_sLogFile.flush();
#endif

	return PLUGIN_OK;
}

bool CPlayerOne::isCameraColor()
{
	return m_cameraProperty.isColorCamera;
}

void CPlayerOne::getBayerPattern(std::string &sBayerPattern)
{
	if(m_cameraProperty.isColorCamera) {
		if(m_nCurrentBin>1 && (m_bPixelMonoBin || m_bHasHardwareBin) ) { // mono bin mode, return MONO
			sBayerPattern.assign("MONO");
			return;
		}
		switch(m_cameraProperty.bayerPattern) {
			case POA_BAYER_RG:
				sBayerPattern.assign("RGGB");
				break;
			case POA_BAYER_BG:
				sBayerPattern.assign("BGGR");
				break;
			case POA_BAYER_GR:
				sBayerPattern.assign("GRBG");
				break;
			case POA_BAYER_GB:
				sBayerPattern.assign("GBRG");
				break;
			case POA_BAYER_MONO:
				sBayerPattern.assign("MONO");
				break;
			default:
				sBayerPattern.assign("MONO");
				break;
		}
	}
	else {
		sBayerPattern.assign("MONO");
	}
}

void CPlayerOne::getFlip(std::string &sFlipMode)
{
	switch(m_nFlip) {
		case POA_FLIP_NONE :
			sFlipMode.assign("None");
			break;
		case POA_FLIP_HORI :
			sFlipMode.assign("Horizontal");
			break;
		case POA_FLIP_VERT :
			sFlipMode.assign("Vertical");
			break;
		case POA_FLIP_BOTH :
			sFlipMode.assign("both horizontal and vertical");
			break;
		default:
			sFlipMode.clear();
			break;
	}
}


int CPlayerOne::getGain(long &nMin, long &nMax, long &nValue)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_GAIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getGain] Error getting gain, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}

	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getGain] Gain is " << nValue << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setGain(long nGain)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setGain] Called with gain =  " << nGain << std::endl;
	m_sLogFile.flush();
#endif

	m_nGain = nGain;
	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nGain;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setGain] Setting Gain to " << m_nGain << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_GAIN, confValue);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setGain] Error setting gain, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERR_CMDFAILED;
	}
	return nErr;
}


int CPlayerOne::getWB_R(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	ret = getConfigValue(POA_WB_R, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] Error getting WB_R, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] WB_R is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] WB_R min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] WB_R max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setWB_R(long nWB_R, bool bIsAuto)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nWbR = nWB_R;
	m_bR_Auto = bIsAuto;

	if(!m_bConnected)
		return nErr;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	confValue.intValue = nWB_R;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_R] WB_R set to " << m_nWbR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_R] WB_R m_bR_Auto " << (m_bR_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_R, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_R] Error setting WB_R, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	return nErr;
}

int CPlayerOne::getWB_G(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	ret = getConfigValue(POA_WB_G, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] Error getting WB_G, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] WB_G is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] WB_G min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] WB_G max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setWB_G(long nWB_G, bool bIsAuto)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nWbG = nWB_G;
	m_bG_Auto = bIsAuto;

	if(!m_bConnected)
		return nErr;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	confValue.intValue = m_nWbG;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_G] WB_R set to " << m_nWbG << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_G] WB_R m_bR_Auto " << (m_bG_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_G, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_G] Error setting WB_G, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	return nErr;
}

int CPlayerOne::getWB_B(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	ret = getConfigValue(POA_WB_B, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] Error getting WB_B, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] WB_B is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] WB_B min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] WB_B max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setWB_B(long nWB_B, bool bIsAuto)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nWbB = nWB_B;
	m_bB_Auto = bIsAuto;

	if(!m_bConnected)
		return nErr;

	if(!m_cameraProperty.isColorCamera) {
		return VAL_NOT_AVAILABLE;
	}

	confValue.intValue = m_nWbB;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_B] WB_B set to " << m_nWbG << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_B] WB_B m_bR_Auto " << (m_bG_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_B, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK){
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWB_B] Error setting WB_B, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	return nErr;
}



int CPlayerOne::getFlip(long &nMin, long &nMax, long &nValue)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;


	ret = getConfigValue(POA_FLIP_NONE, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFlip] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_NONE;
	}

	ret = getConfigValue(POA_FLIP_HORI, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFlip] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_HORI;
	}

	ret = getConfigValue(POA_FLIP_VERT, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFlip] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_VERT;
	}

	ret = getConfigValue(POA_FLIP_BOTH, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFlip] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_BOTH;
	}

	m_nFlip = nValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFlip] Flip mode is " << nValue << std::endl;
	m_sLogFile.flush();
#endif

	return 0;
}

int CPlayerOne::setFlip(long nFlip)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nFlip = nFlip;

	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nFlip;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setFlip] Flip set to " << m_nFlip << std::endl;
	m_sLogFile.flush();
#endif
	switch(nFlip) {
		case FLIP_NONE :
			ret = setConfigValue(POA_FLIP_NONE, confValue);
			break;
		case FLIP_HORI :
			ret = setConfigValue(POA_FLIP_HORI, confValue);
			break;
		case FLIP_VERT :
			ret = setConfigValue(POA_FLIP_VERT, confValue);
			break;
		case FLIP_BOTH :
			ret = setConfigValue(POA_FLIP_BOTH, confValue);
			break;
		default:
			ret = setConfigValue(POA_FLIP_NONE, confValue);
			break;
	}
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setFlip] Error setting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERR_CMDFAILED;
	}
	return nErr;
}


int CPlayerOne::getOffset(long &nMin, long &nMax, long &nValue)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_OFFSET, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getOffset] Error getting Offset, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}

	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getOffset] Offset is " << nValue << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setOffset(long nOffset)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nOffset = nOffset;

	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nOffset;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setOffset] black offset set to " << m_nOffset << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_OFFSET, confValue);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setOffset] Error setting offset, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERR_CMDFAILED;
	}
	return nErr;
}


int CPlayerOne::getUSBBandwidth(long &nMin, long &nMax, long &nValue)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_USB_BANDWIDTH_LIMIT, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getUSBBandwidth] Error getting USB Bandwidth, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getUSBBandwidth] bandwidth is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getUSBBandwidth] min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getUSBBandwidth] max is " << nMax << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setUSBBandwidth(long nBandwidth)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nUSBBandwidth = nBandwidth;

	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nUSBBandwidth;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setUSBBandwidth] USB Bandwidth set to " << m_nUSBBandwidth << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_USB_BANDWIDTH_LIMIT, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setUSBBandwidth] Error setting USB Bandwidth, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	return nErr;
}

bool CPlayerOne::isLensHeaterAvailable()
{
	return m_bHasLensHeater;
}

int CPlayerOne::getLensHeaterPowerPerc(long &nMin, long &nMax, long &nValue)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_HEATER_POWER, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getLensHeaterPowerPerc] Error getting lens power percentage, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getLensHeaterPowerPerc] lens power is at " << nValue << "%" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getLensHeaterPowerPerc] min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getLensHeaterPowerPerc] max is " << nMax << std::endl;
	m_sLogFile.flush();
#endif
	return 0;
}

int CPlayerOne::setLensHeaterPowerPerc(long nPercent)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_nLensHeaterPowerPerc = nPercent;

	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nLensHeaterPowerPerc;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setLensHeaterPowerPerc] Lens power percentage set to " << m_nLensHeaterPowerPerc << "%" << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_HEATER_POWER, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setLensHeaterPowerPerc] Error setting Lens power percentage, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	return nErr;
}

bool CPlayerOne::isHardwareBinAvailable()
{
	return m_bHasHardwareBin;
}

int CPlayerOne::getHardwareBinOn(bool &bOn)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	bOn = false;

	ret = getConfigValue(POA_HARDWARE_BIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getHardwareBinOn] Error getting hardware bin mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return VAL_NOT_AVAILABLE;
	}
	m_bHardwareBinEnabled = bool(confValue.boolValue == POA_TRUE);
	bOn = m_bHardwareBinEnabled;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getHardwareBinOn] Hardware bin set to " << (bOn?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CPlayerOne::setHardwareBinOn(bool bOn)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	m_bHardwareBinEnabled = bOn;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setHardwareBinOn] called." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setHardwareBinOn] m_bHardwareBinEnabled : " << (m_bHardwareBinEnabled?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERR_NOLINK;
	// POA_TRUE is sum and POA_FLASE is average
	confValue.boolValue = bOn?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setHardwareBinOn] Hardware bin set to " << (bOn?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_HARDWARE_BIN, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setHardwareBinOn] Error setting Hardware bin, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	return nErr;

}


POAErrors CPlayerOne::setConfigValue(POAConfig confID , POAConfigValue confValue,  POABool bAuto)
{
	POAErrors ret;

	if(!m_bConnected)
		return POA_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confID = " << confID << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confValue.intValue = " << confValue.intValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confValue.floatValue = " << confValue.floatValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confValue.boolValue = " << (confValue.boolValue?"True":"False") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] bAuto = " << (bAuto == POA_TRUE? "Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif

	ret = POASetConfig(m_nCameraID, confID, confValue, bAuto);

	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] Error setting value, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif

	}
	return ret;
}


POAErrors CPlayerOne::getConfigValue(POAConfig confID , POAConfigValue &confValue, POAConfigValue &minValue, POAConfigValue &maxValue,  POABool &bAuto)
{
	POAErrors ret;
	POAConfigAttributes confAttr;
	int nControlID;
	bool bControlAvailable  = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] getting values for attribute " << confID << std::endl;
	m_sLogFile.flush();
#endif

	// look for the control
	for(nControlID = 0; nControlID< m_nControlNums; nControlID++) {
		if(m_ControlList.at(nControlID).configID == confID) {
			bControlAvailable = true;
			break;
		}
	}

	if(!bControlAvailable) {
		return POA_ERROR_INVALID_CONFIG;
	}

	ret = POAGetConfig(m_nCameraID, confID, &confValue, &bAuto);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Error getting value, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ret;
	}

	ret = POAGetConfigAttributesByConfigID(m_nCameraID, confID, &confAttr);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Error getting attributes, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ret;
	}


	switch(confAttr.valueType) {
		case VAL_INT :
			minValue.intValue = confAttr.minValue.intValue;
			maxValue.intValue = confAttr.maxValue.intValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Value       = " << confValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << confAttr.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.intValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
		case VAL_FLOAT :
			minValue.floatValue = confAttr.minValue.floatValue;
			maxValue.floatValue = confAttr.maxValue.floatValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Value       = " << confValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << confAttr.minValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.floatValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
		case VAL_BOOL :
			minValue.boolValue = confAttr.minValue.boolValue;
			maxValue.boolValue = confAttr.maxValue.boolValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Value       = " << (confValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << (confAttr.minValue.boolValue?"True":"False")  << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << (confAttr.maxValue.boolValue?"True":"False")  << std::endl;
			m_sLogFile.flush();
#endif
			break;
		default:
			minValue.intValue = confAttr.minValue.intValue;
			maxValue.intValue = confAttr.maxValue.intValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Value       = " << confValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << confAttr.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.intValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
	}
	return ret;
}

#pragma mark - Camera frame

int CPlayerOne::setROI(int nLeft, int nTop, int nWidth, int nHeight)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	int nNewLeft = 0;
	int nNewTop = 0;
	int nNewWidth = 0;
	int nNewHeight = 0;

	m_nReqROILeft = nLeft;
	m_nReqROITop = nTop;
	m_nReqROIWidth = nWidth;
	m_nReqROIHeight = nHeight;


	ret = POASetImageSize(m_nCameraID,m_nReqROIWidth,m_nReqROIHeight);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	ret = POAGetImageSize(m_nCameraID, &nNewWidth, &nNewHeight);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error getting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	ret = POASetImageStartPos(m_nCameraID, m_nReqROILeft, m_nReqROITop);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	ret = POAGetImageStartPos(m_nCameraID, &nNewLeft, &nNewTop);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error getting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] m_cameraProperty.maxWidth  = " << m_cameraProperty.maxWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] m_cameraProperty.maxHeigh = " << m_cameraProperty.maxHeight << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nLeft        = " << nLeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nTop         = " << nTop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nWidth       = " << nWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nHeight      = " << nHeight << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nNewLeft     = " << nNewLeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nNewTop      = " << nNewTop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nNewWidth    = " << nNewWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nNewHeight   = " << nNewHeight << std::endl;
	m_sLogFile.flush();
#endif

	m_nROILeft = nNewLeft;
	m_nROITop = nNewTop;
	m_nROIWidth = nNewWidth;
	m_nROIHeight = nNewHeight;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] new ROI set" << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CPlayerOne::clearROI()
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	ret = POASetImageSize(m_nCameraID,  m_cameraProperty.maxWidth/m_nCurrentBin , m_cameraProperty.maxHeight/m_nCurrentBin);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [clearROI] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	ret = POASetImageStartPos(m_nCameraID, 0, 0);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [clearROI] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	return nErr;
}


bool CPlayerOne::isFrameAvailable()
{
	bool bFrameAvailable = false;
	POABool pIsReady = POA_FALSE;
	POAErrors ret;

	if(m_ExposureTimer.GetElapsedSeconds()<m_dCaptureLenght)
		return bFrameAvailable;

	if(m_bAbort)
		return true;

	POACameraState cameraState;
	POAGetCameraState(m_nCameraID, &cameraState);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] Checking if a frame is ready." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] cameraState  = " << cameraState << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] cameraState still exposing = " << (cameraState==STATE_EXPOSING?"Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif
	/*
	 if(cameraState == STATE_EXPOSING)
	 {
	 bFrameAvailable = false;
	 }
	 else {
	 */
	ret = POAImageReady(m_nCameraID, &pIsReady);
	if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] POAImageReady error :  " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	bFrameAvailable = bool(pIsReady);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] bFrameAvailable : " << (bFrameAvailable?"Yes":"No")<< std::endl;
	m_sLogFile.flush();
#endif
	// }
	if(bFrameAvailable)
		POAStopExposure(m_nCameraID);

	return bFrameAvailable;
}

uint32_t CPlayerOne::getBitDepth()
{
	if(m_nImageFormat == POA_RAW16)
		return 16;
	else
		return 8;
}


int CPlayerOne::getFrame(int nHeight, int nMemWidth, unsigned char* frameBuffer)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	int sizeReadFromCam;
	unsigned char* imgBuffer = nullptr;
	int i = 0;
	uint16_t *buf;
	int srcMemWidth;
	int copyWidth = 0;
	int copyHeight = 0;

	int tmp1, tmp2;

	if(!frameBuffer)
		return ERR_POINTER;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nHeight         = " << nHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nMemWidth       = " << nMemWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nROILeft      = " << m_nROILeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nReqROILeft   = " << m_nReqROILeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nROITop       = " << m_nROITop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nReqROITop    = " << m_nReqROITop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nROIWidth     = " << m_nROIWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nReqROIWidth  = " << m_nReqROIWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nROIHeight    = " << m_nROIHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] m_nReqROIHeight = " << m_nReqROIHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] getBitDepth()/8 = " << getBitDepth()/8 << std::endl;
	m_sLogFile.flush();
#endif
	try {
		POAGetImageStartPos(m_nCameraID, &tmp1, &tmp2);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageStartPos x  : " << tmp1 << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageStartPos y  : " << tmp2 << std::endl;
		m_sLogFile.flush();
#endif

		POAGetImageSize(m_nCameraID, &tmp1, &tmp2);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageSize w  : " << tmp1 << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageSize h  : " << tmp2 << std::endl;
		m_sLogFile.flush();
#endif

		// do we need to extract data as ROI was re-aligned to match PlayerOne specs of heigth%2 and width%8
		if(m_nROIWidth != m_nReqROIWidth || m_nROIHeight != m_nReqROIHeight) {
			// me need to extract the data so we allocate a buffer
			srcMemWidth = m_nROIWidth * (getBitDepth()/8);
			imgBuffer = (unsigned char*)malloc(m_nROIHeight * srcMemWidth);
		}
		else {
			imgBuffer = frameBuffer;
			srcMemWidth = nMemWidth;
		}

		sizeReadFromCam = m_nROIHeight * srcMemWidth;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] srcMemWidth     = " << srcMemWidth << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nMemWidth       = " << nMemWidth << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] sizeReadFromCam = " << sizeReadFromCam << std::endl;
		m_sLogFile.flush();
#endif
		ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, 500);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageData error, retrying :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			// wait and retry
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			std::this_thread::yield();
			ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, 500);
			if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageData error :  " << POAGetErrorString(ret) << std::endl;
				m_sLogFile.flush();
#endif
				POAStopExposure(m_nCameraID);
				if(imgBuffer != frameBuffer)
					free(imgBuffer);
				return ERR_RXTIMEOUT;
			}
		}
		// POAStopExposure(m_nCameraID);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] Frame downloaded to buffer." << std::endl;
		m_sLogFile.flush();
#endif

		// shift data
		if(m_nNbBitToShift) {
			buf = (uint16_t *)imgBuffer;
			for(int i=0; i<sizeReadFromCam/2; i++)
				buf[i] = buf[i]<<m_nNbBitToShift;
		}

		if(imgBuffer != frameBuffer) {
			copyWidth = srcMemWidth>nMemWidth?nMemWidth:srcMemWidth;
			copyHeight = m_nROIHeight>nHeight?nHeight:m_nROIHeight;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] copying ("<< m_nROILeft <<","
			<< m_nROITop <<","
			<< m_nROIWidth <<","
			<< m_nROIHeight <<") => ("
			<< m_nReqROILeft <<","
			<< m_nReqROITop <<","
			<< m_nReqROIWidth <<","
			<< m_nReqROIHeight <<")"
			<< std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] srcMemWidth       : " << srcMemWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nMemWidth         : " << nMemWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] copyHeight        : " << copyHeight << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] copyWidth         : " << copyWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] sizeReadFromCam   : " << sizeReadFromCam << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] size to TSX       : " << nHeight * nMemWidth << std::endl;
			m_sLogFile.flush();
#endif
			// copy every line from source buffer newly aligned into TSX buffer cutting at copyWidth
			for(i=0; i<copyHeight; i++) {
				memcpy(frameBuffer+(i*nMemWidth), imgBuffer+(i*srcMemWidth), copyWidth);
			}
			free(imgBuffer);
		}

	}  catch(const std::exception& e) {
		if(imgBuffer) {
			POAStopExposure(m_nCameraID);
			try {
				free(imgBuffer);
				imgBuffer = nullptr;
				nErr = ERR_RXTIMEOUT;
			}  catch(const std::exception& e) {

			}
		}
	}
	catch (...) {
		if(imgBuffer) {
			POAStopExposure(m_nCameraID);
			try {
				free(imgBuffer);
				imgBuffer = nullptr;
				nErr = ERR_RXTIMEOUT;
			}  catch(const std::exception& e) {

			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] Done." << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}


#pragma mark - Camera relay

// a lot of this Relay code is based on code from Richard Wright Jr.

int CPlayerOne::RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;
	POABool bAuto = POA_FALSE;
	int netX;
	int netY;
	bool bNorth, bSouth, bEast, bWest;
	float timeToWait =0.0;
	CStopWatch pulseTimer;

	if(!m_cameraProperty.isHasST4Port)
		return ERR_COMMANDNOTSUPPORTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] nXPlus       : " << nXPlus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] nXMinus      : " << nXMinus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] nYPlus       : " << nYPlus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] nYMinus      : " << nYMinus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] bSynchronous : " << (bSynchronous?"True":"False") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] bAbort       : " << (bAbort?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	if(bAbort) {
		confValue.boolValue = POA_FALSE;
		ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);
		return nErr;
	}

	if(!bSynchronous) { // done for the GUI
		if(nXPlus == 0 && nXMinus ==0 && nYPlus == 0 && nYMinus ==0) {
			confValue.boolValue = POA_FALSE; // stop guiding, m_confGuideDir should still contain the last guide direction
		}
		else {
			confValue.boolValue = POA_TRUE; // enable guiding
			if(nXPlus != 0 && nXMinus ==0)
				m_confGuideDir = POA_GUIDE_EAST;
			else if(nXPlus == 0 && nXMinus !=0)
				m_confGuideDir = POA_GUIDE_WEST;
			else if(nYPlus != 0 && nYMinus ==0)
				m_confGuideDir = POA_GUIDE_NORTH;
			else if(nYPlus == 0 && nYMinus !=0)
				m_confGuideDir = POA_GUIDE_SOUTH;
			else { // dual direction, can't be done from the GUI
				return ERR_COMMANDNOTSUPPORTED;
			}
		}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] GUI move m_confGuideDir    : " << m_confGuideDir << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] GUI move confValue.boolValue : " << (confValue.boolValue?"True":"False") << std::endl;
		m_sLogFile.flush();
#endif

		ret = POASetConfig(m_nCameraID, m_confGuideDir, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] GUI move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		return nErr;
	}

	// start guiding / calibrating
	bEast = false;
	bWest = false;
	bNorth = false;
	bSouth = false;
	confValue.boolValue = POA_TRUE; // enable guiding
	pulseTimer.Reset();
	// East/West
	if(nXPlus != 0 && nXMinus ==0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		bEast = true;
	}
	if(nXPlus == 0 && nXMinus !=0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		bWest = true;
	}
	// North/South
	if(nYPlus != 0 && nYMinus ==0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		bNorth = true;
	}

	if(nYPlus == 0 && nYMinus !=0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		bSouth = true;
	}

	confValue.boolValue = POA_FALSE; // to stop move
	// One of these will always be zero, so this gives me the net
	// plus or minus movement
	netX = nXPlus - nXMinus;
	netY = nYPlus - nYMinus;
	if(netX == 0) {   // netY will not be zero
		// One of nYPLus and nYMinus will be zero, so this expression will work
		timeToWait = float(nYPlus + nYMinus)/100.0f;
		// Just wait for time to expire and stop relay
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		return nErr;
	}

	if(netY == 0)  {  // netX will not be zero
		// Again, one of these will be zero
		timeToWait = float(nXPlus + nXMinus)/100.0f;
		// Just wait for time to expire and stop relay
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		return nErr;
	}
	//
	//  Dual axis movement, Are they both the same. Wait and then terminate both at same time
	//
	if(abs(netY) == abs(netX)) {
		// Pick one, doesn't matter which
		timeToWait = float(nXPlus + nXMinus)/100.0f;
		// Just wait for time to expire and stop relay
		while(pulseTimer.GetElapsedSeconds() < timeToWait);

		// need to know which one to stop
		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);

		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		return nErr;
	}
	//
	// Dual axis movement, not the same time for each!
	//
	if(abs(netY) < abs(netX)) { // East-West movement was greater
		// Wait for shorter time
		timeToWait = float(nYPlus + nYMinus)/100.0f;
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}
		// Longer time
		timeToWait = float(nXPlus + nXMinus)/100.0f;
		while(pulseTimer.GetElapsedSeconds() < timeToWait);

		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}

		return nErr;
	}
	else { // North-South movement was greater
		// Wait for shorter time
		timeToWait = float(nXPlus + nXMinus)/100.0f;
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}

		// Longer time
		timeToWait = float(nYPlus + nYMinus)/100.0f;
		while(pulseTimer.GetElapsedSeconds() < timeToWait);

		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERR_CMDFAILED;
		}

		return nErr;
	}
	return nErr;
}

#pragma mark - helper functions

void CPlayerOne::buildGainList()
{
	std::stringstream ssTmp;

	m_GainList.clear();
	m_GainListLabel.clear();
	m_nNbGainValue = 0;

	ssTmp << "High Conversion Gain (" << m_nHCGain <<")";
	m_GainListLabel.push_back(ssTmp.str());
	m_GainList.push_back(m_nHCGain);
	std::stringstream().swap(ssTmp);
	m_nNbGainValue++;

	ssTmp << "Lowest read noise (" << m_nGainLowestRN <<")";
	m_GainListLabel.push_back(ssTmp.str());
	m_GainList.push_back(m_nGainLowestRN);
	std::stringstream().swap(ssTmp);
	m_nNbGainValue++;

	ssTmp << "Highest dynamic range (" << m_nGainHighestDR <<")";
	m_GainListLabel.push_back(ssTmp.str());
	m_GainList.push_back(m_nGainHighestDR);
	std::stringstream().swap(ssTmp);
	m_nNbGainValue++;

	ssTmp << "Unity (" << m_nUnityGain <<")";
	m_GainListLabel.push_back(ssTmp.str());
	m_GainList.push_back(m_nUnityGain);
	std::stringstream().swap(ssTmp);
	m_nNbGainValue++;

	ssTmp << "User value (" << m_nGain <<")";
	m_GainListLabel.push_back(ssTmp.str());
	m_GainList.push_back((int)m_nGain);
	std::stringstream().swap(ssTmp);
	m_nNbGainValue++;
}

int CPlayerOne::getNbGainInList()
{
	return m_nNbGainValue;
}

void CPlayerOne::rebuildGainList()
{
	long nMin, nMax;
	getGain(nMin, nMax, m_nGain);
	buildGainList();
}

std::string CPlayerOne::getGainLabelFromListAtIndex(int nIndex)
{
	if(nIndex<m_GainListLabel.size())
		return m_GainListLabel.at(nIndex);
	else
		return std::string("N/A");
}


int CPlayerOne::getGainFromListAtIndex(int nIndex)
{
	if(nIndex<m_GainList.size())
		return m_GainList.at(nIndex);
	else
		return m_nHCGain;
}

#ifdef PLUGIN_DEBUG
void CPlayerOne::log(std::string sLogEntry)
{
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [log] " << sLogEntry << std::endl;
	m_sLogFile.flush();

}

const std::string CPlayerOne::getTimeStamp()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	return buf;
}
#endif
