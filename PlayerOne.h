//
//  PlayerOne.hpp
//
//  Created by Rodolphe Pineau on 2022/07/06
//  Copyright © 2022 RTI-Zone. All rights reserved.
//

#ifndef PlayerOne_hpp
#define PlayerOne_hpp

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <thread>
#include <fstream>
#include <iomanip>


#ifndef SB_WIN_BUILD
#include <unistd.h>
#endif

#include "../../licensedinterfaces/sberrorx.h"

#include "PlayerOneCamera.h"
#include "StopWatch.h"

// #define PLUGIN_DEBUG    3

#define PLUGIN_VERSION      1.23
#define BUFFER_LEN 128
#define PLUGIN_OK   0
#define MAX_NB_BIN  8
#define VAL_NOT_AVAILABLE           0xDEADBEEF

typedef struct _camera_info {
    int     cameraId;
    std::string    Sn;
    std::string    model;
} camera_info_t;

enum Plugin_Flip_Mode { FLIP_NONE, FLIP_HORI, FLIP_VERT, FLIP_BOTH};

class CPlayerOne {
public:
    CPlayerOne();
    ~CPlayerOne();

    void        setUserConf(bool bUserConf);
    int         Connect(int nCameraId);
    void        Disconnect(void);
    void        setCameraId(int nCameraId);
    void        getCameraId(int &nCcameraId);
    int         getCameraIdFromSerial(int &nCameraId, std::string sSerial);
    int         getCameraSerialFromID(int nCameraId, std::string &sSerial);
    void        getCameraNameFromID(int nCameraId, std::string &sName);
    
    void        getCameraName(std::string &sName);
    void        setCameraSerial(std::string sSerial);
    void        getCameraSerial(std::string &sSerial);

    int         listCamera(std::vector<camera_info_t>  &cameraIdList);

    void        getFirmwareVersion(std::string &sVersion);
    void        isUSB3(bool &bUSB3);
    
    int         getNumBins();
    int         getBinFromIndex(int nIndex);
    int         startCaputure(double dTime);
    void        abortCapture(void);

    int         getTemperture(double &dTemp, double &dPower, double &dSetPoint, bool &bEnabled);
    int         setCoolerTemperature(bool bOn, double dTemp);
    int         getWidth();
    int         getHeight();
    double      getPixelSize();
    int         setBinSize(int nBin);
    
    bool        isCameraColor();
    void        getBayerPattern(std::string &sBayerPattern);
    void        getFlip(std::string &sFlipMode);

    int         getGain(long &nMin, long &nMax, long &nValue);
    int         setGain(long nGain);
    int         getWB_R(long &nMin, long &nMax, long &nValue, bool &bIsAuto);
    int         setWB_R(long nWB_R, bool bIsAuto = POA_FALSE);
    int         getWB_G(long &nMin, long &nMax, long &nValue, bool &bIsAuto);
    int         setWB_G(long nWB_G, bool bIsAuto = POA_FALSE);
    int         getWB_B(long &nMin, long &nMax, long &nValue, bool &bIsAuto);
    int         setWB_B(long nWB_B, bool bIsAuto = POA_FALSE);
    int         getFlip(long &nMin, long &nMax, long &nValue);
    int         setFlip(long nFlip);
    int         getOffset(long &nMin, long &nMax, long &nValue);
    int         setOffset(long nBlackLevel);

    int         getUSBBandwidth(long &nMin, long &nMax, long &nValue);
    int         setUSBBandwidth(long nBandwidth);

    bool        isLensHeaterAvailable();
    int         getLensHeaterPowerPerc(long &nMin, long &nMax, long &nValue);
    int         setLensHeaterPowerPerc(long nPercent);

    int         setROI(int nLeft, int nTop, int nWidth, int nHeight);
    int         clearROI(void);

    bool        isFameAvailable();
    
    uint32_t    getBitDepth();
    int         getFrame(int nHeight, int nMemWidth, unsigned char* frameBuffer);

    int         RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort);

    int         getNbGainInList();
    std::string getGainLabelFromListAtIndex(int nIndex);
    int         getGainFromListAtIndex(int nIndex);
    
    void        rebuildGainList();

    int         getCurentSensorMode(std::string &sSensorMode, int &nModeIndex);
    int         getSensorModeList(std::vector<std::string> &sModes, int &curentModeIndex);
    int         setSensorMode(int nModeIndex);

    bool        hasMonoBin();
    int         setHardwareBinOn(bool bOn);

    int         getPixelBinMode(bool &bSumMode);
    int         setPixelBinMode(bool bSumMode);
    int         getMonoBin(bool &bMonoBin);
    int         setMonoBin(bool bMonoBin);

    void        getAllUsefulValues(int &nGainHighestDR, int &nHCGain, int &nUnityGain, int &nGainLowestRN,
                                 int &nOffsetHighestDR, int &nOffsetHCGain, int &nOffsetUnityGain, int &nOffsetLowestRN);

#ifdef PLUGIN_DEBUG
    void log(std::string sLogEntry);
#endif

protected:
    
    POAErrors               getConfigValue(POAConfig confID , POAConfigValue &confValue, POAConfigValue &minValue, POAConfigValue &maxValue,  POABool &bAuto);
    POAErrors               setConfigValue(POAConfig confID , POAConfigValue confValue,  POABool bAuto = POA_FALSE);
    void                    buildGainList();

    bool                    m_bSetUserConf;
    int                     m_nCameraNum;
    
    POACameraProperties     m_cameraProperty;
    POAImgFormat            m_nImageFormat;
    std::vector<POASensorModeInfo>       m_sensorModeInfo;
    int                     m_nSensorModeIndex;
    int                     m_nSensorModeCount;

    int                     m_nControlNums;
    std::vector<POAConfigAttributes> m_ControlList;

    std::vector<std::string>    m_GainListLabel;
    std::vector<int>            m_GainList;
    int                     m_nNbGainValue;

    long                    m_nGain;
    long                    m_nWbR;
    bool                    m_bR_Auto;
    long                    m_nWbG;
    bool                    m_bG_Auto;
    long                    m_nWbB;
    bool                    m_bB_Auto;
    long                    m_nFlip;
    long                    m_nAutoExposureTarget;
    long                    m_nOffset;

    bool                    m_bPixelBinMode;
    bool                    m_bPixelMonoBin;
    bool                    m_bHasMonoBinMode;
    long                    m_nUSBBandwidth;
    long                    m_nLensHeaterPowerPerc;
    
    double                  m_dPixelSize;
    int                     m_nMaxWidth;
    int                     m_nMaxHeight;
    bool                    m_bIsColorCam;
    int                     m_nBayerPattern;
    int                     m_nMaxBitDepth;
    int                     m_nNbBin;
    int                     m_SupportedBins[MAX_NB_BIN];
    int                     m_nCurrentBin;
    bool                    m_bHasHardwareBin;
    bool                    m_bHasRelayOutput;

    bool                    m_bConnected;
 
    unsigned char *         m_pframeBuffer;
    int                     m_nCameraID;
    std::string             m_sCameraName;
    std::string             m_sCameraSerial;
    
    bool                    m_bDeviceIsUSB;
    bool                    m_bAbort;
    std::map<int,bool>      m_mAvailableFrameRate;
    int                     m_nNbBitToShift;
    
    double                  m_dCaptureLenght;
    
    int                     m_nROILeft;
    int                     m_nROITop;
    int                     m_nROIWidth;
    int                     m_nROIHeight;

    int                     m_nReqROILeft;
    int                     m_nReqROITop;
    int                     m_nReqROIWidth;
    int                     m_nReqROIHeight;

    bool                    m_bHasLensHeater;
    
    // special gain and offset data
    int                     m_nGainHighestDR;
    int                     m_nHCGain;
    int                     m_nUnityGain;
    int                     m_nGainLowestRN;

    int                     m_nOffsetHighestDR;
    int                     m_nOffsetHCGain;
    int                     m_nOffsetUnityGain;
    int                     m_nOffsetLowestRN;

    POAConfig               m_confGuideDir;

    CStopWatch              m_ExposureTimer;

#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
#endif

};
#endif /* PlayerOne_hpp */
