// IOCP_ECHO_Server.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <process.h>

#include "RingBuffer.h"
#include "Packet.h"

#define WorkerThread_Max 3
#define Session_Max 1000

#define ServerIP L"127.0.0.1"
#define PORT 6000

struct Session
{
	bool UseFlag = false;
	SOCKET sock;
	UINT64 SessionID;

	INT64 Session_Count = 0;

	CRingbuffer SendQ;
	OVERLAPPED SendOver;
	bool SendFlag;

	CRingbuffer RecvQ;
	OVERLAPPED RecvOver;
};


unsigned int WINAPI AcceptThread (LPVOID pParam);

bool InitializeNetwork (void);

//Send,Recv담당.
unsigned int WINAPI WorkerThread (LPVOID pParam);

Session *FindSession (bool NewFlag, UINT64 SessionID = 0);

void PostSend (Session *p);
void PostRecv (Session *p);
void SessionClose (Session *p);


HANDLE g_IOCP;
SOCKET g_ListenSock;
Session User_Array[100];
UINT64 SessionID_Count = 1;

UINT64 g_RecvByte;
UINT64 g_SendByte;
int	AcceptClientCnt;


int main()
{
	wprintf (L"main_thread_Start\n");

	//Listen 소켓 초기화
	if ( InitializeNetwork () == false )
	{
		wprintf (L"Listen ERROR\n");
	}

	//IOCP 핸들 생성
	g_IOCP = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, WorkerThread_Max);


	//스레드 생성
	HANDLE Thread[WorkerThread_Max + 1];

	Thread[0] =(HANDLE) _beginthreadex (NULL, 0, AcceptThread, 0, NULL, NULL);
	for ( int Cnt = 0; Cnt < WorkerThread_Max; Cnt++ )
	{
		Thread[Cnt+1] = ( HANDLE )_beginthreadex (NULL, 0, WorkerThread, 0, NULL, NULL);
	}

	DWORD StartTime = GetTickCount ();
	DWORD EndTime;
	while ( 1 )
	{
		EndTime = GetTickCount ();
		if ( EndTime - StartTime >= 1000 )
		{
			wprintf (L"==========================\nAccept User Cnt = %d \nSec RecvByte = %lld \nSec SendByte = %lld \n==========================\n", AcceptClientCnt, g_RecvByte, g_SendByte);
			InterlockedExchange64 (( volatile LONG64 * )&g_RecvByte, 0);
			InterlockedExchange64 (( volatile LONG64 * )&g_SendByte, 0);
			StartTime = EndTime;
		}


		if ( GetAsyncKeyState ('E') & 0x8001 )
		{
			//키입력 종료처리
			closesocket (g_ListenSock);


			break;
		}
	}


	//스레드 종료 대기.
	WaitForMultipleObjects (WorkerThread_Max + 1, Thread, TRUE, INFINITE);
	
	
	for ( int Cnt = 0; Cnt < WorkerThread_Max + 1; Cnt++ )
	{
		CloseHandle (Thread[Cnt]);
	}
	CloseHandle (g_IOCP);

	if ( WSACleanup () )
	{
		wprintf (L"\n WSACleanUp_Error %d\n",GetLastError());
	}
		
	return 0;
}

bool InitializeNetwork (void)
{
	int retval;

	WSADATA wsaData;
	//윈속 초기화
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
	{
		return false;
	}

	//소켓초기화
	g_ListenSock = socket (AF_INET, SOCK_STREAM, 0);
	if ( g_ListenSock == SOCKET_ERROR )
	{
		return false;
	}


	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	InetPton (AF_INET, ServerIP, &addr.sin_addr);
	addr.sin_port = htons (PORT);

	//bind
	retval = bind (g_ListenSock, ( SOCKADDR * )&addr, sizeof (addr));
	if ( retval == SOCKET_ERROR )
	{
		return false;
	}

	//listen
	retval = listen (g_ListenSock, SOMAXCONN);
	if ( retval == SOCKET_ERROR )
	{
		return false;
	}

	//노딜레이 옵션 여부
	int option = FALSE;
	setsockopt (g_ListenSock, IPPROTO_TCP, TCP_NODELAY, ( const char* )&option, sizeof (option));
	return true;
}

unsigned int WINAPI AcceptThread (LPVOID pParam)
{
	wprintf (L"Accept_thread_Start\n");
	SOCKET hClientSock;
	SOCKADDR_IN ClientAddr;
	Session *p;
	int addrLen = sizeof (ClientAddr);

	while ( 1 )
	{
		hClientSock = accept (g_ListenSock, ( sockaddr * )&ClientAddr, &addrLen);
		if ( hClientSock == INVALID_SOCKET )
		{
			wprintf (L"\n\n\nAccept Error\n\n\n");
			continue;
		}

		p = FindSession (true);
		if ( p == NULL )
		{
			closesocket (hClientSock);
			wprintf (L"\nSession Full\n");
			break;
		}


		p->sock = hClientSock;
		p->SessionID = InterlockedIncrement64 ((volatile LONG64 *)&SessionID_Count);
		p->UseFlag = true;

		//IOCP 포트에 등록
		CreateIoCompletionPort (( HANDLE )p->sock, g_IOCP,p->SessionID, 0);

		PostRecv (p);

		InterlockedIncrement (( volatile long * )&AcceptClientCnt);
	}
	return 0;
}

unsigned int WINAPI WorkerThread (LPVOID pParam)
{
	wprintf (L"worker_thread_Start\n");
	DWORD Transferred;
	INT64 SessionID;
	OVERLAPPED *pOver;

	Session *p;
	INT64 Session_Count_Chk;

	while ( 1 )
	{
		Transferred = 0;
		SessionID = 0;
		pOver = NULL;

		GetQueuedCompletionStatus (g_IOCP, &Transferred, ( UINT64 * )&SessionID, &pOver, INFINITE);



		if ( Transferred == 0 )
		{

			if ( pOver == NULL && SessionID == 0 )
			{
				PostQueuedCompletionStatus (g_IOCP, 0, 0, NULL);
				break;
			}

			//전송데이터가 0 일 경우 해당 세션이 파괴된것이므로 close절차를 밟아나감.
			p = FindSession (false, SessionID);
			shutdown (p->sock, SD_BOTH);

			Session_Count_Chk = InterlockedDecrement64 (( volatile LONG64 * )& p->Session_Count);
			if ( Session_Count_Chk == 0 )
			{
				SessionClose (p);
			}
			//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
			else if ( Session_Count_Chk < 0 )
			{
				int* a = ( int* )1;
				int b = *a;
			}

			continue;
		}


		p = FindSession (false, SessionID);

		if ( p == NULL )
		{
			//존재하지 않는 유저이므로 예외처리하고 continue;
			continue;
		}

		//Recv일 경우
		if ( pOver == &p->RecvOver )
		{
			Packet pack;
			
			//p->RecvQ.Lock ();
			p->RecvQ.MoveWritePos (Transferred);
			p->RecvQ.Get (pack.GetBufferPtr(), Transferred);
			//p->RecvQ.Free ();

			pack.MoveWritePos (Transferred);

			p->SendQ.Lock ();
			p->SendQ.Put (pack.GetBufferPtr(), pack.GetDataSize());
			p->SendQ.Free ();

			PostSend (p);

			PostRecv (p);

			InterlockedAdd64 (( volatile LONG64 * )&g_RecvByte, Transferred);

		}
		else if(pOver == &p->SendOver)
		{
			p->SendQ.Lock ();

			p->SendQ.RemoveData (Transferred);

			p->SendQ.Free ();

			p->SendFlag = false;


			if ( p->SendQ.GetUseSize () > 0 )
			{
				PostSend (p);
			}
			

			InterlockedAdd64 (( volatile LONG64 * )&g_SendByte, Transferred);
		}

		Session_Count_Chk = InterlockedDecrement64 (( volatile LONG64 * )& p->Session_Count);
		if ( Session_Count_Chk == 0 )
		{
			SessionClose (p);
		}
		//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
		else if ( Session_Count_Chk < 0 )
		{
			int* a = ( int* )1;
			int b = *a;
		}
	}
	return 0;
}


Session *FindSession (bool NewFlag, UINT64 SessionID)
{
	int Cnt;
	if ( NewFlag == true )
	{
		for ( Cnt = 0; Cnt < Session_Max; Cnt++ )
		{
			if ( User_Array[Cnt].UseFlag == false )
			{
				break;
			}
		}
	}
	else
	{
		for ( Cnt = 0; Cnt < Session_Max; Cnt++ )
		{
			if ( User_Array[Cnt].UseFlag == true && User_Array[Cnt].SessionID == SessionID )
			{
				break;
			}
		}
	}

	//세션이 다 찾거나 찾는 세션이 존재하지 않을시.
	if ( Cnt >= Session_Max )
	{
		return NULL;
	}

	return &User_Array[Cnt];
}

void PostRecv (Session *p)
{
	int Cnt = 0;
	DWORD RecvByte;
	DWORD dwFlag=0;
	int retval;
	INT64 Session_Count_Chk;


	do
	{

		//세션카운트 1 증가.
		InterlockedIncrement64 (( volatile LONG64 * )&p->Session_Count);

		WSABUF buf[2];

		//p->RecvQ.Lock ();

		buf[0].buf = p->RecvQ.GetWriteBufferPtr ();
		buf[0].len = p->RecvQ.GetNotBrokenPutSize ();
		Cnt++;
		if ( p->RecvQ.GetFreeSize () > p->RecvQ.GetNotBrokenPutSize () )
		{
			buf[1].buf = p->RecvQ.GetBufferPtr ();
			buf[1].len = p->RecvQ.GetFreeSize () - p->RecvQ.GetNotBrokenPutSize ();
			Cnt++;
		}

		//p->RecvQ.Free ();

		memset (&p->RecvOver, 0, sizeof (p->RecvOver));

		//WSARecv등록
		retval = WSARecv (p->sock, buf, Cnt, &RecvByte, &dwFlag, &p->RecvOver, NULL);


		//에러체크
		if ( retval == SOCKET_ERROR )
		{
			DWORD Errcode = GetLastError ();

			//IO_PENDING이라면 문제없이 진행중이므로 그냥 빠져나옴.
			if ( Errcode == WSA_IO_PENDING )
			{
				break;
			}

			Session_Count_Chk = InterlockedDecrement64 (( volatile LONG64 * )& p->Session_Count);

			wprintf (L"\nSessionID = %lld, ErrorCode = %ld\n", p->SessionID, Errcode);

			if ( Session_Count_Chk == 0 )
			{
				SessionClose (p);
			}
			//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
			else if ( Session_Count_Chk < 0 )
			{
				int* a = ( int* )1;
				int b = *a;
			}
			else
			{
				shutdown (p->sock, SD_BOTH);
			}

		}
	} while ( 0 );

	return;
}

void PostSend (Session *p)
{
	int Cnt = 0;
	DWORD SendByte;
	DWORD dwFlag = 0;
	int retval;
	INT64 Session_Count_Chk;

	do
	{
		if ( InterlockedCompareExchange((volatile long *)&p->SendFlag,TRUE,FALSE) == TRUE)
		{
			break;
		}
		InterlockedIncrement64 (( volatile LONG64 * )& p->Session_Count);


		WSABUF buf[2];

		p->SendQ.Lock ();
		if ( p->SendQ.GetUseSize() <= 0 )
		{
			break;
		}

		buf[0].buf = p->SendQ.GetReadBufferPtr ();
		buf[0].len = p->SendQ.GetNotBrokenGetSize ();
		Cnt++;
		if ( p->SendQ.GetUseSize () > p->SendQ.GetNotBrokenGetSize () )
		{
			buf[1].buf = p->SendQ.GetBufferPtr ();
			buf[1].len = p->SendQ.GetUseSize () - p->SendQ.GetNotBrokenGetSize ();
			Cnt++;
		}

		p->SendQ.Free ();

		memset (&p->SendOver, 0, sizeof (p->SendOver));


		//WSASend 등록
		retval = WSASend (p->sock, buf, Cnt, &SendByte, dwFlag, &p->SendOver, NULL);

		//에러체크
		if ( retval == SOCKET_ERROR )
		{
			DWORD Errcode = GetLastError ();

			//IO_PENDING이라면 문제없이 진행중이므로 그냥 빠져나옴.
			if ( Errcode == WSA_IO_PENDING )
			{
				break;
			}

			Session_Count_Chk = InterlockedDecrement64 (( volatile LONG64 * )& p->Session_Count);

			wprintf (L"\nSessionID = %lld, ErrorCode = %ld\n", p->SessionID, Errcode);

			if ( Session_Count_Chk == 0 )
			{
				SessionClose (p);
			}
			//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
			else if ( Session_Count_Chk < 0 )
			{
				int* a = ( int* )1;
				int b = *a;
			}
			else
			{
				shutdown (p->sock, SD_BOTH);
			}
		}
	} while ( 0 );

	return;
}


void SessionClose (Session *p)
{
	closesocket (p->sock);
	
	p->RecvQ.ClearBuffer ();
	p->SendQ.ClearBuffer ();

	p->UseFlag = false;

	InterlockedDecrement (( volatile long * )&AcceptClientCnt);
	return;
}