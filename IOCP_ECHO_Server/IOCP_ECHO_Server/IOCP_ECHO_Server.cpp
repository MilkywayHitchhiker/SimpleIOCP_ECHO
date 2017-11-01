// IOCP_ECHO_Server.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <process.h>

#include "RingBuffer.h"
#include "Packet.h"
#include "System_Log.h"

#define WorkerThread_Max 3
#define Session_Max 1000

#define ServerIP L"127.0.0.1"
#define PORT 6000

struct Session
{
	bool UseFlag = false;
	SOCKET sock;
	UINT64 SessionID;

	INT64 IOCount = 0;

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

bool SendPacket (Packet *pack, Session *p);
void PostSend (Session *p);

void PostRecv (Session *p);

void SessionRelease (Session *p);
void SessionKill (Session *p);

HANDLE g_IOCP;
SOCKET g_ListenSock;
Session User_Array[Session_Max];
UINT64 SessionID_Count = 1;

UINT64 g_RecvPacketTPS;
UINT64 g_SendPacketTPS;
int	AcceptTotal;
int AcceptTPS;
int ConnectSession_Cnt;


hiker::CSystemLog *Log = hiker::CSystemLog::GetInstance (LOG_DEBUG);


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

	Log->SetLogDirectory (L"../LOG_FILE");


	DWORD StartTime = GetTickCount ();
	DWORD EndTime;

	while ( 1 )
	{
		EndTime = GetTickCount ();
		if ( EndTime - StartTime >= 1000 )
		{
			wprintf (L"==========================\n");
			wprintf (L"Accept User Total = %ld \n", AcceptTotal);
			wprintf (L"AcceptTPS = %ld \n", AcceptTPS); 
			wprintf (L"Sec RecvTPS = %lld \n", g_RecvPacketTPS);
			wprintf (L"Sec SendTPS = %lld \n", g_SendPacketTPS);
			wprintf (L"Connect Session Cnt = %ld \n", ConnectSession_Cnt);
			wprintf (L"==========================\n");
			InterlockedExchange64 (( volatile LONG64 * )&g_RecvPacketTPS, 0);
			InterlockedExchange64 (( volatile LONG64 * )&g_SendPacketTPS, 0);
			InterlockedExchange (( volatile LONG * )&AcceptTPS, 0);
			StartTime = EndTime;
		}


		if ( GetAsyncKeyState ('E') & 0x8001 )
		{
			//키입력 종료처리
			closesocket (g_ListenSock);
			
			
			//세션 정리 및 PQCS 호출. 어떻게? = Accept스레드에서 종료하기 전에 세션 전부 닫고 종료.
			WaitForMultipleObjects (1, &Thread[0], TRUE, INFINITE);
			PostQueuedCompletionStatus (g_IOCP, 0, 0, 0);

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
		Log->Log (L"Network", LOG_DEBUG, L"\n WSACleanUp_Error %d\n", GetLastError ());
	}

	wprintf (L"main Thread End\n");
		
	return 0;
}

bool InitializeNetwork (void)
{
	int retval;

	WSADATA wsaData;
	//윈속 초기화
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"WSA Start Up Failed");
		return false;
	}

	//소켓초기화
	g_ListenSock = socket (AF_INET, SOCK_STREAM, 0);
	if ( g_ListenSock == SOCKET_ERROR )
	{
		Log->Log (L"Network", LOG_WARNING, L"Listen_Sock Failed");
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
		Log->Log (L"Network", LOG_WARNING, L"bind Failed");
		return false;
	}

	//listen
	retval = listen (g_ListenSock, SOMAXCONN);
	if ( retval == SOCKET_ERROR )
	{
		Log->Log (L"Network", LOG_WARNING, L"Listen Failed");
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
			break;
		}

		p = FindSession (true);
		if ( p == NULL )
		{
			closesocket (hClientSock);
			Log->Log (L"Network", LOG_SYSTEM, L"Session %dUser Full ", Session_Max);
			break;
		}


		p->sock = hClientSock;
		p->SessionID = InterlockedIncrement64 ((volatile LONG64 *)&SessionID_Count);
		p->UseFlag = true;
		InterlockedIncrement (( volatile long * )&ConnectSession_Cnt);


		//IOCP 포트에 등록
		CreateIoCompletionPort (( HANDLE )p->sock, g_IOCP,(ULONG_PTR) p, 0);

		PostRecv (p);

		InterlockedIncrement (( volatile long * )&AcceptTPS);
		InterlockedIncrement (( volatile long * )&AcceptTotal);
	}


	//세션 정리
	for ( int Cnt = 0; Cnt < Session_Max; Cnt++ )
	{
		if ( User_Array[Cnt].UseFlag == true )
		{
			shutdown (User_Array[Cnt].sock, SD_BOTH);
		}
	}

	wprintf (L"\n\n\nAccept Thread End\n\n\n");
	return 0;
}

unsigned int WINAPI WorkerThread (LPVOID pParam)
{
	wprintf (L"worker_thread_Start\n");
	DWORD Transferred;
	BOOL GQCS_Return;
	OVERLAPPED *pOver;

	Session *pSession;

	while ( 1 )
	{
		Transferred = 0;
		pSession = NULL;
		pOver = NULL;

		GQCS_Return = GetQueuedCompletionStatus (g_IOCP, &Transferred, (PULONG_PTR)&pSession, &pOver, INFINITE);

		//IOCP 자체 에러부
		if ( GQCS_Return == false && pOver == NULL )
		{
			Log->Log (L"Network", LOG_WARNING, L"IOCP GQCS ERROR");
			break;
		}


		//Transferred가 0이 나왔을 경우 처리부
		if ( Transferred == 0 )
		{

			if ( pOver == NULL && pSession == 0 )
			{
				PostQueuedCompletionStatus (g_IOCP, 0, 0, NULL);
				break;
			}
			else
			{
				//Transferred가 0 일 경우 해당 세션이 파괴된것이므로 종료절차를 밟아나감.

				shutdown (pSession->sock, SD_BOTH);

				if ( InterlockedDecrement64 (( volatile LONG64 * )& pSession->IOCount) == 0 )
				{
					SessionRelease (pSession);
				}
				//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
				else if ( pSession->IOCount < 0 )
				{
					int* a = ( int* )1;
					int b = *a;
				}
			}
		}

		//실제 Recv,Send 처리부
		else
		{
			//Recv일 경우
			if ( pOver == &pSession->RecvOver )
			{
				Packet pack;

				pSession->RecvQ.MoveWritePos (Transferred);
				pSession->RecvQ.Get (pack.GetBufferPtr (), Transferred);

				pack.MoveWritePos (Transferred);

				if ( SendPacket (&pack, pSession) )
				{
					PostRecv (pSession);

					InterlockedIncrement64 (( volatile LONG64 * )&g_RecvPacketTPS);
				}


			}
			else if ( pOver == &pSession->SendOver )
			{
				pSession->SendQ.Lock ();

				pSession->SendQ.RemoveData (Transferred);

				pSession->SendQ.Free ();

				pSession->SendFlag = false;


				if ( pSession->SendQ.GetUseSize () > 0 )
				{
					PostSend (pSession);
				}


				InterlockedIncrement64 (( volatile LONG64 * )&g_SendPacketTPS);

			}

			//IOCount 차감
			if ( InterlockedDecrement64 (( volatile LONG64 * )& pSession->IOCount) == 0 )
			{
				SessionRelease (pSession);
			}
			//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
			else if ( pSession->IOCount < 0 )
			{
				int* a = ( int* )1;
				int b = *a;
			}
		}
	}
	wprintf (L"\n\n\nWorker Thread End\n\n\n");
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
	int retval = -1;

		//세션카운트 1 증가.
		InterlockedIncrement64 (( volatile LONG64 * )&p->IOCount);


		//WSARecv 셋팅 및 등록부

		if ( p->RecvQ.GetFreeSize () < 0 )
		{
			wprintf (L"SendBuffer Overflow %lld\n", p->SessionID);
			Log->Log (L"Network", LOG_ERROR, L"Recv Buffer Free 0 Byte SessionID = %lld ", p->SessionID);
			//shutdown (p->sock, SD_BOTH);
			SessionKill (p);
		}
		else
		{
			WSABUF buf[2];

			buf[0].buf = p->RecvQ.GetWriteBufferPtr ();
			buf[0].len = p->RecvQ.GetNotBrokenPutSize ();
			Cnt++;
			if ( p->RecvQ.GetFreeSize () > p->RecvQ.GetNotBrokenPutSize () )
			{
				buf[1].buf = p->RecvQ.GetBufferPtr ();
				buf[1].len = p->RecvQ.GetFreeSize () - p->RecvQ.GetNotBrokenPutSize ();
				Cnt++;
			}


			memset (&p->RecvOver, 0, sizeof (p->RecvOver));


			retval = WSARecv (p->sock, buf, Cnt, &RecvByte, &dwFlag, &p->RecvOver, NULL);
		}



		//에러체크
		if ( retval == SOCKET_ERROR )
		{
			DWORD Errcode = GetLastError ();

			//IO_PENDING이라면 문제없이 진행중이므로 그냥 빠져나옴.
			if ( Errcode != WSA_IO_PENDING )
			{

				if ( Errcode == WSAENOBUFS )
				{
					Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, ErrorCode = %ld WSAENOBUFS ERROR ", p->SessionID, Errcode);
				}
				else if ( retval == -1 )
				{
					Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, Recv Buffer 0 NotRecv", p->SessionID);
				}
				else
				{
					Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld", p->SessionID, Errcode);
				}



				shutdown (p->sock, SD_BOTH);

				if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) )
				{
					SessionRelease (p);
				}
				//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
				else if ( p->IOCount < 0 )
				{
					int* a = ( int* )1;
					int b = *a;
				}
			}
		}

	return;
}

void PostSend (Session *p)
{
	int Cnt = 0;
	DWORD SendByte;
	DWORD dwFlag = 0;
	int retval;


	InterlockedIncrement64 (( volatile LONG64 * )& p->IOCount);

	if ( InterlockedCompareExchange (( volatile long * )&p->SendFlag, TRUE, FALSE) == TRUE )
	{

		if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) )
		{
			SessionRelease (p);
		}
		//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
		else if ( p->IOCount < 0 )
		{
			int* a = ( int* )1;
			int b = *a;
		}

		return;
	}


	//WSASend 셋팅 및 등록부

	WSABUF buf[2];
	
	buf[0].buf = p->SendQ.GetReadBufferPtr ();
	buf[0].len = p->SendQ.GetNotBrokenGetSize ();
	Cnt++;
	if ( p->SendQ.GetUseSize () > p->SendQ.GetNotBrokenGetSize () )
	{
		buf[1].buf = p->SendQ.GetBufferPtr ();
		buf[1].len = p->SendQ.GetUseSize () - p->SendQ.GetNotBrokenGetSize ();
		Cnt++;
	}
	
	memset (&p->SendOver, 0, sizeof (p->SendOver));



	retval = WSASend (p->sock, buf, Cnt, &SendByte, dwFlag, &p->SendOver, NULL);




	//에러체크
	if ( retval == SOCKET_ERROR )
	{
		DWORD Errcode = GetLastError ();

		//IO_PENDING이라면 문제없이 진행중이므로 그냥 빠져나옴.
		if ( Errcode != WSA_IO_PENDING )
		{

			if ( Errcode == WSAENOBUFS )
			{
				Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, ErrorCode = %ld WSAENOBUFS ERROR ", p->SessionID, Errcode);
			}
			else
			{
				Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld", p->SessionID, Errcode);
			}


			shutdown (p->sock, SD_BOTH);

			if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) )
			{
				SessionRelease (p);
			}
			//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
			else if ( p->IOCount < 0 )
			{
				int* a = ( int* )1;
				int b = *a;
			}
		}
	}

	return;
}


bool SendPacket (Packet *pack,Session *p)
{
	//Send버퍼 초과로 해당 세션을 강제로 끊어줘야 된다.
	p->SendQ.Lock ();


	if ( p->SendQ.GetFreeSize () < pack->GetDataSize () )
	{
		p->SendQ.Free ();
		wprintf (L"SendBuffer Overflow %lld\n", p->SessionID);
		Log->Log (L"Network", LOG_ERROR, L"SendBuffer Overflow SessionID = %lld ", p->SessionID);
		//shutdown (p->sock, SD_BOTH);
		SessionKill (p);
		return false;
	}



	p->SendQ.Put (pack->GetBufferPtr (), pack->GetDataSize ());
	p->SendQ.Free ();


	PostSend (p);

	return true;

}

void SessionRelease (Session *p)
{
	closesocket (p->sock);
	
	p->RecvQ.ClearBuffer ();
	p->SendQ.ClearBuffer ();

	p->UseFlag = false;

	InterlockedDecrement (( volatile long * )&ConnectSession_Cnt);
	return;
}


void SessionKill (Session *p)
{
	CancelIoEx (&p->sock, &p->SendOver);
	CancelIoEx (&p->sock, &p->RecvOver);
	return;
}