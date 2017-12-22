
#pragma once
#include <Windows.h>
#include "MemoryPool.h"
class Packet
{
public:
	enum PACKET
	{
		BUFFER_DEFAULT			= 1024		// ��Ŷ�� �⺻ ���� ������.
	};

	// ������, �ı���.
			Packet();
			Packet(int iBufferSize);
			Packet(const Packet &SrcPacket);

	virtual	~Packet();

	// ��Ŷ �ʱ�ȭ.
	void	Initial(int iBufferSize = BUFFER_DEFAULT);
	// ��Ŷ  �ı�.
	void	Release(void);


	// ��Ŷ ����.
	void	Clear(void);


	// ���� ������ ���.
	int		GetBufferSize(void) { return _iBufferSize; }

	// ���� ������� ������ ���.
	int		GetDataSize(void) { return _iDataSize; }



	// ���� ������ ���.
	char	*GetBufferPtr(void) { return Buffer; }

	// ���� Pos �̵�.
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);





	//=================================================================================================
	// ������ ���۷�����.
	//=================================================================================================
	Packet	&operator = (Packet &SrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
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
	// ����.	�� ���� Ÿ�Ը��� ��� ����.
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




	// ����Ÿ ���.
	// Return ������ ������.
	int		GetData(char *chpDest, int iSize);

	// ����Ÿ ����.
	// Return ������ ������.
	int		PutData(char *chpSrc, int iSrcSize);



public:
	//ũ��Ƽ�� ����.
	CRITICAL_SECTION cs;


	//ũ��Ƽ�� ���� ��
	void Lock (void);
	//ũ��Ƽ�� ���� �� ����
	void Free (void);


protected:

	//------------------------------------------------------------
	// ��Ŷ���� / ���� ������.
	//------------------------------------------------------------
	char	BufferDefault[BUFFER_DEFAULT];
	char	*BufferExpansion;

	char	*Buffer;
	int		_iBufferSize;
	//------------------------------------------------------------
	// ��Ŷ���� ���� ��ġ.
	//------------------------------------------------------------
	char	*DataFieldStart;
	char	*DataFieldEnd;


	//------------------------------------------------------------
	// ������ ���� ��ġ, ���� ��ġ.
	//------------------------------------------------------------
	char	*ReadPos;
	char	*WritePos;


	//------------------------------------------------------------
	// ���� ���ۿ� ������� ������.
	//------------------------------------------------------------
	int		_iDataSize;

	



	//=================================================================================================
	//����� �޸�Ǯ
	//=================================================================================================
public:

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
			//���� 1�� new�� �Ҵ�޾Ƽ� ����ȴ�.
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