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
	if ( bServerOn == true )
	{
		return false;
	}

	Log->SetLogDirectory (L"../LOG_FILE");
	wprintf (L"\n NetworkModule Start \n");
	//소켓 초기화 및 Listen작업.
	if ( InitializeNetwork (ServerIP, PORT) == false)
	{
		return false;
	}

	//세션 배열 생성
	_Session_Max = Session_Max;
	Session_Array = new Session[Session_Max];

	//비어있는 세션배열 번호 전부 저장.
	for ( int Cnt = 0; Cnt < Session_Max; Cnt++ )
	{
		if ( Session_Array[Cnt].UseFlag == false )
		{
			emptySession.push (Cnt);
		}
	}

	//패킷내장 메모리 풀 초기화 작업.
	Packet::initializePacketPool ();

	//IOCP 포트 생성 및 스레드 생성.
	_WorkerThread_Num = WorkerThread_Num;
	_IOCP = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, WorkerThread_Num);

	Thread = new HANDLE[WorkerThread_Num + 1];

	Thread[0] = ( HANDLE )_beginthreadex (NULL,0,AcceptThread,(void *)this,NULL,NULL );

	for ( int Cnt = 0; Cnt < WorkerThread_Num; Cnt++ )
	{
		Thread[Cnt + 1] = ( HANDLE )_beginthreadex (NULL, 0, WorkerThread, (void *)this, NULL, NULL);
	}

	Log->Log (L"Network", LOG_SYSTEM, L" NetworkStart");
	bServerOn = true;
	return true;
}

bool CLanServer::Stop (void)
{
	if ( bServerOn == false )
	{
		return false;
	}

	
	//AcceptThread 종료
	closesocket (_ListenSock);

	//세션 정리
	for ( int Cnt = 0; Cnt < _Session_Max; Cnt++ )
	{
		if ( Session_Array[Cnt].UseFlag == true )
		{
			shutdown (Session_Array[Cnt].sock, SD_BOTH);
		}
	}



	//PQCS로 워커 스레드 끄기.
	WaitForSingleObject (&Thread[0], INFINITE);

	PostQueuedCompletionStatus (_IOCP, 0, 0, 0);

	//스레드 종료 대기.
	WaitForMultipleObjects (_WorkerThread_Num + 1, Thread, TRUE, INFINITE);

	wprintf (L"\nNetworkModule End \n");
	Log->Log (L"Network", LOG_SYSTEM, L" NetworkStop");
	bServerOn = false;
	return true;
}

void CLanServer::SendPacket (UINT64 SessionID, Packet *pack)
{

	Session *p = FindSession (SessionID);
	if ( p == NULL )
	{
		return;
	}

	do
	{
		//Send버퍼 초과로 해당 세션을 강제로 끊어줘야 된다.
		if ( p->SendQ.GetFreeSize () < 8 )
		{
			Log->Log (L"Network", LOG_ERROR, L"SendBuffer Overflow SessionID = %lld, ", p->SessionID);
			shutdown (p->sock, SD_BOTH);
			break;
		}

		pack->AddRefCnt ();
		p->SendQ.Lock ();
		p->SendQ.Put ((char *)&pack, 8);
		p->SendQ.Free ();
	} while ( 0 );


	PostSend (p);

	return;
}

void CLanServer::Disconnect (UINT64 SessionID)
{
	Session *p = FindSession (SessionID);

	shutdown (p->sock, SD_BOTH);

	return;
}

void CLanServer::IODecrement (Session * p)
{
	int Num =(int) InterlockedDecrement64 (( volatile LONG64 * )& p->IOCount);
	if ( Num == 0 )
	{
		SessionRelease (p);
	}
	//세션카운터가 0이하면 잘못만들은것이므로 크래쉬 일으켜서 확인.
	else if ( Num < 0 )
	{
		CCrashDump::Crash ();
	}
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

		//비어있는 세션찾기.
		//비어있는 세션이 없다면 로그로 남기고 서버 종료.
		emptySession.LOCK ();
		if ( emptySession.empty () )
		{
			closesocket (hClientSock);
			Log->Log (L"Network", LOG_SYSTEM, L"Session %d Use Full ", _Session_Max);
			break;
		}
		int Cnt = emptySession.pop ();
		emptySession.Free ();

		p = &Session_Array[Cnt];
		

		p->sock = hClientSock;
		p->SessionID = CreateSessionID (Cnt, InterlockedIncrement64 (( volatile LONG64 * )&_SessionID_Count));
		p->UseFlag = true;
		p->SendFlag = false;

		InterlockedIncrement (( volatile long * )&_Use_Session_Cnt);

		//IOCP 포트에 등록
		CreateIoCompletionPort (( HANDLE )p->sock, _IOCP, ( ULONG_PTR )p, 0);


		//새로운 접속자 알림.
		InterlockedIncrement64 (&p->IOCount);


		WCHAR IP[36];
		WSAAddressToString (( SOCKADDR * )&ClientAddr, sizeof (ClientAddr), NULL, IP, (DWORD *)&addrLen);
		if ( OnClientJoin (p->SessionID, IP, ntohs (ClientAddr.sin_port)) == false )
		{
			SessionRelease (p);
			continue;
		}

		PostRecv (p);

		InterlockedIncrement (( volatile long * )&_AcceptTPS);
		InterlockedIncrement (( volatile long * )&_AcceptTotal);
		
		IODecrement (p);
	}
	return;
}

unsigned int CLanServer::WorkerThread (LPVOID pParam)
{
	CLanServer *p = ( CLanServer * )pParam;
	wprintf (L"worker_thread_Start\n");

	p->WorkerThread ();


	wprintf (L"\nWorker Thread End\n");
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
				if ( pOver == &pSession->RecvOver )
				{
					Log->Log (L"Network", LOG_DEBUG, L"Session %lld, Transferred 0 RecvOver", pSession->SessionID);
				}
				else
				{
					Log->Log (L"Network", LOG_DEBUG, L"Session %lld, Transferred 0 SendOver", pSession->SessionID);
				}
					//Transferred가 0 일 경우 해당 세션이 파괴된것이므로 종료절차를 밟아나감.
				shutdown (pSession->sock, SD_BOTH);

				IODecrement (pSession);
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
						Log->Log (L"Network", LOG_DEBUG, L"SessionID 0x%p, Header %d", pSession->SessionID,Header);
						shutdown (pSession->sock, SD_BOTH);
						break;
					}
					
					//데이터가 전부 오지 않았다.
					if ( Size < sizeof (Header) + Header )
					{
						break;
					}

					pSession->RecvQ.RemoveData (sizeof (Header));

					Packet *Pack = Packet::Alloc ();

					pSession->RecvQ.Get (Pack->GetBufferPtr(),Header);

					Pack->MoveWritePos (Header);


					OnRecv (pSession->SessionID, Pack);

					Packet::Free(Pack);

					InterlockedIncrement (( volatile LONG * )&_RecvPacketTPS);
				}

				PostRecv (pSession);

			}
			//Send일 경우
			else if ( pOver == &pSession->SendOver )
			{

				OnSend (pSession->SessionID, Transferred);

				Packet *Pack;
				pSession->SendPack.LOCK ();
				while ( 1 )
				{
					if ( pSession->SendPack.empty() )
					{
						break;
					}

					Pack = pSession->SendPack.pop ();
					Packet::Free (Pack);
				}
				pSession->SendPack.Free ();

				
				pSession->SendFlag = FALSE;


				if ( pSession->SendQ.GetUseSize () > 0 )
				{
					PostSend (pSession);
				}
				
				InterlockedIncrement (( volatile LONG * )&_SendPacketTPS);
			}

			IODecrement (pSession);
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


	return true;

}

CLanServer::Session * CLanServer::FindSession (UINT64 SessionID)
{
	int Cnt;

	Cnt = indexSessionID (SessionID);

	//index로 찾은 해당 세션의 id가 내가 찾던 세션이 아닐 경우
	if ( Session_Array[Cnt].UseFlag == false || Session_Array[Cnt].SessionID != SessionID )
	{
		Log->Log (L"Network", LOG_WARNING, L"SessionID not match search = %p, Array = %p",SessionID, Session_Array[Cnt].SessionID);
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
		IODecrement (p);

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

	memset (&p->RecvOver, 0, sizeof (p->RecvOver));

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
				Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld PostRecv", p->SessionID, Errcode);
			}

			shutdown (p->sock, SD_BOTH);
			IODecrement (p);
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
		IODecrement (p);
		return;
	}


	if ( p->SendQ.GetUseSize () <= 0 )
	{
		Log->Log (L"Network", LOG_DEBUG, L"SessionID = %lld, 0 Buffer PostSend", p->SessionID);
		
		IODecrement (p);
		p->SendFlag = FALSE;
		return;
	}

	//WSASend 셋팅 및 등록부
	WSABUF buf[SendbufMax];

	Packet *pack;
	p->SendQ.Lock ();
	while ( 1 )
	{
		if ( p->SendQ.GetUseSize () <= 0 || Cnt == SendbufMax-1)
		{
			break;
		}

		p->SendQ.Get ((char *)&pack, 8);

		buf[Cnt].buf = (char *)pack->GetBufferPtr();
		buf[Cnt].len = pack->GetDataSize ();
		Cnt++;
		
		p->SendPack.push (pack);

	}
	p->SendQ.Free ();

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
				Log->Log (L"Network", LOG_SYSTEM, L"SessionID = %lld, ErrorCode = %ld PostSend", p->SessionID, Errcode);
			}

			shutdown (p->sock, SD_BOTH);
			IODecrement (p);
		}
	}
	return;
}




void CLanServer::SessionRelease (Session * p)
{
	OnClientLeave (p->SessionID);

	closesocket (p->sock);

	p->RecvQ.ClearBuffer ();
	p->SendQ.ClearBuffer ();

	p->UseFlag = false;

	emptySession.LOCK ();
	emptySession.push (indexSessionID (p->SessionID));
	emptySession.Free ();

	InterlockedDecrement (( volatile long * )&_Use_Session_Cnt);

	return;
}