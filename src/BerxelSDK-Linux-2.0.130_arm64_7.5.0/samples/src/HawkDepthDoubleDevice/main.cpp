#include <stdio.h>
#include <string.h>
#include <fstream>
// Berxel Head File
#include "BerxelHawkContext.h"
#include "BerxelHawkDevice.h"
#include "BerxelHawkFrame.h"
#include "BerxelHawkDefines.h"
#include "BerxelCommonFunc.h"
#include "BerxelImageRender.h"

using namespace std;

berxel::BerxelHawkContext*   g_context		= NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice  = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;
berxel::BerxelHawkDevice*    g_pHawkDevice1  = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo1;
BerxelImageRender*			 g_pImageRender  = NULL;
static int  g_imageWidth	 = 400;
static int  g_imageHeight = 640;
static char g_errMsg[256] = {0};
static char g_deviceShowInfo[256];
static bool g_bStartStream = false;
static bool g_bSave = false;

static bool renderImage()
{
	static char		mesgInfo[256] = {0};
	static RGB888   rgbImage[1280 * 800];
	static RGB888	rgbImage1[1280 * 800];

	if(false == g_bStartStream)
	{
		g_pImageRender->initView();
		g_pImageRender->drawLine(0,35, g_imageWidth + 80 ,40);
		g_pImageRender->drawString(g_errMsg, 5, 25 , (void *)0x0008);
		g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth + 80 ,40);
		g_pImageRender->updateView();
		return false;
	}

	berxel::BerxelHawkFrame *pHawkFrame = NULL;
	berxel::BerxelHawkFrame *pHawkFrame1 = NULL;

	if(g_pHawkDevice)
	{
		int ret = g_pHawkDevice->readDepthFrame(pHawkFrame,30);
		if (pHawkFrame == NULL)
		{
			return false;
		}
	}

	if (g_pHawkDevice1)
	{
		int ret = g_pHawkDevice1->readDepthFrame(pHawkFrame1,30);
		if (pHawkFrame1 == NULL)
		{
			g_pHawkDevice->releaseFrame(pHawkFrame);
			return false;
		}
	}

	BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkFrame->getData(), (uint8_t *)rgbImage,pHawkFrame->getHeight(),pHawkFrame->getWidth() , pHawkFrame->getPixelType());
	BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkFrame1->getData(), (uint8_t *)rgbImage1,pHawkFrame1->getHeight(),pHawkFrame1->getWidth() ,pHawkFrame1->getPixelType());

	memset(mesgInfo, 0, sizeof(mesgInfo));
	sprintf(mesgInfo, "%d*%d@fps %d", pHawkFrame->getWidth(), pHawkFrame->getHeight(), pHawkFrame->getFPS());

	//Render
	g_pImageRender->initView();
	g_pImageRender->drawLine(0,35, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(mesgInfo ,5, 25 , (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo ,5,g_imageHeight + 40 + 25 , (void *)0x0007);
	WinRect rect(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImage, pHawkFrame->getWidth() , pHawkFrame->getHeight(), rect);
	rect.x += g_imageWidth + 2;
	g_pImageRender->drawColorImage((uint8_t*)rgbImage1, pHawkFrame1->getWidth(), pHawkFrame1->getHeight(),  rect);
	g_pImageRender->updateView();
	
	// free frame
	g_pHawkDevice->releaseFrame(pHawkFrame);
	g_pHawkDevice->releaseFrame(pHawkFrame1);
    return true;
}

void keyCallBack(unsigned char key)
{
	switch (key)
	{
	case 'S':
	case 's':
		g_bSave = true;
		break;
	default:
		printf("Please input 's' or 'S'  to save image: \n");
		break;
	}
}

int Exit()
{
	if(g_pHawkDevice)
	{
		g_pHawkDevice->stopStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
	}

	if(g_context)
	{
		g_context->closeDevice(g_pHawkDevice);
	}

	if(g_context)
	{
        berxel::BerxelHawkContext::destroyBerxelContext(g_context);
		g_context = NULL;
	}

	return 0;
}


int creatWindow(int argc, char** argv)
{
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkDepth", g_imageWidth * 2 + 80 , g_imageHeight + 80); // window title & size
	g_pImageRender->setInfoCallback(renderImage , keyCallBack);
	g_pImageRender->startView();
	return 0;
}



int main(int argc, char** argv)
{
	//获取context
	g_context = berxel::BerxelHawkContext::getBerxelContext();

	//打开设备
	berxel::BerxelHawkDeviceInfo* pDeviceInfo = NULL;
	uint32_t deviceCount = 0;
	g_context->getDeviceList(&pDeviceInfo, &deviceCount);
    if((deviceCount <= 1) || (NULL == pDeviceInfo))
	{
		sprintf(g_errMsg,"%s", "Get No Connected BerxelDevice");
		return creatWindow(argc ,argv);
    }

	g_CurrentDeviceInfo = pDeviceInfo[0];
	g_CurrentDeviceInfo1 = pDeviceInfo[1];

	g_pHawkDevice = g_context->openDevice(g_CurrentDeviceInfo);
    if(NULL == g_pHawkDevice)
	{       
		sprintf(g_errMsg,"%s", "Open BerxelDevice Failed");
        return creatWindow(argc ,argv);
    }
	
    //同步当前系统时钟到设备中
	g_pHawkDevice->setSystemClock();

	//设置流模式
	g_pHawkDevice->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

	//设置分辨率
	berxel::BerxelHawkStreamFrameMode frameMode; 
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode);

	// 打开数据流
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
	if(ret != 0)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Stream Failed");
		return creatWindow(argc ,argv);
	}

	g_pHawkDevice1 = g_context->openDevice(g_CurrentDeviceInfo1);
	if(NULL == g_pHawkDevice1)
	{       
		sprintf(g_errMsg,"%s", "Open BerxelDevice Failed");
		return creatWindow(argc ,argv);
	}

	//同步当前系统时钟到设备中
	g_pHawkDevice1->setSystemClock();

	//设置流模式
	g_pHawkDevice1->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

	//设置分辨率
	berxel::BerxelHawkStreamFrameMode frameMode1; 
	g_pHawkDevice1->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode1);
	g_pHawkDevice1->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode1);

	if (frameMode1.resolutionX < frameMode1.resolutionY)
	{
		g_imageWidth  =  400 ;
		g_imageHeight =  640;
	}
	else
	{
		g_imageWidth  =  640 ;
		g_imageHeight =  400;
	}

	// 打开数据流
	ret = g_pHawkDevice1->startStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
	if(ret != 0)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Stream Failed");
		return creatWindow(argc ,argv);
	}

	berxel::BerxelHawkDeviceInfo tempCurInfo;
	berxel::BerxelHawkVersions   tempVersions;
	g_pHawkDevice->getCurrentDeviceInfo(&tempCurInfo);
	g_pHawkDevice->getVersion(&tempVersions);
	sprintf(g_deviceShowInfo , "SN :%s  SDK(%d.%d.%d) FW(%d.%d.%d-%s) HW(%d.%d.%d) " ,tempCurInfo.serialNumber , tempVersions.sdkVersion.major ,tempVersions.sdkVersion.minor, tempVersions.sdkVersion.revision ,
		tempVersions.fwVersion.major,tempVersions.fwVersion.minor,tempVersions.fwVersion.revision, tempVersions.fwVersion.chipVersion,tempVersions.hwVersion.major, tempVersions.hwVersion.minor, tempVersions.hwVersion.revision);

	g_bStartStream = true;	
	creatWindow(argc,argv);
	return Exit();
}
