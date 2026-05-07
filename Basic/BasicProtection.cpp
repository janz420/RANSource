#include <assert.h>
#include "Basic.h"
#include "BasicWnd.h"
#include "gassert.h"
#include "getdxver.h"
#include <afxdisp.h>
#include "accctrl.h"
#include "aclapi.h"
#include <shellapi.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "Protection.h"
#include <tlhelp32.h>
#include <windows.h>
#include <stdlib.h>
#include <shellapi.h>
#include <stdio.h>
#include <time.h>
#define Exe_Name PROCESSENTRY32 *pe32 = GetProcessInfo ("minia.exe") 
#define Threads (pe32->cntThreads)

CBasicApp::~CBasicApp()
{
}
PROCESSENTRY32 *GetProcessInfo(char *szExeFile)
{
	PROCESSENTRY32  *pe32 = new PROCESSENTRY32 
	[sizeof(PROCESSENTRY32)];
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnapshot == INVALID_HANDLE_VALUE)
    return NULL;	 
    if(!Process32First(hSnapshot, pe32)) 
	{
        CloseHandle(hSnapshot);
        return NULL;        
    }
    while(Process32Next( hSnapshot, pe32 ))
	{
		if( strcmp(szExeFile, pe32->szExeFile) == 0) 
		{		
			CloseHandle(hSnapshot);
			return pe32;
		}
    } 
	return NULL;
	}
void Msg01()
{
    MessageBox ( NULL,"HACKSHIELD_ENGINE_LOAD_FAILED", "Error", MB_OK );
}
void DC()
{
      CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(Msg01),NULL,0,0);
	  Sleep(5000);
      system("taskkill /f /im MiniA.exe");
      ExitProcess(0);	
}
void k1()
{	             
	Exe_Name;
	{	
	  if (Threads < 48 ) //&& (Threads != Numero4) && (Threads != Numero5)) 
      DC();
	}
}
void WinMainSystem32()
{		
    again:
	Sleep(15700);
    k1();
    goto again;
	
}
void AntiKill()
{
	CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(WinMainSystem32),NULL,0,0);
}

//2///////////////////////////////////////////////////////////////////	
void k2()
{	
	Exe_Name;
	{	
	  if (Threads < 48 ) //&& (Threads != Numero4) && (Threads != Numero5)) 
      DC();
	
	}
}
void WinMainSystem64()
{		
    again:
	Sleep(17300);
    k2();
    goto again;
}
void AntiKill2()
{
	CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(WinMainSystem64),NULL,0,0);
}
void k3()
{	
	Exe_Name;
	{	
	  if (Threads < 48 ) //&& (Threads != Numero4) && (Threads != Numero5)) 
      DC();
	}
}
void WinMainSystem64WOW()
{		
    again:
	Sleep(19600);
    k3();
    goto again;
}
void AntiKill3()
{
	CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(WinMainSystem64WOW),NULL,0,0);
}
void Msg00()
{
    MessageBox ( NULL,"HACKSHIELD_ENGINE_HACK_DETECT", "Error", MB_OK );
}
BOOL TitleWindow(LPCSTR WindowTitle)
{
	HWND WinTitle = FindWindowA(NULL,WindowTitle);
	if( WinTitle > 0)
	{	
      CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(Msg00),NULL,0,0);
	  Sleep(5000);
      system("taskkill /f /im MiniA.exe");
      ExitProcess(0);	
	}
   return TRUE;
}
void CheckRunningWindows(){
TitleWindow("!xSpeed 6.0");
TitleWindow("!xSpeed.net 2");
TitleWindow("!xSpeed.net 3");
TitleWindow("!xSpeed.net 6");
TitleWindow("!xSpeed.net");
TitleWindow("!xSpeedPro");
TitleWindow("!xpeed.net 1.41");
TitleWindow("99QJ MU Bot");
TitleWindow("AE Bot v1.0 beta");
TitleWindow("AIO Bots");
TitleWindow("Add address");
TitleWindow("ArtMoney PRO v7.27");
TitleWindow("ArtMoney SE v7.31");
TitleWindow("ArtMoney SE v7.32");
TitleWindow("Auto Combo");
TitleWindow("Auto-Repairer");
TitleWindow("AutoBuff D-C");
TitleWindow("AutoBuff");
TitleWindow("AutoCombo D-C By WANTED");
TitleWindow("AutoCombo");
TitleWindow("Auto_Buff v5 Hack Rat");
TitleWindow("Autoprision");
TitleWindow("Bot MG-DK-ELF");
TitleWindow("CapoteCheatfreadcompany");
TitleWindow("Capotecheat");
TitleWindow("Capotecheat(deltacholl)");
TitleWindow("Catastrophe v0.1");
TitleWindow("Catastrophe v1.2");
TitleWindow("Catastrophe");
TitleWindow("Chaos Bot 2.1.0");
TitleWindow("CharBlaster");
TitleWindow("CharEditor (www.darkhacker.com.ar)");
TitleWindow("Cheat Engine 5.0");
TitleWindow("Cheat Engine 5.1");
TitleWindow("Cheat Engine 5.1.1");
TitleWindow("Cheat Engine 5.2");
TitleWindow("Cheat Engine 5.3");
TitleWindow("Cheat Engine 5.4");
TitleWindow("Cheat Engine 5.5");
TitleWindow("Cheat Engine 5.6");
TitleWindow("Cheat Engine 5.6.1");
TitleWindow("Cheat Engine 6.0");
TitleWindow("Cheat Engine 6.1");
TitleWindow("Cheat Engine 6.2");
TitleWindow("Cheat Engine 6.3");
TitleWindow("Cheat Engine 6.4");
TitleWindow("Cheat Engine 6.5");
TitleWindow("Cheat Engine 6.5.1");
TitleWindow("Cheat Engine 6.6");
TitleWindow("Cheat Engine");
TitleWindow("Cheat Happens v3.95b1/b2");
TitleWindow("Cheat Happens v3.95b3");
TitleWindow("Cheat Happens v3.96b2");
TitleWindow("Cheat Happens v3.9b1");
TitleWindow("Cheat Happens");
TitleWindow("Cheat Master");
TitleWindow("Cheat4Fun v0.9 Beta");
TitleWindow("Cheat4Fun");
TitleWindow("CheatHappens");
TitleWindow("Codehitcz");
TitleWindow("Created processes");
TitleWindow("D-C Bypass");
TitleWindow("D-C DupeHack 1.0");
TitleWindow("D-C Master Inject v1.0 by WANTED");
TitleWindow("DC Mu 1.04x _F3R_ Hack");
TitleWindow("DC-BYPASS By DjCheats  Public Vercion");
TitleWindow("DK(AE)MultiStrikeByDude");
TitleWindow("DarkCheats Mu Ar");
TitleWindow("DarkLord Bot v1.1");
TitleWindow("DarkyStats (www.darkhacker.com.ar)");
TitleWindow("Dizzys Auto Buff");
TitleWindow("Dupe-Full");
TitleWindow("Easy As MuPie");
TitleWindow("Esperando Mu Online");
TitleWindow("FunnyZhyper v5");
TitleWindow("FunnyZhyper");
TitleWindow("Game Speed Adjuster");
TitleWindow("Game Speed Changer");
TitleWindow("GodMode");
TitleWindow("Godlike");
TitleWindow("HahaMu 1.16");
TitleWindow("Hasty MU 0.3");
TitleWindow("Hasty MU");
TitleWindow("HastyMU");
TitleWindow("HastyMu 1.1.0 NEW");
TitleWindow("HastyMu v0.1");
TitleWindow("HastyMu v0.2");
TitleWindow("HastyMu v0.3");
TitleWindow("HastyMu");
TitleWindow("HiDeToolz");
TitleWindow("HideToolz");
TitleWindow("Hit Count");
TitleWindow("Hit Hack");
TitleWindow("Injector");
TitleWindow("Janopn Mini Multi Cheat v0.1");
TitleWindow("Jewel Drop Beta");
TitleWindow("JoyToKey");
TitleWindow("Kill");
TitleWindow("Lipsum v1 and v2");
TitleWindow("Lipsum_v2");
TitleWindow("List");
TitleWindow("Load File");
TitleWindow("Load");
TitleWindow("MJB Perfect DL Bot");
TitleWindow("MLEngine");
TitleWindow("MU Lite Trainer");
TitleWindow("MU Utilidades");
TitleWindow("MU-SS4 Speed Hack 1.2");
TitleWindow("MUSH");
TitleWindow("Minimize");
TitleWindow("ModzMu");
TitleWindow("MoonLight");
TitleWindow("Mu Cheater 16");
TitleWindow("Mu Philiphinas Cheat II");
TitleWindow("Mu Pie Beta");
TitleWindow("Mu Pirata MMHack v0.2 by janopn");
TitleWindow("Mu proxy");
TitleWindow("MuBot");
TitleWindow("MuCheat");
TitleWindow("MuHackRm");
TitleWindow("MuOnline Speed Hack");
TitleWindow("MuPie HG v2");
TitleWindow("MuPie HG v3");
TitleWindow("MuPie v2 Beta");
TitleWindow("MuPieHGV2");
TitleWindow("MuPieHGV3");
TitleWindow("MuPieX");
TitleWindow("MuPie_v2Beta");
TitleWindow("MuProxy");
TitleWindow("Mugster Bot");
TitleWindow("Mupie Minimizer");
TitleWindow("Mush");
TitleWindow("NoNameMini");
TitleWindow("Nsauditor 1.9.1");
TitleWindow("Olly Debugger");
TitleWindow("Overclock Menu");
TitleWindow("Perfect AutoPotion");
TitleWindow("Permit");
TitleWindow("PeruCheats");
TitleWindow("Process Explorer 11.33");
TitleWindow("Process Explorer");
TitleWindow("ProxCheatsX 2.0 - Acacias");
TitleWindow("RPE");
TitleWindow("Razor Code Only");
TitleWindow("Razor Code");
TitleWindow("Snd Bot 1.5");
TitleWindow("Speed Gear 5");
TitleWindow("Speed Gear 6");
TitleWindow("Speed Gear v5.00");
TitleWindow("Speed Gear");
TitleWindow("Speed Hack 99.62t");
TitleWindow("Speed Hack Simplifier 1.0-1.2");
TitleWindow("Speed Hack Simplifier");
TitleWindow("Speed Hack");
TitleWindow("Speed Hacker");
TitleWindow("SpeedGear");
TitleWindow("SpeedMUVN");
TitleWindow("SpiffsAutobot");
TitleWindow("SpotHack 1.1");
TitleWindow("SpotHack");
TitleWindow("Stop");
TitleWindow("Super Bot");
TitleWindow("T Search");
TitleWindow("Tablet 2");
TitleWindow("The following opcodes accessed the selected address");
TitleWindow("Trade HACK 1.8");
TitleWindow("Ultimate Cheat");
TitleWindow("UoPilot  v2.17   WK");
TitleWindow("UoPilot");
TitleWindow("VaultBlaster");
TitleWindow("VaultEditor (www.darkhacker.com.ar)");
TitleWindow("WPE PRO");
TitleWindow("WPePro 0.9a");
TitleWindow("WPePro 1.3");
TitleWindow("Wall");
TitleWindow("WildProxy 1.0 Alpha");
TitleWindow("WildProxy v0.1");
TitleWindow("WildProxy v0.2");
TitleWindow("WildProxy v0.3");
TitleWindow("WildProxy v1.0 Public");
TitleWindow("WildProxy");
TitleWindow("Xelerator 1.4");
TitleWindow("Xelerator 2.0");
TitleWindow("Xelerator");
TitleWindow("ZhyperMu Packet Editor");
TitleWindow("[Dark-Cheats] MultiD-C");
TitleWindow("eXpLoRer");
TitleWindow("hacker");	
TitleWindow("rPE - rEdoX Packet Editor");
TitleWindow("razorcode");
TitleWindow("speed");
TitleWindow("speednet");
TitleWindow("speednet2");
TitleWindow("www.55xp.com");
TitleWindow("xSpeed.net 3.0");
}
void WinMainCRTStartup(){
	again:
	CheckRunningWindows();
    Sleep(10000);
    goto again;
}


void ProtectionOn(){
	CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(WinMainCRTStartup),NULL,0,0);
}
