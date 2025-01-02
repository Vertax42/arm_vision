
#include <stdio.h>
#include <string.h>

// Berxel Head File
#include "BerxelHawkContext.h"
#include "BerxelHawkDevice.h"
#include "BerxelHawkFrame.h"
#include "BerxelHawkDefines.h"
#include "BerxelCommonFunc.h"
#include "BerxelImageRender.h"
#include <mutex>

#include <fstream>
using namespace std;
berxel::BerxelHawkContext*   g_context		= NULL;
berxel::BerxelHawkDevice*    g_pHawkDevice[6] = {NULL};
berxel::BerxelHawkDeviceInfo g_CurrentDeviceInfo[6];

BerxelImageRender*			 g_pImageRender  = NULL;

bool g_bSave = false;
bool g_bAEStatus = false;

static int  IMAGE_WIDTH	 = 640;
static int  IMAGE_HEIGHT = 400;
static int  nDevCount = 0;
static int  nDevFps = 0;
static int  g_bSlaveDevMode[6] = {false};
bool g_bOpenStream[6] = { false };
bool g_bDeviceStatus[6] = { false };
static char errMsg[256] = {0};
bool g_bStartStream = false; 
static char deviceShowInfo[6][256];
static uint32_t g_exposure_time = 10;
std::mutex m_mutex;

static bool renderImage()
{
	static char mMesgInfo[6][256] = {0};
	static RGB888			s_rgbImage[6][1280 * 800];
	//static RGB888			s_rgbImage1[1280 * 800];
	static int s_nfps[6] = {0};
	if(false == g_bStartStream)
	{
		g_pImageRender->initView();
		g_pImageRender->drawLine(0,35, IMAGE_WIDTH * nDevCount + 80 ,40);
		g_pImageRender->drawString(errMsg, 5, 25 , (void *)0x0008);

		g_pImageRender->drawLine(0,IMAGE_HEIGHT + 42, IMAGE_WIDTH * nDevCount + 80 ,40);
		g_pImageRender->updateView();

		return false;
	}
	m_mutex.lock();
	berxel::BerxelHawkFrame *pHawkFrame[6];
	for (int i = 0; i < nDevCount; i++)
	{
		memset(s_rgbImage[i], 0x00, 1280 * 800 * sizeof(RGB888));
		pHawkFrame[i] = NULL;
		if (g_pHawkDevice[i])
		{
			if (g_bDeviceStatus[i])
				g_pHawkDevice[i]->readDepthFrame(pHawkFrame[i], 30);
		}

		if (pHawkFrame[i] != NULL)
		{
			s_nfps[i] = pHawkFrame[i]->getFPS();
			BerxelCommonFunc::getInstance()->ImageScaleColor((uint8_t *)s_rgbImage[i], (uint16_t*)pHawkFrame[i]->getData(), pHawkFrame[i]->getWidth(), pHawkFrame[i]->getHeight(), 0, 4096, pHawkFrame[i]->getPixelType());
		}

		uint32_t nMode = 0;
		if (g_pHawkDevice[i])
		{
			g_pHawkDevice[i]->releaseFrame(pHawkFrame[i]);
			g_pHawkDevice[i]->getDeviceMasterSlaveMode(&nMode);
		}

		char slave_mode[16] = {0x00};
		sprintf(slave_mode, g_bSlaveDevMode[i] ? "ON" : "OFF");

		memset(mMesgInfo[i], 0, sizeof(mMesgInfo));
		sprintf(mMesgInfo[i], "%d*%d@FPS %d@SLAVE MODE:%s", IMAGE_WIDTH, IMAGE_HEIGHT, s_nfps[i], slave_mode);
	}

	m_mutex.unlock();

	//Render
	g_pImageRender->initView();
	int n = nDevCount < 3 ? nDevCount : 3;
	int q = nDevCount > 3 ? 2 : 1;

	for (int i = 0; i < q; i++)
	{
		g_pImageRender->drawLine(0, (IMAGE_HEIGHT + 80) * i + 35, IMAGE_WIDTH * n + 80, 40);
		g_pImageRender->drawLine(0, IMAGE_HEIGHT * (i + 1) + 80 * i + 42, IMAGE_WIDTH * n + 80, 40);
	}

	WinRect rect(40, 40, IMAGE_WIDTH, IMAGE_HEIGHT);
	WinRect rect_bootom(40, IMAGE_HEIGHT + 120, IMAGE_WIDTH, IMAGE_HEIGHT);
	for (int i = 0; i < nDevCount; i++)
	{
		if (i < 3)
		{
			if (i != 0)
				rect.x += IMAGE_WIDTH + 2;

			g_pImageRender->drawString(mMesgInfo[i], rect.x + 5, 25, (void *)0x0008);
			g_pImageRender->drawString(deviceShowInfo[i], rect.x + 5, IMAGE_HEIGHT + 40 + 25, (void *)0x0007);
			g_pImageRender->drawColorImage((uint8_t*)s_rgbImage[i], IMAGE_WIDTH, IMAGE_HEIGHT, rect);
		}
		else
		{
			if (i != 3)
				rect_bootom.x += IMAGE_WIDTH + 2;
			g_pImageRender->drawString(mMesgInfo[i], rect_bootom.x + 5, IMAGE_HEIGHT + 105, (void *)0x0008);
			g_pImageRender->drawString(deviceShowInfo[i], rect_bootom.x + 5, IMAGE_HEIGHT * 2 + 145, (void *)0x0007);
			g_pImageRender->drawColorImage((uint8_t*)s_rgbImage[i], IMAGE_WIDTH, IMAGE_HEIGHT, rect_bootom);
		}
	}
	
	g_pImageRender->updateView();
	
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
	case 'o':
	case 'O':
		{
			g_bAEStatus = !g_bAEStatus;
			for (int i = 0; i < nDevCount; i++)
			{
				g_pHawkDevice[i]->setDepthAEStatus(g_bAEStatus);
			}
		}
		break;
	case 'u':
	case 'U':
		{
			g_exposure_time++;
			if (g_exposure_time > 43)
				g_exposure_time = 43;

			printf("Set Exposure Time : %d\n", g_exposure_time);
			for (int i = 0; i < nDevCount; i++)
			{
				if (g_pHawkDevice[i])
				{
					g_pHawkDevice[i]->setDepthExposure(g_exposure_time);
				}
			}
		}
		break;
	case 'i':
	case 'I':
		{
			g_exposure_time--;
			if (g_exposure_time < 1)
				g_exposure_time = 1;
			printf("Set Exposure Time : %d\n", g_exposure_time);
			for (int i = 0; i < nDevCount; i++)
			{
				if (g_pHawkDevice[i])
				{
					g_pHawkDevice[i]->setDepthExposure(g_exposure_time);
				}
			}
		}
		break;
	case 'p':
	case 'P':
		{
			static bool bValue = false;
			if (bValue)
				return;
			printf("################################\n");
			for (int i = 0; i < nDevCount; i++)
			{
				if (g_bSlaveDevMode[i] == false)
				{
					// 打开数据流
					int ret = g_pHawkDevice[i]->startStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
					if (ret != 0)
					{
						printf("Open Berxel Stream Failed SN:%s\n", g_CurrentDeviceInfo[i].serialNumber);
						g_bDeviceStatus[i] = false;
						//continue;
					}
				}
			}

			bValue = true;
		}
		break;
	default:
		printf("P or p ----> Open master mode stream\n");
		printf("U or u ----> exposure time add\n");
		printf("I or i ----> exposure time reduce\n");
		printf("O or o ----> set ae status\n");
		break;
	}
}

int Exit()
{
	for (int i = 0; i < nDevCount; i++)
	{
		if (g_pHawkDevice[i])
		{
			g_pHawkDevice[i]->stopStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
		}
		if (g_context)
		{
			g_context->closeDevice(g_pHawkDevice[i]);
		}
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
	int width = IMAGE_WIDTH * nDevCount + 80;
	int height = IMAGE_HEIGHT + 80;
	if (nDevCount > 3 && nDevCount <= 6)
	{
		width = IMAGE_WIDTH * 3 + 80;
		height = (IMAGE_HEIGHT + 80)* 2;
	}

	g_pImageRender = new BerxelImageRender(argc, argv, "Berxel HawkDepth", width, height); // window title & size
	g_pImageRender->setInfoCallback(renderImage , keyCallBack);
	g_pImageRender->startView();
	return 0;
}

static void _stdcall onDeviceStatusCallback(const char* deviceAddress, const char* deviceSerialNumber, berxel::BerxelHawkDeviceStatus deviceState, void* pUserData)
{
	if (deviceState == berxel::BERXEL_HAWK_DEVICE_CONNECT)
	{

	}
	else
	{
		m_mutex.lock();
		printf("Device : %s disconnected\n", deviceSerialNumber);
		for (int i = 0; i < nDevCount; i++)
		{
			if (strcmp(deviceSerialNumber, g_CurrentDeviceInfo[i].serialNumber) == 0)
			{
				g_bDeviceStatus[i] = false;
				//g_pHawkDevice[i]->stopStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
				//g_context->closeDevice(g_pHawkDevice[i]);
				g_pHawkDevice[i] = NULL;
				break;
			}
		}
		m_mutex.unlock();
	}
}

int main(int argc, char** argv)
{
	printf("Please Input Device Num : ");
	//int nCount = 0;
	scanf("%d", &nDevCount);

	printf("Device Support FPS : \n");
	printf("0 -> 5fps\n");
	printf("1 -> 10fps\n");
	printf("2 -> 15fps\n");
	printf("3 -> 20fps\n");
	printf("4 -> 25fps\n");
	printf("5 -> 30fps\n");

	printf("Please Input FPS : ");
	scanf("%d", &nDevFps);

	if (nDevFps == 0)
		nDevFps = 5;
	else if (nDevFps == 1)
		nDevFps = 10;
	else if (nDevFps == 2)
		nDevFps = 15;
	else if (nDevFps == 3)
		nDevFps = 20;
	else if (nDevFps == 4)
		nDevFps = 25;
	else
		nDevFps = 30;

	if (nDevCount <= 0)
	{
		sprintf(errMsg, "%s", "Input error!!!");
		return creatWindow(argc, argv);
	}

	//获取context
	g_context = berxel::BerxelHawkContext::getBerxelContext();

	g_context->setDeviceStateCallback(onDeviceStatusCallback, NULL);

	//打开设备
	berxel::BerxelHawkDeviceInfo* pDeviceInfo = NULL;
	uint32_t deviceCount = 0;
	g_context->getDeviceList(&pDeviceInfo, &deviceCount);
    if((deviceCount < nDevCount) || (NULL == pDeviceInfo))
	{
		sprintf(errMsg,"%s", "Get No Enough Connected BerxelDevice");
		return creatWindow(argc ,argv);
    }

	for (int i = 0; i < nDevCount; i++)
	{
		g_CurrentDeviceInfo[i] = pDeviceInfo[i];
		g_pHawkDevice[i] = g_context->openDevice(g_CurrentDeviceInfo[i]);
		if (NULL == g_pHawkDevice[i])
		{
			printf("Open BerxelDevice Failed SN:%s", g_CurrentDeviceInfo[i].serialNumber);
			g_bDeviceStatus[i] = false;
			continue;
		}
		g_bDeviceStatus[i] = true;
		uint32_t nMode = 0;
		g_pHawkDevice[i]->getDeviceMasterSlaveMode(&nMode);
		g_bSlaveDevMode[i] = nMode == 1 ? true : false;

		g_pHawkDevice[i]->setDepthAEStatus(g_bAEStatus);

		//同步当前系统时钟到设备中
		g_pHawkDevice[i]->setSystemClock();

		//设置流模式
		g_pHawkDevice[i]->setStreamFlagMode(berxel::BERXEL_HAWK_SINGULAR_STREAM_FLAG_MODE);

		//设置分辨率
		berxel::BerxelHawkStreamFrameMode frameMode;
		g_pHawkDevice[i]->getCurrentFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode);
		frameMode.framerate = nDevFps;
		g_pHawkDevice[i]->setFrameMode(berxel::BERXEL_HAWK_DEPTH_STREAM, &frameMode);
		
		g_exposure_time = g_pHawkDevice[i]->getDepthExposure(&g_exposure_time);

		if (g_bSlaveDevMode[i])
		{
			// 打开数据流
			int ret = g_pHawkDevice[i]->startStreams(berxel::BERXEL_HAWK_DEPTH_STREAM);
			if (ret != 0)
			{
				printf("Open Berxel Stream Failed SN:%s\n", g_CurrentDeviceInfo[i].serialNumber);
				g_bOpenStream[i] = false;
				continue;
			}
		}

		g_bOpenStream[i] = true;

		

		berxel::BerxelHawkDeviceInfo tempCurInfo;
		berxel::BerxelHawkVersions   tempVersions;

		g_pHawkDevice[i]->getCurrentDeviceInfo(&tempCurInfo);
		g_pHawkDevice[i]->getVersion(&tempVersions);

		sprintf(deviceShowInfo[i], "SN :%s  SDK(%d.%d.%d) FW(%d.%d.%d-%s) HW(%d.%d.%d) ", tempCurInfo.serialNumber, tempVersions.sdkVersion.major, tempVersions.sdkVersion.minor, tempVersions.sdkVersion.revision,
			tempVersions.fwVersion.major, tempVersions.fwVersion.minor, tempVersions.fwVersion.revision, tempVersions.fwVersion.chipVersion, tempVersions.hwVersion.major, tempVersions.hwVersion.minor, tempVersions.hwVersion.revision);
	}

	g_bStartStream = true;	
	creatWindow(argc,argv);
	return Exit();
}
