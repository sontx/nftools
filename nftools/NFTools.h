#ifndef NFTOOLS_H_INCLUDED
#define NFTOOLS_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <conio.h>
#include <string>
#include <iostream>
#include <vector>
#include <wininet.h>
#include <iomanip>
#include <tchar.h>
#include <time.h>
#include <urlmon.h>
#include "Resources.h"
#define HK_NET_FRAMEWORK_SETUP "SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\"
#define MAX_KEY_LENGTH         0xFF
#define MAX_VALUE_NAME         0x3FFF
#define NET_VERSION_45         0x5C615 // .NET Framework 4.5
#define NET_VERSION_451        0x5C733 // .NET Framework 4.5.1 installed with Windows 8.1
#define NET_VERSION_451_       0x5C786 // .NET Framework 4.5.1 installed on Windows 8, Windows 7 SP1, or Windows Vista SP2
#define NET_VERSION_452        0x5CBF5 // .NET Framework 4.5.2
using namespace std;

#endif // NFTOOLS_H_INCLUDED
