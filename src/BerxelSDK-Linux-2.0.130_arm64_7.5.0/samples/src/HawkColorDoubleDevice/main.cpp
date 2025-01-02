#include <stdio.h>
#include <string.h>
#include <queue>

// Berxel Head File
#include "BerxelHawkContext.h"
#include "BerxelHawkDevice.h"
#include "BerxelHawkFrame.h"
#include "BerxelHawkDefines.h"
#include "BerxelCommonFunc.h"
#include "BerxelImageRender.h"

#ifdef WIN32
#include <Windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

berxel::BerxelHawkContext*   g_context = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice1 = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo1;
BerxelImageRender*			 g_pImageRender = NULL;

static int  g_imageWidth	 = 400;
static int  g_imageHeight = 640;
static char g_errMsg[256] = {0};
static bool g_bStartStream = false;
static bool g_bSave = false;
static char g_deviceShowInfo[256];

static bool renderImage()
{	
	static char       mesgInfo[256] = {0};
	static char       mMesgInfo1[256] = {0};
	static RGB888	  rgbImage[1920 * 1080];
	static RGB888	  rgbImage1[1920 * 1080];

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
	int32_t ret = 0;
	if(g_pHawkDevice)
	{
		ret = g_pHawkDevice->readColorFrame(pHawkFrame,30);
		if (pHawkFrame == NULL)
		{
			return false;
		}
	}
	if(g_pHawkDevice1)
	{
		ret = g_pHawkDevice1->readColorFrame(pHawkFrame1,30);
		if (pHawkFrame1 == NULL)
		{
			g_pHawkDevice->releaseFrame(pHawkFrame);
			return false;
		}
	}
	if (pHawkFrame != NULL)
	{
		memcpy(rgbImage, pHawkFrame->getData(), pHawkFrame->getDataSize());

		memset(mesgInfo ,0 ,sizeof(mesgInfo));
		sprintf(mesgInfo , "%d*%d@fps %d" ,pHawkFrame->getWidth() , pHawkFrame->getHeight() ,pHawkFrame->getFPS());
	}
	if (pHawkFrame1 != NULL)
	{
		memcpy(rgbImage1, pHawkFrame1->getData(), pHawkFrame1->getDataSize());
		memset(mMesgInfo1 ,0 ,sizeof(mMesgInfo1));
		sprintf(mMesgInfo1 , "%d*%d@fps %d" ,pHawkFrame1->getWidth() , pHawkFrame1->getWidth() ,pHawkFrame1->getFPS());
	}

	g_pImageRender->initView();
	g_pImageRender->drawLine(0,35, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(mesgInfo,5,25 , (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo, 5, g_imageHeight + 40 + 25 , (void *)0x0007);
	WinRect rect(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImage,pHawkFrame->getWidth(), pHawkFrame->getHeight(),  rect);
	rect.x += g_imageWidth + 2;
	g_pImageRender->drawColorImage((uint8_t*)rgbImage1, pHawkFrame1->getWidth(), pHawkFrame1->getHeight(),  rect);
	g_pImageRender->updateView();
	
	// free frame
	if (pHawkFrame != NULL)
		g_pHawkDevice->releaseFrame(pHawkFrame);
	if (pHawkFrame1 != NULL)
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
		g_pHawkDevice->stopStreams(berxel::BERXEL_HAWK_COLOR_STREAM);
	}

	if (g_pHawkDevice1)
	{
		g_pHawkDevice1->stopStreams(berxel::BERXEL_HAWK_COLOR_STREAM);
	}

	if(g_context)
	{
		g_context->closeDevice(g_pHawkDevice);
		g_context->closeDevice(g_pHawkDevice1);
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
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkColor", g_imageWidth * 2 + 80 , g_imageHeight + 80); // window title & size
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
		sprintf(g_errMsg,"%s", "Open Berxel Device Failed");
        return creatWindow(argc ,argv);
    }
	g_pHawkDevice1 = g_context->openDevice(g_CurrentDeviceInfo1);
	if(NULL == g_pHawkDevice1)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Device 1 Failed");
		return creatWindow(argc ,argv);
	}

	//同步当前系统时钟到设备中
	g_pHawkDevice->setSystemClock();
	g_pHawkDevice1->setSystemClock();


	//设置流模式
	g_pHawkDevice->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

	//设置分辨率
    berxel::BerxelHawkStreamFrameMode frameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM ,&frameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM, &frameMode);

	// 打开数据流
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_COLOR_STREAM);
	if(ret != 0)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Stream Failed");
		return creatWindow(argc ,argv);
	}

	//设置流模式
	g_pHawkDevice1->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

	//设置分辨率
	berxel::BerxelHawkStreamFrameMode frameMode1;
	g_pHawkDevice1->getCurrentFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM ,&frameMode1);
	g_pHawkDevice1->setFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM, &frameMode1);

	if (frameMode.resolutionX < frameMode.resolutionY)
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
	ret = g_pHawkDevice1->startStreams(berxel::BERXEL_HAWK_COLOR_STREAM);
	if(ret != 0)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Stream Failed");
		return creatWindow(argc ,argv);
	}
	
	g_bStartStream = true;

	berxel::BerxelHawkDeviceInfo tempCurInfo;
	berxel::BerxelHawkVersions   tempVersions;
	g_pHawkDevice->getCurrentDeviceInfo(&tempCurInfo);
	g_pHawkDevice->getVersion(&tempVersions);
	sprintf(g_deviceShowInfo , "SN :%s  SDK(%d.%d.%d) FW(%d.%d.%d) HW(%d.%d.%d) " ,tempCurInfo.serialNumber , tempVersions.sdkVersion.major ,tempVersions.sdkVersion.minor, tempVersions.sdkVersion.revision ,
		tempVersions.fwVersion.major,tempVersions.fwVersion.minor,tempVersions.fwVersion.revision,tempVersions.hwVersion.major, tempVersions.hwVersion.minor, tempVersions.hwVersion.revision);
	creatWindow(argc,argv);

	return Exit();
}
