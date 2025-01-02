#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <getopt.h> 

#include <vector>
#include <map>
#include "iniReader.h"

using namespace std;

#define MAX_INTERFACE	(16)
#define ETH_DEVCTRL_PROTOCOL_H2D        (0x7711)
#define ETH_DEVCTRL_PROTOCOL_D2H        (0x7712)

bool global_debugPrint_flag =true;
int r_sockfd =-1;
int s_sockfd =-1;
unsigned char ifaddr[6]={0};
unsigned char bcaddr[6]={0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char network_dev_name[16] = {0};
unsigned char _device_addr[6][6] = {0};
int _deviceCount = 0;
std::map<std::string, std::string> _host_mac_addr;

std::string _DeviceIp;
std::string _DeviceMask;
std::string _DeviceGatway;
std::string _HostMac;
int _dhcp;
typedef struct ETH_HEADER
{
	uint8_t   dest_mac[6];
	uint8_t   src_mac[6];
	uint16_t  etype;
}__attribute__((packed))ETH_HEADER;

enum eth_cmd_id{
	ETH_CMD_ID_NULL = 0,
	ETH_CMD_ID_SEARCH_DEV,
	ETH_CMD_ID_GET_DEV_NETCFG,
	ETH_CMD_ID_SET_DEV_NETCFG,
	ETH_CMD_ID_SET_DEV_REBOOT,
};

typedef struct _IP_CONFIG
{
	uint8_t  static_ip;    //0 dhcp client, 1 static ip
	uint32_t ip_addr;
	uint32_t net_mask; //子网掩码
	uint32_t gw_addr;	//网关
	uint32_t dns_addr;
	uint8_t  dhcps_enable; //dhcp server, 0 disable 1 enable
	uint32_t dhcps_saddr;	//dhcp server a 服务端地址
	uint32_t dhcps_eaddr;	//dhcp server e 服务端地址
	uint8_t  reserved[256 - 27];
	uint8_t user_setting;
}__attribute__((packed))IP_CONFIG;

typedef struct CMD_HEADER
{
    uint8_t   cmd_id;
    uint16_t  cmd_payload_len;
}__attribute__((packed))CMD_HEADER;

#define print(format,...)  if(global_debugPrint_flag){printf("%s--%s--%d: ",__FILE__,__FUNCTION__,__LINE__), printf(format,##__VA_ARGS__), printf("\n");}

int get_local_mac()
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq buf[MAX_INTERFACE];	
	struct ifconf ifc;
	int ret = 0;
	int if_num = 0;
 
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = (caddr_t) buf;
	
	ret = ioctl(fd, SIOCGIFCONF, (char*)&ifc);
	if(ret)
	{
		printf("get if config info failed");
		close(fd);
		return -1;
	}
	if_num = ifc.ifc_len/sizeof(struct ifreq);
	if (if_num <= 0)
	{
		printf("Can not find any mac device!");
		close(fd);
		return -1;
	}
	// printf("Local Mac Address : \n");
	for (int i = 0; i < if_num; i++)
	{
		/* 获取当前网卡的mac */
		ret = ioctl(fd, SIOCGIFHWADDR, (char*)&buf[i]);
		if(ret)
		{
			continue;
		}

		char mac[18] = {0};
		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)buf[i].ifr_hwaddr.sa_data[0],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[1], (unsigned char)buf[i].ifr_hwaddr.sa_data[2],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[3], (unsigned char)buf[i].ifr_hwaddr.sa_data[4],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[5]);

		char _dev_name[16] = {0};
		std::string _mac = mac;
		memcpy(_dev_name, (char*)buf[i].ifr_name, sizeof(buf[i].ifr_name));
		_host_mac_addr[_dev_name] = _mac;
		// printf("%s ---> %s\n", _dev_name, _mac.c_str());
	}
	close(fd);
	return 0;

}

void dumphex(void *data, uint32_t size)
{
#if 1
#define dbg_printf printf
#else 
#define dbg_printf {;}
#endif

	char ascii[17];
	unsigned int i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		if (i % 16 == 0) {
		    //dbg_printf("%p: ", data + i);
            dbg_printf("%08x: ", i);
		}

		dbg_printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			dbg_printf(" ");
			if ((i+1) % 16 == 0) {
				dbg_printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					dbg_printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					dbg_printf("   ");
				}
				dbg_printf("|  %s \n", ascii);
			}
		}
	}
}

int create_raw_socket(char const *ifname, unsigned short type,unsigned char *ifaddr, int *ifindex)
{
	int optval=1;
	int fd;
	struct ifreq ifr;
	int domain, stype;
	struct sockaddr_ll sa;

	memset(&sa, 0, sizeof(sa));

	domain = PF_PACKET;
	stype = SOCK_RAW;

	if ((fd = socket(domain, stype, htons(type))) < 0) {
		printf("Create socket failed, fd : %d!\n", fd);
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
		printf("setsockopt SO_BROADCAST failed!\n");
	}

	struct timeval       rtime;
	rtime.tv_sec  = 0 ;
	rtime.tv_usec = 200000 ; 
	setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rtime, sizeof(struct timeval));
	setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&rtime, sizeof(struct timeval)) ;
	
	if (ifaddr) {
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) 
		{
			printf("ioctl(SIOCGIFifaddr) failed!\n");
			return -2;
		}

		memcpy(ifaddr, ifr.ifr_hwaddr.sa_data,6);
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFMTU, &ifr) < 0) {
		printf("ioctl(SIOCGIFMTU) failed!\n");
		return -2;
	}

	if (ifr.ifr_mtu < ETH_DATA_LEN) {
		char buffer[256];
		printf(buffer, "Interface %.16s has MTU of %d -- should be %d.  You may have serious connection problems.\n",
			ifname, ifr.ifr_mtu, ETH_DATA_LEN);
	}

	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(type);
	strncpy(ifr.ifr_name,ifname,sizeof(ifr.ifr_name));

	if (ioctl(fd,SIOCGIFINDEX,&ifr)<0)
	{
		printf("ioctl(SIOCFIGINDEX):Could not get interface index\n");
		return -2;
	}

	sa.sll_ifindex = ifr.ifr_ifindex;
	
    if (ifindex) {
	    *ifindex = ifr.ifr_ifindex;
	}

	if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		printf("bind socket failed!\n");
		return -3;
	}

	return fd;
}

int eth_xfer(unsigned char *dstmac, uint8_t cmd_id, unsigned char *buf, uint16_t payload_len)
{

	printf("eth_xfer -- \n");
	int ret = 0;
	uint8_t sbuf[1024]={0};
	memset(sbuf, 0, sizeof(sbuf));

	ETH_HEADER eth_header={0};
	memcpy(eth_header.dest_mac,  dstmac,   sizeof(eth_header.dest_mac));
	memcpy(eth_header.src_mac,   ifaddr,    sizeof(eth_header.src_mac));
	eth_header.etype = htons(ETH_DEVCTRL_PROTOCOL_H2D);
	memcpy(sbuf, &eth_header, sizeof(eth_header));

	CMD_HEADER cmd_header={0};
	cmd_header.cmd_id = cmd_id;
	cmd_header.cmd_payload_len = htons(payload_len);
	memcpy(sbuf + sizeof(ETH_HEADER), &cmd_header, sizeof(CMD_HEADER));

	if(payload_len != 0 && buf != NULL) {
	    memcpy(sbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER), buf, payload_len);
	}

	ret = send(s_sockfd, sbuf, sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + payload_len, 0);

	return ret;
}

int req_search_dev(void)
{
	int ret = eth_xfer(bcaddr, ETH_CMD_ID_SEARCH_DEV, NULL, 0);
	while (1)
	{
		uint8_t rbuf[1024];
		int data_len = 0;

		memset(rbuf, 0, sizeof(rbuf));
		data_len = recv(r_sockfd, rbuf, 1024,0);
		if (data_len < 0)
		{
			break;
		}
		else
		{
			dumphex(rbuf, data_len);
			memcpy(_device_addr[_deviceCount], rbuf + 6, 6);
			
			printf("Device mac address : \n");
			for (int i = 0; i < 6; i++)
			{
				printf("%02x", _device_addr[_deviceCount][i]);
				if (i < 5)
				{
					printf("-");
				}
			}
			printf("\n");
			_deviceCount++;
			// _device_addr.push_back(destAddr);
		}
	}
}

int req_get_netcfg_cmd(unsigned char* mac)
{
	printf("req_get_netcfg_cmd -- \n");
	int ret = eth_xfer(mac, ETH_CMD_ID_GET_DEV_NETCFG, NULL, 256);
	printf("ret -- %d\n", ret);
	uint8_t rbuf[1024];
	int data_len;
	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
    dumphex(rbuf, data_len);
	IP_CONFIG ip_config;
	memset(&ip_config, 0x00, sizeof(IP_CONFIG));
	memcpy(&ip_config, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER), sizeof(IP_CONFIG));
	struct in_addr ip_addr;
	struct in_addr net_mask;
	struct in_addr gw_addr;
	struct in_addr dns_addr;
	struct in_addr dhcps_saddr;
	struct in_addr dhcps_eaddr;
	printf("Static IP: %d\n", ip_config.static_ip);
	memcpy(&ip_addr, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 1, 4);
	printf("ipaddr: %s\n", inet_ntoa(ip_addr));
	
	//memcpy(&ip_addr, &ip_config.ip_addr, sizeof(uint32_t));
	//printf("ipaddr: %s\n", inet_ntoa(ip_addr));
	memcpy(&net_mask, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 5, sizeof(uint32_t));
	printf("netmask: %s\n", inet_ntoa(net_mask));
	memcpy(&gw_addr, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 9, sizeof(uint32_t));
	printf("gwaddr: %s\n", inet_ntoa(gw_addr));
	memcpy(&dns_addr, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 13, sizeof(uint32_t));
	printf("dnsaddr: %s\n", inet_ntoa(dns_addr));
	printf("dhcps enable: %d\n", ip_config.dhcps_enable);
	memcpy(&dhcps_saddr, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 18, sizeof(uint32_t));
	printf("dhcps_saddr: %s\n", inet_ntoa(dhcps_saddr));
	memcpy(&dhcps_eaddr, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER) + 22, sizeof(uint32_t));
	printf("dhcps_eaddr: %s\n", inet_ntoa(dhcps_eaddr));
}

int req_set_netcfg_cmd(unsigned char* mac)
{
	IP_CONFIG ip_config;
	memset(&ip_config, 0x00, sizeof(IP_CONFIG));
	int  static_ip = _dhcp;
	std::string str_ip = _DeviceIp;
	std::string str_net = _DeviceMask;
	std::string str_gw = _DeviceGatway;
	if (static_ip == 0x01)
	{
		ip_config.static_ip = 0x01;
		ip_config.ip_addr  = inet_addr(str_ip.c_str());
		ip_config.net_mask = inet_addr(str_net.c_str());
		ip_config.gw_addr  = inet_addr(str_gw.c_str());
	}

	ip_config.user_setting = 0x01;
    unsigned char netcfg[256] = {0x00};
	memcpy(netcfg, &ip_config, sizeof(ip_config));

	int ret = eth_xfer(mac, ETH_CMD_ID_SET_DEV_NETCFG, netcfg, 256);

	uint8_t rbuf[1024];
	int data_len;

	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
    dumphex(rbuf, data_len);
}

int req_reboot_cmd(unsigned char* mac)
{
	printf("Device Reboot...\n");
	int ret = eth_xfer(mac, ETH_CMD_ID_SET_DEV_REBOOT, NULL, 0);

	uint8_t rbuf[1024];
	int data_len;

	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
    dumphex(rbuf, data_len);
}
 
int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

void hexstr2mac(unsigned char *dst, const char *src) {
	int i = 0;
	while (i < 6) 
	{
		
		if (' ' == *src || ':' == *src || '"' == *src || '\'' == *src || '-' == *src) {
			src++;
			continue;
		}
		*(dst + i) = ((hex2num(*src) << 4) | hex2num(*(src + 1)));
		i++;
		src += 2;
	}
}

int main(int argc, char** argv)
{
	std::string _config_file;
	if (argc < 2)
	{
		_config_file = "./NetConfig.ini";
	}
	else
	{
		_config_file = argv[1];
	}

	if (access(_config_file.c_str(), F_OK) != 0)
	{
		printf("Net Config Not Found : %s.\n", _config_file.c_str());
		return -1;
	}

	if (get_local_mac() != 0)
	{
		printf("Get local mac error\n");
		return -1;
	}

	printf("Local Mac Address : \n");
	for (auto it_b = _host_mac_addr.begin(); it_b != _host_mac_addr.end(); it_b++)
	{
		printf("%s->%s\n", it_b->first.c_str(), it_b->second.c_str());
	}

	printf("\n");
	
	ini::iniReader config;
	bool ret = config.ReadConfig(_config_file.c_str());
	if (ret == false) 
    {
		printf("ReadConfig is Error,Cfg=%s", "config.ini");
		return -1;
	}


	_HostMac = config.ReadString("Host", "mac", "");
	_dhcp = config.ReadInt("Device", "dhcp", 1);
	_DeviceIp = config.ReadString("Device", "ip", "");
	_DeviceMask = config.ReadString("Device", "mask", "");
	_DeviceGatway = config.ReadString("Device", "gateway", "");
 
	std::cout << "Host Mac Addr : " << _HostMac << std::endl;
	std::cout << "Device DHCP : " << _dhcp << std::endl;
	std::cout << "Device IP : " << _DeviceIp << std::endl;
	std::cout << "Device Mask : " << _DeviceMask << std::endl;
	std::cout << "Device Gatway : " << _DeviceGatway << std::endl;

	bool bFound = false;
	std::string _dev_name;
	std::string _host_mac;
	for (auto it_b = _host_mac_addr.begin(); it_b != _host_mac_addr.end(); it_b++)
	{
		if (_HostMac == it_b->second)
		{
			_dev_name = it_b->first;
			_host_mac = it_b->second;
			bFound = true;
		}
	}

	if (bFound == false)
	{
		printf("Not Found Mac Addr : %s\n", _HostMac.c_str());
		return -1;
	}

	hexstr2mac(ifaddr, _host_mac.c_str());


	s_sockfd = create_raw_socket(_dev_name.c_str(), ETH_DEVCTRL_PROTOCOL_H2D, NULL, NULL);
	if(s_sockfd < 0)
	{
		print("create_raw_socket s_sockfd failed!ret=%d", s_sockfd);
		return -1;
	}

	r_sockfd = create_raw_socket(_dev_name.c_str(), ETH_DEVCTRL_PROTOCOL_D2H, NULL, NULL);
	if(r_sockfd < 0)
	{
		print("create_raw_socket r_sockfd failed!ret=%d", r_sockfd);
		close(s_sockfd);
		return -1;
	}

	//扫描获取设备的mac地址
	req_search_dev();

	if (_deviceCount == 0)
	{
		printf("Not Find Device.\n");
		close(s_sockfd);
		close(r_sockfd);
		return -1;
	}
	unsigned char* deviceMac = NULL;
	deviceMac = _device_addr[0];

	req_set_netcfg_cmd(deviceMac);
	req_reboot_cmd(deviceMac);

	return 0;
}
