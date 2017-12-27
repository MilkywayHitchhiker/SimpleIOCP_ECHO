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
	//���� �ʱ�ȭ �� Listen�۾�.
	if ( InitializeNetwork (ServerIP, PORT) == false)
	{
		return false;
	}

	//���� �迭 ����
	_Session_Max = Session_Max;
	Session_Array = new Session[Session_Max];

	//����ִ� ���ǹ迭 ��ȣ ���� ����.
	for ( int Cnt = 0; Cnt < Session_Max; Cnt++ )
	{
		if ( Session_Array[Cnt].UseFlag == false )
		{
			emptySession.push (Cnt);
		}
	}

	//��Ŷ���� �޸� Ǯ �ʱ�ȭ �۾�.
	Packet::initializePacketPool ();

	//IOCP ��Ʈ ���� �� ������ ����.
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

	
	//AcceptThread ����
	closesocket (_ListenSock);

	//���� ����
	for ( int Cnt = 0; Cnt < _Session_Max; Cnt++ )
	{
		if ( Session_Array[Cnt].UseFlag == true )
		{
			shutdown (Session_Array[Cnt].sock, SD_BOTH);
		}
	}



	//PQCS�� ��Ŀ ������ ����.
	WaitForSingleObject (&Thread[0], INFINITE);

	PostQueuedCompletionStatus (_IOCP, 0, 0, 0);

	//������ ���� ���.
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
		//Send���� �ʰ��� �ش� ������ ������ ������� �ȴ�.
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
	//����ī���Ͱ� 0���ϸ� �߸����������̹Ƿ� ũ���� �����Ѽ� Ȯ��.
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
		//Accept ���
		hClientSock = accept (_ListenSock, ( sockaddr * )&ClientAddr, &addrLen);
		if ( hClientSock == INVALID_SOCKET )
		{
			break;
		}

		//����ִ� ����ã��.
		//����ִ� ������ ���ٸ� �α׷� ����� ���� ����.
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

		//IOCP ��Ʈ�� ���
		CreateIoCompletionPort (( HANDLE )p->sock, _IOCP, ( ULONG_PTR )p, 0);


		//���ο� ������ �˸�.
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

		//IOCP ��ü ������
		if ( GQCS_Return == false && pOver == NULL )
		{
			Log->Log (L"Network", LOG_WARNING, L"IOCP GQCS ERROR");
			break;
		}


		//Transferred�� 0�� ������ ��� ó����
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
					//Transferred�� 0 �� ��� �ش� ������ �ı��Ȱ��̹Ƿ� ���������� ��Ƴ���.
				shutdown (pSession->sock, SD_BOTH);

				IODecrement (pSession);
			}


		}
		
		//���� Recv,Send ó����
		else
		{
			//Recv�� ���
			if ( pOver == &pSession->RecvOver )
			{
				pSession->RecvQ.MoveWritePos (Transferred);

				//��Ŷ ó��.
				while ( 1 )
				{
					short Header;

					int Size = pSession->RecvQ.GetUseSize ();

					if ( Size < sizeof (Header) )
					{
						break;
					}

					pSession->RecvQ.Peek (( char * )&Header, sizeof (Header));

					//����� ���� �ʴ´�. shutdown�ɰ� ����.
					if ( Header != 8 )
					{
						Log->Log (L"Network", LOG_DEBUG, L"SessionID 0x%p, Header %d", pSession->SessionID,Header);
						shutdown (pSession->sock, SD_BOTH);
						break;
					}
					
					//�����Ͱ� ���� ���� �ʾҴ�.
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
			//Send�� ���
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

	//�����ʱ�ȭ
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"WSA Start Up Failed");
		return false;
	}


	//�����ʱ�ȭ
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

	//index�� ã�� �ش� ������ id�� ���� ã�� ������ �ƴ� ���
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

	//RecvQ ���� ������ üũ 0 �̶�� ���������� �Ұ��̹Ƿ� �ش� ������ �����Ű�� �������´�.
	if ( p->RecvQ.GetFreeSize () <= 0 )
	{
		Log->Log (L"Network", LOG_WARNING, L"SessionID = %lld, WSABuffer 0 NotRecv", p->SessionID);

		shutdown (p->sock, SD_BOTH);
		IODecrement (p);

		return;
	}

	//WSARecv ���

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

	//WSASend ���� �� ��Ϻ�
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

	//����üũ
	if ( retval == SOCKET_ERROR )
	{
		DWORD Errcode = GetLastError ();

		//IO_PENDING�̶�� �������� �������̹Ƿ� �׳� ��������.
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