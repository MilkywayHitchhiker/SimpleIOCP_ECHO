// IOCP_ECHO_ClassVer.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "NetworkModule.h"


CCrashDump Dump;

class ECHO:public CLanServer
{
public:
	ECHO (void)
	{
	}
	~ECHO (void)
	{
		Stop ();
	}

	virtual void OnRecv (UINT64 SessionID, Packet *p)
	{
		INT64 Num;

		*p >> Num;
		
		Packet *Pack = Packet::Alloc();

		*Pack << 8;
		*Pack << Num;

		SendPacket (SessionID, Pack);

		Packet::Free(Pack);

		return;
	}
	virtual void OnSend (UINT64 SessionID, INT SendByte)
	{
		return;
	}

	virtual bool OnClientJoin (UINT64 SessionID, WCHAR *IP, int PORT)
	{
		//wprintf (L"SessionID = %lld,IP =%lls, PORT %d\n", SessionID, IP, PORT);
		return true;
	}
	virtual void OnClientLeave (UINT64 SessionID)
	{
		return;
	}
};

ECHO Network;

int main()
{
	wprintf (L"MainThread Start\n");
	Network.Start (L"127.0.0.1", 6000, 200, 3);


	UINT AcceptTotal = 0;
	UINT AcceptTPS = 0;
	UINT RecvTPS = 0;
	UINT SendTPS = 0;
	UINT ConnectSessionCnt = 0;


	DWORD StartTime = GetTickCount ();
	DWORD EndTime;
	while ( 1 )
	{
		EndTime = GetTickCount ();
		if ( EndTime - StartTime >= 1000 )
		{
			wprintf (L"==========================\n");
			wprintf (L"Accept User Total = %ld \n", AcceptTotal);
			wprintf (L"AcceptTPS = %d \n", AcceptTPS);
			wprintf (L"Sec RecvTPS = %d \n", RecvTPS);
			wprintf (L"Sec SendTPS = %d \n", SendTPS);
			wprintf (L"Connect Session Cnt = %d \n", ConnectSessionCnt);
			wprintf (L"==========================\n");

			AcceptTotal = Network.AcceptTotal ();
			AcceptTPS = Network.AcceptTPS (true);
			RecvTPS = Network.RecvTPS (true);
			SendTPS = Network.SendTPS (true);
			ConnectSessionCnt = Network.Use_SessionCnt ();

			StartTime = EndTime;
		}

		if ( GetAsyncKeyState ('E') & 0x8001 )
		{
			Network.Stop ();
			break;
		}
		/*
		else if ( GetAsyncKeyState ('S') & 0x8001 )
		{
			Network.Start (L"127.0.0.1", 6000, 200, 3);
		}
		*/

		Sleep (200);
	}


    return 0;
}

