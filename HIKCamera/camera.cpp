#include "camera.h"
#include <QDebug>

Camera::Camera(QObject *parent)
    : QObject{parent}
{
    m_hDevHandle = NULL;
}





Camera::~Camera()
{
    if (m_hDevHandle)
    {
        MV_CC_DestroyHandle(m_hDevHandle);
        m_hDevHandle    = NULL;
    }
}

int Camera::InitSDK()
{
    return MV_CC_Initialize();
}

int Camera::FinalizeSDK()
{
    return MV_CC_Finalize();
}


int Camera::EnumDevices(QMap<QString, MV_CC_DEVICE_INFO*> &devices){
    devices.clear();
    MV_CC_DEVICE_INFO_LIST m_stDevList;

    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE
                                     | MV_USB_DEVICE
                                     | MV_GENTL_GIGE_DEVICE
                                     | MV_GENTL_CAMERALINK_DEVICE
                                     |MV_GENTL_CXP_DEVICE
                                     | MV_GENTL_XOF_DEVICE,
                                 &m_stDevList);
    if (nRet == MV_OK) {

        if(m_stDevList.nDeviceNum==0)return 0;

        for (int i = 0; i < m_stDevList.nDeviceNum; i++)
        {
            MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList.pDeviceInfo[i];
            if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
                int nIp1 = ((m_stDevList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
                int nIp2 = ((m_stDevList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
                int nIp3 = ((m_stDevList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
                int nIp4 = (m_stDevList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
                QString str = QString("%1.%2.%3.%4").arg(nIp1).arg(nIp2).arg(nIp3).arg(nIp4);
                devices[str] = m_stDevList.pDeviceInfo[i];

            }
            else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE) {
                char strUserName[256] = { 0 };
                strcmp("", (char*)pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
                memset(strUserName, 0, 256);
                sprintf_s(strUserName, 256, "%s (%s)", pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName,
                          pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
                devices[strUserName] = m_stDevList.pDeviceInfo[i];

            }
            else if (pDeviceInfo->nTLayerType == MV_GENTL_GIGE_DEVICE) {
            }
            else if (pDeviceInfo->nTLayerType == MV_GENTL_CAMERALINK_DEVICE) {}
            else if (pDeviceInfo->nTLayerType == MV_GENTL_CXP_DEVICE) {}
            else if (pDeviceInfo->nTLayerType == MV_GENTL_XOF_DEVICE) {}
            else { }
        }
        return m_stDevList.nDeviceNum;
    }
    return 0;
}

// 打开设备
int Camera::Open(MV_CC_DEVICE_INFO* pstDeviceInfo){
    if (NULL == pstDeviceInfo)return MV_E_PARAMETER;

    int nRet = MV_OK;
    if(m_hDevHandle == NULL){
        nRet  = MV_CC_CreateHandle(&m_hDevHandle, pstDeviceInfo);
        if (MV_OK != nRet)return nRet;
    }

    nRet = MV_CC_OpenDevice(m_hDevHandle);
    if (MV_OK != nRet){
        MV_CC_DestroyHandle(m_hDevHandle);
        m_hDevHandle = NULL;
        return nRet;
    }

    openFlag = true;
    return MV_OK;
}

bool Camera::isOpened(){
    return openFlag;
}

// 关闭设备
int  Camera::Close(){
    int nRet = MV_OK;
    if (NULL == m_hDevHandle)
    {
        return MV_E_PARAMETER;
    }
    MV_CC_CloseDevice(m_hDevHandle);
    nRet = MV_CC_DestroyHandle(m_hDevHandle);

    m_hDevHandle = NULL;
    openFlag = false;
    return nRet;
}

// 开启抓图
int Camera::StartGrabbing(){
    if(!grabbingFlag)
    {
        int ret = MV_CC_StartGrabbing(m_hDevHandle);
        if(ret==MV_OK)grabbingFlag=true;
        return ret;
    }else return MV_OK;
}

// 停止抓图
int Camera::StopGrabbing(){
    if(grabbingFlag)
    {
        int ret = MV_CC_StopGrabbing(m_hDevHandle);
        if(ret==MV_OK)grabbingFlag=false;
        return ret;
    }else return MV_OK;
}

// 设置显示窗口句柄
int  Camera::Display(void* hWnd){
    if(!displayFlag)
    {
        int ret = MV_CC_Display(m_hDevHandle, hWnd);
        if(ret==MV_OK)displayFlag=true;
        return ret;
    }else return MV_OK;
}

int  Camera::Display(QLabel* label){
    if(!displayFlag)
    {
        hWnd = (HWND)label->winId();
        int ret = MV_CC_Display(m_hDevHandle, hWnd);
        if(ret==MV_OK)displayFlag=true;
        return ret;
    }else return MV_OK;
}



//设置触发
int Camera::SetTriggerMode(MV_CAM_TRIGGER_MODE mode){
    return SetEnumValue("TriggerMode", mode);
}

int Camera::GetTriggerMode(float &m){
    MVCC_ENUMVALUE stEnumValue = {0};

    int nRet = GetEnumValue("TriggerMode", &stEnumValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    m = stEnumValue.nCurValue;
    return MV_OK;
}

//设置触发源
int Camera::SetTriggerSource(MV_CAM_TRIGGER_SOURCE source){
    return SetEnumValue("TriggerSource", source);
}

int Camera::GetTriggerSource(float &s){
    MVCC_ENUMVALUE stEnumValue = {0};
    int nRet = GetEnumValue("TriggerSource", &stEnumValue);
    return MV_OK;
}

//设置曝光
int Camera::SetExposureTime(float time){
    if(time==-1){
        return SetEnumValue("ExposureAuto", MV_EXPOSURE_AUTO_MODE_ONCE);
    }else if(time == -2){
        return SetEnumValue("ExposureAuto", MV_EXPOSURE_AUTO_MODE_CONTINUOUS);

    }else{
        SetEnumValue("ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
        return SetFloatValue("ExposureTime", time);
    }
}

int Camera::GetExposureTime(float &time){

    MVCC_ENUMVALUE stEnumValue = {0};
    GetEnumValue("ExposureAuto", &stEnumValue);
    if(stEnumValue.nCurValue==1)
        time=-1;
    else if(stEnumValue.nCurValue==2){
        time=-2;
    }else{
        MVCC_FLOATVALUE stFloatValue = {0};
        GetFloatValue("ExposureTime", &stFloatValue);
        time = stFloatValue.fCurValue;
    }
    return 0;
}


//设置增益
int Camera::SetGain(float gain){
    qDebug()<<"gain"<<gain;
    if(gain==-1){
        //单次增益
        return SetEnumValue("GainAuto", 1);
    }if(gain==-2){
        //自动增益
        return SetEnumValue("GainAuto", 2);
    }else{
        SetEnumValue("GainAuto", 0);
        return SetFloatValue("Gain", gain);
    }
}

int Camera::GetGain(float &gain){

    MVCC_ENUMVALUE stEnumValue = {0};
    GetEnumValue("GainAuto", &stEnumValue);
    if(stEnumValue.nCurValue==1)
        gain=-1;
    else if(stEnumValue.nCurValue==2){
        gain=-2;
    }else{
        MVCC_FLOATVALUE stFloatValue = {0};
        GetFloatValue("Gain", &stFloatValue);
        gain = stFloatValue.fCurValue;
    }
    return 0;
}

//设置帧率
int Camera::SetFrameRate(float frameRate){
    if(frameRate==-1)
        return SetBoolValue("DigitalShiftEnable", false);
    else{
        SetBoolValue("AcquisitionFrameRateEnable", true);
        return SetFloatValue("AcquisitionFrameRate", frameRate);
    }
}

int Camera::GetFrameRate(int &frameRate){
    MVCC_FLOATVALUE stFloatValue = {0};

    int nRet = GetFloatValue("ResultingFrameRate", &stFloatValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    frameRate = stFloatValue.fCurValue;
    return MV_OK;
}

int Camera::SetDigitalGain(float gain){
    if(gain==-1)
        return SetBoolValue("DigitalShiftEnable", false);
    else{
        SetBoolValue("DigitalShiftEnable", true);
        return SetFloatValue("DigitalShift", gain);
    }
}

void Camera::getDigitalGain(float *gain){
    bool enable;
    GetBoolValue("DigitalShiftEnable",&enable);
    if(enable)
    {
        MVCC_FLOATVALUE stFloatValue = {0};
        GetFloatValue("DigitalShift", &stFloatValue);
        *gain = stFloatValue.fCurValue;
    }
    else{
        *gain = -1;
    }
}

//设置白平衡
int Camera::setBanlenceWhite(float white){
    if(white==-1)
        return SetEnumValue("BalanceWhiteAuto", 1);
    else if(white==-2){
        return SetEnumValue("BalanceWhiteAuto", 2);
    }
    else{
        SetEnumValue("BalanceWhiteAuto", 0);
        return SetFloatValue("BalanceRatio", white);
    }
}

void Camera::getBanlenceWhite(float *white){
    MVCC_ENUMVALUE stEnumValue = {0};
    GetEnumValue("BalanceWhiteAuto", &stEnumValue);
    if(stEnumValue.nCurValue==1)
        *white=-1;
    else if(stEnumValue.nCurValue==2){
        *white=-2;
    }else{
        MVCC_FLOATVALUE stFloatValue = {0};
        GetFloatValue("BalanceRatio", &stFloatValue);
        *white = stFloatValue.fCurValue;
    }
}

//设置gamma
int Camera::setGamma(float gamma){
    if(gamma==-1)
        return SetBoolValue("GammaEnable", false);
    else{
        SetBoolValue("GammaEnable", true);
        return SetFloatValue("Gamma", gamma);
    }

}

void Camera::getGamma(float *gamma){
    bool enable;
    GetBoolValue("GammaEnable",&enable);
    if(enable)
        *gamma=-1;
    else{
        MVCC_FLOATVALUE stFloatValue = {0};
        GetFloatValue("Gamma", &stFloatValue);
        *gamma = stFloatValue.fCurValue;
    }
}

int Camera::setHeight(float h){

    return SetIntValue("Height",h);
}

void Camera::getHeight(float *h){
    MVCC_INTVALUE_EX stIntValue = {0};
    GetIntValue("Height", &stIntValue);
    *h = stIntValue.nCurValue;
}

void Camera::getMaxHeight(float *h){
    MVCC_INTVALUE_EX stIntValue = {0};
    GetIntValue("HeightMax", &stIntValue);
    *h = stIntValue.nCurValue;
}

int Camera::setWidth(float w){
    return SetIntValue("Width",w);
}

void Camera::getMaxWidth(float *w){
    MVCC_INTVALUE_EX stIntValue = {0};
    GetIntValue("WidthMax", &stIntValue);
    *w = stIntValue.nCurValue;
}

void Camera::getWidth(float *w){
    MVCC_INTVALUE_EX stIntValue = {0};
    GetIntValue("Width", &stIntValue);
    *w = stIntValue.nCurValue;
}

int Camera::setXReverse(bool e){
    return SetBoolValue("ReverseX",e);
}

void Camera::getXReverse(bool *e){
    GetBoolValue("ReverseX",e);
}

int Camera::setYReverse(bool e){
    return SetBoolValue("ReverseY",e);

}

void Camera::getYReverse(bool *e){
    GetBoolValue("ReverseY",e);
}

// 主动获取一帧图像数据
int Camera::GetOneFrameTimeout(unsigned char* pData,
                                  unsigned int* pnDataLen,
                                  unsigned int nDataSize,
                                  MV_FRAME_OUT_INFO_EX* pFrameInfo,
                                  int nMsec){
    if (NULL == pnDataLen)
    {
        return MV_E_PARAMETER;
    }

    int nRet = MV_OK;

    *pnDataLen  = 0;

    nRet = MV_CC_GetOneFrameTimeout(m_hDevHandle, pData, nDataSize, pFrameInfo, nMsec);
    if (MV_OK != nRet)
    {
        return nRet;
    }

    *pnDataLen = pFrameInfo->nFrameLen;


    return nRet;
}


// ch:主动获取一帧图像数据 | en:Get one frame initiatively
int Camera::GetImageBuffer(MV_FRAME_OUT* pFrame, int nMsec)
{
    return MV_CC_GetImageBuffer(m_hDevHandle, pFrame, nMsec);
}

// ch:释放图像缓存 | en:Free image buffer
int Camera::FreeImageBuffer(MV_FRAME_OUT* pFrame)
{
    return MV_CC_FreeImageBuffer(m_hDevHandle, pFrame);
}

// ch:设置显示窗口句柄 | en:Set Display Window Handle
int Camera::DisplayOneFrame(void* hwndDisplay,MV_CC_IMAGE* pImageInfo)
{
    return MV_CC_DisplayOneFrameEx2(m_hDevHandle, hwndDisplay, pImageInfo,0);

}

int Camera::DisplayOneFrame(QLabel* label, MV_CC_IMAGE* pImageInfo)
{
    hWnd = (HWND)label->winId();
    return MV_CC_DisplayOneFrameEx2(m_hDevHandle, hWnd, pImageInfo, 0);
}

// ch:设置SDK内部图像缓存节点个数 | en:Set the number of the internal image cache nodes in SDK
int Camera::SetImageNodeNum(unsigned int nNum)
{
    return MV_CC_SetImageNodeNum(m_hDevHandle, nNum);
}

// ch:获取设备信息 | en:Get device information
int Camera::GetDeviceInfo(MV_CC_DEVICE_INFO* pstDevInfo)
{
    return MV_CC_GetDeviceInfo(m_hDevHandle, pstDevInfo);
}

// ch:获取GEV相机的统计信息 | en:Get detect info of GEV camera
int Camera::GetGevAllMatchInfo(MV_MATCH_INFO_NET_DETECT* pMatchInfoNetDetect)
{
    if (MV_NULL == pMatchInfoNetDetect)
    {
        return MV_E_PARAMETER;
    }

    MV_CC_DEVICE_INFO stDevInfo = {0};
    GetDeviceInfo(&stDevInfo);
    if (stDevInfo.nTLayerType != MV_GIGE_DEVICE)
    {
        return MV_E_SUPPORT;
    }

    MV_ALL_MATCH_INFO struMatchInfo = {0};

    struMatchInfo.nType = MV_MATCH_TYPE_NET_DETECT;
    struMatchInfo.pInfo = pMatchInfoNetDetect;
    struMatchInfo.nInfoSize = sizeof(MV_MATCH_INFO_NET_DETECT);
    memset(struMatchInfo.pInfo, 0, sizeof(MV_MATCH_INFO_NET_DETECT));

    return MV_CC_GetAllMatchInfo(m_hDevHandle, &struMatchInfo);
}

// ch:获取U3V相机的统计信息 | en:Get detect info of U3V camera
int Camera::GetU3VAllMatchInfo(MV_MATCH_INFO_USB_DETECT* pMatchInfoUSBDetect)
{
    if (MV_NULL == pMatchInfoUSBDetect)
    {
        return MV_E_PARAMETER;
    }

    MV_CC_DEVICE_INFO stDevInfo = {0};
    GetDeviceInfo(&stDevInfo);
    if (stDevInfo.nTLayerType != MV_USB_DEVICE)
    {
        return MV_E_SUPPORT;
    }

    MV_ALL_MATCH_INFO struMatchInfo = {0};

    struMatchInfo.nType = MV_MATCH_TYPE_USB_DETECT;
    struMatchInfo.pInfo = pMatchInfoUSBDetect;
    struMatchInfo.nInfoSize = sizeof(MV_MATCH_INFO_USB_DETECT);
    memset(struMatchInfo.pInfo, 0, sizeof(MV_MATCH_INFO_USB_DETECT));

    return MV_CC_GetAllMatchInfo(m_hDevHandle, &struMatchInfo);
}

// ch:获取和设置Int型参数，如 Width和Height
// en:Get Int type parameters, such as Width and Height
int Camera::GetIntValue(IN const char* strKey, OUT MVCC_INTVALUE_EX *pIntValue)
{
    return MV_CC_GetIntValueEx(m_hDevHandle, strKey, pIntValue);
}

int Camera::SetIntValue(IN const char* strKey, IN int64_t nValue)
{
    return MV_CC_SetIntValueEx(m_hDevHandle, strKey, nValue);
}

// ch:获取和设置Enum型参数，如 PixelFormat
// en:Get Enum type parameters, such as PixelFormat
int Camera::GetEnumValue(IN const char* strKey, OUT MVCC_ENUMVALUE *pEnumValue)
{
    return MV_CC_GetEnumValue(m_hDevHandle, strKey, pEnumValue);
}

int Camera::SetEnumValue(IN const char* strKey, IN unsigned int nValue)
{
    return MV_CC_SetEnumValue(m_hDevHandle, strKey, nValue);
}

int Camera::SetEnumValueByString(IN const char* strKey, IN const char* sValue)
{
    return MV_CC_SetEnumValueByString(m_hDevHandle, strKey, sValue);
}

int Camera::GetEnumEntrySymbolic(IN const char* strKey, IN MVCC_ENUMENTRY* pstEnumEntry)
{
    return MV_CC_GetEnumEntrySymbolic(m_hDevHandle, strKey, pstEnumEntry);
}

// ch:获取和设置Float型参数，如 ExposureTime和Gain
// en:Get Float type parameters, such as ExposureTime and Gain
int Camera::GetFloatValue(IN const char* strKey, OUT MVCC_FLOATVALUE *pFloatValue)
{
    return MV_CC_GetFloatValue(m_hDevHandle, strKey, pFloatValue);
}

int Camera::SetFloatValue(IN const char* strKey, IN float fValue)
{
    return MV_CC_SetFloatValue(m_hDevHandle, strKey, fValue);
}

// ch:获取和设置Bool型参数，如 ReverseX
// en:Get Bool type parameters, such as ReverseX
int Camera::GetBoolValue(IN const char* strKey, OUT bool *pbValue)
{
    return MV_CC_GetBoolValue(m_hDevHandle, strKey, pbValue);
}

int Camera::SetBoolValue(IN const char* strKey, IN bool bValue)
{
    return MV_CC_SetBoolValue(m_hDevHandle, strKey, bValue);
}

// ch:获取和设置String型参数，如 DeviceUserID
// en:Get String type parameters, such as DeviceUserID
int Camera::GetStringValue(IN const char* strKey, MVCC_STRINGVALUE *pStringValue)
{
    return MV_CC_GetStringValue(m_hDevHandle, strKey, pStringValue);
}

int Camera::SetStringValue(IN const char* strKey, IN const char* strValue)
{
    return MV_CC_SetStringValue(m_hDevHandle, strKey, strValue);
}

// ch:执行一次Command型命令，如 UserSetSave
// en:Execute Command once, such as UserSetSave
int Camera::CommandExecute(IN const char* strKey)
{
    return MV_CC_SetCommandValue(m_hDevHandle, strKey);
}

// ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
int Camera::GetOptimalPacketSize(unsigned int* pOptimalPacketSize)
{
    if (MV_NULL == pOptimalPacketSize)
    {
        return MV_E_PARAMETER;
    }

    int nRet = MV_CC_GetOptimalPacketSize(m_hDevHandle);
    if (nRet < MV_OK)
    {
        return nRet;
    }

    *pOptimalPacketSize = (unsigned int)nRet;

    return MV_OK;
}

// ch:注册消息异常回调 | en:Register Message Exception CallBack
int Camera::RegisterExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType, void* pUser),void* pUser)
{
    return MV_CC_RegisterExceptionCallBack(m_hDevHandle, cbException, pUser);
}

// ch:注册单个事件回调 | en:Register Event CallBack
int Camera::RegisterEventCallBack(const char* pEventName, void(__stdcall* cbEvent)(MV_EVENT_OUT_INFO * pEventInfo, void* pUser), void* pUser)
{
    return MV_CC_RegisterEventCallBackEx(m_hDevHandle, pEventName, cbEvent, pUser);
}

// ch:强制IP | en:Force IP
int Camera::ForceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay)
{
    return MV_GIGE_ForceIpEx(m_hDevHandle, nIP, nSubNetMask, nDefaultGateWay);
}

// ch:配置IP方式 | en:IP configuration method
int Camera::SetIpConfig(unsigned int nType)
{
    return MV_GIGE_SetIpConfig(m_hDevHandle, nType);
}

// ch:设置网络传输模式 | en:Set Net Transfer Mode
int Camera::SetNetTransMode(unsigned int nType)
{
    return MV_GIGE_SetNetTransMode(m_hDevHandle, nType);
}

// ch:像素格式转换 | en:Pixel format conversion
int Camera::ConvertPixelType(MV_CC_PIXEL_CONVERT_PARAM_EX* pstCvtParam)
{
    return MV_CC_ConvertPixelTypeEx(m_hDevHandle, pstCvtParam);
}

// ch:保存图片 | en:save image
int Camera::SaveImage(MV_SAVE_IMAGE_PARAM_EX3* pstParam)
{
    return MV_CC_SaveImageEx3(m_hDevHandle, pstParam);
}

// ch:保存图片为文件 | en:Save the image as a file
int Camera::SaveImageToFile(MV_CC_IMAGE* pstImage, MV_CC_SAVE_IMAGE_PARAM* pSaveImageParam, const char* pcImagePath)
{
    return MV_CC_SaveImageToFileEx2(m_hDevHandle, pstImage, pSaveImageParam, pcImagePath);
}

int Camera::DrawCircle(MVCC_CIRCLE_INFO* pCircleInfo)
{
    return MV_CC_DrawCircle(m_hDevHandle, pCircleInfo);
}

// ch:绘制线形辅助线 | en:Draw lines auxiliary line
int Camera::DrawLines(MVCC_LINES_INFO* pLinesInfo)
{
    return MV_CC_DrawLines(m_hDevHandle, pLinesInfo);
}

//矩形框
int Camera::DrawRect(MVCC_RECT_INFO* pRectInfo) {
    return MV_CC_DrawRect(m_hDevHandle, pRectInfo);
}

int Camera::getOneImgMat(cv::Mat* img, int t) {
    pFrame = { 0 };
    int ret = GetImageBuffer(&pFrame, t);
    if (ret != MV_OK) {
        //qDebug() << "GetImageBuffer failed";
        return ret;
    }
    stImageInfo = pFrame.stFrameInfo;
    FreeImageBuffer(&pFrame);

    unsigned int m_nBufSizeForSaveImage = stImageInfo.nWidth * stImageInfo.nHeight * 3 + 2048;
    unsigned char* m_pBufForSaveImage = (unsigned char*)malloc(m_nBufSizeForSaveImage);
    if (!m_pBufForSaveImage) {
        qDebug() << "Memory allocation failed";
        return -1; // 或其他错误码表示内存分配失败
    }

    unsigned char* pData = pFrame.pBufAddr;

    MV_CC_PIXEL_CONVERT_PARAM stConvertParam = { 0 };
    memset(&stConvertParam, 0, sizeof(MV_CC_PIXEL_CONVERT_PARAM));
    stConvertParam.nWidth = stImageInfo.nWidth;
    stConvertParam.nHeight = stImageInfo.nHeight;
    stConvertParam.pSrcData = pData;
    stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;
    stConvertParam.enSrcPixelType = stImageInfo.enPixelType;
    stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
    stConvertParam.pDstBuffer = m_pBufForSaveImage;
    stConvertParam.nDstBufferSize = m_nBufSizeForSaveImage;
    MV_CC_ConvertPixelType(m_hDevHandle, &stConvertParam);

    *img = cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, m_pBufForSaveImage).clone();

    free(m_pBufForSaveImage);
    return MV_OK;
}

int Camera::getOneImgEx(MV_CC_IMAGE* stImageData) {
    stImageData = { 0 };
    pFrame = { 0 };
    int ret = GetImageBuffer(&pFrame, 1000);
    if (ret != MV_OK) {
        qDebug() << "GetImageBuffer failed";
        return ret;
    }
    stImageData->nWidth = pFrame.stFrameInfo.nExtendWidth;
    stImageData->nHeight = pFrame.stFrameInfo.nExtendHeight;
    stImageData->enPixelType = pFrame.stFrameInfo.enPixelType;
    stImageData->nImageBufLen = pFrame.stFrameInfo.nFrameLenEx;
    stImageData->pImageBuf = pFrame.pBufAddr;

    FreeImageBuffer(&pFrame);
    return MV_OK;
}



