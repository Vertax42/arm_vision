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

typedef struct _BerxelRect
{
	int32_t startX;
	int32_t startY;
	int32_t endX;
	int32_t endY;
}BerxelRect;

berxel::BerxelHawkContext*   g_context		= NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice  = NULL;
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo;
BerxelImageRender*			 g_pImageRender  = NULL;

static int  g_imageWidth= 640;
static int  g_imageHeight = 400;
static char g_errMsg[256] = {0};
static char g_deviceShowInfo[256];
static bool g_bColorDepth = true;
static bool g_bTemtureOpen = false;
static bool g_bSave = false;
static bool g_bStartStream = false;
static BerxelRect g_berxelRect;
static vector<BerxelRect> g_vecRect;
static bool g_bClearVect = false;
static bool g_bAddRect = false;

double calcAverageDepth(berxel::BerxelHawkFrame* pFrame, BerxelRect berxelRect)
{
	float averageDepthValue = 0;
	int32_t totalCount = 0;

	if (pFrame != NULL)
	{
		uint16_t* pData = (uint16_t*)pFrame->getData();

		int maxX = berxelRect.startX > berxelRect.endX ? berxelRect.startX : berxelRect.endX;
		int minX = berxelRect.startX <= berxelRect.endX ? berxelRect.startX : berxelRect.endX;
		int maxY = berxelRect.startY > berxelRect.endY ? berxelRect.startY : berxelRect.endY;
		int minY = berxelRect.startY <= berxelRect.endY ? berxelRect.startY : berxelRect.endY;

		for (int i= minY; i<maxY; i++)
		{
			for (int j= minX; j<maxX; j++)
			{
				int32_t index = i * pFrame->getWidth() + j;
				if (pData[index] > 0)
				{
					totalCount++;
					if (pFrame->getPixelType() == berxel::BERXEL_HAWK_PIXEL_TYPE_DEP_16BIT_13I_3D)
						averageDepthValue += ((pData[index] *1.0)/8.0);
					else
						averageDepthValue += ((pData[index] *1.0)/16.0);
				}
			}
		}
	}

	if (totalCount == 0)
	{
		return 0;
	}

	return (double)averageDepthValue/(double)totalCount;
}

static bool renderImage()
{
	static char mesgInfo[256] = {0};
	static RGB888			rgbImage[1280 * 800];
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
		int ret = g_pHawkDevice->readDepthFrame(pHawkFrame,30);
	}
		
	if(pHawkFrame == NULL)
	{
		return false;
	}

	if (g_bColorDepth)
	{
		BerxelCommonFunc::getInstance()->ImageScaleColor((uint16_t*)pHawkFrame->getData(), (uint8_t*)rgbImage,  pHawkFrame->getWidth(), pHawkFrame->getHeight(), pHawkFrame->getPixelType());
	}
	else
	{
		BerxelCommonFunc::getInstance()->convertDepthToRgbByHist((uint16_t*)pHawkFrame->getData(), rgbImage, pHawkFrame->getWidth(), pHawkFrame->getHeight(), pHawkFrame->getPixelType());
	}

	static int index = 0;
	if(g_bSave)
	{
		index ++;
		BerxelCommonFunc::getInstance()->saveRawData((uint8_t*)pHawkFrame->getData(), pHawkFrame->getDataSize(),"Depth", index);
		BerxelCommonFunc::getInstance()->takePhoto("depth",index ,(uint8_t *)rgbImage,pHawkFrame->getWidth(),pHawkFrame->getHeight());

		static berxel::BerxelHawkPoint3D point3D[1280 * 800];
		g_pHawkDevice->convertDepthToPointCloud(pHawkFrame,1000.0, point3D);
		char filename[128] = {0};
		sprintf(filename, "berxelPoint3D_%d.ply", index);
		ofstream fout(filename, ios::binary);
		fout<<"ply"<<"\r\n";	
		fout<<"format ascii 1.0"<<"\r\n";
		fout << "element vertex "<< pHawkFrame->getWidth() * pHawkFrame->getHeight() << "\r\n";
		fout<<"property float x"<<"\r\n";
		fout<<"property float y"<<"\r\n";
		fout<<"property float z"<<"\r\n";
		fout<<"end_header"<<"\r\n";
		int pcdCount = pHawkFrame->getDataSize() / 2;
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
	sprintf(temStatus, g_bTemtureOpen ? "ON" : "OFF");
	memset(mesgInfo ,0 ,sizeof(mesgInfo));
	sprintf(mesgInfo , "%d*%d@fps %d###Tempture %d####Status %s" ,pHawkFrame->getWidth() , pHawkFrame->getHeight() ,pHawkFrame->getFPS(), nTemp, temStatus);

	if (g_bAddRect)
	{
		g_vecRect.push_back(g_berxelRect);
		g_bAddRect = false;
	}
	if (g_bClearVect)
	{
		g_bClearVect = false;
		g_vecRect.clear();
	}

	//Render
	g_pImageRender->initView();
	g_pImageRender->drawLine(0,35, g_imageWidth + 80 ,40);
	g_pImageRender->drawString(mesgInfo ,5, 25 , (void *)0x0008);
	g_pImageRender->drawLine(0,g_imageHeight + 42, g_imageWidth + 80 ,40);
	g_pImageRender->drawString(g_deviceShowInfo ,5,g_imageHeight + 40 + 25 , (void *)0x0007);
	WinRect rect(40, 40, g_imageWidth, g_imageHeight);
	g_pImageRender->drawColorImage((uint8_t*)rgbImage, pHawkFrame->getWidth() , pHawkFrame->getHeight(), rect);
	g_pImageRender->drawDepthValue(pHawkFrame, rect);
	WinRect rect1(0, 0 ,0 ,0);
	for (int i = 0; i < g_vecRect.size(); i++)
	{
		g_pImageRender->drawRect(g_vecRect[i].startX + 40, g_vecRect[i].startY + 40, g_vecRect[i].endX - g_vecRect[i].startX, g_vecRect[i].endY - g_vecRect[i].startY, rect1);
		double meanDepth  = calcAverageDepth(pHawkFrame, g_vecRect[i]);
		char bufValue[64] = {0};
		sprintf(bufValue, "%.4f", meanDepth);
		g_pImageRender->drawString(bufValue, g_vecRect[i].startX + 35, g_vecRect[i].startY + 35, (void *)0x0008);
	}
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
		{
			g_bSave = true;
		}
		break;
	case 'O':
	case 'o':
		{	
			if (g_pHawkDevice) 
			{
				g_bTemtureOpen = !g_bTemtureOpen;
				g_pHawkDevice->setTemperatureCompensationEnable(g_bTemtureOpen);
			}
		}
		break;
	case 'L':
	case 'l':
		{
			g_bColorDepth = !g_bColorDepth;
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
	default:
		printf("Please input 's' or 'S' to save image; \n");
		printf("Please input 'o' or 'O' to open or close temperature compensation; \n");
		printf("Please input 'l' or 'L' to switch color for depth; \n");
		printf("Please input 'd' or 'D' to open denoise; \n");
		printf("Please input 'f' or 'F' to close denois; \n");
		break;
	}
}

void onMouseCallBack(int button, int state, int x, int y)
{
	if (button == GLUT_RIGHT_BUTTON)
	{
		static bool bValid = false;
		if (x < 40 || x > (40 + g_imageWidth) || y < 40 || y > (40 + g_imageHeight))
		{
			bValid = false;
			return;
		}

		if (state == GLUT_UP)
		{
			if (bValid == false)
				return;
			g_berxelRect.endX = x - 40;
			g_berxelRect.endY = y - 40;

			if (g_berxelRect.endX < g_berxelRect.startX)
			{
				int32_t temp = g_berxelRect.endX;
				g_berxelRect.endX = g_berxelRect.startX;
				g_berxelRect.startX = temp;

				temp = g_berxelRect.endY;
				g_berxelRect.endY = g_berxelRect.startY;
				g_berxelRect.startY = temp;
			}

			g_bAddRect = true;
		}
		else if (state == GLUT_DOWN)
		{
			g_berxelRect.startX = x - 40;
			g_berxelRect.startY = y - 40;
			bValid = true;
		}
	}
	else if (button == GLUT_MIDDLE_BUTTON)
	{
		g_bClearVect = true;
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
	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkDepth", g_imageWidth+ 80 , g_imageHeight + 80); // window title & size
	g_pImageRender->setInfoCallback(renderImage , keyCallBack);
	g_pImageRender->setMosueCallback(onMouseCallBack);
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
	int ret = g_pHawkDevice->startStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
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
