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
//using namespace berxel;

berxel::BerxelHawkContext*	 g_context = NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;
BerxelImageRender*			 g_pImageRender = NULL;

static int  g_imageWidth	 = 640;
static int  g_imageHeight = 400;
static char g_errMsg[256] = {0};
static char g_deviceShowInfo[256];
static bool g_bStartStream = false;
static bool g_bSave = false;
static bool g_bColorDepth = true;
static bool g_bTemtureOpen = false;

static bool renderImage()
{
	static char mesgInfo[256] = {0};
	static RGB888			rgbImageColor[1920 * 1080]  = { 0 };
	static RGB888			rgbImageDepth[800 * 1280] = { 0 };
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

	if (g_bColorDepth) 
	{		
		BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkDepthFrame->getData(), (uint8_t*)rgbImageDepth, pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight(),pHawkDepthFrame->getPixelType());
	}
	else 
	{
		BerxelCommonFunc::getInstance()->convertDepthToRgbByHist((uint16_t*)pHawkDepthFrame->getData(), rgbImageDepth, pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight(), pHawkDepthFrame->getPixelType());
	}
	memcpy(rgbImageColor,pHawkColorFrame->getData(),pHawkColorFrame->getDataSize());

	static int index = 0;
	if(g_bSave)
	{
		index ++;
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t * )pHawkColorFrame->getData(), pHawkColorFrame->getDataSize() ,   "Color",   index);
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t *) pHawkDepthFrame->getData(),  pHawkDepthFrame->getDataSize() ,   "Depth",  index);
	
		BerxelCommonFunc::getInstance()->takePhoto("Color",index ,(uint8_t *)rgbImageColor, pHawkColorFrame->getWidth(),pHawkColorFrame->getHeight());
		BerxelCommonFunc::getInstance()->takePhoto("Depth",index ,(uint8_t *)rgbImageDepth, pHawkDepthFrame->getWidth(),pHawkDepthFrame->getHeight());
	
		static berxel::BerxelHawkPoint3D point3D[1280 * 800];
		g_pHawkDevice->convertDepthToPointCloud(pHawkDepthFrame,1000.0, point3D);
		char filename[128] = {0};
		sprintf(filename, "berxelPoint3D_%d.ply", index);
		ofstream fout(filename, ios::binary);
		fout<<"ply"<<"\r\n";	
		fout<<"format ascii 1.0"<<"\r\n";
		fout << "element vertex " << pHawkDepthFrame->getWidth() * pHawkDepthFrame->getHeight() << "\r\n";
		fout<<"property float x"<<"\r\n";
		fout<<"property float y"<<"\r\n";
		fout<<"property float z"<<"\r\n";
		fout<<"end_header"<<"\r\n";
		int pcdCount = pHawkDepthFrame->getDataSize() / 2;
		for(int i = 0; i < pcdCount; ++i) 
		{	
			fout<< point3D[i].x << " " <<  point3D[i].y << " " <<  point3D[i].z << "\r\n";	
		}
		fout.close();

		g_bSave = false;
	}

	static int32_t nTemp = 0;
	static int nIndex = 0;
	if (nIndex++ > 90)
	{
		nIndex = 0;
		g_pHawkDevice->getDeviceTemperature(&nTemp);
	}

	char temStatus[4] = { 0 };
	if (g_bTemtureOpen)
	{
		sprintf(temStatus, "ON");
	}
	else
	{
		sprintf(temStatus, "OFF");
	}

	memset(mesgInfo ,0 ,sizeof(mesgInfo));
	sprintf(mesgInfo , "Color:%d*%d@fps %d Depth:%d*%d@fps %d###Tempture %d####Status %s" ,
		pHawkColorFrame->getWidth() ,pHawkColorFrame->getHeight(), pHawkColorFrame->getFPS(), pHawkDepthFrame->getWidth(), pHawkDepthFrame->getHeight(), pHawkDepthFrame->getFPS(), nTemp, temStatus);

	g_pImageRender->initView();
	g_pImageRender->drawLine(0,	35, g_imageWidth*3 + 80, 40);
	g_pImageRender->drawString(mesgInfo, 5, 25, (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight  + 45, g_imageWidth * 3 + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo, 5, g_imageHeight + 40 + 25 , (void *)0x0007);
	WinRect rect(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImageColor, pHawkColorFrame->getWidth(), pHawkColorFrame->getHeight(), rect);
	rect.x += g_imageWidth + 2;
	g_pImageRender->drawColorImage((uint8_t*)rgbImageDepth, pHawkDepthFrame->getWidth(),  pHawkDepthFrame->getHeight(), rect);
	g_pImageRender->drawDepthValue(pHawkDepthFrame, rect);
	g_pImageRender->updateView();
	
	// free frame
	g_pHawkDevice->releaseFrame(pHawkColorFrame);
	g_pHawkDevice->releaseFrame(pHawkDepthFrame);
   
	return true;
}

void keyCallBack(unsigned char key)
{
	switch (key)
	{
	case 'S':
	case 's':
	{
		g_bSave = true;
	}
	break;
	case 'O':
	case 'o':
	{
		if (g_bTemtureOpen == false)
		{
			if (g_pHawkDevice) 
			{
				g_bTemtureOpen = true;
				g_pHawkDevice->setTemperatureCompensationEnable(g_bTemtureOpen);
			}
		}
	}
	break;
	case 'P':
	case 'p':
	{
		if (g_bTemtureOpen)
		{
			if (g_pHawkDevice) 
			{
				g_bTemtureOpen = false;
				g_pHawkDevice->setTemperatureCompensationEnable(g_bTemtureOpen);
			}
		}
	}
	break;
	case 'D':
	case 'd':
		{
			if (g_pHawkDevice) 
			{
				g_pHawkDevice->setDenoiseStatus(true);
			}
		}
		break;
	case 'F':
	case 'f':
		{
			if (g_pHawkDevice) 
			{
				g_pHawkDevice->setDenoiseStatus(false);
			}
		}
		break;
	case 'E':
	case 'e':
	{
		if (g_pHawkDevice) 
		{
			g_pHawkDevice->setEdgeOptimizationStatus(true);
		}
	}
	break;
	case 'R':
	case 'r':
	{
		if (g_pHawkDevice) 
		{
			g_pHawkDevice->setEdgeOptimizationStatus(false);
		}
	}
	break;
	case 'L':
	case 'l':
	{
		g_bColorDepth = !g_bColorDepth;
	}
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
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkColorDepth", g_imageWidth * 2 + 80 , g_imageHeight + 80); // window title & size
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

	g_CurrentDeviceInfo = pDeviceInfo[0];

	g_pHawkDevice = g_context->openDevice(g_CurrentDeviceInfo);
    if(NULL == g_pHawkDevice)
	{
		sprintf(g_errMsg,"%s", "Open Berxel Device Failed");
        return creatWindow(argc ,argv);
    }

	//同步当前系统时钟到设备中
	g_pHawkDevice->setSystemClock();

	//设置流模式
	g_pHawkDevice->setStreamFlagMode(berxel::BERXEL_HAWK_MIX_STREAM_FLAG_MODE);

	//设置分辨率
    berxel::BerxelHawkStreamFrameMode colorFrameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM , &colorFrameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_COLOR_STREAM, &colorFrameMode);

	berxel::BerxelHawkStreamFrameMode depthFrameMode;
	g_pHawkDevice->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM , &depthFrameMode);
	g_pHawkDevice->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &depthFrameMode);

	if (g_CurrentDeviceInfo.vendorId == 0x0603 && g_CurrentDeviceInfo.productId == 0x0005)
	{
		g_imageWidth = 568;
		g_imageHeight = 350;
	}
	else
	{
		if (depthFrameMode.resolutionX < depthFrameMode.resolutionY)
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
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_COLOR_STREAM | berxel::BERXEL_HAWK_DEPTH_STREAM);
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

	//g_pHawkDevice->setRegistrationEnable(true);
	creatWindow(argc,argv);

	return Exit();
}
