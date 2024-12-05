//
//  PlayerOne.h
//
//  Created by Rodolphe Pineau on 2022/07/06
//  Copyright © 2022 Rodolphe Pineau. All rights reserved.
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


#ifndef WIN32
#include <unistd.h>
#endif

#define ERROR_CMDFAILED				-1
#define ERROR_NODEVICESELECTED 		-2
#define ERROR_NOLINK				-3
#define ERROR_COMMANDNOTSUPPORTED	-4
#define ERROR_POINTER				-5
#define ERROR_COMMANDINPROGRESS		-6
#define ERROR_RXTIMEOUT				-7

#include "PlayerOneCamera.h"
#include "StopWatch.h"

#define PLUGIN_DEBUG    3


#define CODE_VERSION      1.31
#define BUFFER_LEN 128
#define PLUGIN_OK   0
#define MAX_NB_BIN  8
#define VAL_NOT_AVAILABLE           0xDEADBEEF

typedef struct _camera_info {
	int			cameraId;
	std::string	Sn;
	std::string	model;
} camera_info_t;

enum Plugin_Flip_Mode { FLIP_NONE, FLIP_HORI, FLIP_VERT, FLIP_BOTH};
enum cameraState {C_IDLE, C_EXPOSING};

class CPlayerOne {
public:
	CPlayerOne();
	~CPlayerOne();

	void        setUserConf(bool bUserConf);
	int         Connect(std::string sSerial);
	void        Disconnect(void);

	int         getCameraIdFromSerial(int &nCameraId, std::string sSerial);
	int         getCameraSerialFromID(int nCameraId, std::string &sSerial);
	void        getCameraNameFromID(int nCameraId, std::string &sName);

	void        getCameraName(std::string &sName);
	void        getCameraSerial(std::string &sSerial);

	int         listCamera(std::vector<camera_info_t>  &cameraIdList);

	void        getFirmwareVersion(std::string &sVersion);
	void        isUSB3(bool &bUSB3);

	int         getNumBins();
	int         getBinFromIndex(int nIndex);
	int         startCapture(double dTime);
	void        abortCapture(void);

	int         getTemperture(double &dTemp, double &dPower, double &dSetPoint, bool &bEnabled);
	int         setCoolerTemperature(bool bOn, double dTemp);
	double		getCoolerSetTemp();

	int			setCoolerState(bool bOn);

	bool		isCoolerAvailable();
	int         getWidth();
	int         getHeight();
	double      getPixelSize();
	int         setBinSize(int nBin);
	int			getCurrentBin();

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

	int         getROI(int &nLeft, int &nTop, int &nWidth, int &nHeight);
	int         setROI(int nLeft, int nTop, int nWidth, int nHeight);
	int         clearROI(void);

	bool        isFrameAvailable();
	void        getCameraState(int &nState);

	uint32_t    getBitDepth();
	int         getFrame(int nHeight, int nMemWidth, unsigned char* frameBuffer);

	bool		isST4Available();
	int         RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort);

	int         getNbGainInList();
	std::string getGainLabelFromListAtIndex(unsigned int nIndex);
	int         getGainFromListAtIndex(unsigned int nIndex);

	void        rebuildGainList();

	int         getCurentSensorMode(std::string &sSensorMode, int &nModeIndex);
	int         getSensorModeList(std::vector<std::string> &sModes, int &curentModeIndex);
	int         setSensorMode(int nModeIndex);

	bool		isHardwareBinAvailable();
	int         setHardwareBinOn(bool bOn);
	int         getHardwareBinOn(bool &bOn);

	int         getPixelBinMode(bool &bSumMode);
	int         setPixelBinMode(bool bSumMode);
	int         getMonoBin(bool &bMonoBin);
	int         setMonoBin(bool bMonoBin);

	void        getAllUsefulValues(int &nGainHighestDR, int &nHCGain, int &nUnityGain, int &nGainLowestRN,
								   int &nOffsetHighestDR, int &nOffsetHCGain, int &nOffsetUnityGain, int &nOffsetLowestRN);

	int			getGainAdu(double &dMin, double &nMax, double &dValue);
	int			getExposureMinMax(long &nMin, long &nMax);
	long		getExposureMin();
	long		getExposureMax();

	bool		getFastReadoutAvailable();
	bool		isFastReadoutEnabled();

	int			getMaxBin();
	std::string	getSensorName();
#ifdef PLUGIN_DEBUG
	void log(std::string sLogEntry);
#endif

protected:

	POAErrors               getConfigValue(POAConfig confID , POAConfigValue &confValue, POAConfigValue &minValue, POAConfigValue &maxValue,  POABool &bAuto);
	POAErrors               setConfigValue(POAConfig confID , POAConfigValue confValue,  POABool bAuto = POA_FALSE);
	void                    buildGainList();

	bool                    m_bSetUserConf = false;
	int                     m_nCameraNum = 0;

	int                     m_nCameraID = 0;
	std::string             m_sCameraName = "";
	std::string             m_sCameraSerial = "";
	std::string				m_sSensorModelName;

	POACameraProperties     m_cameraProperty;
	POAImgFormat            m_nImageFormat = POA_RAW16;
	std::vector<POASensorModeInfo>       m_sensorModeInfo;
	int                     m_nSensorModeIndex = 0;
	int                     m_nSensorModeCount = 0;

	int                     m_nControlNums = 0;
	std::vector<POAConfigAttributes> m_ControlList;

	std::vector<std::string>    m_GainListLabel;
	std::vector<int>        m_GainList;
	int                     m_nNbGainValue = VAL_NOT_AVAILABLE;

	long                    m_nGain = VAL_NOT_AVAILABLE;
	long                    m_nWbR = VAL_NOT_AVAILABLE;
	bool                    m_bR_Auto = false;
	long                    m_nWbG = VAL_NOT_AVAILABLE;
	bool                    m_bG_Auto = false;
	long                    m_nWbB = VAL_NOT_AVAILABLE;
	bool                    m_bB_Auto = false;
	long                    m_nFlip = VAL_NOT_AVAILABLE;
	long                    m_nAutoExposureTarget = 0;
	long                    m_nOffset = VAL_NOT_AVAILABLE;

	bool                    m_bPixelBinMode = false;
	bool                    m_bPixelMonoBin = false;
	bool                    m_bHasMonoBinMode = false;
	long                    m_nUSBBandwidth = 100;
	long                    m_nLensHeaterPowerPerc = VAL_NOT_AVAILABLE;

	double                  m_dPixelSize = 0;
	int                     m_nMaxWidth = -1;
	int                     m_nMaxHeight = -1;
	bool                    m_bIsColorCam = false;
	int                     m_nBayerPattern = POA_BAYER_BG;
	int                     m_nMaxBitDepth = 16;
	int                     m_nNbBin = 1;
	int                     m_SupportedBins[MAX_NB_BIN] = {};
	int                     m_nCurrentBin = 1;
	bool                    m_bHasHardwareBin = false;
	bool					m_bHardwareBinEnabled = false;
	bool                    m_bHasRelayOutput = false;

	bool                    m_bConnected = false;

	unsigned char *         m_pframeBuffer = nullptr;

	bool                    m_bDeviceIsUSB = true;
	bool                    m_bAbort = false;
	std::map<int,bool>      m_mAvailableFrameRate;
	int                     m_nNbBitToShift = 0;

	double                  m_dCaptureLenght = 0;

	long					m_nExposureMax = 0;
	long					m_nExposureMin = 0;

	int                     m_nROILeft = -1;
	int                     m_nROITop = -1;
	int                     m_nROIWidth = -1;
	int                     m_nROIHeight = -1;

	int                     m_nReqROILeft = -1;
	int                     m_nReqROITop = -1;
	int                     m_nReqROIWidth = -1;
	int                     m_nReqROIHeight = -1;

	bool                    m_bHasLensHeater = false;
	double					m_dCoolerSetTemp = 0;

	// special gain and offset data
	int                     m_nGainHighestDR = VAL_NOT_AVAILABLE;
	int                     m_nHCGain = VAL_NOT_AVAILABLE;
	int                     m_nUnityGain = VAL_NOT_AVAILABLE;
	int                     m_nGainLowestRN = VAL_NOT_AVAILABLE;

	int                     m_nOffsetHighestDR = VAL_NOT_AVAILABLE;
	int                     m_nOffsetHCGain = VAL_NOT_AVAILABLE;
	int                     m_nOffsetUnityGain = VAL_NOT_AVAILABLE;
	int                     m_nOffsetLowestRN = VAL_NOT_AVAILABLE;

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
