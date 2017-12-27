
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
private:

	static CMemoryPool<Packet> *PacketPool;
	int RefCnt = 0;

public:

	static void initializePacketPool ()
	{
		if ( PacketPool == NULL )
		{
			//���� 1�� new�� �Ҵ�޾Ƽ� ����ȴ�.
			PacketPool = new CMemoryPool<Packet>;
		}
		return;
	}

	//return: Packet *
	static Packet *Alloc (void)
	{
		PacketPool->LOCK ();
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
			PacketPool->LOCK ();
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
	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (int) �޸� Ǯ ���� ��ü ����
	//////////////////////////////////////////////////////////////////////////
	static int	GetMemoryPoolFullCount (void)
	{
		return PacketPool->GetMemoryPoolFullCount();
	}

	//////////////////////////////////////////////////////////////////////////
	// ���� ������� �� ������ ��´�.
	//
	// Parameters: ����.
	// Return: (int) ������� �� ����.
	//////////////////////////////////////////////////////////////////////////
	static int GetUseCount (void)
	{
		return PacketPool->GetUseCount ();
	}


};