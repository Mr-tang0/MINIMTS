#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include "MvCameraControl.h"
#include<QMap>
#include<QLabel>
#include "opencv2/opencv.hpp"

#ifndef MV_NULL
#define MV_NULL    0
#endif


class Camera : public QObject
{
    Q_OBJECT
public:
    explicit Camera(QObject *parent = nullptr);
    ~Camera();

    // 初始化SDK
    static int InitSDK();
    static int FinalizeSDK();

    // 查找设备
    static int EnumDevices(QMap<QString, MV_CC_DEVICE_INFO*> &devices);

public slots:
    // 打开设备
    int Open(MV_CC_DEVICE_INFO* pstDeviceInfo);
    // 关闭设备
    int  Close();

    // 开启抓图
    int StartGrabbing();
    // 停止抓图
    int StopGrabbing();

    // 设置显示窗口
    int  Display(void* hWnd);
    int  Display(QLabel* label);

    //设置触发
    int SetTriggerMode(MV_CAM_TRIGGER_MODE);
    int SetTriggerSource(MV_CAM_TRIGGER_SOURCE s);
    //设置曝光
    int SetExposureTime(float time);
    //设置增益
    int SetGain(float gain);
    //帧率
    int SetFrameRate(float frameRate);
    //设置数字增益
    int SetDigitalGain(float gain);
    //设置白平衡
    int setBanlenceWhite(float white);
    //设置gamma
    int setGamma(float gamma);
    int setHeight(float h);
    int setWidth(float w);
    int setXReverse(bool e);
    int setYReverse(bool e);


    int GetTriggerMode(float &m);
    int GetTriggerSource(float &s);
    int GetExposureTime(float &time);
    int GetGain(float &gain);
    int GetFrameRate(int &frameRate);

    void getDigitalGain(float *gain);
    void getBanlenceWhite(float *white);
    void getGamma(float *gamma);
    void getHeight(float *h);
    void getWidth(float *w);
    void getXReverse(bool *e);
    void getYReverse(bool *e);

    bool isOpened();
    bool isGrabbing() { return grabbingFlag; }
    bool isDisplaying() { return displayFlag; }
    bool isAlreadyHaveWidget() { return haveWinFlag; }

    int getOneImgMat(cv::Mat* img, int t);
    int getOneImgEx(MV_CC_IMAGE*img);


public:

    // ch:判断设备是否可达 | en:Is the device accessible
    static bool IsDeviceAccessible(MV_CC_DEVICE_INFO* pstDevInfo, unsigned int nAccessMode);


    // 主动获取一帧图像数据
    int GetOneFrameTimeout(unsigned char* pData, unsigned int* pnDataLen, unsigned int nDataSize, MV_FRAME_OUT_INFO_EX* pFrameInfo, int nMsec);


    // ch:注册图像数据回调 | en:Register Image Data CallBack
    int RegisterImageCallBack(void(__stdcall* cbOutput)(unsigned char * pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser), void* pUser);

    // ch:主动获取一帧图像数据 | en:Get one frame initiatively
    int GetImageBuffer(MV_FRAME_OUT* pFrame, int nMsec);

    // ch:释放图像缓存 | en:Free image buffer
    int FreeImageBuffer(MV_FRAME_OUT* pFrame);

    // ch:显示一帧图像 | en:Display one frame image
    int DisplayOneFrame(void* hDisplay, MV_CC_IMAGE* pImageInfo);
    int DisplayOneFrame(QLabel* label, MV_CC_IMAGE* pImageInfo);

    // ch:设置SDK内部图像缓存节点个数 | en:Set the number of the internal image cache nodes in SDK
    int SetImageNodeNum(unsigned int nNum);

    // ch:获取设备信息 | en:Get device information
    int GetDeviceInfo(MV_CC_DEVICE_INFO* pstDevInfo);

    // ch:获取GEV相机的统计信息 | en:Get detect info of GEV camera
    int GetGevAllMatchInfo(MV_MATCH_INFO_NET_DETECT* pMatchInfoNetDetect);

    // ch:获取U3V相机的统计信息 | en:Get detect info of U3V camera
    int GetU3VAllMatchInfo(MV_MATCH_INFO_USB_DETECT* pMatchInfoUSBDetect);

    // ch:获取和设置Int型参数，如 Width和Height
    // en:Get Int type parameters, such as Width and Height
    int GetIntValue(IN const char* strKey, OUT MVCC_INTVALUE_EX *pIntValue);
    int SetIntValue(IN const char* strKey, IN int64_t nValue);

    // ch:获取和设置Enum型参数，如 PixelFormat
    // en:Get Enum type parameters, such as PixelFormat
    int GetEnumValue(IN const char* strKey, OUT MVCC_ENUMVALUE *pEnumValue);
    int SetEnumValue(IN const char* strKey, IN unsigned int nValue);
    int SetEnumValueByString(IN const char* strKey, IN const char* sValue);
    int GetEnumEntrySymbolic(IN const char* strKey, IN MVCC_ENUMENTRY* pstEnumEntry);

    // ch:获取和设置Float型参数，如 ExposureTime和Gain
    // en:Get Float type parameters, such as ExposureTime and Gain
    int GetFloatValue(IN const char* strKey, OUT MVCC_FLOATVALUE *pFloatValue);
    int SetFloatValue(IN const char* strKey, IN float fValue);

    // ch:获取和设置Bool型参数，如 ReverseX
    // en:Get Bool type parameters, such as ReverseX
    int GetBoolValue(IN const char* strKey, OUT bool *pbValue);
    int SetBoolValue(IN const char* strKey, IN bool bValue);

    // ch:获取和设置String型参数，如 DeviceUserID
    // en:Get String type parameters, such as DeviceUserID
    int GetStringValue(IN const char* strKey, MVCC_STRINGVALUE *pStringValue);
    int SetStringValue(IN const char* strKey, IN const char * strValue);

    // ch:执行一次Command型命令，如 UserSetSave
    // en:Execute Command once, such as UserSetSave
    int CommandExecute(IN const char* strKey);

    // ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
    int GetOptimalPacketSize(unsigned int* pOptimalPacketSize);

    // ch:注册消息异常回调 | en:Register Message Exception CallBack
    int RegisterExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType, void* pUser), void* pUser);

    // ch:注册单个事件回调 | en:Register Event CallBack
    int RegisterEventCallBack(const char* pEventName, void(__stdcall* cbEvent)(MV_EVENT_OUT_INFO * pEventInfo, void* pUser), void* pUser);

    // ch:强制IP | en:Force IP
    int ForceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay);

    // ch:配置IP方式 | en:IP configuration method
    int SetIpConfig(unsigned int nType);

    // ch:设置网络传输模式 | en:Set Net Transfer Mode
    int SetNetTransMode(unsigned int nType);

    // ch:像素格式转换 | en:Pixel format conversion
    int ConvertPixelType(MV_CC_PIXEL_CONVERT_PARAM_EX* pstCvtParam);

    // ch:保存图片 | en:save image
    int SaveImage(MV_SAVE_IMAGE_PARAM_EX3* pstParam);

    // ch:保存图片为文件 | en:Save the image as a file
    int SaveImageToFile(MV_CC_IMAGE* pstImage, MV_CC_SAVE_IMAGE_PARAM* pSaveImageParam, const char* pcImagePath);

    // ch:绘制圆形辅助线 | en:Draw circle auxiliary line
    int DrawCircle(MVCC_CIRCLE_INFO* pCircleInfo);

    // ch:绘制线形辅助线 | en:Draw lines auxiliary line
    int DrawLines(MVCC_LINES_INFO* pLinesInfo);

    //矩形框
    int DrawRect(MVCC_RECT_INFO* pRectInfo);



    void*  m_hDevHandle;

signals:

private:
    bool openFlag = false;

    bool grabbingFlag = false;
    bool displayFlag = false;
    bool haveWinFlag = false;
    HWND hWnd;

    MV_FRAME_OUT pFrame;
    MV_FRAME_OUT_INFO_EX stImageInfo;
    MV_CC_PIXEL_CONVERT_PARAM stConvertParam;
};

#endif // CAMERA_H
