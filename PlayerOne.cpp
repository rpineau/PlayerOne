//
//  PlayerOne.cpp
//
//  Created by Rodolphe Pineau on 06/12/2017
//  Copyright © 2017 RTI-Zone. All rights reserved.
//

#include "PlayerOne.h"


CPlayerOne::CPlayerOne()
{

    m_bSetUserConf = false;

    m_nImageFormat = POA_RAW16;
    m_sensorModeInfo.clear();
    m_nSensorModeIndex = 0;
    m_nSensorModeCount = 0;
    m_nControlNums = 0;
    m_ControlList.clear();
    m_GainList.clear();
    m_nNbGainValue = VAL_NOT_AVAILABLE;
    m_nGain = VAL_NOT_AVAILABLE;
    m_nWbR = VAL_NOT_AVAILABLE;
    m_bR_Auto = false;;
    m_nWbG = VAL_NOT_AVAILABLE;
    m_bG_Auto = false;;
    m_nWbB = VAL_NOT_AVAILABLE;
    m_bB_Auto =false;
    m_nFlip = POA_FLIP_NONE;
    m_nAutoExposureTarget = 0;
    m_nOffset = VAL_NOT_AVAILABLE;
    m_bPixelBinMode = false;
    m_nUSBBandwidth = 100;
    m_dPixelSize = 0;
    m_nMaxWidth = -1;
    m_nMaxHeight = -1;
    m_bIsColorCam = false;
    m_nBayerPattern = POA_BAYER_BG;
    m_nMaxBitDepth = 16;
    m_nNbBin = 1;
    m_nCurrentBin  = 1;
    m_bHasRelayOutput = false;
    m_bConnected = false;
    m_pframeBuffer = nullptr;
    m_nCameraID = 0;
    m_sCameraName.clear();
    m_sCameraSerial.clear();
    m_bDeviceIsUSB = true;
    m_bAbort = false;
    m_mAvailableFrameRate.clear();
    m_nNbBitToShift=0;
    m_ExposureTimer.Reset();
    m_ImageDownloadTimer.Reset();
    m_dCaptureLenght = 0;
    m_nROILeft = -1;
    m_nROITop = -1;
    m_nROIWidth = 1;
    m_nROIHeight = -1;
    m_nReqROILeft = -1;
    m_nReqROITop = -1;
    m_nReqROIWidth = -1;
    m_nReqROIHeight = -1;
    m_nOffsetHighestDR = VAL_NOT_AVAILABLE;
    m_nOffsetUnityGain = VAL_NOT_AVAILABLE;
    m_nGainLowestRN = VAL_NOT_AVAILABLE;
    m_nOffsetLowestRN = VAL_NOT_AVAILABLE;
    m_nHCGain = VAL_NOT_AVAILABLE;

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
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] " << sSDKVersion << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CPlayerOne] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif
    
}

CPlayerOne::~CPlayerOne()
{

    if(m_pframeBuffer)
        free(m_pframeBuffer);


}

#pragma mark - Camera access
void CPlayerOne::setUserConf(bool bUserConf)
{
    m_bSetUserConf = bUserConf;
}

int CPlayerOne::Connect(int nCameraID)
{
    int nErr = PLUGIN_OK;
    int i;

    POAErrors ret;
    POAConfigAttributes Attribute;
    POASensorModeInfo sensorMode;
    std::string sTmp;

    long nMin, nMax;

    if(nCameraID)
        m_nCameraID = nCameraID;
    else {
        // check if there is at least one camera connected to the system
        if(POAGetCameraCount() == 1) {
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
    if (ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect]  Error connecting to camera ID " << m_nCameraID << " serial " << m_sCameraSerial << " , Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_NORESPONSE;
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
    ret = POAGetGainOffset(m_nCameraID, &m_nOffsetHighestDR, &m_nOffsetUnityGain, &m_nGainLowestRN, &m_nOffsetLowestRN, &m_nHCGain);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at highest dynamic range        : " << m_nOffsetHighestDR << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at unity gain                   : " << m_nOffsetHighestDR << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Gain at lowest read noise              : " << m_nGainLowestRN << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Offset at lowest read noise            : " << m_nOffsetLowestRN << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Gain at HCG Mode(High Conversion Gain) : " << m_nHCGain << std::endl;
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
        setSensorMode(m_nSensorModeIndex);
        setUSBBandwidth(m_nUSBBandwidth);
        setPixelBinMode(m_bPixelBinMode);
    }
    else {
        getGain(nMin, nMax , m_nGain);
        getOffset(nMin, nMax, m_nOffset);
        getWB_R(nMin, nMax, m_nWbR, m_bR_Auto);
        getWB_G(nMin, nMax, m_nWbG, m_bG_Auto);
        getWB_B(nMin, nMax, m_nWbB, m_bB_Auto);
        getFlip(nMin, nMax, m_nFlip);
        getCurentSensorMode(sTmp, m_nSensorModeIndex);
        getUSBBandwidth(nMin, nMax, m_nUSBBandwidth);
        getPixelBinMode(m_bPixelBinMode);
    }

    rebuildGainList();

    ret = POASetImageFormat(m_nCameraID, m_nImageFormat);

    POAGetSensorModeCount(m_nCameraID, &m_nSensorModeCount);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode count : " << m_nSensorModeCount << std::endl;
    m_sLogFile.flush();
#endif
    // if the camera supports it, in general, there are at least two sensor modes[Normal, LowNoise, ...]
    for(i=0; i< m_nSensorModeCount; i++) {
        ret = POAGetSensorModeInfo(m_nCameraID, i, &sensorMode);
        if(!ret)
            continue;
        m_sensorModeInfo.push_back(sensorMode);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode " << i << " name : " << sensorMode.name << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] sensor mode " << i << " desc : " << sensorMode.desc << std::endl;
        m_sLogFile.flush();
#endif
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected" << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

void CPlayerOne::Disconnect()
{

    POAStopExposure(m_nCameraID);
    POACloseCamera(m_nCameraID);

    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }
    m_bConnected = false;

}

void CPlayerOne::setCameraId(int nCameraId)
{
    m_nCameraID = nCameraId;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCameraId] nCameraId = " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif
}

void CPlayerOne::getCameraId(int &nCameraId)
{
    nCameraId = m_nCameraID;
}

void CPlayerOne::getCameraIdFromSerial(int &nCameraId, std::string sSerial)
{
    POAErrors ret;

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
}

void CPlayerOne::getCameraSerialFromID(int nCameraId, std::string &sSerial)
{
    POAErrors ret;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] nCameraId = " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif

    nCameraId = 0;
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

void CPlayerOne::setCameraSerial(std::string sSerial)
{
    m_sCameraSerial.assign(sSerial);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCameraId] sSerial = " << m_sCameraSerial << std::endl;
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
    // list camera connected to the system
    int cameraNum = POAGetCameraCount();

    POAErrors ret;
    for (int i = 0; i < cameraNum; i++)
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
        strncpy(tCameraInfo.model, m_cameraProperty.cameraModelName, BUFFER_LEN);
        strncpy((char *)tCameraInfo.Sn, m_cameraProperty.SN, 64);
            
        cameraIdList.push_back(tCameraInfo);
        }
    }

    return nErr;
}

void CPlayerOne::getFirmwareVersion(std::string &sVersion)
{
    std::string sTmp;
    sTmp.assign(POAGetSDKVersion());
    sVersion = "SDK version " + sTmp;
}


int CPlayerOne::getNumBins()
{
    return m_nNbBin;
}

int CPlayerOne::getBinFromIndex(int nIndex)
{
    if(nIndex>(m_nNbBin-1))
        return 1;
    
    return m_SupportedBins[nIndex];        
}

int CPlayerOne::getCurentSensorMode(std::string &sSensorMode, int &nModeIndex)
{
    int nErr = PLUGIN_OK;
    POAErrors ret;

    ret =  POAGetSensorMode(m_nCameraID, &nModeIndex);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorMode] Error getting current sensor mode." << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    sSensorMode = m_sensorModeInfo[nModeIndex].name;
    return nErr;
}

int CPlayerOne::getSensorModeList(std::vector<std::string> &sModes, int &curentModeIndex)
{
    int nErr = PLUGIN_OK;
    POAErrors ret;

    if(!m_nSensorModeCount) // sensor mode no supported by camera
        return -1;

    ret = POAGetSensorMode(m_nCameraID, &curentModeIndex);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getSensorMode] Error getting current sensor mode." << std::endl;
        m_sLogFile.flush();
#endif
        return -1;
    }
    sModes.clear();
    for (POASensorModeInfo mode : m_sensorModeInfo) {
        sModes.push_back(mode.name);
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCurentSensorMode] Current Sensor mode is " << sModes.at(curentModeIndex) << std::endl;
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

    ret = getConfigValue(POA_PIXEL_BIN_SUM, confValue, minValue, maxValue, bAuto);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPixelBinMode] Error getting camera plixel mode. Error= " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    // POA_TRUE is sum and POA_FALSE is average,
    bSumMode = (confValue.boolValue == POA_TRUE)?true:false;

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

    if(m_bConnected)
    // POA_TRUE is sum and POA_FALSE is average,
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

void CPlayerOne::getUserfulValues(int &nOffsetHighestDR, int &nOffsetUnityGain, int &nGainLowestRN, int &nOffsetLowestRN, int &nHCGain)
{
    nOffsetHighestDR = m_nOffsetHighestDR;
    nOffsetUnityGain = m_nOffsetUnityGain;
    nGainLowestRN = m_nGainLowestRN;
    nOffsetLowestRN = m_nOffsetLowestRN;
    nHCGain = m_nHCGain;
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
        if(m_cameraProperty.isHasCooler) {
            ret = getConfigValue(POA_TARGET_TEMP, confValue, minValue, maxValue, bAuto);
            if(ret == POA_OK)
                dPower = double(confValue.intValue);
            else
                dPower = 0;

            ret = getConfigValue(POA_COOLER_POWER, confValue, minValue, maxValue, bAuto);
            if(ret == POA_OK)
                dSetPoint = double(confValue.intValue);
            else
                dSetPoint = dTemp;

            ret = getConfigValue(POA_COOLER, confValue, minValue, maxValue, bAuto);
            if(ret == POA_OK)
                bEnabled = bool(confValue.boolValue);
            else
                bEnabled = false;
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

bool CPlayerOne::isCameraColor()
{
    return m_cameraProperty.isColorCamera;
}

void CPlayerOne::getBayerPattern(std::string &sBayerPattern)
{
    if(m_cameraProperty.isColorCamera) {
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
        return -1;
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

    ret = getConfigValue(POA_WB_R, confValue, minValue, maxValue, bAuto);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] Error getting WB_R, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return -1;
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

    ret = getConfigValue(POA_WB_G, confValue, minValue, maxValue, bAuto);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_G] Error getting WB_G, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return -1;
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

    ret = getConfigValue(POA_WB_B, confValue, minValue, maxValue, bAuto);
    if(ret) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_B] Error getting WB_B, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return -1;
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
        return -1;
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
        return -1;
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
        return -1;
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
        return -1;
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
        return -1;
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
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWB_R] Error getting USB Bandwidth, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return -1;
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


POAErrors CPlayerOne::setConfigValue(POAConfig confID , POAConfigValue confValue,  POABool bAuto)
{
    POAErrors ret;
    
    if(!m_bConnected)
        return POA_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confID = " << confID << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setConfigValue] confValue = " << confValue.intValue << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] getting values for attribute " << confID << std::endl;
    m_sLogFile.flush();
#endif


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
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << confAttr.minValue.intValue << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.intValue << std::endl;
            m_sLogFile.flush();
#endif
            break;
        case VAL_FLOAT :
            minValue.floatValue = confAttr.minValue.floatValue;
            maxValue.floatValue = confAttr.maxValue.floatValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << confAttr.minValue.floatValue << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << confAttr.maxValue.floatValue << std::endl;
            m_sLogFile.flush();
#endif
            break;
        case VAL_BOOL :
            minValue.boolValue = confAttr.minValue.boolValue;
            maxValue.boolValue = confAttr.maxValue.boolValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Min Value   = " << (confAttr.minValue.boolValue?"True":"False")  << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getConfigValue] Attribute Max Value   = " << (confAttr.maxValue.boolValue?"True":"False")  << std::endl;
            m_sLogFile.flush();
#endif
            break;
        default:
            minValue.intValue = confAttr.minValue.intValue;
            maxValue.intValue = confAttr.maxValue.intValue;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
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

    // X
    if( m_nReqROILeft % 4 != 0)
        nNewLeft = (m_nReqROILeft/4) * 4;  // round to lower 4 pixel. boundary
    else
        nNewLeft = m_nReqROILeft;

    // W
    if( (m_nReqROIWidth % 4 != 0) || (nLeft!=nNewLeft)) {// Adjust width to upper 4 boundary or if the left border changed we need to adjust the width
        nNewWidth = (( (m_nReqROIWidth + (nNewLeft%4)) /4) + 1) * 4;
        if ((nNewLeft + nNewWidth) > int(m_cameraProperty.maxWidth/m_nCurrentBin)) {
            nNewLeft -=4;
            if(nNewLeft<0) {
                nNewLeft = 0;
                nNewWidth = nNewWidth - 4;
            }
        }
    }
    else
        nNewWidth = m_nReqROIWidth;

    // Y
    if( m_nReqROITop % 2 != 0)
        nNewTop = (m_nReqROITop/2) * 2;  // round to lower even pixel.
    else
        nNewTop = m_nReqROITop;

    // H
    if( (m_nReqROIHeight % 2 != 0) || (nTop!=nNewTop)) {// Adjust height to lower 2 boundary or if the top changed we need to adjust the height
        nNewHeight = (((m_nReqROIHeight + (nNewTop%2))/2) + 1) * 2;
        if((nNewTop + nNewHeight) > int(m_cameraProperty.maxHeight/m_nCurrentBin)) {
            nNewTop -=2;
            if(nNewTop <0) {
                nNewTop = 0;
                nNewHeight = nNewHeight - 2;
            }
        }
    }
    else
        nNewHeight = m_nReqROIHeight;

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


    if( m_nROILeft == nNewLeft && m_nROITop == nNewTop && m_nROIWidth == nNewWidth && m_nROIHeight == nNewHeight) {
        return nErr; // no change since last ROI change request
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Requested x, y, w, h : " << nLeft << ", " << nTop << ", " << nWidth << ", " << nHeight <<  std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Set to    x, y, w, h : " << nNewLeft << ", " << nNewTop << ", " << nNewWidth << ", " << nNewHeight <<  std::endl;
    m_sLogFile.flush();
#endif

    ret = POASetImageStartPos(m_nCameraID, nNewLeft, nNewTop);
    if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    ret = POASetImageSize(m_nCameraID,nNewWidth,nNewHeight);
    if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

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

    ret = POASetImageStartPos(m_nCameraID, 0, 0);
    if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [clearROI] Error setting new Left and top, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    ret = POASetImageSize(m_nCameraID,  m_cameraProperty.maxWidth/m_nCurrentBin , m_cameraProperty.maxHeight/m_nCurrentBin);
    if(ret != POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [clearROI] Error setting new Width and Height, Error = " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}


bool CPlayerOne::isFameAvailable()
{
    bool bFrameAvailable = false;
    POACameraState cameraState;
    POABool pIsReady = POA_FALSE;
    POAErrors ret;

    if(m_ExposureTimer.GetElapsedSeconds() > m_dCaptureLenght) {
        POAGetCameraState(m_nCameraID, &cameraState);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] Checking if a frame is ready." << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] cameraState  = " << cameraState << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] cameraState still exposing = " << (cameraState==STATE_EXPOSING?"Yes":"No") << std::endl;
        m_sLogFile.flush();
#endif
        if(cameraState == STATE_EXPOSING)
        {
            bFrameAvailable = false;
        }
        else {
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
        }
    }
    return bFrameAvailable;
}

uint32_t CPlayerOne::getBitDepth()
{
    return 16; // m_cameraProperty.bitDepth;
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
    int exposure_ms = 0;

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
    exposure_ms = (int)(m_dCaptureLenght * 1000);
    ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, exposure_ms + 500);
    if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageData error, retrying :  " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        // wait and retry
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::this_thread::yield();
        ret = POAGetImageData(m_nCameraID, imgBuffer, sizeReadFromCam, exposure_ms + 500);
        if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] POAGetImageData error :  " << POAGetErrorString(ret) << std::endl;
            m_sLogFile.flush();
#endif
            if(imgBuffer)
                free(imgBuffer);
            return ERR_RXTIMEOUT;
        }
    }
    // shift data
    if(m_nNbBitToShift) {
        buf = (uint16_t *)imgBuffer;
        for(int i=0; i<sizeReadFromCam/2; i++)
            buf[i] = buf[i]<<m_nNbBitToShift;
    }

    if(imgBuffer != frameBuffer) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] copying ("<< m_nROILeft <<","
                                                                        << m_nROITop <<","
                                                                        << m_nROIWidth <<","
                                                                        << m_nROIHeight <<") => ("
                                                                        << m_nReqROILeft <<","
                                                                        << m_nReqROITop <<","
                                                                        << m_nReqROIWidth <<","
                                                                        << m_nReqROIHeight <<","
                                                                        << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] srcMemWidth  =>  nMemWidth   : " << srcMemWidth << " => " << nMemWidth << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] sizeReadFromCam              : " << sizeReadFromCam << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] size to TSX                  : " << nHeight * nMemWidth << std::endl;
        m_sLogFile.flush();
#endif
        // copy every line from source buffer newly aligned into TSX buffer cutting at nMemWidth
        for(i=0; i<nHeight; i++) {
            memcpy(frameBuffer+(i*nMemWidth), imgBuffer+(i*srcMemWidth), nMemWidth);
        }
        free(imgBuffer);
    }
    return nErr;
}


#pragma mark - Camera relay


int CPlayerOne::RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort)
{
    int nErr = PLUGIN_OK;
    POAErrors ret;
    POAConfigValue confValue;
    POABool bAuto = POA_FALSE;

    if(!m_cameraProperty.isHasST4Port)
        return ERR_COMMANDNOTSUPPORTED;

    if(!bAbort) {
        confValue.boolValue = POA_TRUE;
        if(nXPlus != 0 && nXMinus ==0)
            m_confIDGuideDir = POA_GUIDE_WEST;
        if(nXPlus == 0 && nXMinus !=0)
            m_confIDGuideDir = POA_GUIDE_EAST;

        if(nYPlus != 0 && nYMinus ==0)
            m_confIDGuideDir = POA_GUIDE_SOUTH;
        if(nYPlus == 0 && nYMinus !=0)
            m_confIDGuideDir = POA_GUIDE_NORTH;

        ret = POASetConfig(m_nCameraID, m_confIDGuideDir, confValue, bAuto);
    }
    else {
        confValue.boolValue = POA_FALSE;
        ret = POASetConfig(m_nCameraID, m_confIDGuideDir, confValue, bAuto);
    }

    if(ret!=POA_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [RelayActivate] POASetConfig error :  " << POAGetErrorString(ret) << std::endl;
        m_sLogFile.flush();
#endif
        nErr = ERR_CMDFAILED;
    }
    return nErr;

}

#pragma mark - helper functions

void CPlayerOne::buildGainList(long nMin, long nMax, long nValue)
{
    long i = 0;
    int nStep = 1;
    m_GainList.clear();
    m_nNbGainValue = 0;

    if(nMin != nValue) {
        m_GainList.push_back(std::to_string(nValue));
        m_nNbGainValue++;
    }

    nStep = int(float(nMax-nMin)/20);
    for(i=nMin; i<nMax; i+=nStep) {
        m_GainList.push_back(std::to_string(i));
        m_nNbGainValue++;
    }
    m_GainList.push_back(std::to_string(nMax));
    m_nNbGainValue++;
}
int CPlayerOne::getNbGainInList()
{
    return m_nNbGainValue;
}

void CPlayerOne::rebuildGainList()
{
    long nMin, nMax, nVal;
    getGain(nMin, nMax, nVal);
    buildGainList(nMin, nMax, nVal);
}

std::string CPlayerOne::getGainFromListAtIndex(int nIndex)
{
    if(nIndex<m_GainList.size())
        return m_GainList.at(nIndex);
    else
        return std::string("N/A");
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
