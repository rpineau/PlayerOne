//  Created by Rodolphe Pineau on 2022/07/06
//  Copyright © 2022 Rodolphe Pineau. All rights reserved.
//

#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../licensedinterfaces/cameradriverinterface.h"
#include "../../licensedinterfaces/pixelsizeinterface.h"
#include "../../licensedinterfaces/noshutterinterface.h"
#include "../../licensedinterfaces/modalsettingsdialoginterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/x2guiinterface.h"
#include "../../licensedinterfaces/addfitskeyinterface.h"
#include "../../licensedinterfaces/subframeinterface.h"
#include "../../licensedinterfaces/cameradependentsettinginterface.h"


#include "PlayerOne.h"

#define PLUGIN_VERSION CODE_VERSION
class SerXInterface;
class TheSkyXFacadeForDriversInterface;
class SleeperInterface;
class BasicIniUtilInterface;
class LoggerInterface;
class MutexInterface;
class TickCountInterface;
class AddFITSKeyInterface;

//For properties that need to be persistent
#define KEY_X2CAM_ROOT              "PlayerOne"
#define KEY_USER_CONF               "PlayerOne_User_Conf"
#define KEY_GUID                    "PlayerOne_GUID"
#define KEY_GAIN                    "Gain"
#define KEY_WHITE_BALANCE_R         "WB_R"
#define KEY_WHITE_BALANCE_R_AUTO    "WB_R_Auto"
#define KEY_WHITE_BALANCE_G         "WB_G"
#define KEY_WHITE_BALANCE_G_AUTO    "WB_G_Auto"
#define KEY_WHITE_BALANCE_B         "WB_B"
#define KEY_WHITE_BALANCE_B_AUTO    "WB_B_Auto"
#define KEY_FLIP                    "Flip"
#define KEY_SENSOR_MODE             "SensorMode"
#define KEY_OFFSET                  "Offset"
#define KEY_USB_BANDWIDTH           "USBBandwidth"
#define PIXEL_HARD_BIN              "PixelHardBin"
#define PIXEL_BIN_MODE              "PixelBinMode"
#define PIXEL_MONO_BIN              "PixelMonoBin"
#define LENS_POWER                  "LensPower"

enum DIALOGS {SELECT, SETTINGS };


// extra fit heard indexes
enum EXTRA_FIT_STRING {
    F_BAYER = 0,
    F_BAYERPAT,
    F_FLIP,
    F_SENSOR_MODE
};

enum EXTRA_FIT_INT {
    F_GAIN = 0,
    F_EXPOSURE,
    F_WB_R,
    F_WB_G,
    F_WB_B,
    F_BLACK_OFFSET
};


/*!
 \b
 rief The X2Camera example.

\ingroup Example

Use this example to write an X2Camera driver.
*/
class X2Camera: public CameraDriverInterface, public SubframeInterface, public  NoShutterInterface, public ModalSettingsDialogInterface, public X2GUIEventInterface, public PixelSizeInterface, public  AddFITSKeyInterface, public CameraDependentSettingInterface
{
public: 
	/*!Standard X2 constructor*/
	X2Camera(const char* pszSelectionString, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface* pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*				pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*					pIOMutex,
					TickCountInterface*				pTickCount);
	virtual ~X2Camera();  

	/*!\name DriverRootInterface Implementation
	See DriverRootInterface.*/
	//@{ 
	virtual int queryAbstraction(const char* pszName, void** ppVal);
	//@} 

	/*!\name DriverInfoInterface Implementation
	See DriverInfoInterface.*/
	//@{ 
	virtual void    driverInfoDetailedInfo(BasicStringInterface& str) const;
	virtual double  driverInfoVersion(void) const;
	//@} 

	/*!\name HardwareInfoInterface Implementation
	See HardwareInfoInterface.*/
	//@{ 
	virtual void deviceInfoNameShort(BasicStringInterface& str) const;
	virtual void deviceInfoNameLong(BasicStringInterface& str) const;
	virtual void deviceInfoDetailedDescription(BasicStringInterface& str) const;
	virtual void deviceInfoFirmwareVersion(BasicStringInterface& str);
	virtual void deviceInfoModel(BasicStringInterface& str);
	//@} 

public://Properties

	/*!\name CameraDriverInterface Implementation
	See CameraDriverInterface.*/
	//@{ 

	virtual enumCameraIndex	cameraId();
	virtual	void            setCameraId(enumCameraIndex Cam);
    virtual bool            isLinked()					{return m_bLinked;};
    virtual void            setLinked(const bool bYes)	{m_bLinked = bYes;};
	
    virtual int             GetVersion(void)			{return CAMAPIVERSION;};
	virtual CameraDriverInterface::ReadOutMode readoutMode(void);
	virtual int             pathTo_rm_FitsOnDisk(char* lpszPath, const int& nPathSize);

public://Methods

	virtual int CCSettings(const enumCameraIndex& Camera, const enumWhichCCD& CCD);

	virtual int CCEstablishLink(enumLPTPort portLPT, const enumWhichCCD& CCD, enumCameraIndex DesiredCamera, enumCameraIndex& CameraFound, const int nDesiredCFW, int& nFoundCFW);
	virtual int CCDisconnect(const bool bShutDownTemp);

	virtual int CCGetChipSize(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nXBin, const int& nYBin, const bool& bOffChipBinning, int& nW, int& nH, int& nReadOut);
	virtual int CCGetNumBins(const enumCameraIndex& Camera, const enumWhichCCD& CCD, int& nNumBins);
	virtual	int CCGetBinSizeFromIndex(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nIndex, long& nBincx, long& nBincy);

	virtual int CCSetBinnedSubFrame(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nLeft, const int& nTop, const int& nRight, const int& nBottom);

	virtual void CCMakeExposureState(int* pnState, enumCameraIndex Cam, int nXBin, int nYBin, int abg, bool bRapidReadout);//SBIG specific

	virtual int CCStartExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const double& dTime, enumPictureType Type, const int& nABGState, const bool& bLeaveShutterAlone);
	virtual int CCIsExposureComplete(const enumCameraIndex& Cam, const enumWhichCCD CCD, bool* pbComplete, unsigned int* pStatus);
	virtual int CCEndExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const bool& bWasAborted, const bool& bLeaveShutterAlone);

	virtual int CCReadoutLine(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& pixelStart, const int& pixelLength, const int& nReadoutMode, unsigned char* pMem);
	virtual int CCDumpLines(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nReadoutMode, const unsigned int& lines);

	virtual int CCReadoutImage(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nWidth, const int& nHeight, const int& nMemWidth, unsigned char* pMem);

	virtual int CCRegulateTemp(const bool& bOn, const double& dTemp);
	virtual int CCQueryTemperature(double& dCurTemp, double& dCurPower, char* lpszPower, const int nMaxLen, bool& bCurEnabled, double& dCurSetPoint);
	virtual int	CCGetRecommendedSetpoint(double& dRecSP);
	virtual int	CCSetFan(const bool& bOn);

	virtual int CCActivateRelays(const int& nXPlus, const int& nXMinus, const int& nYPlus, const int& nYMinus, const bool& bSynchronous, const bool& bAbort, const bool& bEndThread);

	virtual int CCPulseOut(unsigned int nPulse, bool bAdjust, const enumCameraIndex& Cam);

	virtual int CCSetShutter(bool bOpen);
	virtual int CCUpdateClock(void);

	virtual int CCSetImageProps(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nReadOut, void* pImage);	
	virtual int CCGetFullDynamicRange(const enumCameraIndex& Camera, const enumWhichCCD& CCD, unsigned long& dwDynRg);
	
	virtual void CCBeforeDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD);
	virtual void CCAfterDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD);
	//@} 

    virtual int PixelSize1x1InMicrons(const enumCameraIndex &Camera, const enumWhichCCD &CCD, double &x, double &y);

	//
	/*!\name ModalSettingsDialogInterface Implementation
	See ModalSettingsDialogInterface.*/
	//@{ 
	virtual int initModalSettingsDialog(void){return 0;}
	virtual int execModalSettingsDialog(void);
	//@} 
	
	//
	/*!\name X2GUIEventInterface Implementation
	See X2GUIEventInterface.*/
	//@{ 
	virtual void uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);
	//@} 

    // AddFITSKeyInterface
    virtual int     countOfIntegerFields (int &nCount);
    virtual int     valueForIntegerField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, int &nFieldValue);
    virtual int     countOfDoubleFields (int &nCount);
    virtual int     valueForDoubleField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, double &dFieldValue);
    virtual int     countOfStringFields (int &nCount);
    virtual int     valueForStringField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, BasicStringInterface &sFieldValue);
    
    // SubframeInterface
    virtual int     CCSetBinnedSubFrame3(const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int &nLeft, const int &nTop,  const int &nWidth,  const int &nHeight);

    // CameraDependentSettingInterface
    virtual int     CCGetExtendedSettingName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, BasicStringInterface &sSettingName);
    virtual int     CCGetExtendedValueCount (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, int &nCount);
    virtual int     CCGetExtendedValueName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int nIndex, BasicStringInterface &sName);
    virtual int     CCStartExposureAdditionalArgInterface (const enumCameraIndex &Cam, const enumWhichCCD CCD, const double &dTime, enumPictureType Type, const int &nABGState, const bool &bLeaveShutterAlone, const int &nIndex);

    // NoShutterInterface
    virtual int     CCHasShutter (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, bool &bHasShutter);

    
	//Implemenation below here
private:

	SerXInterface 									*	GetSerX() {return m_pSerX; }		
	TheSkyXFacadeForDriversInterface				*	GetTheSkyXFacadeForDrivers() {return m_pTheSkyXForMounts;}
	SleeperInterface								*	GetSleeper() {return m_pSleeper; }
	BasicIniUtilInterface							*	GetBasicIniUtil() {return m_pIniUtil; }
	LoggerInterface									*	GetLogger() {return m_pLogger; }
	MutexInterface									*	GetMutex() const  {return m_pIOMutex;}
	TickCountInterface								*	GetTickCountInterface() {return m_pTickCount;}
    
	SerXInterface									*	m_pSerX;		
	TheSkyXFacadeForDriversInterface				*	m_pTheSkyXForMounts;
	SleeperInterface								*	m_pSleeper;
	BasicIniUtilInterface							*	m_pIniUtil;
	LoggerInterface									*	m_pLogger;
	MutexInterface									*	m_pIOMutex;
	TickCountInterface								*	m_pTickCount;

    double  mPixelSizeX = 0;
    double  mPixelSizeY = 0;

	int     m_nPrivateISIndex;

	double  m_dCurTemp = -100;
    double  m_dCurSetPoint = 0;
	double  m_dCurPower = 0;

    int     m_nCurrentDialog = SELECT;

    CPlayerOne      m_Camera;
    enumCameraIndex m_CameraIdx;
    std::string     m_sCameraSerial = "";
    std::vector<camera_info_t>           m_tCameraIdList;

    int loadCameraSettings(std::string sSerial);
    int doPlayerOneCAmFeatureConfig();
    void doSelectCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);
    void doSettingsCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);

	int	pluginErrorToTsxError(int nErr);
};



