// x2camera.cpp  
//  Created by Rodolphe Pineau on 2022/07/06
//  Copyright © 2022 Rodolphe Pineau. All rights reserved.
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
    int  nErr = PLUGIN_OK;
    char szCameraSerial[128];
	int nCameraID;

	m_nPrivateISIndex				= nISIndex;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

    // Read in settings
    if (m_pIniUtil) {
        m_pIniUtil->readString(KEY_X2CAM_ROOT, KEY_GUID, "0", szCameraSerial, 128);
        m_sCameraSerial.assign(szCameraSerial);
        nErr = m_Camera.getCameraIdFromSerial(nCameraID, m_sCameraSerial);
        if(nErr) { // we don't know that camera, we'll use the default from the first camera
			nCameraID = 0;
			m_Camera.getCameraSerialFromID(nCameraID, m_sCameraSerial);
			m_Camera.setUserConf(false);
            return;
        }
        nErr = loadCameraSettings(m_sCameraSerial);
    }
}

X2Camera::~X2Camera()
{
	// if disconnect was not called ...
	m_Camera.Disconnect();
	setLinked(false);


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
    bool bPressedOK = false;
    int i;
    std::stringstream ssTmp;
    int nCamIndex;
    bool bCameraFound = false;


    if(m_bLinked) {
        nErr = doPlayerOneCAmFeatureConfig();
        return nErr;
    }

    X2GUIExchangeInterface* dx = NULL;//Comes after ui is loaded
    X2ModalUIUtil           uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*         ui = uiutil.X2UI();

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
        dx->comboBoxAppendString("comboBox","No Camera found");
        dx->setCurrentIndex("comboBox",0);
    }
    else {
        bCameraFound = true;
        nCamIndex = 0;
        for(i = 0; i < m_tCameraIdList.size(); i++) {
            //Populate the camera combo box and set the current index (selection)
            ssTmp << m_tCameraIdList[i].model << " [" << m_tCameraIdList[i].Sn << "]";
            dx->comboBoxAppendString("comboBox",ssTmp.str().c_str());
            if(m_tCameraIdList[i].Sn == m_sCameraSerial)
                nCamIndex = i;
            std::stringstream().swap(ssTmp);
        }
        dx->setCurrentIndex("comboBox",nCamIndex);
    }

    m_nCurrentDialog = SELECT;
    
    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK)
    {
        if(bCameraFound) {
            int nCamera;
            std::string sCameraSerial;
            //Camera
            nCamera = dx->currentIndex("comboBox");
			m_sCameraSerial.assign(m_tCameraIdList[nCamera].Sn);
            // store camera Serial
            m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_GUID, m_sCameraSerial.c_str());
            loadCameraSettings(m_sCameraSerial);
        }
    }

    return nErr;
}

int X2Camera::doPlayerOneCAmFeatureConfig()
{
    int nErr = SB_OK;
    long nVal, nMin, nMax;
    int nCtrlVal=0;
    bool bIsAuto = false;
    bool bPressedOK = false;
    std::stringstream ssTmp;
    std::vector<std::string> svModes;
    int nCurrentSensorMode;
	bool bHardwareBinPresent = false;
	bool bHardwareBinEnable = false;
    bool bBinPixelSumMode = false;
    bool bPixelBinMono = false;
    int i = 0;

    int nGainHighestDR;
    int nHCGain;
    int nUnityGain;
    int nGainLowestRN;

    int nOffsetHighestDR;
    int nOffsetHCGain;
    int nOffsetUnityGain;
    int nOffsetLowestRN;

    bool bIsUSB3;

    X2GUIExchangeInterface* dx = NULL;
    X2ModalUIUtil           uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*         ui = uiutil.X2UI();

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("PlayerOneCamera.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    m_Camera.getCameraSerial(m_sCameraSerial);

    if(m_bLinked){
        nErr = m_Camera.getGain(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("Gain", false);
            dx->setText("gainRange", "");
        }
        else {
            dx->setPropertyInt("Gain", "minimum", (int)nMin);
            dx->setPropertyInt("Gain", "maximum", (int)nMax);
            dx->setPropertyInt("Gain", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("gainRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getOffset(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("Offset", false);
            dx->setText("offsetRange", "");
        }
        else {
            dx->setPropertyInt("Offset", "minimum", (int)nMin);
            dx->setPropertyInt("Offset", "maximum", (int)nMax);
            dx->setPropertyInt("Offset", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("offsetRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

        nErr = m_Camera.getWB_R(nMin, nMax, nVal, bIsAuto);
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("WB_R", false);
            dx->setText("RwbRange", "");
            dx->setEnabled("checkBox_2", false);
        }
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
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("WB_G", false);
            dx->setText("GwbRange", "");
            dx->setEnabled("checkBox_3", false);
        }
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
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("WB_B", false);
            dx->setText("BwbRange", "");
            dx->setEnabled("checkBox_4", false);
        }
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
        nErr = m_Camera.getSensorModeList(svModes, nCurrentSensorMode);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("SensorMode", false);
        else {
            if(svModes.size()) {
                for(i = 0; i < svModes.size(); i++){
                    dx->comboBoxAppendString("SensorMode", svModes.at(i).c_str());
                }
                dx->setCurrentIndex("SensorMode",nCurrentSensorMode);
            }
            else    // no sensor mode selactable
                dx->setEnabled("SensorMode", false);
        }

        nErr = m_Camera.getUSBBandwidth(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE) {
            dx->setEnabled("USBBandwidth", false);
            dx->setText("UsbBandwidthRange", "");
        }
        else {
            dx->setPropertyInt("USBBandwidth", "minimum", (int)nMin);
            dx->setPropertyInt("USBBandwidth", "maximum", (int)nMax);
            dx->setPropertyInt("USBBandwidth", "value", (int)nVal);
            ssTmp << nMin << " to " << nMax;
            dx->setText("UsbBandwidthRange", ssTmp.str().c_str());
            dx->setText("UsbBandwidthRange", ssTmp.str().c_str());
            std::stringstream().swap(ssTmp);
        }

		bHardwareBinPresent = m_Camera.isHardwareBinAvailable();
		m_Camera.getHardwareBinOn(bHardwareBinEnable);

		if(nErr == VAL_NOT_AVAILABLE)
			dx->setEnabled("checkBox_6", false);
		else {
			dx->setEnabled("checkBox_6", bHardwareBinPresent?true:false);
			if(bHardwareBinPresent) {
				// is hardware bin enabled ?
				m_Camera.getHardwareBinOn(bHardwareBinEnable);
				dx->setChecked("checkBox_6", bHardwareBinEnable?1:0);
			}
			else
				dx->setEnabled("checkBox_6", false);
		}


        nErr = m_Camera.getPixelBinMode(bBinPixelSumMode);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("PixelBinMode", false);
        else {
            dx->setCurrentIndex("PixelBinMode", bBinPixelSumMode?0:1);
        }
		// pixel bin mode not available with hardware bin
		if(bHardwareBinPresent && bHardwareBinEnable)
			dx->setEnabled("PixelBinMode", false);

		// camera without hardware bin
        if(!bHardwareBinPresent) {
            nErr = m_Camera.getMonoBin(bPixelBinMono);
            if(nErr == VAL_NOT_AVAILABLE)
                dx->setEnabled("checkBox_5", false);
            else {
                dx->setChecked("checkBox_5", bPixelBinMono?1:0);
            }
        }
		// mono camera with hardware bin
		else if(!m_Camera.isCameraColor()) {
				dx->setEnabled("checkBox_5", false); // no need for mono bin on mono camera
		}
		else { // color camera with hardware bin
			if(!bHardwareBinEnable) {
				nErr = m_Camera.getMonoBin(bPixelBinMono);
				if(nErr == VAL_NOT_AVAILABLE)
					dx->setEnabled("checkBox_5", false);
				else {
					dx->setChecked("checkBox_5", bPixelBinMono?1:0);
				}
			}
			else
				dx->setEnabled("checkBox_5", false); // no mono bin with hardware bin
		}

        nErr = m_Camera.getLensHeaterPowerPerc(nMin, nMax, nVal);
        if(nErr == VAL_NOT_AVAILABLE)
            dx->setEnabled("LensHeaterPower", false);
        else {
            dx->setPropertyInt("LensHeaterPower", "minimum", (int)nMin);
            dx->setPropertyInt("LensHeaterPower", "maximum", (int)nMax);
            dx->setPropertyInt("LensHeaterPower", "value", (int)nVal);
        }


        m_Camera.isUSB3(bIsUSB3);
        dx->setText("USBMode", bIsUSB3?"<html><head/><body><p><span style=\" color:#00FF00;\">USB 3.0</span></p></body></html>" : "<html><head/><body><p><span style=\" color:#FF0000;\">USB 2.0</span></p></body></html>");

        m_Camera.getAllUsefulValues(nGainHighestDR, nHCGain, nUnityGain, nGainLowestRN, nOffsetHighestDR, nOffsetHCGain, nOffsetUnityGain, nOffsetLowestRN);

        ssTmp<< "Gain at highest dynamic range : " << nGainHighestDR;
        dx->setText("HDR_value", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Gain at HCG Mode (High Conversion Gain) : " << nHCGain;
        dx->setText("HCG_value", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Unity Gain : " << nUnityGain;
        dx->setText("UnityGain", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Gain at lowest read noise : " << nGainLowestRN;
        dx->setText("gainLowestReadNoise", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Offset at highest dynamic range : " << nOffsetHighestDR;
        dx->setText("offsetHDR", ssTmp.str().c_str());
        std::stringstream().swap(ssTmp);

        ssTmp<< "Offset at HCG Mode : " << nOffsetHCGain;
        dx->setText("offsetHCG", ssTmp.str().c_str());
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

        dx->invokeMethod("SensorMode","clear");
        dx->setEnabled("SensorMode", false);
        
        dx->setEnabled("USBBandwidth", false);
        dx->setText("UsbBandwidthRange","");

		dx->setEnabled("checkBox_6", false);
        dx->setEnabled("LensHeaterPower",false);

        dx->setText("USBMode","");
    }

    m_nCurrentDialog = SETTINGS;
    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        dx->propertyInt("Gain", "value", nCtrlVal);
        nErr = m_Camera.setGain((long)nCtrlVal);
        if(!nErr) {
            m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_GAIN, nCtrlVal);
            m_Camera.rebuildGainList();
        }
        else {
#if defined PLUGIN_DEBUG
            m_Camera.log("Error setting Gain");
#endif
        }
        dx->propertyInt("Offset", "value", nCtrlVal);
        nErr = m_Camera.setOffset((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_OFFSET, nCtrlVal);
        else {
#if defined PLUGIN_DEBUG
            m_Camera.log("Error setting Offset");
#endif
        }
        if(dx->isEnabled("WB_R") || dx->isEnabled("checkBox_2")) {
            dx->propertyInt("WB_R", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_2");
            nErr = m_Camera.setWB_R((long)nCtrlVal, bIsAuto);
            if(!nErr) {
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_R, nCtrlVal);
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_R_AUTO, bIsAuto?1:0);
            }
            else {
#if defined PLUGIN_DEBUG
                m_Camera.log("Error setting WB_R");
#endif
            }
        }

        if(dx->isEnabled("WB_G") || dx->isEnabled("checkBox_3")) {
            dx->propertyInt("WB_G", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_3");
            nErr = m_Camera.setWB_G((long)nCtrlVal, bIsAuto);
            if(!nErr){
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_G, nCtrlVal);
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_G_AUTO, bIsAuto?1:0);
            }
            else {
#if defined PLUGIN_DEBUG
                m_Camera.log("Error setting WB_G");
#endif
            }
        }

        if(dx->isEnabled("WB_B") || dx->isEnabled("checkBox_4")) {
            dx->propertyInt("WB_B", "value", nCtrlVal);
            bIsAuto = dx->isChecked("checkBox_4");
            nErr = m_Camera.setWB_B((long)nCtrlVal, bIsAuto);
            if(!nErr) {
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_B, nCtrlVal);
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_WHITE_BALANCE_B_AUTO, bIsAuto?1:0);
            }
            else {
#if defined PLUGIN_DEBUG
                m_Camera.log("Error setting WB_B");
#endif
            }
        }

        nCtrlVal = dx->currentIndex("Flip");
        nErr = m_Camera.setFlip((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_FLIP, nCtrlVal);
        else {
#if defined PLUGIN_DEBUG
            m_Camera.log("Error setting Flip");
#endif
        }

        if(dx->isEnabled("SensorMode")) {
            nCtrlVal = dx->currentIndex("SensorMode");
            nErr = m_Camera.setSensorMode(nCtrlVal);
            if(!nErr)
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_SENSOR_MODE, nCtrlVal);
            else {
#if defined PLUGIN_DEBUG
                m_Camera.log("Error setting SensorMode");
#endif
            }
        }

        dx->propertyInt("USBBandwidth", "value", nCtrlVal);
        nErr = m_Camera.setUSBBandwidth((long)nCtrlVal);
        if(!nErr)
            m_pIniUtil->writeInt(m_sCameraSerial.c_str(), KEY_USB_BANDWIDTH, nCtrlVal);
        else {
#if defined PLUGIN_DEBUG
            m_Camera.log("Error setting USBBandwidth");
#endif
        }

        nCtrlVal = dx->currentIndex("PixelBinMode");
        nErr = m_Camera.setPixelBinMode((nCtrlVal==0)?true:false); // true = Sum mode, False = Average mode
        if(!nErr)
            m_pIniUtil->writeInt(m_sCameraSerial.c_str(), PIXEL_BIN_MODE, nCtrlVal);
        else {
#if defined PLUGIN_DEBUG
            m_Camera.log("Error setting PixelBinMode");
#endif
        }

		if(bHardwareBinPresent) {
			if(dx->isEnabled("checkBox_6")) {
				bHardwareBinEnable = dx->isChecked("checkBox_6");
				nErr = m_Camera.setHardwareBinOn(bHardwareBinEnable);
				if(!nErr)
					m_pIniUtil->writeInt(m_sCameraSerial.c_str(), PIXEL_HARD_BIN, bHardwareBinEnable?1:0);
				else {
#if defined PLUGIN_DEBUG
					m_Camera.log("Error setting MonoBin");
#endif
				}
			}
		}
        
		if(!bHardwareBinEnable) {
            if(dx->isEnabled("checkBox_5")) {
                bPixelBinMono = dx->isChecked("checkBox_5");
                nErr = m_Camera.setMonoBin(bPixelBinMono);
                if(!nErr)
                    m_pIniUtil->writeInt(m_sCameraSerial.c_str(), PIXEL_MONO_BIN, bPixelBinMono?1:0);
                else {
#if defined PLUGIN_DEBUG
                    m_Camera.log("Error setting MonoBin");
#endif
                }
            }
        }

        if(m_Camera.isLensHeaterAvailable()) {
            dx->propertyInt("LensHeaterPower", "value", nCtrlVal);
            nErr = m_Camera.setLensHeaterPowerPerc((long)nCtrlVal);
            if(!nErr)
                m_pIniUtil->writeInt(m_sCameraSerial.c_str(), LENS_POWER, nCtrlVal);
            else {
#if defined PLUGIN_DEBUG
                m_Camera.log("Error setting LensHeaterPower");
#endif
            }
        }
    }

    return nErr;
}


int X2Camera::loadCameraSettings(std::string sSerial)
{
    int nErr = SB_OK;
    int nValue = 0;
    bool bIsAuto;

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_GAIN, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setGain((long)nValue);
    else {
        m_Camera.setUserConf(false); // better not set any bad value and read defaults from camera
        return VAL_NOT_AVAILABLE;
    }
    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_OFFSET, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setOffset((long)nValue);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_R, VAL_NOT_AVAILABLE);
    bIsAuto =  (m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_R_AUTO, 0) == 0?false:true);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setWB_R((long)nValue, bIsAuto);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_G, VAL_NOT_AVAILABLE);
    bIsAuto =  (m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_G_AUTO, 0) == 0?false:true);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setWB_G((long)nValue, bIsAuto);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_B, VAL_NOT_AVAILABLE);
    bIsAuto =  (m_pIniUtil->readInt(sSerial.c_str(), KEY_WHITE_BALANCE_B_AUTO, 0) == 0?false:true);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setWB_B((long)nValue, bIsAuto);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_FLIP, 0);
    m_Camera.setFlip((long)nValue);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_SENSOR_MODE, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setSensorMode(nValue);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), KEY_USB_BANDWIDTH, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setUSBBandwidth(long(nValue));

	nValue = m_pIniUtil->readInt(sSerial.c_str(), PIXEL_HARD_BIN, VAL_NOT_AVAILABLE);
	if(nValue!=VAL_NOT_AVAILABLE)
		m_Camera.setHardwareBinOn((nValue==0)?false:true);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), PIXEL_BIN_MODE, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setPixelBinMode((nValue==0)?true:false);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), PIXEL_MONO_BIN, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setMonoBin((nValue==1)?true:false);

    nValue = m_pIniUtil->readInt(sSerial.c_str(), LENS_POWER, VAL_NOT_AVAILABLE);
    if(nValue!=VAL_NOT_AVAILABLE)
        m_Camera.setLensHeaterPowerPerc((long)nValue);

    m_Camera.setUserConf(true);
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
}

void X2Camera::doSettingsCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	int nErr = PLUGIN_OK;
    bool bEnable = false;
	bool bTmp = false;

    if (!strcmp(pszEvent, "on_checkBox_stateChanged")) {
        bEnable = uiex->isChecked("checkBox");
        uiex->setEnabled("Gain", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_2_stateChanged")) {
        bEnable = uiex->isChecked("checkBox_2");
        uiex->setEnabled("WB_R", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_3_stateChanged")) {
        bEnable = uiex->isChecked("checkBox_3");
        uiex->setEnabled("WB_G", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_4_stateChanged")) {
        bEnable = uiex->isChecked("checkBox_4");
        uiex->setEnabled("WB_B", !bEnable);
    }
	if (!strcmp(pszEvent, "on_checkBox_6_stateChanged")) {
		bEnable = uiex->isChecked("checkBox_6");
		if(m_Camera.isCameraColor()) {
			if(bEnable) {
				uiex->setEnabled("checkBox_5", false);
				uiex->setEnabled("PixelBinMode", false);
			}
			else {
				nErr = m_Camera.getMonoBin(bTmp);
				if(nErr == VAL_NOT_AVAILABLE)
					uiex->setEnabled("checkBox_5", false);
				else
					uiex->setEnabled("PixelBinMode", true);
			}
		}
		else {
			if(bEnable) {
				uiex->setEnabled("PixelBinMode", false);
			}
			else {
				uiex->setEnabled("PixelBinMode", true);
			}
		}
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
        std::string sCameraSerial;
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        str = sCameraName.c_str();
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
        std::stringstream cDevName;
        std::string sCameraSerial;
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        pMe->m_Camera.getCameraSerial(sCameraSerial);
        cDevName << sCameraName << " (" << sCameraSerial <<")";
        str = cDevName.str().c_str();
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
        std::string sCameraSerial;
        std::string sCameraName;
        m_Camera.getCameraName(sCameraName);
        str = sCameraName.c_str();
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

	m_Camera.setUserConf(true);
	nErr = loadCameraSettings(m_sCameraSerial);
    nErr = m_Camera.Connect(m_sCameraSerial);
    if(nErr) {
        m_bLinked = false;
        return nErr;
    }
    else
        m_bLinked = true;
    // store camera ID
    m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_GUID, m_sCameraSerial.c_str());
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

    if(m_Camera.isFrameAvailable())
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

	nW = (m_Camera.getWidth()/nXBin);
    nH = (m_Camera.getHeight()/nYBin);
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

	if (m_bLinked) {
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
	bool bHardwareBinPresent = false;
	bool bHardwareBinEnable = false;
	bool bMonoBin = false;
    X2MutexLocker ml(GetMutex());

	bHardwareBinPresent = m_Camera.isHardwareBinAvailable();
	nErr = m_Camera.getHardwareBinOn(bHardwareBinEnable);
	nErr = m_Camera.getMonoBin(bMonoBin);

    switch(nIndex) {
        case F_BAYER :
            if(m_Camera.isCameraColor()) { // color camera
				if(bHardwareBinPresent && bHardwareBinEnable ) { // no mono bin available if harware bin is enabled
					m_Camera.getBayerPattern(sTmp);
					sFieldName = "DEBAYER";
					sFieldComment = "Bayer pattern to use to decode color image";
					sFieldValue = sTmp.c_str();
				}
				else if(!bHardwareBinPresent || !bHardwareBinEnable) {
					if(bMonoBin) {
						sFieldName = "DEBAYER";
						sFieldComment = "Bayer pattern to use to decode color image";
						sFieldValue = "MONO";
					}
					else {
						m_Camera.getBayerPattern(sTmp);
						sFieldName = "DEBAYER";
						sFieldComment = "Bayer pattern to use to decode color image";
						sFieldValue = sTmp.c_str();
					}
				}
            }
            else { // mono camera
                sFieldName = "DEBAYER";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = "MONO";
            }
            break;

        case F_BAYERPAT: // PixInsight
			if(m_Camera.isCameraColor()) { // color camera
				if(bHardwareBinPresent && bHardwareBinEnable ) { // no mono bin available if harware bin is enabled
					m_Camera.getBayerPattern(sTmp);
					sFieldName = "BAYERPAT";
					sFieldComment = "Bayer pattern to use to decode color image";
					sFieldValue = sTmp.c_str();
				}
				else if(!bHardwareBinPresent || !bHardwareBinEnable) {
					if(bMonoBin) {
						sFieldName = "BAYERPAT";
						sFieldComment = "Bayer pattern to use to decode color image";
						sFieldValue = "MONO";
					}
					else {
						m_Camera.getBayerPattern(sTmp);
						sFieldName = "BAYERPAT";
						sFieldComment = "Bayer pattern to use to decode color image";
						sFieldValue = sTmp.c_str();
					}
				}
			}
			else { // mono camera
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

    sName = m_Camera.getGainLabelFromListAtIndex(nIndex).c_str();
    return nErr;
}

int X2Camera::CCStartExposureAdditionalArgInterface (const enumCameraIndex &Cam, const enumWhichCCD CCD, const double &dTime, enumPictureType Type, const int &nABGState, const bool &bLeaveShutterAlone, const int &nIndex)
{
    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    bool bLight = true;
    int nErr = SB_OK;

    nErr = m_Camera.setGain(m_Camera.getGainFromListAtIndex(nIndex));
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
