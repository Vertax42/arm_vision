#include <iostream>
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

#define MAX_INTERFACE	(16)
#define ETH_DEVCTRL_PROTOCOL_H2D        (0x7711)
#define ETH_DEVCTRL_PROTOCOL_D2H        (0x7712)

bool global_debugPrint_flag =true;
int r_sockfd =-1;
int s_sockfd =-1;
unsigned char ifaddr[6]={0};
unsigned char bcaddr[6]={0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char network_dev_name[16] = {0};
unsigned char destAddr[6] = {0};

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
	uint8_t  user_setting;
}__attribute__((packed))IP_CONFIG;

typedef struct CMD_HEADER
{
    uint8_t   cmd_id;
    uint16_t  cmd_payload_len;
}__attribute__((packed))CMD_HEADER;

#define print(format,...)  if(global_debugPrint_flag){printf("%s--%s--%d: ",__FILE__,__FUNCTION__,__LINE__), printf(format,##__VA_ARGS__), printf("\n");}

int get_local_mac(unsigned char* mac_buf, int buf_size)
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
	printf("Local Mac Address : \n");
	for (int i = 0; i < if_num; i++)
	{
		/* 获取当前网卡的mac */
		ret = ioctl(fd, SIOCGIFHWADDR, (char*)&buf[i]);
		if(ret)
		{
			continue;
		}

		printf("%i. %02x:%02x:%02x:%02x:%02x:%02x\n", i + 1,
			(unsigned char)buf[i].ifr_hwaddr.sa_data[0],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[1],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[2],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[3],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[4],
			(unsigned char)buf[i].ifr_hwaddr.sa_data[5]
			);
	}
	close(fd);
	int num = 0;
	do
	{
		printf("Please Select Mac Address : ");
		std::cin >> num;
		if (num <=0 || num > if_num)
		{
			printf("Select mac address invalid, please input agin!\n");
		}
		
	}while (num <=0 || num > if_num);

	memcpy(mac_buf, buf[num - 1].ifr_hwaddr.sa_data, buf_size);
	memcpy(network_dev_name, (char*)buf[num - 1].ifr_name, sizeof(buf[num - 1]));
	// for (int i = 0; i < buf_size; i++)
	// {
	// 	printf("%02x", mac_buf[i]);
	// 	if (i != buf_size - 1)
	// 		printf("-");
	// }
	// printf("\n");
	
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

// int req_search_dev(void)
// {
// 	printf("req_search_dev ---\n");
// 	int ret = eth_xfer(bcaddr, ETH_CMD_ID_SEARCH_DEV, NULL, 0);

// 	printf("--------------------\n");
// 	uint8_t rbuf[1024];
// 	int data_len;

// 	memset(rbuf, 0, sizeof(rbuf));
// 	data_len = recv(r_sockfd, rbuf, 1024,0);
// 	dumphex(rbuf, data_len);

// 	memcpy(destAddr, rbuf + 6, sizeof(destAddr));
// 	printf("Device mac address : \n");
// 	for (int i = 0; i < 6; i++)
// 	{
// 		printf("%02x", destAddr[i]);
// 		if (i < 5)
// 		{
// 			printf("-");
// 		}
// 	}
// 	printf("\n");
// }

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

			memcpy(destAddr, rbuf + 6, sizeof(destAddr));
			printf("Device mac address : \n");
			for (int i = 0; i < 6; i++)
			{
				printf("%02x", destAddr[i]);
				if (i < 5)
				{
					printf("-");
				}
			}
			printf("\n");
		}
	}
}



int req_get_netcfg_cmd(void)
{
	printf("req_get_netcfg_cmd -- \n");
	int ret = eth_xfer(destAddr, ETH_CMD_ID_GET_DEV_NETCFG, NULL, 256);
	printf("ret -- %d\n", ret);
	uint8_t rbuf[1024];
	int data_len;

	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
	printf("---1\n");
    dumphex(rbuf, data_len);
	printf("---2\n");

	IP_CONFIG ip_config;
	memset(&ip_config, 0x00, sizeof(IP_CONFIG));
	memcpy(&ip_config, rbuf + sizeof(ETH_HEADER) + sizeof(CMD_HEADER), sizeof(IP_CONFIG));
	printf("---3\n");
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

int req_set_netcfg_cmd(void)
{
	IP_CONFIG ip_config;
	memset(&ip_config, 0x00, sizeof(IP_CONFIG));
	int  static_ip;
	std::string str_ip;
	std::string str_net;
	std::string str_gw;
	std::string str_dns;
	uint8_t dhcps_enable;
	std::string str_dhcps_s;
	std::string str_dhcps_e;
	std::cout << "是否设置静态IP(0:DHCP动态分配， 1:静态IP) : ";
	std::cin >> static_ip;
	if (static_ip == 0x01)
	{
		std::cout << "设置IP地址：eg->192.168.10.123";
		std::cin >> str_ip;
		std::cout << "设置子网掩码地址：eg->255.255.255.0";
		std::cin >> str_net;
		std::cout << "设置网关地址：eg->192.168.10.1";
		std::cin >> str_gw;
		std::cout << "设置DNS地址：eg->0.0.0.0";
		std::cin >> str_dns;
		std::cout << "是否开启DHCP服务器(0:禁用 1:开启)：eg->0";
		std::cin >> dhcps_enable;

		ip_config.static_ip = 1;
		ip_config.ip_addr  = inet_addr(str_ip.c_str());
		ip_config.net_mask = inet_addr(str_net.c_str());
		ip_config.gw_addr  = inet_addr(str_gw.c_str());
		ip_config.dns_addr = inet_addr(str_dns.c_str());
		ip_config.dhcps_enable = dhcps_enable;

		if (dhcps_enable == 0x01)
		{
			std::cout << "设置DHCP服务端开始地址：";
			std::cin >> str_dhcps_s;
			std::cout << "设置DHCP服务端结束地址：";
			std::cin >> str_dhcps_e;
			ip_config.dhcps_saddr  = inet_addr(str_dhcps_s.c_str());
			ip_config.dhcps_eaddr  = inet_addr(str_dhcps_e.c_str());
		}
	}
	ip_config.user_setting = 0x01;
    unsigned char netcfg[256] = {0x00};
	memcpy(netcfg, &ip_config, sizeof(ip_config));

	int ret = eth_xfer(destAddr, ETH_CMD_ID_SET_DEV_NETCFG, netcfg, 256);

	uint8_t rbuf[1024];
	int data_len;

	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
    dumphex(rbuf, data_len);
}

int req_reboot_cmd(void)
{
	printf("Device Reboot...\n");
	int ret = eth_xfer(destAddr, ETH_CMD_ID_SET_DEV_REBOOT, NULL, 0);

	uint8_t rbuf[1024];
	int data_len;

	memset(rbuf, 0, sizeof(rbuf));
	data_len = recv(r_sockfd, rbuf, 1024,0);
    dumphex(rbuf, data_len);
}
 
int main()
{
	if (get_local_mac(ifaddr, sizeof(ifaddr)) != 0)
	{
		printf("Get local mac error\n");
	}

	s_sockfd = create_raw_socket(network_dev_name, ETH_DEVCTRL_PROTOCOL_H2D, NULL, NULL);
	if(s_sockfd < 0)
	{
		print("create_raw_socket s_sockfd failed!ret=%d", s_sockfd);
	}

	r_sockfd = create_raw_socket(network_dev_name, ETH_DEVCTRL_PROTOCOL_D2H, NULL, NULL);
	if(r_sockfd < 0)
	{
		print("create_raw_socket r_sockfd failed!ret=%d", r_sockfd);
	}

	//扫描获取设备的mac地址
	req_search_dev();

	// std::cin >> 
	printf("1. Get device info\n");
	printf("2. Set device info\n");
	int count = 0;
	do
	{
		printf("Please select operation : ");
		std::cin >> count;
	} while (count <= 0 || count > 2);
	
	if (count == 1)
	{
		req_get_netcfg_cmd();
	}
	else if (count == 2)
	{
		//set net config
		req_set_netcfg_cmd();

		//reboot device
		req_reboot_cmd();
	}
	

	return 0;
}
