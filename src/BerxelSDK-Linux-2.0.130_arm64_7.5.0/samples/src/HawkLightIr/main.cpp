#include <stdio.h>
#include <string.h>
// Berxel Head File
#include "BerxelHawkContext.h"
#include "BerxelHawkDevice.h"
#include "BerxelHawkFrame.h"
#include "BerxelHawkDefines.h"
#include "BerxelCommonFunc.h"
#include "BerxelImageRender.h"

//using namespace berxel;

berxel::BerxelHawkContext*	 g_context = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice = NULL;
BerxelImageRender*			 g_pImageRender = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;

static int  g_imageWidth  = 400;
static int  g_imageHeight = 640;
static char g_errMsg[256] = { 0 };
static char g_deviceShowInfo[256] = { 0 };
static bool g_bStartStream = false;
static bool g_bSave = false;

static bool renderImage()
{
	static char mesgInfo[256] = { 0 };
	static RGB888	rgbImage[1280 * 800] = { 0 };
	berxel::BerxelHawkFrame *pHawkFrame = NULL;

	if(false == g_bStartStream)
	{
		g_pImageRender->initView();
		g_pImageRender->drawLine(0,35, g_imageWidth + 80 ,40);
		g_pImageRender->drawString(g_errMsg, 5, 25 , (void *)0x0008);
		g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth + 80 ,40);
		g_pImageRender->updateView();
		return false;
	}
	
	if(g_pHawkDevice)
	{
		g_pHawkDevice->readLightIrFrame(pHawkFrame,30);
	}	
	if(pHawkFrame == NULL)
	{
		return false;
	}

	BerxelCommonFunc::getInstance()->convertIrToRGB((uint16_t*)pHawkFrame->getData(), rgbImage, pHawkFrame->getWidth(), pHawkFrame->getHeight());

	static int index = 0;
	if(g_bSave)
	{
		index ++;
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t*)pHawkFrame->getData(), pHawkFrame->getDataSize(),"LightIr", index);
		BerxelCommonFunc::getInstance()->takePhoto("LightIr", index, (uint8_t *)rgbImage, pHawkFrame->getWidth(), pHawkFrame->getHeight());
		g_bSave = false;
	}

	memset(mesgInfo ,0 ,sizeof(mesgInfo));
	sprintf(mesgInfo , "%d*%d@fps %d" ,pHawkFrame->getWidth() , pHawkFrame->getHeight() , pHawkFrame->getFPS());

	// Render
	g_pImageRender->initView();	
	g_pImageRender->drawLine(0,35, g_imageWidth + 80 ,40);
	g_pImageRender->drawString(mesgInfo ,5, 25, (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo ,5,g_imageHeight + 40 + 25 , (void *)0x0007);
	WinRect hint(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImage,  pHawkFrame->getWidth() , pHawkFrame->getHeight() , hint);
	g_pImageRender->updateView();

	// free frame
	g_pHawkDevice->releaseFrame(pHawkFrame);
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
		g_pHawkDevice->stopStreams(berxel::BERXEL_HAWK_LIGHT_IR_STREAM);
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
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel Hawk Light IR", g_imageWidth + 80 , g_imageHeight + 80); // window title & size
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
    if((deviceCount <= 0) || (NULL == pDeviceInfo))
	{
		sprintf(g_errMsg,"%s", "Get No Connected BerxelDevice");
		return creatWindow(argc ,argv);
    }

	g_pHawkDevice = g_context->openDevice(pDeviceInfo[0]);
    if(NULL == g_pHawkDevice)
	{
		sprintf(g_errMsg,"%s", "Open BerxelDevice Failed");
        return creatWindow(argc ,argv);
    }

	g_CurrentDeviceInfo = pDeviceInfo[0];

	//同步当前系统时钟到设备中
	g_pHawkDevice->setSystemClock();

	//设置流模式
	g_pHawkDevice->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

	//设置分辨率
    berxel::BerxelHawkStreamFrameMode frameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_LIGHT_IR_STREAM, &frameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_LIGHT_IR_STREAM, &frameMode);
	if (g_CurrentDeviceInfo.vendorId == 0x0603 && g_CurrentDeviceInfo.productId == 0x0005)
	{
		g_imageWidth = 568;
		g_imageHeight = 350;
	}
	else
	{
		if (frameMode.resolutionX < frameMode.resolutionY)
		{
			g_imageWidth = 400;
			g_imageHeight = 640;
		}
		else
		{
			g_imageWidth = 640;
			g_imageHeight = 400;
		}
	}

	// 打开数据流
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_LIGHT_IR_STREAM);
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
