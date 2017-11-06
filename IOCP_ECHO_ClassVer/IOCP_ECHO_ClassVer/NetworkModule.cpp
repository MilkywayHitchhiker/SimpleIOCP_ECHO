#include"stdafx.h"
#include "NetworkModule.h"
#include "System_Log.h"

hiker::CSystemLog *Log = hiker::CSystemLog::GetInstance (LOG_DEBUG);


CLanServer::CLanServer (void)
{
	bServerOn = false;
}

CLanServer::~CLanServer (void)
{
	if ( bServerOn )
	{
		Stop ();
	}
}

bool CLanServer::Start (WCHAR * ServerIP, int PORT, int Session_Max, int WorkerThread_Num)
{

	Log->SetLogDirectory (L"../LOG_FILE");
	wprintf (L"\n NetworkModule Start \n");
	//소켓 초기화 및 Listen작업.
	if ( InitializeNetwork (ServerIP, PORT) == false)
	{
		return false;
	}



	//세션 배열 생성 및 크리티컬 섹션 초기화
	_Session_Max = Session_Max;
	Session_Array = new Session[Session_Max];

	for ( int Cnt = 0; Cnt < Session_Max; Cnt++ )
	{
		InitializeCriticalSection (&Session_Array[Cnt].SessionCS);
	}



	//IOCP 포트 생성 및 스레드 생성.
	_WorkerThread_Num = WorkerThread_Num;
	_IOCP = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, WorkerThread_Num);

	Thread = new HANDLE[WorkerThread_Num + 1];

	Thread[0] = ( HANDLE )_beginthreadex (NULL,0,AcceptThread,(void *)this,NULL,NULL );

	for ( int Cnt = 0; Cnt < WorkerThread_Num; Cnt++ )
	{
		Thread[Cnt + 1] = ( HANDLE )_beginthreadex (NULL, 0, WorkerThread, (void *)this, NULL, NULL);
	}


	bServerOn = true;
	return true;
}

bool CLanServer::Stop (void)
{
	if ( bServerOn == false )
	{
		return false;
	}

	//세션정리. 스레드 정리. 

	//키입력 종료처리
	closesocket (_ListenSock);

	//세션 정리
	for ( int Cnt = 0; Cnt < _Session_Max; Cnt++ )
	{
		if ( Session_Array[Cnt].UseFlag == true )
		{
			shutdown (Session_Array[Cnt].sock, SD_BOTH);
		}

		DeleteCriticalSection (&Session_Array[Cnt].SessionCS);
	}

	//PQCS로 워커 스레드 끄기.
	WaitForSingleObject (&Thread[0], INFINITE);
	PostQueuedCompletionStatus (_IOCP, 0, 0, 0);

	for ( int Cnt = 0; Cnt < _WorkerThread_Num + 1; Cnt++ )
	{
		CloseHandle (Thread[Cnt+1]);
	}
	CloseHandle (_IOCP);

	if ( WSACleanup () )
	{
		Log->Log (L"Network", LOG_DEBUG, L"\n WSACleanUp_Error %d\n", GetLastError ());
	}

	//스레드 종료 대기.
	WaitForMultipleObjects (_WorkerThread_Num + 1, Thread, TRUE, INFINITE);

	wprintf (L"\nNetworkModule End \n");

	return true;
}

void CLanServer::SendPacket (UINT64 SessionID, Packet *pack)
{

	Session *p = FindSession (false, SessionID);
	if ( p == NULL )
	{
		return;
	}

	EnterCriticalSection (&p->SessionCS);

	do
	{
		//락을 걸고 들어오기 직전에 해당 세션이 삭제된 경우
		if ( p->UseFlag == false || p->SessionID != SessionID )
		{
			break;
		}

		p->SendQ.Lock ();

		//Send버퍼 초과로 해당 세션을 강제로 끊어줘야 된다.
		if ( p->SendQ.GetFreeSize () < pack->GetDataSize () )
		{
			p->SendQ.Free ();
			Log->Log (L"Network", LOG_ERROR, L"SendBuffer Overflow SessionID = %lld ", p->SessionID);
			shutdown (p->sock, SD_BOTH);
			//SessionKill (p);
			break;
		}

		short header = pack->GetDataSize ();

		p->SendQ.Put (( char * )&header, sizeof (header));
		p->SendQ.Put (pack->GetBufferPtr (), pack->GetDataSize ());
		p->SendQ.Free ();

		PostSend (p);

	} while ( 0 );


	LeaveCriticalSection (&p->SessionCS);


	return;
}

void CLanServer::Disconnect (UINT64 SessionID)
{
	Session *p = FindSession (false, SessionID);

	EnterCriticalSection (&p->SessionCS);

	shutdown (p->sock, SD_BOTH);

	LeaveCriticalSection (&p->SessionCS);

	return;
}

unsigned int CLanServer::AcceptThread (LPVOID pParam)
{
	CLanServer *p = (CLanServer * )pParam;

	wprintf (L"Accept_thread_Start\n");

	p->AcceptThread ();

	wprintf (L"\n\n\nAccept Thread End\n\n\n");

	return 0;
}

void CLanServer::AcceptThread (void)
{

	SOCKET hClientSock = 0;
	SOCKADDR_IN ClientAddr;
	Session *p;
	int addrLen;

	while ( 1 )
	{
		addrLen = sizeof (ClientAddr);
		//Accept 대기
		hClientSock = accept (_ListenSock, ( sockaddr * )&ClientAddr, &addrLen);
		if ( hClientSock == INVALID_SOCKET )
		{
			break;
		}

		//Find Session;
		p = FindSession (true);
		if ( p == NULL )
		{
			closesocket (hClientSock);
			Log->Log (L"Network", LOG_SYSTEM, L"Session %d Use Full ", _Session_Max);
			break;
		}

		p->sock = hClientSock;
		p->SessionID = InterlockedIncrement64 (( volatile LONG64 * )&_SessionID_Count);
		p->UseFlag = true;


		InterlockedIncrement (( volatile long * )&_Use_Session_Cnt);

		//IOCP 포트에 등록
		CreateIoCompletionPort (( HANDLE )p->sock, _IOCP, ( ULONG_PTR )p, 0);


		//새로운 접속자 알림.
		WCHAR IP[24];
		WSAAddressToString (( SOCKADDR * )&ClientAddr, sizeof (ClientAddr), NULL, IP, (DWORD *)&addrLen);
		if ( OnClientJoin (p->SessionID, IP, ntohs (ClientAddr.sin_port)) == false )
		{
			SessionRelease (p);
			continue;
		}


		PostRecv (p);

		InterlockedIncrement (( volatile long * )&_AcceptTPS);
		InterlockedIncrement (( volatile long * )&_AcceptTotal);
	}
	return;
}

unsigned int CLanServer::WorkerThread (LPVOID pParam)
{
	CLanServer *p = ( CLanServer * )pParam;
	wprintf (L"worker_thread_Start\n");

	p->WorkerThread ();


	wprintf (L"\n\n\nWorker Thread End\n\n\n");
	return 0;
}

void CLanServer::WorkerThread (void)
{

	BOOL GQCS_Return;

	DWORD Transferred;
	OVERLAPPED *pOver;
	Session *pSession;

	while ( 1 )
	{
		Transferred = 0;
		pSession = NULL;
		pOver = NULL;

		GQCS_Return = GetQueuedCompletionStatus (_IOCP, &Transferred, ( PULONG_PTR )&pSession, &pOver, INFINITE);

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
				PostQueuedCompletionStatus (_IOCP, 0, 0, NULL);
				break;
			}
			else
			{
				//Transferred가 0 일 경우 해당 세션이 파괴된것이므로 종료절차를 밟아나감.
				shutdown (pSession->sock, SD_BOTH);
				if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& pSession->IOCount) )
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
				pSession->RecvQ.MoveWritePos (Transferred);




				//패킷 처리.
				while ( 1 )
				{
					short Header;
					int Size = pSession->RecvQ.GetUseSize ();


					if ( Size < sizeof (Header) )
					{
						break;
					}

					pSession->RecvQ.Peek (( char * )&Header, sizeof (Header));

					//헤더가 맞지 않는다. shutdown걸고 빠짐.
					if ( Header != 8 )
					{
						shutdown (pSession->sock, SD_BOTH);
						break;
					}
					
					//데이터가 전부 오지 않았다.
					if ( Size < sizeof (Header) + Header )
					{
						break;
					}

					pSession->RecvQ.RemoveData (sizeof (Header));

					

					Packet Pack;
					pSession->RecvQ.Get (Pack.GetBufferPtr(),Header);

					Pack.MoveWritePos (Header);

					OnRecv (pSession->SessionID, &Pack);


					InterlockedIncrement (( volatile LONG * )&_RecvPacketTPS);
				}
			
					PostRecv (pSession);

			}
			//Send일 경우
			else if ( pOver == &pSession->SendOver )
			{

				pSession->SendQ.RemoveData (Transferred);

				pSession->SendFlag = false;

				OnSend (pSession->SessionID, Transferred);

				if ( pSession->SendQ.GetUseSize () > 0 )
				{
					PostSend (pSession);
				}

				InterlockedIncrement (( volatile LONG * )&_SendPacketTPS);
			}

			if ( 0 == InterlockedDecrement64 (( volatile LONG64 * )& pSession->IOCount) )
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
}







bool CLanServer::InitializeNetwork (WCHAR *IP, int PORT)
{
	int retval;
	WSADATA wsaData;

	//윈속초기화
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"WSA Start Up Failed");
		return false;
	}


	//소켓초기화
	_ListenSock = socket (AF_INET, SOCK_STREAM, 0);
	if ( _ListenSock == SOCKET_ERROR )
	{
		Log->Log (L"Network", LOG_WARNING, L"Listen_Sock Failed");
		return false;
	}



	//bind
	SOCKADDR_IN addr;

	addr.sin_family = AF_INET;
	InetPton (AF_INET, IP, &addr.sin_addr);
	addr.sin_port = htons (PORT);

	retval = bind (_ListenSock, ( SOCKADDR * )&addr, sizeof (addr));
	if ( retval == SOCKET_ERROR )
	{
		Log->Log (L"Network", LOG_WARNING, L"bind Failed");
		return false;
	}


	//listen
	retval = listen (_ListenSock, SOMAXCONN);
	if ( retval == SOCKET_ERROR )
	{
		Log->Log (L"Network", LOG_WARNING, L"Listen Failed");
		return false;
	}

	/*
	//노딜레이 옵션 여부
	int option = FALSE;
	setsockopt (_ListenSock, IPPROTO_TCP, TCP_NODELAY, ( const char* )&option, sizeof (option));
	*/
	return true;

}

CLanServer::Session * CLanServer::FindSession (bool NewFlag, UINT64 SessionID)
{
	int Cnt;
	if ( NewFlag == true )
	{
		for ( Cnt = 0; Cnt < _Session_Max; Cnt++ )
		{
			if ( Session_Array[Cnt].UseFlag == false )
			{
				break;
			}
		}
	}
	else
	{
		for ( Cnt = 0; Cnt < _Session_Max; Cnt++ )
		{
			if ( Session_Array[Cnt].UseFlag == true && Session_Array[Cnt].SessionID == SessionID )
			{
				break;
			}
		}
	}

	//세션이 다 찾거나 찾는 세션이 존재하지 않을시.
	if ( Cnt >= _Session_Max )
	{
		return NULL;
	}

	return &Session_Array[Cnt];
}

void CLanServer::PostRecv (Session * p)
{
	int Cnt = 0;
	DWORD RecvByte;
	DWORD dwFlag = 0;
	int retval;

	InterlockedIncrement64 (&p->IOCount);

	//RecvQ 버퍼 사이즈 체크 0 이라면 정상진행이 불가이므로 해당 세션을 종료시키고 빠져나온다.
	if ( p->RecvQ.GetFreeSize () <= 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, WSABuffer 0 NotRecv", p->SessionID);

		shutdown (p->sock, SD_BOTH);
		//SessionKill (p);

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

	//WSARecv 등록

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

	memset (&p->RecvOver, 0, sizeof (OVERLAPPED));

	retval = WSARecv (p->sock, buf, Cnt, &RecvByte, &dwFlag, &p->RecvOver, NULL);


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

void CLanServer::PostSend (Session * p)
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




void CLanServer::SessionRelease (Session * p)
{
	OnClientLeave (p->SessionID);

	EnterCriticalSection (&p->SessionCS);

	closesocket (p->sock);

	p->RecvQ.ClearBuffer ();
	p->SendQ.ClearBuffer ();

	p->UseFlag = false;

	LeaveCriticalSection (&p->SessionCS);

	InterlockedDecrement (( volatile long * )&_Use_Session_Cnt);

	return;
}
