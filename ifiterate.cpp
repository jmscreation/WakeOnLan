#include "ifiterate.h"

#ifdef __WIN32__
# include <windows.h>
# include <winsock.h>
# include <iphlpapi.h>
#else
# include <unistd.h>
# include <stdlib.h>
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
# include <net/if.h>
# include <sys/ioctl.h>
#endif

#include <sys/stat.h>

typedef unsigned long uint32;

#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__) || defined(__linux__)
# define USE_GETIFADDRS 1
# include <ifaddrs.h>
static uint32_t SockAddrToUint32(struct sockaddr * a)
{
   return ((a)&&(a->sa_family == AF_INET)) ? ntohl(((struct sockaddr_in *)a)->sin_addr.s_addr) : 0;
}
#endif

// convert a numeric IP address into its string representation
void Inet_NtoA(uint32_t addr, char * ipbuf)
{
   sprintf(ipbuf, "%li.%li.%li.%li", (addr>>24)&0xFF, (addr>>16)&0xFF, (addr>>8)&0xFF, (addr>>0)&0xFF);
}

// convert a string represenation of an IP address into its numeric equivalent
uint32_t Inet_AtoN(const char * buf)
{
   // net_server inexplicably doesn't have this function; so I'll just fake it
   uint32_t ret = 0;
   int shift = 24;  // fill out the MSB first
   bool startQuad = true;
   while((shift >= 0)&&(*buf))
   {
      if (startQuad)
      {
         unsigned char quad = (unsigned char) atoi(buf);
         ret |= (((uint32_t)quad) << shift);
         shift -= 8;
      }
      startQuad = (*buf == '.');
      buf++;
   }
   return ret;
}

bool GetNetworkInterfaceInfos(std::vector<NetworkInterface>& iflist)
{
#if defined(USE_GETIFADDRS)
   // BSD-style implementation
   struct ifaddrs * ifap;
   if (getifaddrs(&ifap) == 0)
   {
      struct ifaddrs * p = ifap;
      while(p)
      {
         uint32_t ifaAddr  = SockAddrToUint32(p->ifa_addr);
         uint32_t maskAddr = SockAddrToUint32(p->ifa_netmask);
         uint32_t dstAddr  = SockAddrToUint32(p->ifa_dstaddr);
         p->ifa_
         if (ifaAddr > 0)
         {
            char ifaAddrStr[32];  Inet_NtoA(ifaAddr,  ifaAddrStr);
            char maskAddrStr[32]; Inet_NtoA(maskAddr, maskAddrStr);
            char dstAddrStr[32];  Inet_NtoA(dstAddr,  dstAddrStr);

            NetworkInterface tmpif;
            tmpif.name = p->ifa_name;
            tmpif.description = "";
            tmpif.address = ifaAddrStr;
            tmpif.netmask = maskAddrStr;
            tmpif.broadcast = dstAddrStr;
            tmpif.gateway = "";

            iflist.emplace_back(std::move(tmpif));
         }
         p = p->ifa_next;
      }
      freeifaddrs(ifap);
      return 1;
   }
#elif defined(__WIN32__)
   // Windows XP style implementation

   // Adapted from example code at http://msdn2.microsoft.com/en-us/library/aa365917.aspx
   // Now get Windows' IPv4 addresses table.  Once again, we gotta call GetIpAddrTable()
   // multiple times in order to deal with potential race conditions properly.
   MIB_IPADDRTABLE * ipTable = NULL;
   {
      ULONG bufLen = 0;
      for (int i=0; i<5; i++)
      {
         DWORD ipRet = GetIpAddrTable(ipTable, &bufLen, false);
         if (ipRet == ERROR_INSUFFICIENT_BUFFER)
         {
            free(ipTable);  // in case we had previously allocated it
            ipTable = (MIB_IPADDRTABLE *) malloc(bufLen);
         }
         else if (ipRet == NO_ERROR) break;
         else
         {
            free(ipTable);
            ipTable = NULL;
            break;
         }
     }
   }

   if (ipTable)
   {
      // Try to get the Adapters-info table, so we can given useful names to the IP
      // addresses we are returning.  Gotta call GetAdaptersInfo() up to 5 times to handle
      // the potential race condition between the size-query call and the get-data call.
      // I love a well-designed API :^P
      IP_ADAPTER_INFO * pAdapterInfo = NULL;
      {
         ULONG bufLen = 0;
         for (int i=0; i<5; i++)
         {
            DWORD apRet = GetAdaptersInfo(pAdapterInfo, &bufLen);
            if (apRet == ERROR_BUFFER_OVERFLOW)
            {
               free(pAdapterInfo);  // in case we had previously allocated it
               pAdapterInfo = (IP_ADAPTER_INFO *) malloc(bufLen);
            }
            else if (apRet == ERROR_SUCCESS) break;
            else
            {
               free(pAdapterInfo);
               pAdapterInfo = NULL;
               break;
            }
         }
      }

      for (DWORD i=0; i<ipTable->dwNumEntries; i++)
      {
         const MIB_IPADDRROW & row = ipTable->table[i];

         // Now lookup the appropriate adaptor-name in the pAdaptorInfos, if we can find it
         const char * name = NULL;
         const char * desc = NULL;
         const char * gw = NULL;
         if (pAdapterInfo)
         {
            IP_ADAPTER_INFO * next = pAdapterInfo;
            while((next)&&(name==NULL))
            {
               IP_ADDR_STRING * ipAddr = &next->IpAddressList;
               while(ipAddr)
               {
                  if (Inet_AtoN(ipAddr->IpAddress.String) == ntohl(row.dwAddr))
                  {
                     name = next->AdapterName;
                     desc = next->Description;
                     gw = next->GatewayList.IpAddress.String;
                     break;
                  }
                  ipAddr = ipAddr->Next;
               }
               next = next->Next;
            }
         }

         char buf[128];
         if (name == NULL)
         {
            sprintf(buf, "unnamed-%i", i);
            name = buf;
         }

         uint32_t ipAddr  = ntohl(row.dwAddr);
         uint32_t netmask = ntohl(row.dwMask);
         uint32_t baddr   = ipAddr & netmask;
         if (row.dwBCastAddr) baddr |= ~netmask;

         char ifaAddrStr[32];  Inet_NtoA(ipAddr,  ifaAddrStr);
         char maskAddrStr[32]; Inet_NtoA(netmask, maskAddrStr);
         char dstAddrStr[32];  Inet_NtoA(baddr,   dstAddrStr);

         NetworkInterface tmpif {};
         tmpif.name = name ? name : "unavailable";
         tmpif.description = desc ? desc : "unavailable";
         tmpif.address = ifaAddrStr;
         tmpif.netmask = maskAddrStr;
         tmpif.broadcast = dstAddrStr;
         tmpif.gateway = gw ? gw : "unavailable";

         iflist.emplace_back(std::move(tmpif));
      }

      free(pAdapterInfo);
      free(ipTable);
      return 1;
   }
#else
   // Dunno what we're running on here!
#  error "Don't know how to implement PrintNetworkInterfaceInfos() on this OS!"
#endif
   return 0;
}


void PrintNetworkInterfaceInfos() {
   std::vector<NetworkInterface> list;
   if(!GetNetworkInterfaceInfos(list)){
      std::cout << "Failed to enumerate network interfaces\n";
      return;
   }

   for(const NetworkInterface& ii : list){
      std::cout << "Found interface:  name=[" << ii.name << " desc=[" << ii.description << "] address=[" << ii.address << "] netmask=[" << ii.netmask << "] gateway=[" << ii.gateway << "] broadcastAddr=[" << ii.broadcast << "]\n";
   }

}