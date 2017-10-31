// IOCP_ECHO_Server.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
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

//Send,Recv���.
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

UINT64 g_RecvByte;
UINT64 g_SendByte;
int	AcceptClientCnt;


hiker::CSystemLog *Log = hiker::CSystemLog::GetInstance (LOG_DEBUG);


int main()
{
	wprintf (L"main_thread_Start\n");

	//Listen ���� �ʱ�ȭ
	if ( InitializeNetwork () == false )
	{
		wprintf (L"Listen ERROR\n");
	}

	//IOCP �ڵ� ����
	g_IOCP = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, WorkerThread_Max);


	//������ ����
	HANDLE Thread[WorkerThread_Max + 1];

	Thread[0] =(HANDLE) _beginthreadex (NULL, 0, AcceptThread, 0, NULL, NULL);
	for ( int Cnt = 0; Cnt < WorkerThread_Max; Cnt++ )
	{
		Thread[Cnt+1] = ( HANDLE )_beginthreadex (NULL, 0, WorkerThread, 0, NULL, NULL);
	}

	Log->SetLogDirectory (L"../IOCP_ECHO_Server/LOG_FILE");


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
			//Ű�Է� ����ó��
			closesocket (g_ListenSock);
			
			
			//���� ���� �� PQCS ȣ��. ���? = Accept�����忡�� �����ϱ� ���� ���� ���� �ݰ� ����.
			WaitForMultipleObjects (1, &Thread[0], TRUE, INFINITE);
			PostQueuedCompletionStatus (g_IOCP, 0, 0, 0);

			break;
		}
	}


	//������ ���� ���.
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
	//���� �ʱ�ȭ
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"WSA Start Up Failed");
		return false;
	}

	//�����ʱ�ȭ
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

	
	//������� �ɼ� ����
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

		//IOCP ��Ʈ�� ���
		CreateIoCompletionPort (( HANDLE )p->sock, g_IOCP,p->SessionID, 0);

		PostRecv (p);

		InterlockedIncrement (( volatile long * )&AcceptClientCnt);
	}


	//���� ����
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
	INT64 SessionID;
	BOOL GQCS_Return;
	OVERLAPPED *pOver;

	Session *p;

	while ( 1 )
	{
		Transferred = 0;
		SessionID = 0;
		pOver = NULL;

		GQCS_Return = GetQueuedCompletionStatus (g_IOCP, &Transferred, ( UINT64 * )&SessionID, &pOver, INFINITE);

		//IOCP ��ü ������
		if ( GQCS_Return == false && pOver == NULL )
		{
			Log->Log (L"Network", LOG_WARNING, L"IOCP GQCS ERROR");
			break;
		}


		//Transferred�� 0�� ������ ��� ó����
		if ( Transferred == 0 )
		{

			if ( pOver == NULL && SessionID == 0 )
			{
				PostQueuedCompletionStatus (g_IOCP, 0, 0, NULL);
				break;
			}
			else
			{
				//Transferred�� 0 �� ��� �ش� ������ �ı��Ȱ��̹Ƿ� ���������� ��Ƴ���.
				p = FindSession (false, SessionID);
				if ( p == NULL )
				{
					//�������� �ʴ� �����̹Ƿ� ����ó���ϰ� continue;
					Log->Log (L"Network", LOG_DEBUG, L"Not Found User %lld\n", SessionID);
					continue;
				}

				shutdown (p->sock, SD_BOTH);

				if ( InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) == 0 )
				{
					SessionRelease (p);
				}
				//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
				else if ( p->IOCount < 0 )
				{
					int* a = ( int* )1;
					int b = *a;
				}
			}
		}

		//���� Recv,Send ó����
		else
		{
			p = FindSession (false, SessionID);
			if ( p == NULL )
			{
				//�������� �ʴ� �����̹Ƿ� ����ó���ϰ� continue;
				Log->Log (L"Network", LOG_DEBUG, L"Not Found User %lld\n", SessionID);
				continue;
			}

			//Recv�� ���
			if ( pOver == &p->RecvOver )
			{
				Packet pack;

				p->RecvQ.MoveWritePos (Transferred);
				p->RecvQ.Get (pack.GetBufferPtr (), Transferred);

				pack.MoveWritePos (Transferred);

				if ( SendPacket (&pack, p) )
				{
					PostRecv (p);

					InterlockedAdd64 (( volatile LONG64 * )&g_RecvByte, Transferred);
				}


			}
			else if ( pOver == &p->SendOver )
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

			//IOCount ����
			if ( InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) == 0 )
			{
				SessionRelease (p);
			}
			//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
			else if ( p->IOCount < 0 )
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

	//������ �� ã�ų� ã�� ������ �������� ������.
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

		//����ī��Ʈ 1 ����.
		InterlockedIncrement64 (( volatile LONG64 * )&p->IOCount);


		//WSARecv ���� �� ��Ϻ�

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


		//����üũ
		if ( retval == SOCKET_ERROR )
		{
			DWORD Errcode = GetLastError ();

			//IO_PENDING�̶�� �������� �������̹Ƿ� �׳� ��������.
			if ( Errcode != WSA_IO_PENDING )
			{
				shutdown (p->sock, SD_BOTH);

				if ( Errcode == WSAENOBUFS )
				{
					Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, ErrorCode = %ld WSAENOBUFS ERROR ", p->SessionID, Errcode);
				}
				else
				{
					Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld", p->SessionID, Errcode);
				}

				if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) )
				{
					SessionRelease (p);
				}
				//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
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
		//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
		else if ( p->IOCount < 0 )
		{
			int* a = ( int* )1;
			int b = *a;
		}

		return;
	}


	//WSASend ���� �� ��Ϻ�

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




	//����üũ
	if ( retval == SOCKET_ERROR )
	{
		DWORD Errcode = GetLastError ();

		//IO_PENDING�̶�� �������� �������̹Ƿ� �׳� ��������.
		if ( Errcode != WSA_IO_PENDING )
		{
			shutdown (p->sock, SD_BOTH);

			if ( Errcode == WSAENOBUFS )
			{
				Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, ErrorCode = %ld WSAENOBUFS ERROR ", p->SessionID, Errcode);
			}
			else
			{
				Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld", p->SessionID, Errcode);
			}

			if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount) )
			{
				SessionRelease (p);
			}
			//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
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
	//Send���� �ʰ��� �ش� ������ ������ ������� �ȴ�.
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

	InterlockedDecrement (( volatile long * )&AcceptClientCnt);
	return;
}


void SessionKill (Session *p)
{
	CancelIoEx (&p->sock, &p->SendOver);
	CancelIoEx (&p->sock, &p->RecvOver);
	return;
}