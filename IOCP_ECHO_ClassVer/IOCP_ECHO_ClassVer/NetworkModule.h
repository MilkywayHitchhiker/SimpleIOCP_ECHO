#pragma once



#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <process.h>

#include "Packet.h"
#include "RingBuffer.h"
#include "CrashDump.h"


class CLanServer
{
protected:
	struct Session
	{
		bool UseFlag = false;
		SOCKET sock;
		UINT64 SessionID;

		INT64 IOCount = 0;
		
		long SendFlag = FALSE;
		CRingbuffer SendQ;
		OVERLAPPED SendOver;
		
		CRingbuffer RecvQ;
		OVERLAPPED RecvOver;
	};

	HANDLE _IOCP;
	SOCKET _ListenSock;
	int _Session_Max;
	Session *Session_Array;

	UINT64 _SessionID_Count = 0;

	UINT _RecvPacketTPS;
	UINT _SendPacketTPS;
	UINT _AcceptTotal;
	UINT _AcceptTPS;
	UINT _Use_Session_Cnt;

	HANDLE *Thread;
	int _WorkerThread_Num;

	bool bServerOn;

	//생성자
	CLanServer (void);
	
	//파괴자
	~CLanServer (void);

	//실제 AcceptThread
	void AcceptThread (void);
	//실제 WorkerThread
	void WorkerThread (void);

	bool InitializeNetwork (WCHAR *IP,int PORT);

	Session *FindSession (bool NewFlag, UINT64 SessionID = 0);

	void PostRecv (Session *p);
	void PostSend (Session *p);

	void SessionRelease (Session *p);

	void IODecrement (Session *p);

public :
	virtual void OnRecv (UINT64 SessionID, Packet *p) = 0;
	virtual void OnSend (UINT64 SessionID, INT SendByte) = 0;
	virtual bool OnClientJoin (UINT64 SessionID, WCHAR *IP, int PORT) = 0;
	virtual void OnClientLeave (UINT64 SessionID) = 0;

	void SendPacket (UINT64 SessionID, Packet *pack);
	void Disconnect (UINT64 SessionID);

	bool Start (WCHAR *ServerIP, int PORT, int Session_Max, int WorkerThread_Num);
	bool Stop (void);



	UINT RecvTPS (bool Reset)
	{
		UINT RecvTPS = _RecvPacketTPS;
		if ( Reset )
		{
			InterlockedExchange ((volatile LONG *)&_RecvPacketTPS, 0);
		}
		return RecvTPS;
	}
	UINT SendTPS (bool Reset)
	{
		UINT SendTPS = _SendPacketTPS;
		if ( Reset )
		{
			InterlockedExchange (( volatile LONG * )&_SendPacketTPS, 0);
		}
		return SendTPS;
	}
	UINT AcceptTotal (void)
	{
		return _AcceptTotal;
	}
	UINT AcceptTPS (bool Reset)
	{
		UINT AcceptTPS = _AcceptTPS;
		if ( Reset )
		{
			InterlockedExchange (( volatile LONG * )&_AcceptTPS, 0);
		}
		return AcceptTPS;
	}
	UINT Use_SessionCnt (void)
	{
		return _Use_Session_Cnt;
	}

protected:
	static unsigned int WINAPI AcceptThread (LPVOID pParam);
	static unsigned int WINAPI WorkerThread (LPVOID pParam);
};
