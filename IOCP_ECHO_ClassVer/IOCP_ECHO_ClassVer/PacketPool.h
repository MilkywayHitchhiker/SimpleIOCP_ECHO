
#pragma once
#include <Windows.h>
#include "MemoryPool.h"
class Packet
{
public:
	enum PACKET
	{
		BUFFER_DEFAULT			= 10000		// 패킷의 기본 버퍼 사이즈.
	};

	// 생성자, 파괴자.
			Packet();
			Packet(int iBufferSize);
			Packet(const Packet &SrcPacket);

	virtual	~Packet();

	// 패킷 초기화.
	void	Initial(int iBufferSize = BUFFER_DEFAULT);

	//RefCnt를 1 증가시킴. 
	void	AddRefCnt (void);

	// RefCnt를 하나 차감시키고 REfCnt가 0이 되면 자기자신 delete하고 빠져나옴.
	void	Release(void);


	// 패킷 비우기.
	void	Clear(void);


	// 버퍼 사이즈 얻기.
	int		GetBufferSize(void) { return _iBufferSize; }

	// 현재 사용중인 사이즈 얻기.
	int		GetDataSize(void) { return _iDataSize; }



	// 버퍼 포인터 얻기.
	char	*GetBufferPtr(void) { return Buffer; }

	// 버퍼 Pos 이동.
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);





	//=================================================================================================
	// 연산자 오퍼레이터.
	//=================================================================================================
	Packet	&operator = (Packet &SrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	Packet	&operator << (BYTE byValue);
	Packet	&operator << (char chValue);

	Packet	&operator << (short shValue);
	Packet	&operator << (WORD wValue);

	Packet	&operator << (int iValue);
	Packet	&operator << (DWORD dwValue);
	Packet	&operator << (float fValue);

	Packet	&operator << (__int64 iValue);
	Packet	&operator << (double dValue);

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	Packet	&operator >> (BYTE &byValue);
	Packet	&operator >> (char &chValue);

	Packet	&operator >> (short &shValue);
	Packet	&operator >> (WORD &wValue);

	Packet	&operator >> (int &iValue);
	Packet	&operator >> (DWORD &dwValue);
	Packet	&operator >> (float &fValue);

	Packet	&operator >> (__int64 &iValue);
	Packet	&operator >> (double &dValue);




	// 데이타 얻기.
	// Return 복사한 사이즈.
	int		GetData(char *chpDest, int iSize);

	// 데이타 삽입.
	// Return 복사한 사이즈.
	int		PutData(char *chpSrc, int iSrcSize);


protected:

	//------------------------------------------------------------
	// 패킷버퍼 / 버퍼 사이즈.
	//------------------------------------------------------------
	char	BufferDefault[BUFFER_DEFAULT];
	char	*BufferExpansion;

	char	*Buffer;
	int		_iBufferSize;
	//------------------------------------------------------------
	// 패킷버퍼 시작 위치.
	//------------------------------------------------------------
	char	*DataFieldStart;
	char	*DataFieldEnd;


	//------------------------------------------------------------
	// 버퍼의 읽을 위치, 넣을 위치.
	//------------------------------------------------------------
	char	*ReadPos;
	char	*WritePos;


	//------------------------------------------------------------
	// 현재 버퍼에 사용중인 사이즈.
	//------------------------------------------------------------
	int		_iDataSize;

<<<<<<< HEAD:IOCP_ECHO_ClassVer/IOCP_ECHO_ClassVer/Packet.h
	//------------------------------------------------------------
	// 현재 Packet의 RefCnt
	//------------------------------------------------------------
	int iRefCnt;
=======
	



	//=================================================================================================
	//내장된 메모리풀
	//=================================================================================================
public:
>>>>>>> e28bb97cc9c0741d7d8da3b270a98b9ed06e4d6d:IOCP_ECHO_ClassVer/IOCP_ECHO_ClassVer/PacketPool.h

	static Hitchhiker::CMemoryPool<Packet> *PacketPool;
	int RefCnt=0;
public:
	static void InitializePacketPool (void)
	{
		if ( PacketPool == NULL )
		{
			PacketPool = new Hitchhiker::CMemoryPool<Packet>;
		}
		return;
	}
	//return: Packet *
	static Packet *Alloc (void)
	{

		if ( PacketPool == NULL )
		{
			//최초 1번 new로 할당받아서 저장된다.
			InterlockedCompareExchangePointer (( volatile PVOID * )PacketPool, NULL, new Hitchhiker::CMemoryPool<Packet>);
		}

		PacketPool->Lock ();
		Packet *p = PacketPool->Alloc ();
		PacketPool->Free ();

		p->AddRefCnt ();

		return p;

	}

	//return void
	static void Free (Packet *p)
	{
		if ( 0 == InterlockedDecrement (( volatile long * )&p->RefCnt) )
		{
			PacketPool->Lock ();
			PacketPool->Free (p);
			PacketPool->Free ();
		}

		return;
	}
	void  AddRefCnt ()
	{
		InterlockedIncrement (( volatile long * )&RefCnt);
		
		return;
	}

};