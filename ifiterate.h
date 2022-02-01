#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>

struct NetworkInterface {
    std::string name, description, address, netmask, broadcast, gateway;
};

extern void Inet_NtoA(uint32_t addr, char * ipbuf);
extern uint32_t Inet_AtoN(const char * buf);
extern bool GetNetworkInterfaceInfos(std::vector<NetworkInterface>& iflist);
extern void PrintNetworkInterfaceInfos();