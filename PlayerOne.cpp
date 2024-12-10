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
#if defined(WIN32)
	m_sLogfilePath = getenv("HOMEDRIVE");
	m_sLogfilePath += getenv("HOMEPATH");
	m_sLogfilePath += "\\PlayerOneLog.txt";
#else
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/PlayerOneLog.txt";
#endif
	m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

	std::string sSDKVersion;
	getFirmwareVersion(sSDKVersion);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Version " << std::fixed << std::setprecision(2) << CODE_VERSION << " build " << __DATE__ << " " << __TIME__ << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] " << sSDKVersion << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Constructor Called." << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]" << " [" << __func__ << "] Set m_bSetUserConf to : " << (bUserConf?"Yes":"No") << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Trying to connect to camera with serial : " << m_sCameraSerial << std::endl;
		m_sLogFile.flush();
#endif

	if(!sSerial.size())
		return ERROR_NODEVICESELECTED;

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
				return ERROR_NODEVICESELECTED;
		}
		else
			return ERROR_NODEVICESELECTED;
	}
	ret = POAOpenCamera(m_nCameraID);
	if (ret != POA_OK) { // we had a camera selected but it's not connected,
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error connecting to camera ID " << m_nCameraID << " serial " << m_sCameraSerial << " , Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ERROR_NODEVICESELECTED;
	}

	ret = POAInitCamera(m_nCameraID);
	if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error inittializing camera , Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

	m_bConnected = true;
	getCameraNameFromID(m_nCameraID, m_sCameraName);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Connected to camera ID  " << m_nCameraID << " Serial " << m_sCameraSerial << std::endl;
	m_sLogFile.flush();
#endif

	ret = POAGetCameraPropertiesByID(m_nCameraID, &m_cameraProperty);
	if (ret != POA_OK)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetCameraProperties Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERROR_CMDFAILED;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_SupportedBins[" << i <<"] : " << m_SupportedBins[i] << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Camera properties:" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.maxWidth        : " << m_cameraProperty.maxWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.maxHeigh        : " << m_cameraProperty.maxHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.bitDepth        : " << m_cameraProperty.bitDepth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.isColorCamera   : " << (m_cameraProperty.isColorCamera?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.isHasST4Port    : " << (m_cameraProperty.isHasST4Port?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.isHasCooler     : " << (m_cameraProperty.isHasCooler?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.isUSB3Speed     : " << (m_cameraProperty.isUSB3Speed?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.bayerPattern    : " << m_cameraProperty.bayerPattern << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.pixelSize       : " << m_cameraProperty.pixelSize << "um" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.SN              : " << m_cameraProperty.SN << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.sensorModelName : " << m_cameraProperty.sensorModelName << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nNbBitToShift                  : " << m_nNbBitToShift << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nNbBin                         : " << m_nNbBin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.isSupportHardBin: " << (m_cameraProperty.isSupportHardBin?"Yes":"No") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_dPixelSize                     : " << m_dPixelSize << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting ROI to max size." << std::endl;
	m_sLogFile.flush();

#endif
	nErr = setBinSize(1);
	if (nErr)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] setBinSize Error = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERROR_CMDFAILED;
	}


	nErr = setROI(0, 0, m_cameraProperty.maxWidth, m_cameraProperty.maxHeight);
	if (nErr)
	{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] setROI Error = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERROR_CMDFAILED;
	}

	ret = POAGetConfigsCount(m_nCameraID, &m_nControlNums);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetCameraProperties Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		POACloseCamera(m_nCameraID);
		m_bConnected = false;
		return ERROR_CMDFAILED;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nControlNums  = " << m_nControlNums << std::endl;
	m_sLogFile.flush();
#endif

	for (i = 0; i < m_nControlNums; i++) {
		ret = POAGetConfigAttributes(m_nCameraID, i, &Attribute);
		if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting attribute, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			continue;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute ID   = " << Attribute.configID << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Name   = " << Attribute.szConfName << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Description   = " << Attribute.szDescription << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute type   = " << Attribute.valueType << std::endl;
		if(Attribute.valueType == VAL_INT) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Min Value   = " << Attribute.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Max Value   = " << Attribute.maxValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Default Value   = " << Attribute.defaultValue.intValue << std::endl;
		}
		else if (Attribute.valueType == VAL_FLOAT) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute MinValue   = " << Attribute.minValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute MaxValue   = " << Attribute.maxValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Default Value   = " << Attribute.defaultValue.floatValue << std::endl;
		}
		else if (Attribute.valueType == VAL_BOOL) {
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute MinValue   = " << (Attribute.minValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute MaxValue   = " << (Attribute.maxValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Default Value   = " << (Attribute.defaultValue.boolValue?"True":"False") << std::endl;
		}

		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute isSupportAuto   = " << (Attribute.isSupportAuto?"Yes":"No") << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute isWritable   = " << Attribute.isWritable << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute isReadable   = " << Attribute.isReadable << std::endl << "-------------------------------" << std::endl;
		m_sLogFile.flush();
#endif
		m_ControlList.push_back(Attribute);
	}

	// get usefull gains and offsets
	ret = POAGetGainsAndOffsets(m_nCameraID, &m_nGainHighestDR, &m_nHCGain, &m_nUnityGain, &m_nGainLowestRN, &m_nOffsetHighestDR, &m_nOffsetHCGain, &m_nOffsetUnityGain, &m_nOffsetLowestRN);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset at highest dynamic range        : " << m_nOffsetHighestDR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Gain at HCG Mode(High Conversion Gain) : " << m_nHCGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Gain at unity                          : " << m_nUnityGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Gain at lowest read noise              : " << m_nGainLowestRN << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset at highest dynamic range        : " << m_nOffsetHighestDR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset at HCG                          : " << m_nOffsetHCGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset at unity gain                   : " << m_nOffsetUnityGain << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset at lowest read noise            : " << m_nOffsetLowestRN << std::endl;
	m_sLogFile.flush();
#endif

	getExposureMinMax(m_nExposureMin, m_nExposureMax);

	POAGetSensorModeCount(m_nCameraID, &m_nSensorModeCount);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Number of sensor mode : " << m_nSensorModeCount << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_bSetUserConf : " << (m_bSetUserConf?"Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif
	m_bHasLensHeater = true;
	nErr = getLensHeaterPowerPerc(nMin, nMax, nValue);
	if(nErr == VAL_NOT_AVAILABLE) {
		m_bHasLensHeater = false;
		nErr = PLUGIN_OK;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_bHasLensHeater : " << (m_bHasLensHeater?"Yes":"No") << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] after m_bSetUserConf, m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif

	rebuildGainList();

	ret = POASetImageFormat(m_nCameraID, m_nImageFormat);


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sensor mode count : " << m_nSensorModeCount << std::endl;
	m_sLogFile.flush();
#endif
	// if the camera supports it, in general, there are at least two sensor modes[Normal, LowNoise, ...]
	m_sensorModeInfo.clear();
	for(i=0; i< m_nSensorModeCount; i++) {
		ret = POAGetSensorModeInfo(m_nCameraID, i, &sensorMode);
		if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting sensor mode info : "<< ret << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting sensor mode info : "<< POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			continue;
		}
		m_sensorModeInfo.push_back(sensorMode);
		sTmp.assign(sensorMode.name);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sensor mode " << i << " name : " << sensorMode.name << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sensor mode " << i << " desc : " << sensorMode.desc << std::endl;
		m_sLogFile.flush();
#endif
		if(m_nSensorModeIndex == VAL_NOT_AVAILABLE) { // there was no values set
			if(std::string(sensorMode.name).find("Low Noise") != std::string::npos) {
				m_nSensorModeIndex = i;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting Low Noise mode as the default as nothing was saved" << std::endl;
				m_sLogFile.flush();
#endif
			}
		}

	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] after building m_sensorModeInfo, m_nSensorModeIndex = " << m_nSensorModeIndex << std::endl;
	m_sLogFile.flush();
#endif

	if(m_nSensorModeCount) {
		if(m_nSensorModeIndex == VAL_NOT_AVAILABLE) {
			getCurentSensorMode(sTmp, m_nSensorModeIndex);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] got sensor mode from camera = " << m_nSensorModeIndex << std::endl;
			m_sLogFile.flush();
#endif
		}
		else {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] setting sensor mode to " << m_nSensorModeIndex << std::endl;
			m_sLogFile.flush();
#endif
			ret = (POAErrors)setSensorMode(m_nSensorModeIndex);
			if(ret) {
				setSensorMode(0);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting sensor mode to " << m_nSensorModeIndex<<", Error = " << POAGetErrorString(ret) << std::endl;
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting to default mode" << std::endl;
				m_sLogFile.flush();
#endif
			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Connected, nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	POAStopExposure(m_nCameraID); // make sure nothing is running
	return nErr;
}

void CPlayerOne::Disconnect()
{
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Disconnect."<< std::endl;
	m_sLogFile.flush();
#endif


	nErr = POAStopExposure(m_nCameraID);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAStopExposure : " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	nErr = POACloseCamera(m_nCameraID);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POACloseCamera : " << nErr << std::endl;
	m_sLogFile.flush();
#endif

	if(m_pframeBuffer) {
		free(m_pframeBuffer);
		m_pframeBuffer = NULL;
	}
	m_bConnected = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Disconnect done."<< std::endl;
	m_sLogFile.flush();
#endif

}

int CPlayerOne::getCameraIdFromSerial(int &nCameraId, std::string sSerial)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sSerial = " << sSerial << std::endl;
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
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] found nCameraId = " << nCameraId << std::endl;
				m_sLogFile.flush();
#endif
				break;
			}
		}
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	if(nCameraId>=0) {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera id  = " << nCameraId <<" SN: "<< sSerial << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera id  not found for SN: "<< sSerial << std::endl;
	}
	m_sLogFile.flush();
#endif
	if(nCameraId<0)
		nErr = ERROR_NODEVICESELECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CPlayerOne::getCameraSerialFromID(int nCameraId, std::string &sSerial)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nCameraId = " << nCameraId << std::endl;
	m_sLogFile.flush();
#endif

	if(nCameraId<0)
		return ERROR_NODEVICESELECTED;

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
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sSerial = " << sSerial << std::endl;
				m_sLogFile.flush();
#endif
				break;
			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	if(sSerial.size()) {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera id  = " << nCameraId <<" SN: "<< sSerial << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera serial  not found for id: "<< nCameraId << std::endl;
	}
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

void CPlayerOne::getCameraNameFromID(int nCameraId, std::string &sName)
{

	POAErrors ret;
	sName.clear();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nCameraId = " << nCameraId << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera id  = " << nCameraId <<" name : "<< sName << std::endl;
	}
	else {
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] camera name  not found for id: "<< nCameraId << std::endl;

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
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Name : " << m_cameraProperty.cameraModelName << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] USB Port type : " << (m_cameraProperty.isUSB3Speed?"USB3":"USB2") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] SN  : " << m_cameraProperty.SN << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Camera ID : " << m_cameraProperty.cameraID << std::endl;
			m_sLogFile.flush();
#endif
			tCameraInfo.cameraId = m_cameraProperty.cameraID;
			tCameraInfo.model.assign(m_cameraProperty.cameraModelName);
			tCameraInfo.Sn.assign(m_cameraProperty.SN);
			cameraIdList.push_back(tCameraInfo);
		}
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
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

bool CPlayerOne::isBinSupported(int nRequestedBin)
{
	for( int &nBin : m_SupportedBins) {
		if(nRequestedBin == nBin)
			return true;
	}
	return false;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting current sensor mode." << std::endl;
		m_sLogFile.flush();
#endif
		return ERROR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nModeIndex = " << nModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_sensorModeInfo size = " <<  m_sensorModeInfo.size() << std::endl;
	m_sLogFile.flush();
#endif
	if(nModeIndex>=m_sensorModeInfo.size()) {
		// not good
		sSensorMode.assign("Bad index");
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error Bad index nModeIndex >= m_sensorModeInfo.size() " << std::endl;
		m_sLogFile.flush();
#endif
	}
	else
		sSensorMode.assign(m_sensorModeInfo[nModeIndex].name);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getSensorModeList(std::vector<std::string> &svModes, int &curentModeIndex)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	svModes.clear();
	curentModeIndex = -1;

	if(!m_nSensorModeCount) {// sensor mode no supported by camera
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	if(m_sensorModeInfo.size()==0) {
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	ret = POAGetSensorMode(m_nCameraID, &curentModeIndex);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetSensorMode Error getting current sensor mode." << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] curentModeIndex = " << curentModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_sensorModeInfo size = " <<  m_sensorModeInfo.size() << std::endl;
	m_sLogFile.flush();
#endif

	svModes.clear();
	for (POASensorModeInfo &mode : m_sensorModeInfo) {
		svModes.push_back(mode.name);
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Current Sensor mode is " << svModes.at(curentModeIndex) << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
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
		return ERROR_NOLINK;

	if(!m_nSensorModeCount) {// sensor mode no supported by camera
		nErr = ERROR_COMMANDNOTSUPPORTED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting sensor mode to index  = " << nModeIndex << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Sensor mode name   = " << m_sensorModeInfo[nModeIndex].name << std::endl;
	m_sLogFile.flush();
#endif

	ret = POASetSensorMode(m_nCameraID, m_nSensorModeIndex);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting sensor mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getPixelBinMode(bool &bSumMode)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] called." << std::endl;
	m_sLogFile.flush();
#endif

	ret = getConfigValue(POA_PIXEL_BIN_SUM, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting camera pixel mode. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	// POA_TRUE is sum and POA_FALSE is average,
	bSumMode = (confValue.boolValue == POA_TRUE);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Pixel bin mode confValue.boolValue =  " << (confValue.boolValue==POA_TRUE?"POA_TRUE":"POA_FALSE") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Pixel bin mode set to " << (bSumMode?"Sum":"Average") << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERROR_NOLINK;
	// POA_TRUE is sum and POA_FLASE is average
	confValue.boolValue = m_bPixelBinMode?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Pixel bin mode set to " << (bSumMode?"Sum":"Average") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_PIXEL_BIN_SUM, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting Pixel bin mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


int CPlayerOne::getMonoBin(bool &bMonoBin)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] called." << std::endl;
	m_sLogFile.flush();
#endif
	bMonoBin = false;
	ret = getConfigValue(POA_MONO_BIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting camera pixel mono bin. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	// POA_TRUE is mono bin and POA_FALSE is no mono bin,
	bMonoBin = (confValue.boolValue == POA_TRUE);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Pixel mono bin confValue.boolValue =  " << (confValue.boolValue==POA_TRUE?"POA_TRUE":"POA_FALSE") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Pixel mono bin set to " << (bMonoBin?"Mono":"Color") << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERROR_NOLINK;
	// POA_TRUE is mono bin and POA_FALSE is no mono bin,
	confValue.boolValue = m_bPixelMonoBin?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< "  [" << __func__ << "] Pixel mono bin set to " << (bMonoBin?"Mono":"Color") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_MONO_BIN, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< "  [" << __func__ << "] Error setting Pixel mono bin, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
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


int CPlayerOne::getGainAdu(double &dMin, double &dMax, double &dValue)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	ret = getConfigValue(POA_EGAIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting gain ADU, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	dMin = minValue.floatValue;
	dMax = maxValue.floatValue;
	dValue = confValue.floatValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] GainAUD is " << dValue << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getExposureMinMax(long &nMin, long &nMax)
{
	POAErrors ret;
	int nErr = PLUGIN_OK;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	ret = getConfigValue(POA_EXPOSURE, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting min and max exposure time, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	nMin = minValue.intValue;
	nMax = maxValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nMax is " << nMax << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

long CPlayerOne::getExposureMin()
{
	return m_nExposureMin;
}

long CPlayerOne::getExposureMax()
{
	return m_nExposureMax;
}

bool CPlayerOne::getFastReadoutAvailable()
{
	// more than 1 mode, there is a fast mode
	if(m_nSensorModeCount > 1) {
		return true;
	}
	// if there is only 1 mode, is it fast mode ?
	for(auto &mode: m_sensorModeInfo) {
		if(std::string(mode.name).find("Low Noise") != std::string::npos)
			return false;

	}

	return true;
}

bool CPlayerOne::isFastReadoutEnabled()
{
	int nErr = 0;
	std::string sSensorModeName;

	if(m_nSensorModeCount == 1) // only 1 mode then it's fast readount by default
		return true;

	nErr = getCurentSensorMode(sSensorModeName, m_nSensorModeIndex);
	if(nErr)
		return true; // if we can get the mode we assume it's fast readout

	if(sSensorModeName.find("low noise") != std::string::npos) // if we're in low noise mode, we're not in fast readout.
		return false;

	return true;
}

int CPlayerOne::getMaxBin()
{
	return m_nNbBin;
}

std::string	CPlayerOne::getSensorName()
{
	return std::string(m_cameraProperty.sensorModelName);
}


#pragma mark - Camera capture

int CPlayerOne::startCapture(double dTime)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	int nTimeout;
	m_bAbort = false;
	POACameraState cameraState;
	POAConfigValue exposure_value;

	nTimeout = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Start Capture." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Waiting for camera to be idle." << std::endl;
	m_sLogFile.flush();
#endif
	// POAStopExposure(m_nCameraID);

	ret = POAGetCameraState(m_nCameraID, &cameraState);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting camera state. Error= " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	if(cameraState != STATE_OPENED) {
		nErr = ERROR_COMMANDINPROGRESS;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Starting Capture." << std::endl;
	m_sLogFile.flush();
#endif

	// set exposure time (s -> us)
	exposure_value.intValue = (int)(dTime * 1000000);
	ret = POASetConfig(m_nCameraID, POA_EXPOSURE, exposure_value, POA_FALSE); //set exposure time
	if(ret!=POA_OK)
		return ERROR_CMDFAILED;

	ret = POAStartExposure(m_nCameraID, POA_TRUE); // single frame(Snap mode)
	// ret = POAStartExposure(m_nCameraID, POA_FALSE); // continuous exposure mode
	if(ret!=POA_OK)
		nErr = ERROR_CMDFAILED;

	m_dCaptureLenght = dTime;
	m_ExposureTimer.Reset();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


void CPlayerOne::abortCapture(void)
{
	POAErrors ret;
	m_bAbort = true;

	ret = POAStopExposure(m_nCameraID);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] capture aborted." << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] dTemp " << dTemp << std::endl;
		m_sLogFile.flush();
#endif
		if(m_cameraProperty.isHasCooler) {
			ret = getConfigValue(POA_TARGET_TEMP, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				dSetPoint = double(confValue.intValue);
			else
				dSetPoint = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] dSetPoint " << dSetPoint << std::endl;
			m_sLogFile.flush();
#endif

			ret = getConfigValue(POA_COOLER_POWER, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				dPower = double(confValue.intValue);
			else
				dPower = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] dPower " << dPower << std::endl;
			m_sLogFile.flush();
#endif

			ret = getConfigValue(POA_COOLER, confValue, minValue, maxValue, bAuto);
			if(ret == POA_OK)
				bEnabled = bool(confValue.boolValue);
			else
				bEnabled = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bEnabled " << (bEnabled?"Yes":"No") << std::endl;
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
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::setCoolerTemperature(bool bOn, double dTemp)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;


	if(m_cameraProperty.isHasCooler) {
		m_dCoolerSetTemp = dTemp;
		confValue.intValue = int(dTemp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting temperature to " << int(dTemp) << std::endl;
		m_sLogFile.flush();
#endif

		ret = setConfigValue(POA_TARGET_TEMP, confValue);
		if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting temperature, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}

		confValue.boolValue = (bOn?POA_TRUE:POA_FALSE);
		ret = setConfigValue(POA_COOLER, confValue);
		if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting cooler, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
	}
	else {
		nErr = ERROR_CMDFAILED; // can't set the cooler temp if we don't have one.
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

double CPlayerOne::getCoolerSetTemp()
{
	return m_dCoolerSetTemp;
}

int CPlayerOne::setCoolerState(bool bOn)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

	if(m_cameraProperty.isHasCooler) {
		confValue.boolValue = (bOn?POA_TRUE:POA_FALSE);
		ret = setConfigValue(POA_COOLER, confValue);
		if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting cooler, Error = " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

bool CPlayerOne::isCoolerAvailable()
{
	return m_cameraProperty.isHasCooler;
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
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nBin = " << nBin << std::endl;
	m_sLogFile.flush();
#endif

	m_nCurrentBin = nBin;
	ret = POASetImageBin(m_nCameraID, m_nCurrentBin);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting bin size, Error = " << ret << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getCurrentBin()
{
	return m_nCurrentBin;
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
	int nErr = PLUGIN_OK;
	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_GAIN, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting gain, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Gain is " << nValue << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::setGain(long nGain)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue confValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called with gain =  " << nGain << std::endl;
	m_sLogFile.flush();
#endif

	m_nGain = nGain;
	if(!m_bConnected)
		return nErr;

	confValue.intValue = m_nGain;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Setting Gain to " << m_nGain << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_GAIN, confValue);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting gain, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


int CPlayerOne::getWB_R(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;
	int nErr = PLUGIN_OK;
	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = getConfigValue(POA_WB_R, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting WB_R, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	confValue.intValue = nWB_R;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R set to " << m_nWbR << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R m_bR_Auto " << (m_bR_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_R, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting WB_R, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getWB_G(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = getConfigValue(POA_WB_G, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting WB_G, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_G is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_G min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_G max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	confValue.intValue = m_nWbG;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R set to " << m_nWbG << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_R m_bR_Auto " << (m_bG_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_G, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting WB_G, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CPlayerOne::getWB_B(long &nMin, long &nMax, long &nValue, bool &bIsAuto)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	if(!m_cameraProperty.isColorCamera) {
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = getConfigValue(POA_WB_B, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting WB_B, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	bIsAuto = (bool)bAuto;
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_B is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_B min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_B max is " << nMax << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bIsAuto is " << (bIsAuto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	confValue.intValue = m_nWbB;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_B set to " << m_nWbG << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] WB_B m_bR_Auto " << (m_bG_Auto?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_WB_B, confValue, bIsAuto?POA_TRUE:POA_FALSE);
	if(ret != POA_OK){
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting WB_B, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}



int CPlayerOne::getFlip(long &nMin, long &nMax, long &nValue)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;


	ret = getConfigValue(POA_FLIP_NONE, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_NONE;
	}

	ret = getConfigValue(POA_FLIP_HORI, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_VERT;
	}

	ret = getConfigValue(POA_FLIP_BOTH, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	if(confValue.boolValue == POA_TRUE) {
		nMin = minValue.boolValue?1:0;
		nMax = maxValue.boolValue?1:0;;
		nValue = FLIP_BOTH;
	}

	m_nFlip = nValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Flip mode is " << nValue << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Flip set to " << m_nFlip << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting Flip mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


int CPlayerOne::getOffset(long &nMin, long &nMax, long &nValue)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_OFFSET, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Offset, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Offset is " << nValue << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] black offset set to " << m_nOffset << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_OFFSET, confValue);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting offset, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


int CPlayerOne::getUSBBandwidth(long &nMin, long &nMax, long &nValue)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_USB_BANDWIDTH_LIMIT, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting USB Bandwidth, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bandwidth is " << nValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] max is " << nMax << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] USB Bandwidth set to " << m_nUSBBandwidth << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_USB_BANDWIDTH_LIMIT, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting USB Bandwidth, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

bool CPlayerOne::isLensHeaterAvailable()
{
	return m_bHasLensHeater;
}

int CPlayerOne::getLensHeaterPowerPerc(long &nMin, long &nMax, long &nValue)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;
	POAConfigValue minValue, maxValue, confValue;
	POABool bAuto;

	nMin = 0;
	nMax = 0;
	nValue = 0;

	ret = getConfigValue(POA_HEATER_POWER, confValue, minValue, maxValue, bAuto);
	if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting lens power percentage, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	nMin = minValue.intValue;
	nMax = maxValue.intValue;
	nValue = confValue.intValue;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] lens power is at " << nValue << "%" << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] min is " << nMin << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] max is " << nMax << std::endl;
	m_sLogFile.flush();
#endif
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Lens power percentage set to " << m_nLensHeaterPowerPerc << "%" << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_HEATER_POWER, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting Lens power percentage, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting hardware bin mode, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = VAL_NOT_AVAILABLE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	m_bHardwareBinEnabled = bool(confValue.boolValue == POA_TRUE);
	bOn = m_bHardwareBinEnabled;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Hardware bin set to " << (bOn?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] called." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_bHardwareBinEnabled : " << (m_bHardwareBinEnabled?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bConnected)
		return ERROR_NOLINK;
	// POA_TRUE is sum and POA_FLASE is average
	confValue.boolValue = bOn?POA_TRUE:POA_FALSE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Hardware bin set to " << (bOn?"On":"Off") << std::endl;
	m_sLogFile.flush();
#endif

	ret = setConfigValue(POA_HARDWARE_BIN, confValue, POA_FALSE);
	if(ret != POA_OK) {
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting Hardware bin, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


POAErrors CPlayerOne::setConfigValue(POAConfig confID , POAConfigValue confValue,  POABool bAuto)
{
	POAErrors ret;

	if(!m_bConnected)
		return POA_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] confID = " << confID << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] confValue.intValue = " << confValue.intValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] confValue.floatValue = " << confValue.floatValue << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] confValue.boolValue = " << (confValue.boolValue?"True":"False") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bAuto = " << (bAuto == POA_TRUE? "Yes":"No") << std::endl;
	m_sLogFile.flush();
#endif

	ret = POASetConfig(m_nCameraID, confID, confValue, bAuto);

	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting value, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif

	}
	return ret;
}


POAErrors CPlayerOne::getConfigValue(POAConfig confID , POAConfigValue &confValue, POAConfigValue &minValue, POAConfigValue &maxValue,  POABool &bAuto)
{
	POAErrors ret;
	POAConfigAttributes confAttr;
	bool bControlAvailable  = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] getting values for attribute " << confID << std::endl;
	m_sLogFile.flush();
#endif

	// look for the control
	for(auto &listEntry: m_ControlList) {
		if(listEntry.configID == confID) {
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting value, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ret;
	}

	ret = POAGetConfigAttributesByConfigID(m_nCameraID, confID, &confAttr);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting attributes, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		return ret;
	}


	switch(confAttr.valueType) {
		case VAL_INT :
			minValue.intValue = confAttr.minValue.intValue;
			maxValue.intValue = confAttr.maxValue.intValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Value       = " << confValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Min Value   = " << confAttr.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Max Value   = " << confAttr.maxValue.intValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
		case VAL_FLOAT :
			minValue.floatValue = confAttr.minValue.floatValue;
			maxValue.floatValue = confAttr.maxValue.floatValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Value       = " << confValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Min Value   = " << confAttr.minValue.floatValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.floatValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
		case VAL_BOOL :
			minValue.boolValue = confAttr.minValue.boolValue;
			maxValue.boolValue = confAttr.maxValue.boolValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Value       = " << (confValue.boolValue?"True":"False") << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Min Value   = " << (confAttr.minValue.boolValue?"True":"False")  << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Max Value   = " << (confAttr.maxValue.boolValue?"True":"False")  << std::endl;
			m_sLogFile.flush();
#endif
			break;
		default:
			minValue.intValue = confAttr.minValue.intValue;
			maxValue.intValue = confAttr.maxValue.intValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Value       = " << confValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Min Value   = " << confAttr.minValue.intValue << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Attribute Max Value   = " << confAttr.maxValue.intValue << std::endl;
			m_sLogFile.flush();
#endif
			break;
	}
	return ret;
}

#pragma mark - Camera frame
int CPlayerOne::getROI(int &nLeft, int &nTop, int &nWidth, int &nHeight)
{
	int nErr = PLUGIN_OK;
	POAErrors ret;

	ret = POAGetImageSize(m_nCameraID, &nWidth, &nHeight);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = POAGetImageStartPos(m_nCameraID, &nLeft, &nTop);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = POAGetImageSize(m_nCameraID, &nNewWidth, &nNewHeight);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = POASetImageStartPos(m_nCameraID, m_nReqROILeft, m_nReqROITop);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr =  ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = POAGetImageStartPos(m_nCameraID, &nNewLeft, &nNewTop);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error getting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.maxWidth  = " << m_cameraProperty.maxWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_cameraProperty.maxHeigh = " << m_cameraProperty.maxHeight << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nLeft        = " << nLeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nTop         = " << nTop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nWidth       = " << nWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nHeight      = " << nHeight << std::endl;

	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nNewLeft     = " << nNewLeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nNewTop      = " << nNewTop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nNewWidth    = " << nNewWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nNewHeight   = " << nNewHeight << std::endl;
	m_sLogFile.flush();
#endif

	m_nROILeft = nNewLeft;
	m_nROITop = nNewTop;
	m_nROIWidth = nNewWidth;
	m_nROIHeight = nNewHeight;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] new ROI set" << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	ret = POASetImageStartPos(m_nCameraID, 0, 0);
	if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
		nErr = ERROR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
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
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Checking if a frame is ready." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] cameraState  = " << cameraState << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] cameraState still exposing = " << (cameraState==STATE_EXPOSING?"Yes":"No") << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAImageReady error :  " << POAGetErrorString(ret) << std::endl;
		m_sLogFile.flush();
#endif
	}
	bFrameAvailable = bool(pIsReady);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bFrameAvailable : " << (bFrameAvailable?"Yes":"No")<< std::endl;
	m_sLogFile.flush();
#endif
	// }
	if(bFrameAvailable)
		POAStopExposure(m_nCameraID);

	return bFrameAvailable;
}

void CPlayerOne::getCameraState(int &nState)
{
	POACameraState cameraState;

	POAGetCameraState(m_nCameraID, &cameraState);

	if(cameraState == STATE_EXPOSING) {
		nState = C_EXPOSING;
		return;
	}

	nState = C_IDLE;
	return;

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

	if(!frameBuffer) {
		nErr = ERROR_POINTER;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nHeight         = " << nHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nMemWidth       = " << nMemWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nROILeft      = " << m_nROILeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nReqROILeft   = " << m_nReqROILeft << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nROITop       = " << m_nROITop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nReqROITop    = " << m_nReqROITop << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nROIWidth     = " << m_nROIWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nReqROIWidth  = " << m_nReqROIWidth << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nROIHeight    = " << m_nROIHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nReqROIHeight = " << m_nReqROIHeight << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] getBitDepth()/8 = " << getBitDepth()/8 << std::endl;
	m_sLogFile.flush();
#endif
	try {
		POAGetImageStartPos(m_nCameraID, &tmp1, &tmp2);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageStartPos x  : " << tmp1 << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageStartPos y  : " << tmp2 << std::endl;
		m_sLogFile.flush();
#endif

		POAGetImageSize(m_nCameraID, &tmp1, &tmp2);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageSize w  : " << tmp1 << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageSize h  : " << tmp2 << std::endl;
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
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] srcMemWidth     = " << srcMemWidth << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nMemWidth       = " << nMemWidth << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sizeReadFromCam = " << sizeReadFromCam << std::endl;
		m_sLogFile.flush();
#endif
		ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, 500);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageData error, retrying :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			// wait and retry
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			std::this_thread::yield();
			ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, 500);
			if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POAGetImageData error :  " << POAGetErrorString(ret) << std::endl;
				m_sLogFile.flush();
#endif
				POAStopExposure(m_nCameraID);
				if(imgBuffer != frameBuffer)
					free(imgBuffer);
				nErr = ERROR_RXTIMEOUT;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
				m_sLogFile.flush();
#endif
				return nErr;
			}
		}
		// POAStopExposure(m_nCameraID);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Frame downloaded to buffer." << std::endl;
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
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] copying ("<< m_nROILeft <<","
			<< m_nROITop <<","
			<< m_nROIWidth <<","
			<< m_nROIHeight <<") => ("
			<< m_nReqROILeft <<","
			<< m_nReqROITop <<","
			<< m_nReqROIWidth <<","
			<< m_nReqROIHeight <<")"
			<< std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] srcMemWidth       : " << srcMemWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nMemWidth         : " << nMemWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] copyHeight        : " << copyHeight << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] copyWidth         : " << copyWidth << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] sizeReadFromCam   : " << sizeReadFromCam << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] size to TSX       : " << nHeight * nMemWidth << std::endl;
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
				nErr = ERROR_RXTIMEOUT;
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
				nErr = ERROR_RXTIMEOUT;
			}  catch(const std::exception& e) {

			}
		}
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Done." << std::endl;
	m_sLogFile.flush();
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}


#pragma mark - Camera relay

bool CPlayerOne::isST4Available()
{
	return m_cameraProperty.isHasST4Port;
}

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
	float timeToWait = 0.0;
	CStopWatch pulseTimer;

	if(!m_cameraProperty.isHasST4Port) {
		nErr =  ERROR_COMMANDNOTSUPPORTED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nXPlus       : " << nXPlus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nXMinus      : " << nXMinus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nYPlus       : " << nYPlus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nYMinus      : " << nYMinus << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bSynchronous : " << (bSynchronous?"True":"False") << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] bAbort       : " << (bAbort?"True":"False") << std::endl;
	m_sLogFile.flush();
#endif

	if(bAbort) {
		confValue.boolValue = POA_FALSE;
		ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
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
				nErr = ERROR_COMMANDNOTSUPPORTED;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
				m_sLogFile.flush();
#endif
				return nErr;
			}
		}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] GUI move m_confGuideDir    : " << m_confGuideDir << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] GUI move confValue.boolValue : " << (confValue.boolValue?"True":"False") << std::endl;
		m_sLogFile.flush();
#endif

		ret = POASetConfig(m_nCameraID, m_confGuideDir, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] GUI move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
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
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
		bEast = true;
	}
	if(nXPlus == 0 && nXMinus !=0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
		bWest = true;
	}
	// North/South
	if(nYPlus != 0 && nYMinus ==0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
		bNorth = true;
	}

	if(nYPlus == 0 && nYMinus !=0) {
		ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] AutoGuide move POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
		bSouth = true;
	}

	confValue.boolValue = POA_FALSE; // to stop move
	// One of these will always be zero, so this gives the net
	// plus or minus movement
	netX = nXPlus - nXMinus;
	netY = nYPlus - nYMinus;
	if(netX == 0) {   // netY will not be zero
		// One of nYPLus and nYMinus will be zero, so this expression will work
		timeToWait = float(nYPlus + nYMinus)/1000.0f; // ms -> s
		// Just wait for time to expire and stop relay
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);
		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	if(netY == 0)  {  // netX will not be zero
		// Again, one of these will be zero
		timeToWait = float(nXPlus + nXMinus)/1000.0f; // ms -> s
		// Just wait for time to expire and stop relay
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	//
	//  Dual axis movement, Are they both the same. Wait and then terminate both at same time
	//
	if(abs(netY) == abs(netX)) {
		// Pick one, doesn't matter which
		timeToWait = float(nXPlus + nXMinus)/1000.0f; // ms -> s
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
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	//
	// Dual axis movement, not the same time for each!
	//
	if(abs(netY) < abs(netX)) { // East-West movement was greater
		// Wait for shorter time
		timeToWait = float(nYPlus + nYMinus)/1000.0f; // ms -> s
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}
		// Longer time
		timeToWait = float(nXPlus + nXMinus)/1000.0f; // ms -> s
		while(pulseTimer.GetElapsedSeconds() < timeToWait);

		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
	else { // North-South movement was greater
		// Wait for shorter time
		timeToWait = float(nXPlus + nXMinus)/1000.0f; // ms -> s
		while(pulseTimer.GetElapsedSeconds() < timeToWait);
		// need to know which one to stop
		if(bEast)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_EAST, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_WEST, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}

		// Longer time
		timeToWait = float(nYPlus + nYMinus)/1000.0f; // ms -> s
		while(pulseTimer.GetElapsedSeconds() < timeToWait);

		if(bNorth)
			ret = POASetConfig(m_nCameraID, POA_GUIDE_NORTH, confValue, bAuto);
		else
			ret = POASetConfig(m_nCameraID, POA_GUIDE_SOUTH, confValue, bAuto);

		if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
			m_sLogFile.flush();
#endif
			nErr = ERROR_CMDFAILED;
		}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] nErr = " << nErr << std::endl;
	m_sLogFile.flush();
#endif
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

std::string CPlayerOne::getGainLabelFromListAtIndex(unsigned int nIndex)
{
	if(nIndex<m_GainListLabel.size())
		return m_GainListLabel.at(nIndex);
	else
		return std::string("N/A");
}


int CPlayerOne::getGainFromListAtIndex(unsigned int nIndex)
{
	if(nIndex<m_GainList.size())
		return m_GainList.at(nIndex);
	else
		return m_nHCGain;
}

#ifdef PLUGIN_DEBUG
void CPlayerOne::log(std::string sLogEntry)
{
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] " << sLogEntry << std::endl;
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
