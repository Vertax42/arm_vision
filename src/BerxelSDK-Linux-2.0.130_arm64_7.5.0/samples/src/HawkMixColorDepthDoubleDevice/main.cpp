#include <fstream>
#include <stdio.h>
#include <string.h>
// Berxel Head File
#include "BerxelHawkContext.h"
#include "BerxelHawkDevice.h"
#include "BerxelHawkFrame.h"
#include "BerxelHawkDefines.h"
#include "BerxelCommonFunc.h"
#include "BerxelImageRender.h"

using namespace std;
//using namespace berxel;

berxel::BerxelHawkContext*	 g_context = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice1 = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo1;
BerxelImageRender*			 g_pImageRender = NULL;

static int  g_imageWidth	 = 400;
static int  g_imageHeight = 640;
static char g_errMsg[256] = {0};
static char g_deviceShowInfo[256];
static char deviceShowInfo1[256];
static bool g_bStartStream = false;
static bool g_bSave = false;

static bool renderImage()
{
	static char mesgInfo[256] = { 0 };
	static char mMesgInfo1[256] = { 0 };
	static RGB888		rgbImageColor[1920 * 1080];	
	static RGB888		rgbImageDepth[800 * 1280];
	static RGB888		rgbImageColor1[1920 * 1080];	
	static RGB888		rgbImageDepth1[800 * 1280];
	berxel::BerxelHawkFrame *pHawkColorFrame = NULL;
	berxel::BerxelHawkFrame *pHawkDepthFrame = NULL;

	if(false == g_bStartStream)
	{
		g_pImageRender->initView();
		g_pImageRender->drawLine(0,35, g_imageWidth * 3+ 80 ,40);
		g_pImageRender->drawString(g_errMsg, 5, 25 , (void *)0x0008);
		g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth * 3 + 80 ,40);
		g_pImageRender->updateView();
		return false;
	}

	if(g_pHawkDevice)
	{	
		g_pHawkDevice->readDepthFrame(pHawkDepthFrame,30);
		if(pHawkDepthFrame == NULL )
		{
			return false;
		}

		g_pHawkDevice->readColorFrame(pHawkColorFrame,30);
		if(pHawkColorFrame == NULL)
		{
			g_pHawkDevice->releaseFrame(pHawkDepthFrame);
			return false;
		}

	}
	BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkDepthFrame->getData() ,(uint8_t *)rgbImageDepth, pHawkDepthFrame->getWidth() , pHawkDepthFrame->getHeight(),pHawkDepthFrame->getPixelType());
	memcpy(rgbImageColor,pHawkColorFrame->getData(),pHawkColorFrame->getDataSize());
	
	memset(mesgInfo ,0 ,sizeof(mesgInfo));
	sprintf(mesgInfo , "Color:%d*%d@fps %d Depth:%d*%d@fps %d" ,pHawkColorFrame->getWidth() ,pHawkColorFrame->getHeight(), pHawkColorFrame->getFPS(), pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight(), pHawkDepthFrame->getFPS());

	berxel::BerxelHawkFrame *pHawkColorFrame1 = NULL;
	berxel::BerxelHawkFrame *pHawkDepthFrame1 = NULL;

	if(g_pHawkDevice1)
	{	
		g_pHawkDevice1->readDepthFrame(pHawkDepthFrame1,30);
		if(pHawkDepthFrame1 == NULL )
		{
			g_pHawkDevice->releaseFrame(pHawkColorFrame);
			g_pHawkDevice->releaseFrame(pHawkDepthFrame);
			return false;
		}
		g_pHawkDevice1->readColorFrame(pHawkColorFrame1,30);
		if(pHawkColorFrame1 == NULL)
		{
			g_pHawkDevice->releaseFrame(pHawkColorFrame);
			g_pHawkDevice->releaseFrame(pHawkDepthFrame);
			g_pHawkDevice1->releaseFrame(pHawkDepthFrame1);
			return false;
		}

	}

	BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkDepthFrame1->getData() ,(uint8_t *)rgbImageDepth1, pHawkDepthFrame1->getWidth() , pHawkDepthFrame1->getHeight(), pHawkDepthFrame1->getPixelType());
	memcpy(rgbImageColor1, pHawkColorFrame1->getData(), pHawkColorFrame1->getDataSize());



	static int index = 0;
	if (g_bSave)
	{
		index++;
		
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t *)pHawkColorFrame->getData(), pHawkColorFrame->getDataSize(), "Color_1", index);
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t *)pHawkColorFrame1->getData(), pHawkColorFrame->getDataSize(), "Color_2", index);

		
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t *)pHawkDepthFrame->getData(), pHawkDepthFrame->getDataSize(), "Depth_1", index);
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t *)pHawkDepthFrame1->getData(), pHawkDepthFrame1->getDataSize(), "Depth_2", index);

		BerxelCommonFunc::getInstance()->takePhoto("Color_1", index, (uint8_t *)rgbImageColor, pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight());
		BerxelCommonFunc::getInstance()->takePhoto("Color_2", index, (uint8_t *)rgbImageColor1, pHawkDepthFrame1->getWidth(), pHawkDepthFrame1->getHeight());

		BerxelCommonFunc::getInstance()->takePhoto("Depth_1", index, (uint8_t *)rgbImageDepth, pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight());
		BerxelCommonFunc::getInstance()->takePhoto("Depth_2", index, (uint8_t *)rgbImageDepth1, pHawkDepthFrame1->getWidth(), pHawkDepthFrame1->getHeight());
		{

			static berxel::BerxelHawkPoint3D point3D[1280 * 800];
			g_pHawkDevice->convertDepthToPointCloud(pHawkDepthFrame, 1000.0, point3D);
			char filename[128] = { 0 };
			sprintf(filename, "berxelPoint3D_1_%d.ply", index);
			ofstream fout(filename, ios::binary);
			fout << "ply" << "\r\n";
			fout << "format ascii 1.0" << "\r\n";
			fout << "element vertex "<< pHawkDepthFrame->getWidth() * pHawkDepthFrame->getHeight() << "\r\n";
			fout << "property float x" << "\r\n";
			fout << "property float y" << "\r\n";
			fout << "property float z" << "\r\n";
			fout << "end_header" << "\r\n";
			int pcdCount = pHawkDepthFrame->getDataSize() / 2;
			for (int i = 0; i < pcdCount; ++i)
			{
				fout << point3D[i].x << " " << point3D[i].y << " " << point3D[i].z << "\r\n";
			}
			fout.close();

		}
		{

			static berxel::BerxelHawkPoint3D point3D[1280 * 800];
			g_pHawkDevice->convertDepthToPointCloud(pHawkDepthFrame1, 1000.0, point3D);
			char filename[128] = { 0 };
			sprintf(filename, "berxelPoint3D_2_%d.ply", index);
			ofstream fout(filename, ios::binary);
			fout << "ply" << "\r\n";
			fout << "format ascii 1.0" << "\r\n";
			fout << "element vertex " << pHawkDepthFrame1->getWidth() * pHawkDepthFrame1->getHeight() << "\r\n";
			fout << "property float x" << "\r\n";
			fout << "property float y" << "\r\n";
			fout << "property float z" << "\r\n";
			fout << "end_header" << "\r\n";
			int pcdCount = pHawkDepthFrame->getDataSize() / 2;
			for (int i = 0; i < pcdCount; ++i)
			{
				fout << point3D[i].x << " " << point3D[i].y << " " << point3D[i].z << "\r\n";
			}
			fout.close();

		}

		g_bSave = false;
	}

	memset(mMesgInfo1 ,0 ,sizeof(mMesgInfo1));
	sprintf(mMesgInfo1 , "Color:%d*%d@fps %d Depth:%d*%d@fps %d" ,pHawkColorFrame1->getWidth() ,pHawkColorFrame1->getHeight(), pHawkColorFrame1->getFPS(), pHawkDepthFrame1->getWidth(), pHawkDepthFrame1->getHeight(), pHawkDepthFrame1->getFPS());

	g_pImageRender->initView();
	g_pImageRender->drawLine(0,	35, g_imageWidth*2 + 80, 40);
	g_pImageRender->drawString(mesgInfo, 5, 25, (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight  + 45, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo, 5, g_imageHeight + 40 + 25 , (void *)0x0007);
	g_pImageRender->drawLine(0,	g_imageHeight + 115, g_imageWidth*2 + 80, 40);
	g_pImageRender->drawString(mMesgInfo1, 5, g_imageHeight + 105, (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight * 2 + 125, g_imageWidth * 2 + 80 ,40);
	g_pImageRender->drawString(deviceShowInfo1, 5, g_imageHeight * 2 + 145 , (void *)0x0007);
	WinRect rect(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImageColor, pHawkColorFrame->getWidth(), pHawkColorFrame->getHeight(), rect);
	rect.x += g_imageWidth + 2;
	g_pImageRender->drawColorImage((uint8_t*)rgbImageDepth, pHawkDepthFrame->getWidth(),  pHawkDepthFrame->getHeight(), rect);
	g_pImageRender->drawDepthValue(pHawkDepthFrame, rect);
	rect.x = 40;
	rect.y = g_imageHeight + 120;
	g_pImageRender->drawColorImage((uint8_t*)rgbImageColor1, pHawkColorFrame1->getWidth(), pHawkColorFrame1->getHeight(), rect);
	rect.x += g_imageWidth + 2;
	g_pImageRender->drawColorImage((uint8_t*)rgbImageDepth1, pHawkDepthFrame1->getWidth(),  pHawkDepthFrame1->getHeight(), rect);
	g_pImageRender->drawDepthValue(pHawkDepthFrame1, rect);
	g_pImageRender->updateView();
	
	// free frame
	g_pHawkDevice->releaseFrame(pHawkColorFrame);
	g_pHawkDevice->releaseFrame(pHawkDepthFrame);
	g_pHawkDevice1->releaseFrame(pHawkColorFrame1);
	g_pHawkDevice1->releaseFrame(pHawkDepthFrame1);

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
		g_pHawkDevice->stopStreams(berxel::BERXEL_HAWK_COLOR_STREAM | berxel::BERXEL_HAWK_DEPTH_STREAM);
	}

	if(g_pHawkDevice1)
	{
		g_pHawkDevice1->stopStreams(berxel::BERXEL_HAWK_COLOR_STREAM | berxel::BERXEL_HAWK_DEPTH_STREAM);
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
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkColorDepth", g_imageWidth * 2 + 80 , g_imageHeight * 2 + 160); // window title & size
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
	g_pHawkDevice->setStreamFlagMode(berxel::BERXEL_HAWK_MIX_STREAM_FLAG_MODE);

	//设置分辨率
    berxel::BerxelHawkStreamFrameMode colorFrameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM , &colorFrameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM, &colorFrameMode);
	
	berxel::BerxelHawkStreamFrameMode depthFrameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM , &depthFrameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &depthFrameMode);

	// 打开数据流
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_COLOR_STREAM | berxel::BERXEL_HAWK_DEPTH_STREAM);
	if(ret != 0)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Stream Failed");
		return creatWindow(argc ,argv);
	}

	//设置流模式
	g_pHawkDevice1->setStreamFlagMode(berxel::BERXEL_HAWK_MIX_STREAM_FLAG_MODE);

	//设置分辨率
	berxel::BerxelHawkStreamFrameMode colorFrameMode1;
	g_pHawkDevice1->getCurrentFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM , &colorFrameMode1);
	g_pHawkDevice1->setFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM, &colorFrameMode1);

	berxel::BerxelHawkStreamFrameMode depthFrameMode1;
	g_pHawkDevice1->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM , &depthFrameMode1);
	g_pHawkDevice1->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &depthFrameMode1);


	g_pHawkDevice->setRegistrationEnable(true);
	g_pHawkDevice1->setRegistrationEnable(true);

	if (depthFrameMode1.resolutionX < depthFrameMode1.resolutionY)
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
	ret = g_pHawkDevice1->startStreams(berxel::BERXEL_HAWK_COLOR_STREAM | berxel::BERXEL_HAWK_DEPTH_STREAM);
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

	berxel::BerxelHawkDeviceInfo tempCurInfo1;
	berxel::BerxelHawkVersions   tempVersions1;
	g_pHawkDevice1->getCurrentDeviceInfo(&tempCurInfo1);
	g_pHawkDevice1->getVersion(&tempVersions1);
	sprintf(deviceShowInfo1 , "SN :%s  SDK(%d.%d.%d) FW(%d.%d.%d) HW(%d.%d.%d) " ,tempCurInfo1.serialNumber , tempVersions1.sdkVersion.major ,tempVersions1.sdkVersion.minor, tempVersions1.sdkVersion.revision ,
		tempVersions.fwVersion.major,tempVersions1.fwVersion.minor,tempVersions1.fwVersion.revision,tempVersions1.hwVersion.major, tempVersions1.hwVersion.minor, tempVersions1.hwVersion.revision);

	creatWindow(argc,argv);
	return Exit();
}
