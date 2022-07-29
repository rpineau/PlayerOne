// x2camera.cpp  
//
#include "x2camera.h"



X2Camera::X2Camera( const char* pszSelection, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*				pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{
    int nValue = 0;
    bool bIsAuto;
    bool bUserConf;
    
	m_nPrivateISIndex				= nISIndex;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_dCurTemp = -100.0;
	m_dCurPower = 0;

    mPixelSizeX = 0.0;
    mPixelSizeY = 0.0;

    // Read in settings, default values were chosen based on test with a SV305
    if (m_pIniUtil) {
        bUserConf = (m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_USER_CONF, 0) == 0?false:true);
        m_Camera.setUserConf(bUserConf);
        if(bUserConf) {
            m_pIniUtil->readString(KEY_X2CAM_ROOT, KEY_GUID, "0", m_szCameraSerial, 128);
            m_Camera.getCameraIdFromSerial(m_nCameraID, std::string(m_szCameraSerial));
            m_Camera.setCameraSerial(std::string(m_szCameraSerial));
            m_Camera.setCameraId(m_nCameraID);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_GAIN, VAL_NOT_AVAILABLE);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setGain((long)nValue);
            else {
                m_Camera.setUserConf(false); // better not set any bad value and read defaults from camera
                return;
            }
            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_OFFSET, VAL_NOT_AVAILABLE);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setOffset((long)nValue);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_R, VAL_NOT_AVAILABLE);
            bIsAuto =  (m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_R_AUTO, 0) == 0?false:true);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setWB_R((long)nValue, bIsAuto);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_G, VAL_NOT_AVAILABLE);
            bIsAuto =  (m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_G_AUTO, 0) == 0?false:true);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setWB_G((long)nValue, bIsAuto);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_B, VAL_NOT_AVAILABLE);
            bIsAuto =  (m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_B_AUTO, 0) == 0?false:true);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setWB_B((long)nValue, bIsAuto);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_FLIP, 0);
            m_Camera.setFlip((long)nValue);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_SENSOR_MODE, VAL_NOT_AVAILABLE);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setSensorMode(nValue);

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_USB_BANDWIDTH, VAL_NOT_AVAILABLE);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setUSBBandwidth(long(nValue));

            nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, PIXEL_BIN_MODE, VAL_NOT_AVAILABLE);
            if(nValue!=VAL_NOT_AVAILABLE)
                m_Camera.setPixelBinMode((nValue==1)?true:false);
        }
        else {
            m_nCameraID = 0;
            m_Camera.setCameraId(m_nCameraID);
        }
    }

}

X2Camera::~X2Camera()
{
	//Delete objects used through composition
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;

}

#pragma mark DriverRootInterface
int	X2Camera::queryAbstraction(const char* pszName, void** ppVal)			
{
	X2MutexLocker ml(GetMutex());

	if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
		*ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
	else if (!strcmp(pszName, X2GUIEventInterface_Name))
			*ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SubframeInterface_Name))
        *ppVal = dynamic_cast<SubframeInterface*>(this);
    else if (!strcmp(pszName, PixelSizeInterface_Name))
        *ppVal = dynamic_cast<PixelSizeInterface*>(this);
    else if (!strcmp(pszName, AddFITSKeyInterface_Name))
        *ppVal = dynamic_cast<AddFITSKeyInterface*>(this);
    else if (!strcmp(pszName, CameraDependentSettingInterface_Name))
        *ppVal = dynamic_cast<CameraDependentSettingInterface*>(this);
    else if (!strcmp(pszName, NoShutterInterface_Name))
        *ppVal = dynamic_cast<NoShutterInterface*>(this);

	return SB_OK;
}

#pragma mark UI bindings
int X2Camera::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    int i;
    char tmpBuffer[128];
    int nCamIndex;
    bool bCameraFoud = false;
    
    if (NULL == ui)
        return ERR_POINTER;
    nErr = ui->loadUserInterface("PlayerOneCamSelect.ui", deviceType(), m_nPrivateISIndex);
    if (nErr)
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    //Intialize the user interface
    m_Camera.listCamera(m_tCameraIdList);
    if(!m_tCameraIdList.size()) {
        snprintf(tmpBuffer,128,"No Camera found");
        dx->comboBoxAppendString("comboBox",tmpBuffer);
        dx->setCurrentIndex("comboBox",0);
    }
    else {
        bCameraFoud = true;
        nCamIndex = 0;
        for(i=0; i< m_tCameraIdList.size(); i++) {
            //Populate the camera combo box and set the current index (selection)
            snprintf(tmpBuffer, 128, "%s [%s]",m_tCameraIdList[i].model, m_tCameraIdList[i].Sn);
            dx->comboBoxAppendString("comboBox",tmpBuffer);
            if(m_tCameraIdList[i].cameraId == m_nCameraID)
                nCamIndex = i;
        }
        dx->setCurrentIndex("comboBox",nCamIndex);
    }
    if(m_bLinked) {
        dx->setEnabled("pushButton", true);
    }
    else {
        dx->setEnabled("pushButton", false);
    }
    m_nCurrentDialog = SELECT;
    
    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK)
    {
        if(bCameraFoud) {
            int nCamera;
            std::string sCameraSerial;
            //Camera
            nCamera = dx->currentIndex("comboBox");
            m_Camera.setCameraId(m_tCameraIdList[nCamera].cameraId);
            m_nCameraID = m_tCameraIdList[nCamera].cameraId;
            m_Camera.getCameraSerialFromID(m_nCameraID, sCameraSerial);
            m_Camera.setCameraSerial(sCameraSerial);
            // store camera ID
            m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_GUID, sCameraSerial.c_str());
        }
    }

    return nErr;
}

int X2Camera::doPlayerOneCAmFeatureConfig()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;
    long nVal, nMin, nMax;
    int nCtrlVal;
    bool bIsAuto;
    bool bPressedOK = false;
    std::stringstream ssTmp;
    std::vector<std::string> sModes;
    int nCurrentSensorMode;
    bool bBinPixelSumMode;
    int i = 0;
    int nOffsetHighestDR;
    int nOffsetUnityGain;
    int nGainLowestRN;
    int nOffsetLowestRN;
    int nHCGain;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("PlayerOneCamera.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;


    if(m_bLinked){
        nErr = m_Camera.getGain(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("Gain", false);
        else {
            dx->setPropertyInt("Gain", "minimum", (int)nMin);
            dx->setPropertyInt("Gain", "maximum", (int)nMax);
            dx->setPropertyInt("Gain", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("gainRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getOffset(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("Offset", false);
        else {
            dx->setPropertyInt("Offset", "minimum", (int)nMin);
            dx->setPropertyInt("Offset", "maximum", (int)nMax);
            dx->setPropertyInt("Offset", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("offsetRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getWB_R(nMin, nMax, nVal, bIsAuto);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("WB_R", false);
        else {
            dx->setPropertyInt("WB_R", "minimum", (int)nMin);
            dx->setPropertyInt("WB_R", "maximum", (int)nMax);
            dx->setPropertyInt("WB_R", "value", (int)nVal);
            if(bIsAuto){
                dx->setEnabled("WB_R", false);
                dx->setChecked("checkBox_2", 1);
            }
            ssTmp << nMin << " to " << nMax;
            dx->setText("RwbRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getWB_G(nMin, nMax, nVal, bIsAuto);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("WB_G", false);
        else {
            dx->setPropertyInt("WB_G", "minimum", (int)nMin);
            dx->setPropertyInt("WB_G", "maximum", (int)nMax);
            dx->setPropertyInt("WB_G", "value", (int)nVal);
            if(bIsAuto){
                dx->setEnabled("WB_G", false);
                dx->setChecked("checkBox_3", 1);
            }
            ssTmp << nMin << " to " << nMax;
            dx->setText("GwbRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getWB_B(nMin, nMax, nVal, bIsAuto);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("WB_B", false);
        else {
            dx->setPropertyInt("WB_B", "minimum", (int)nMin);
            dx->setPropertyInt("WB_B", "maximum", (int)nMax);
            dx->setPropertyInt("WB_B", "value", (int)nVal);
            if(bIsAuto) {
                dx->setEnabled("WB_B", false);
                dx->setChecked("checkBox_4", 1);
            }
            ssTmp << nMin << " to " << nMax;
            dx->setText("BwbRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getFlip(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("Flip", false);
        else {
            dx->setCurrentIndex("Flip", (int)nVal);
        }

        dx->invokeMethod("SensorMode","clear");
        nErr = m_Camera.getSensorModeList(sModes, nCurrentSensorMode);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("SensorMode", false);
        else {
            if(nCurrentSensorMode) {
                for (i=0; i< sModes.size(); i++){
                    dx->comboBoxAppendString("SensorMode", sModes.at(i).c_str());
                }
                dx->setCurrentIndex("SensorMode",nCurrentSensorMode);
            }
        }

        nErr = m_Camera.getUSBBandwidth(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("USBBandwidth", false);
        else {
            dx->setPropertyInt("USBBandwidth", "minimum", (int)nMin);
            dx->setPropertyInt("USBBandwidth", "maximum", (int)nMax);
            dx->setPropertyInt("USBBandwidth", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("UsbBandwidthRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getPixelBinMode(bBinPixelSumMode);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("PixelBinMode", false);
        else {
            dx->setCurrentIndex("PixelBinMode", bBinPixelSumMode?0:1);
        }
        m_Camera.getUserfulValues(nOffsetHighestDR, nOffsetUnityGain, nGainLowestRN, nOffsetLowestRN, nHCGain);
        ssTmp<< "Gain at HCG Mode(High Conversion Gain) : " << nHCGain;
        dx->setText("HCG_value", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Gain at lowest read noise : " << nGainLowestRN;
        dx->setText("gainLowestReadNoise", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Offset at highest dynamic range : " << nOffsetHighestDR;
        dx->setText("offsetHDR", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Offset at unity gain : " << nOffsetUnityGain;
        dx->setText("offsetUnity", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Offset at lowest read noise  : " << nOffsetLowestRN;
        dx->setText("offsetLowestReadNoise", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);
    }
    else {
        dx->setEnabled("Gain", false);
        dx->setText("gainRange","");

        dx->setEnabled("Offset", false);
        dx->setText("offsetRange","");

        dx->setEnabled("WB_R", false);
        dx->setEnabled("checkBox_2", false);
        dx->setText("RwbRange","");


        dx->setEnabled("WB_G", false);
        dx->setEnabled("checkBox_3", false);
        dx->setText("GwbRange","");

        dx->setEnabled("WB_B", false);
        dx->setEnabled("checkBox_4", false);
        dx->setText("BwbRange","");

        dx->setEnabled("Flip", false);
        dx->setEnabled("SpeedMode", false);

        dx->setEnabled("USBBandwidth", false);
        dx->setText("UsbBandwidthRange","");

    }

    m_nCurrentDialog = SETTINGS;
    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {

        m_Camera.setUserConf(true);
        m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_USER_CONF, 1);

        dx->propertyInt("Gain", "value", nCtrlVal);
        nErr = m_Camera.setGain((long)nCtrlVal);
        if(!nErr) {
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_GAIN, nCtrlVal);
            m_Camera.rebuildGainList();
        }

        dx->propertyInt("Offset", "value", nCtrlVal);
        nErr = m_Camera.setOffset((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_OFFSET, nCtrlVal);

        if(dx->isEnabled("WB_R")) {
            dx->propertyInt("WB_R", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_2");
            nErr = m_Camera.setWB_R((long)nCtrlVal, bIsAuto);
            if(!nErr) {
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_R, nCtrlVal);
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_R_AUTO, bIsAuto?1:0);
            }
        }

        if(dx->isEnabled("WB_G")) {
            dx->propertyInt("WB_G", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_3");
            nErr = m_Camera.setWB_G((long)nCtrlVal, bIsAuto);
            if(!nErr){
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_G, nCtrlVal);
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_G_AUTO, bIsAuto?1:0);
            }
        }

        if(dx->isEnabled("WB_B")) {
            dx->propertyInt("WB_B", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_4");
            nErr = m_Camera.setWB_B((long)nCtrlVal, bIsAuto);
            if(!nErr) {
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_B, nCtrlVal);
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_WHITE_BALANCE_B_AUTO, bIsAuto?1:0);
            }
        }

        nCtrlVal = dx->currentIndex("Flip");
        nErr = m_Camera.setFlip((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_FLIP, nCtrlVal);

        if(dx->isEnabled("SensorMode")) {
            nCtrlVal = dx->currentIndex("SensorMode");
            nErr = m_Camera.setSensorMode(nCtrlVal);
            if(!nErr)
                m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_SENSOR_MODE, nCtrlVal);
        }

        dx->propertyInt("USBBandwidth", "value", nCtrlVal);
        nErr = m_Camera.setUSBBandwidth((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_USB_BANDWIDTH, nCtrlVal);

        nCtrlVal = dx->currentIndex("PixelBinMode");
        nErr = m_Camera.setPixelBinMode((nCtrlVal==0)); // true = Sum mode, False = Average mode
        if(!nErr)
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_SENSOR_MODE, nCtrlVal);
    }

    return nErr;
}


void X2Camera::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    switch(m_nCurrentDialog) {
        case SELECT:
            doSelectCamEvent(uiex, pszEvent);
            break;
        case SETTINGS:
            doSettingsCamEvent(uiex, pszEvent);
            break;
        default :
            break;
    }
}

void X2Camera::doSelectCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    if (!strcmp(pszEvent, "on_pushButton_clicked"))
    {
        int nErr=SB_OK;
        
        nErr = doPlayerOneCAmFeatureConfig();
        m_nCurrentDialog = SELECT;
    }
}

void X2Camera::doSettingsCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool bEnable;
    
    if (!strcmp(pszEvent, "on_checkBox_stateChanged")) {
        bEnable = uiex->isChecked("chackBox");
        uiex->setEnabled("Gain", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_2_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_2");
        uiex->setEnabled("WB_R", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_3_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_3");
        uiex->setEnabled("WB_G", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_4_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_4");
        uiex->setEnabled("WB_B", !bEnable);
    }

}

#pragma mark DriverInfoInterface
void X2Camera::driverInfoDetailedInfo(BasicStringInterface& str) const		
{
	X2MutexLocker ml(GetMutex());

    str = "PlayerOne camera X2 plugin by Rodolphe Pineau";
}

double X2Camera::driverInfoVersion(void) const								
{
	X2MutexLocker ml(GetMutex());

    return PLUGIN_VERSION;
}

#pragma mark HardwareInfoInterface
void X2Camera::deviceInfoNameShort(BasicStringInterface& str) const										
{
    X2Camera* pMe = (X2Camera*)this;
    X2MutexLocker ml(pMe->GetMutex());
    
    if(m_bLinked) {
        char cDevName[BUFFER_LEN];
        std::string sCameraSerial;
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        snprintf(cDevName, BUFFER_LEN, "%s", sCameraName.c_str());
        str = cDevName;
    }
    else {
        str = "";
    }
}

void X2Camera::deviceInfoNameLong(BasicStringInterface& str) const										
{
    X2Camera* pMe = (X2Camera*)this;
    X2MutexLocker ml(pMe->GetMutex());

    if(m_bLinked) {
        char cDevName[BUFFER_LEN];
        std::string sCameraSerial;
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        pMe->m_Camera.getCameraSerial(sCameraSerial);
        snprintf(cDevName, BUFFER_LEN, "%s (%s)", sCameraName.c_str(), sCameraSerial.c_str() );
        str = cDevName;
    }
    else {
        str = "";
    }
}

void X2Camera::deviceInfoDetailedDescription(BasicStringInterface& str) const								
{
	X2MutexLocker ml(GetMutex());

	str = "PlayerOne camera X2 plugin by Rodolphe Pineau";
}

void X2Camera::deviceInfoFirmwareVersion(BasicStringInterface& str)										
{
    X2MutexLocker ml(GetMutex());
    std::string sVersion;
    m_Camera.getFirmwareVersion(sVersion);
    str = sVersion.c_str();
}

void X2Camera::deviceInfoModel(BasicStringInterface& str)													
{
	X2MutexLocker ml(GetMutex());

    if(m_bLinked) {
        char cDevName[BUFFER_LEN];
        std::string sCameraSerial;
        std::string sCameraName;
        m_Camera.getCameraName(sCameraName);
        snprintf(cDevName, BUFFER_LEN, "%s", sCameraName.c_str());
        str = cDevName;
    }
    else {
        str = "";
    }
}

#pragma mark Device Access
int X2Camera::CCEstablishLink(const enumLPTPort portLPT, const enumWhichCCD& CCD, enumCameraIndex DesiredCamera, enumCameraIndex& CameraFound, const int nDesiredCFW, int& nFoundCFW)
{
    int nErr = SB_OK;

    m_bLinked = false;

    m_dCurTemp = -100.0;
    nErr = m_Camera.Connect(m_nCameraID);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    if(!m_nCameraID) {
        m_Camera.getCameraId(m_nCameraID);
        std::string sCameraSerial;
        m_Camera.getCameraSerialFromID(m_nCameraID, sCameraSerial);
        // store camera ID
        m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_GUID, sCameraSerial.c_str());
    }
    
    return nErr;
}


int X2Camera::CCQueryTemperature(double& dCurTemp, double& dCurPower, char* lpszPower, const int nMaxLen, bool& bCurEnabled, double& dCurSetPoint)
{   
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.getTemperture(m_dCurTemp, m_dCurPower, m_dCurSetPoint, bCurEnabled);
    dCurTemp = m_dCurTemp;
	dCurPower = m_dCurPower;
    dCurSetPoint = m_dCurSetPoint;

    return nErr;
}

int X2Camera::CCRegulateTemp(const bool& bOn, const double& dTemp)
{
    int nErr = SB_OK;
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.setCoolerTemperature(bOn, dTemp);
    
	return nErr;
}

int X2Camera::CCGetRecommendedSetpoint(double& RecTemp)
{
	X2MutexLocker ml(GetMutex());

	RecTemp = 100;//Set to 100 if you cannot recommend a setpoint
	return SB_OK;
}  


int X2Camera::CCStartExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const double& dTime, enumPictureType Type, const int& nABGState, const bool& bLeaveShutterAlone)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	bool bLight = true;
    int nErr = SB_OK;

	switch (Type)
	{
		case PT_FLAT:
		case PT_LIGHT:			bLight = true;	break;
		case PT_DARK:	
		case PT_AUTODARK:	
		case PT_BIAS:			bLight = false;	break;
		default:				return ERR_CMDFAILED;
	}

    nErr = m_Camera.startCaputure(dTime);
	return nErr;
}   



int X2Camera::CCIsExposureComplete(const enumCameraIndex& Cam, const enumWhichCCD CCD, bool* pbComplete, unsigned int* pStatus)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    *pbComplete = false;

    if(m_Camera.isFameAvailable())
        *pbComplete = true;

    return SB_OK;
}

int X2Camera::CCEndExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const bool& bWasAborted, const bool& bLeaveShutterAlone)           
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	int nErr = SB_OK;

	if (bWasAborted) {
        m_Camera.abortCapture();
	}

    return nErr;
}

int X2Camera::CCGetChipSize(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nXBin, const int& nYBin, const bool& bOffChipBinning, int& nW, int& nH, int& nReadOut)
{
	X2MutexLocker ml(GetMutex());

	nW = m_Camera.getWidth()/nXBin;
    nH = m_Camera.getHeight()/nYBin;
    nReadOut = CameraDriverInterface::rm_Image;

    m_Camera.setBinSize(nXBin);
    return SB_OK;
}

int X2Camera::CCGetNumBins(const enumCameraIndex& Camera, const enumWhichCCD& CCD, int& nNumBins)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		nNumBins = 1;
	else
        nNumBins = m_Camera.getNumBins();

    return SB_OK;
}

int X2Camera::CCGetBinSizeFromIndex(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nIndex, long& nBincx, long& nBincy)
{
	X2MutexLocker ml(GetMutex());

    nBincx = m_Camera.getBinFromIndex(nIndex);
    nBincy = m_Camera.getBinFromIndex(nIndex);

	return SB_OK;
}

int X2Camera::CCUpdateClock(void)
{   
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int X2Camera::CCSetShutter(bool bOpen)           
{   
	X2MutexLocker ml(GetMutex());
    // PlayerOne camera don't have physical shutter.

	return SB_OK;;
}

int X2Camera::CCActivateRelays(const int& nXPlus, const int& nXMinus, const int& nYPlus, const int& nYMinus, const bool& bSynchronous, const bool& bAbort, const bool& bEndThread)
{   
	X2MutexLocker ml(GetMutex());
    
    m_Camera.RelayActivate(nXPlus, nXMinus, nYPlus, nYMinus, bSynchronous, bAbort);
	return SB_OK;
}

int X2Camera::CCPulseOut(unsigned int nPulse, bool bAdjust, const enumCameraIndex& Cam)
{   
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}

void X2Camera::CCBeforeDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
}


void X2Camera::CCAfterDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
	return;
}

int X2Camera::CCReadoutLine(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& pixelStart, const int& pixelLength, const int& nReadoutMode, unsigned char* pMem)
{   
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}           

int X2Camera::CCDumpLines(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nReadoutMode, const unsigned int& lines)
{                                     
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}           


int X2Camera::CCReadoutImage(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nWidth, const int& nHeight, const int& nMemWidth, unsigned char* pMem)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());
    
    if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.getFrame(nHeight, nMemWidth, pMem);

    return nErr;
}

int X2Camera::CCDisconnect(const bool bShutDownTemp)
{
	X2MutexLocker ml(GetMutex());

	if (m_bLinked)
	{
        m_Camera.Disconnect();
		setLinked(false);
	}

	return SB_OK;
}

int X2Camera::CCSetImageProps(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nReadOut, void* pImage)
{
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int X2Camera::CCGetFullDynamicRange(const enumCameraIndex& Camera, const enumWhichCCD& CCD, unsigned long& dwDynRg)
{
	X2MutexLocker ml(GetMutex());

    uint32_t nBitDepth;

    nBitDepth = m_Camera.getBitDepth();
    dwDynRg = (unsigned long)(1 << nBitDepth);

	return SB_OK;
}

void X2Camera::CCMakeExposureState(int* pnState, enumCameraIndex Cam, int nXBin, int nYBin, int abg, bool bRapidReadout)
{
	X2MutexLocker ml(GetMutex());

	return;
}

int X2Camera::CCSetBinnedSubFrame(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nLeft, const int& nTop, const int& nRight, const int& nBottom)
{
    int nErr = SB_OK;
    
	X2MutexLocker ml(GetMutex());
#ifdef PLUGIN_DEBUG
    std::stringstream ssTmp;
    ssTmp << "[CCSetBinnedSubFrame] nLeft = " << nLeft << ", nTop = " << nTop << ", nRight = " << nRight << ", nBottom = " << nBottom;
    m_Camera.log(ssTmp.str());
#endif
    nErr = m_Camera.setROI(nLeft, nTop, (nRight-nLeft)+1, (nBottom-nTop)+1);
    return nErr;
}

int X2Camera::CCSetBinnedSubFrame3(const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int &nLeft, const int &nTop,  const int &nWidth,  const int &nHeight)
{
    int nErr = SB_OK;
    
    X2MutexLocker ml(GetMutex());

#ifdef PLUGIN_DEBUG
    std::stringstream ssTmp;
    ssTmp << "[CCSetBinnedSubFrame3] nLeft = " << nLeft << ", nTop = " << nTop << ", nWidth = " << nWidth << ", nHeight = " << nHeight;
    m_Camera.log(ssTmp.str());
#endif

    nErr = m_Camera.setROI(nLeft, nTop, nWidth, nHeight);
    
    return nErr;
}


int X2Camera::CCSettings(const enumCameraIndex& Camera, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());

	return ERR_NOT_IMPL;
}

int X2Camera::CCSetFan(const bool& bOn)
{
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int	X2Camera::pathTo_rm_FitsOnDisk(char* lpszPath, const int& nPathSize)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	//Just give a file path to a FITS and TheSkyX will load it
		
	return SB_OK;
}

CameraDriverInterface::ReadOutMode X2Camera::readoutMode(void)		
{
	X2MutexLocker ml(GetMutex());

	return CameraDriverInterface::rm_Image;
}


enumCameraIndex	X2Camera::cameraId()
{
	X2MutexLocker ml(GetMutex());
	return m_CameraIdx;
}

void X2Camera::setCameraId(enumCameraIndex Cam)	
{
    m_CameraIdx = Cam;
}

int X2Camera::PixelSize1x1InMicrons(const enumCameraIndex &Camera, const enumWhichCCD &CCD, double &x, double &y)
{
    int nErr = SB_OK;

    if(!m_bLinked) {
        x = 0.0;
        y = 0.0;
        return ERR_COMMNOLINK;
    }
    X2MutexLocker ml(GetMutex());
    x = m_Camera.getPixelSize();
    y = x;
    return nErr;
}

int X2Camera::countOfIntegerFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 6;
    return nErr;
}

int X2Camera::valueForIntegerField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, int &nFieldValue)
{
    int nErr = SB_OK;
    long nVal = 0;
    long nMin = 0;
    long nMax = 0;
    bool bIsAuto = false;
    
    X2MutexLocker ml(GetMutex());

    switch(nIndex) {
        case F_GAIN :
            sFieldName = "GAIN";
            nErr = m_Camera.getGain(nMin, nMax, nVal);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                nFieldValue = 0;
            }
            else {
                sFieldComment = "";
                nFieldValue = (int)nVal;
            }
            break;
            
        case F_BLACK_OFFSET :
            sFieldName = "BLACK-OFFSET";
            nErr = m_Camera.getOffset(nMin, nMax, nVal);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                nFieldValue = 0;
            }
            else {
                sFieldComment = "";
                nFieldValue = (int)nVal;
            }
            break;

        case F_WB_R :
            sFieldName = "R-WB";
            nErr = m_Camera.getWB_R(nMin, nMax, nVal, bIsAuto);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                nFieldValue = 0;
            }
            else {
                sFieldComment = "";
                nFieldValue = (int)nVal;
            }
            break;
            
        case F_WB_G :
            sFieldName = "G-WB";
            nErr = m_Camera.getWB_G(nMin, nMax, nVal, bIsAuto);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                nFieldValue = 0;
            }
            else {
                sFieldComment = "";
                nFieldValue = (int)nVal;
            }
            break;
            
        case F_WB_B :
            sFieldName = "B-WB";
            nErr = m_Camera.getWB_B(nMin, nMax, nVal, bIsAuto);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                nFieldValue = 0;
            }
            else {
                sFieldComment = "";
                nFieldValue = (int)nVal;
            }
            break;

        default :
            break;

    }
    return nErr;
}

int X2Camera::countOfDoubleFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 0;
    return nErr;
}

int X2Camera::valueForDoubleField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, double &dFieldValue)
{
    int nErr = SB_OK;
    sFieldName = "";
    sFieldComment = "";
    dFieldValue = 0;
    return nErr;
}

int X2Camera::countOfStringFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 4;
    return nErr;
}

int X2Camera::valueForStringField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, BasicStringInterface &sFieldValue)
{
    int nErr = SB_OK;
    std::string sTmp;
    int nModeIndex;

    X2MutexLocker ml(GetMutex());
    
    switch(nIndex) {
        case F_BAYER :
            if(m_Camera.isCameraColor()) {
                m_Camera.getBayerPattern(sTmp);
                sFieldName = "DEBAYER";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = sTmp.c_str();
            }
            else {
                sFieldName = "DEBAYER";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = "MONO";
            }
            break;

        case F_BAYERPAT: // PixInsight
            if(m_Camera.isCameraColor()) {
                m_Camera.getBayerPattern(sTmp);
                sFieldName = "BAYERPAT";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = sTmp.c_str();
            }
            else {
                sFieldName = "BAYERPAT";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = "MONO";
            }
            break;

        case F_FLIP :
            m_Camera.getFlip(sTmp);
            sFieldName = "FLIP";
            sFieldComment = "";
            sFieldValue = sTmp.c_str();
            break;

        case F_SENSOR_MODE :
            sFieldName = "SENSOR_MODE";
            nErr = m_Camera.getCurentSensorMode(sTmp, nModeIndex);
            if(nErr == VAL_NOT_AVAILABLE) {
                sFieldComment = "not available";
                sFieldValue = "";
            }
            else {
                sFieldComment = "";
                sFieldValue = sTmp.c_str();
            }
            break;

        default :
            break;
    }

    return nErr;
}

int X2Camera::CCGetExtendedSettingName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, BasicStringInterface &sSettingName)
{
    int nErr = SB_OK;

    sSettingName="Gain";

    return nErr;
}

int X2Camera::CCGetExtendedValueCount (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, int &nCount)
{
    int nErr = SB_OK;

    nCount = m_Camera.getNbGainInList();
    return nErr;
}

int X2Camera::CCGetExtendedValueName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int nIndex, BasicStringInterface &sName)
{
    int nErr = SB_OK;

    sName = m_Camera.getGainFromListAtIndex(nIndex).c_str();
    return nErr;
}

int X2Camera::CCStartExposureAdditionalArgInterface (const enumCameraIndex &Cam, const enumWhichCCD CCD, const double &dTime, enumPictureType Type, const int &nABGState, const bool &bLeaveShutterAlone, const int &nIndex)
{
    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    bool bLight = true;
    int nErr = SB_OK;

    nErr = m_Camera.setGain(std::stol(m_Camera.getGainFromListAtIndex(nIndex)));
    if(nErr) {
        return nErr; // can't set gain !
    }
    switch (Type)
    {
        case PT_FLAT:
        case PT_LIGHT:            bLight = true;    break;
        case PT_DARK:
        case PT_AUTODARK:
        case PT_BIAS:            bLight = false;    break;
        default:                return ERR_CMDFAILED;
    }

    nErr = m_Camera.startCaputure(dTime);
    return nErr;

}

int X2Camera::CCHasShutter (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, bool &bHasShutter)
{
    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    int nErr = SB_OK;

    bHasShutter = false;

    return nErr;
}
