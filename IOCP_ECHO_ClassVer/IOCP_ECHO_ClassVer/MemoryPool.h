#pragma once

/*---------------------------------------------------------------

procademy MemoryPool.

�޸� Ǯ Ŭ����.
Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

- ����.

procademy::CMemoryPool<DATA> MemPool(300, FALSE);
DATA *pData = MemPool.Alloc();

pData ���

MemPool.Free(pData);


----------------------------------------------------------------*/
#ifndef  __MEMORY_POOL__
#define  __MEMORY_POOL__
#include <assert.h>
#include <new.h>
#include <malloc.h>
#pragma pack(1)

<<<<<<< HEAD
namespace Hitchhiker
{


	template <class DATA>
	class CMemoryPool
	{
#define SafeLine 0x89777789

	private:

		/* **************************************************************** */
		// �� �� �տ� ���� ��� ����ü.
		//�� �հ� �ڿ��� �ش� ����ü�� ������ üũ�� ������ġ�� ����.
		/* **************************************************************** */
		struct st_BLOCK_NODE
		{
			unsigned int FrontSafeLine;
			DATA T;
			st_BLOCK_NODE *stpNextBlock;
			unsigned int LastSafeLine;
		};

		//���س��
		st_BLOCK_NODE *HeadNode;
		

		void *DestroyPointer;
		//������ �۵���ų ��ġ
		//false�� ��� �޸�Ǯ ������ �����ڸ� �ѹ��� �Ҵ���.
		bool PlacementNew;

		//�ִ�ġ�� ������ �ִ°��
		bool BlockFlag;

		//�޸�Ǯ�� �Ҵ�� ��� ��ü ����
		int MemoryPoolNodeCnt;

		//���� ������� ��� ����
		int UseSize;

		SRWLOCK _CS;

	public:

		//////////////////////////////////////////////////////////////////////////
		// ������, �ı���.
		//
		// Parameters:	(int) �ִ� �� ����.
		//				(bool) ������ ȣ�� ����.
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool (int iBlockNum = 0, bool bPlacementNew = false)
		{
			HeadNode = NULL;
			PlacementNew = bPlacementNew;
			InitializeSRWLock (&_CS);
			if ( iBlockNum == 0 )
			{
				BlockFlag = false;
				return;
			}



			BlockFlag = true;

			if ( PlacementNew == false )
			{
				//���1. ������� ��ϵ��� ������ �ѹ��� ����� ���.
				st_BLOCK_NODE *NewBlock = ( st_BLOCK_NODE * )malloc (sizeof (st_BLOCK_NODE) * iBlockNum);
				DestroyPointer = NewBlock;
				for ( int cnt = iBlockNum - 1; cnt >= 0; cnt-- )
				{
					st_BLOCK_NODE *p = NewBlock + cnt;

					p->FrontSafeLine = SafeLine;
					new(&p->T) DATA ();
					p->stpNextBlock = HeadNode;
					p->LastSafeLine = SafeLine;

					HeadNode = p;
				}
				
				MemoryPoolNodeCnt = iBlockNum;

				/*
				//���2. ������ �˴� ���� �Ҵ� �ϴ� ���.
				for ( int cnt = 0; cnt < iBlockNum; cnt++ )
				{
				//���� ��带 �Ҵ����
				st_BLOCK_NODE *NewNode = new st_BLOCK_NODE;

				NewNode->FrontSafeLine = SafeLine;
				new(&p->T) DATA ();
				NewNode->stpNextBlock = HeadNode;

				NewNode->LastSafeLine = SafeLine;

				HeadNode = NewNode;

				}
				*/
				return;
			}



			////////////////////////////////////////////
			//Placement New�� true�϶�
			////////////////////////////////////////////
=======
template <class DATA>
class CMemoryPool
{
#define SafeLine 0x89777789

private:

	/* **************************************************************** */
	// �� �� �տ� ���� ��� ����ü.
	//�� �հ� �ڿ��� �ش� ����ü�� ������ üũ�� ������ġ�� ����.
	/* **************************************************************** */
	struct st_BLOCK_NODE
	{
		unsigned int FrontSafeLine;
		DATA T;
		st_BLOCK_NODE *stpNextBlock;
		unsigned int LastSafeLine;
	};

	//���س��
	st_BLOCK_NODE *HeadNode;


	void *DestroyPointer;
	//������ �۵���ų ��ġ
	//false�� ��� �޸�Ǯ ������ �����ڸ� �ѹ��� �Ҵ���.
	bool PlacementNew;

	//�ִ�ġ�� ������ �ִ°��
	bool BlockFlag;

	//�޸�Ǯ�� �Ҵ�� ��� ��ü ����
	int MemoryPoolNodeCnt;

	//���� ������� ��� ����
	int UseSize;

	SRWLOCK _CS;

public:

	//////////////////////////////////////////////////////////////////////////
	// ������, �ı���.
	//
	// Parameters:	(int) �ִ� �� ����.
	//				(bool) ������ ȣ�� ����.
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CMemoryPool (int iBlockNum = 0, bool bPlacementNew = false)
	{
		HeadNode = NULL;
		PlacementNew = bPlacementNew;
		InitializeSRWLock (&_CS);
		if ( iBlockNum == 0 )
		{
			BlockFlag = false;
			return;
		}



		BlockFlag = true;

		if ( PlacementNew == false )
		{
			//���1. ������� ��ϵ��� ������ �ѹ��� ����� ���.
>>>>>>> new1
			st_BLOCK_NODE *NewBlock = ( st_BLOCK_NODE * )malloc (sizeof (st_BLOCK_NODE) * iBlockNum);
			DestroyPointer = NewBlock;
			for ( int cnt = iBlockNum - 1; cnt >= 0; cnt-- )
			{
				st_BLOCK_NODE *p = NewBlock + cnt;

				p->FrontSafeLine = SafeLine;
<<<<<<< HEAD
=======
				new(&p->T) DATA ();
>>>>>>> new1
				p->stpNextBlock = HeadNode;
				p->LastSafeLine = SafeLine;

				HeadNode = p;
			}
<<<<<<< HEAD
			

			return;


		}
		virtual	~CMemoryPool ()
		{
			//�ΰ����� ������. Placement new�� true�� ��� false�ϰ��
			if ( PlacementNew == true )
			{
				//ó������ Maxġ�� �������� ���� ���
				if ( BlockFlag == true )
				{
					free (DestroyPointer);
					return;
				}

				//�ڵ��Ҵ����� ���� ���

				st_BLOCK_NODE *p;
				p = HeadNode;

				while ( p != NULL )
				{
					st_BLOCK_NODE *Delete;

					Delete = p;
					p = p->stpNextBlock;

					free (Delete);
				}

				return;

			}


			//Placement new�� false�� ���
			

			
			st_BLOCK_NODE *p;
			p = HeadNode;
			
			//ó������ Maxġ�� �������� ���� ���
			if ( BlockFlag == true )
			{
				//����� ���鼭 �ı��� ���� ȣ��
				while ( p != NULL )
				{
					p->T.~DATA ();
					p = p->stpNextBlock;
				}
=======

			MemoryPoolNodeCnt = iBlockNum;

			/*
			//���2. ������ �˴� ���� �Ҵ� �ϴ� ���.
			for ( int cnt = 0; cnt < iBlockNum; cnt++ )
			{
			//���� ��带 �Ҵ����
			st_BLOCK_NODE *NewNode = new st_BLOCK_NODE;

			NewNode->FrontSafeLine = SafeLine;
			new(&p->T) DATA ();
			NewNode->stpNextBlock = HeadNode;

			NewNode->LastSafeLine = SafeLine;

			HeadNode = NewNode;

			}
			*/
			return;
		}



		////////////////////////////////////////////
		//Placement New�� true�϶�
		////////////////////////////////////////////
		st_BLOCK_NODE *NewBlock = ( st_BLOCK_NODE * )malloc (sizeof (st_BLOCK_NODE) * iBlockNum);
		DestroyPointer = NewBlock;
		for ( int cnt = iBlockNum - 1; cnt >= 0; cnt-- )
		{
			st_BLOCK_NODE *p = NewBlock + cnt;

			p->FrontSafeLine = SafeLine;
			p->stpNextBlock = HeadNode;
			p->LastSafeLine = SafeLine;

			HeadNode = p;
		}


		return;


	}
	virtual	~CMemoryPool ()
	{
		//�ΰ����� ������. Placement new�� true�� ��� false�ϰ��
		if ( PlacementNew == true )
		{
			//ó������ Maxġ�� �������� ���� ���
			if ( BlockFlag == true )
			{
>>>>>>> new1
				free (DestroyPointer);
				return;
			}

			//�ڵ��Ҵ����� ���� ���

<<<<<<< HEAD

=======
			st_BLOCK_NODE *p;
			p = HeadNode;
>>>>>>> new1

			while ( p != NULL )
			{
				st_BLOCK_NODE *Delete;

				Delete = p;
				p = p->stpNextBlock;
<<<<<<< HEAD
				Delete->T.~DATA ();

				free (Delete);
			}
			return;



		}


		//////////////////////////////////////////////////////////////////////////
		// �� �ϳ��� �Ҵ�޴´�.
		//
		// Parameters: ����.
		// Return: (DATA *) ����Ÿ �� ������.
		//////////////////////////////////////////////////////////////////////////
		DATA	*Alloc (void)
		{
			//����尡 NULL�� ��� �ΰ����� ����.
			if ( HeadNode == NULL )
			{
				//���� �ִ�ġ�� �����Ǿ����� ���
				if ( BlockFlag == true )
				{
					return NULL;
				}

				//���� �ִ�ġ�� �����Ǿ����� ���� ��� ���� �Ҵ� �޾Ƽ� �������ش�.
				st_BLOCK_NODE *p =(st_BLOCK_NODE *) malloc (sizeof (st_BLOCK_NODE));
				p->FrontSafeLine = SafeLine;
				new(&p->T) DATA ();
				p->stpNextBlock = NULL;
				p->LastSafeLine = SafeLine;

				MemoryPoolNodeCnt++;
				UseSize += sizeof (st_BLOCK_NODE);
				return &p->T;

				
			}


			//p�����Ϳ� ���� ��� �ӽ�����. ��� ���� ������带 ������.
			st_BLOCK_NODE *p = HeadNode;
			HeadNode = HeadNode->stpNextBlock;
			
			//���� �÷��̽���Ʈ ���� ������̶�� ���⼭ ���� ���� �����ڸ� �������ش�.
			if ( PlacementNew == true )
			{
				new(&p->T) DATA ();
			}
			
			//�������͸� ��ȯ����.
			UseSize += sizeof (st_BLOCK_NODE);

			return &p->T;
		}

		//////////////////////////////////////////////////////////////////////////
		// ������̴� ���� �����Ѵ�.
		//
		// Parameters: (DATA *) �� ������.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free (DATA *pData)
		{
			//�� ���� �� �������� ���� ĳ����
			st_BLOCK_NODE *p = (st_BLOCK_NODE *) (( char *)pData - 4);

			//���� �����Ͱ� �޸�Ǯ���� ���� �� �����Ͱ� �´��� Ȯ��. �ƴ϶�� false��ȯ
			if ( p->FrontSafeLine != SafeLine || p->LastSafeLine != SafeLine )
			{
				return false;
			}

			//���� �����Ͱ� �޸�Ǯ�� �������Ͱ� �´ٸ� PlacementnewȮ���� �ı��� ȣ��
			if ( PlacementNew == true )
			{
				p->T.~DATA ();
			}

			//���� ���� �ؽ�Ʈ �� ������ ���� �� ��忡 �ž� �ִ´�.
			p->stpNextBlock = HeadNode;
			HeadNode = p;

			UseSize -= sizeof (st_BLOCK_NODE);
			return true;
		}


		//////////////////////////////////////////////////////////////////////////
		// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
		//
		// Parameters: ����.
		// Return: (int) �޸� Ǯ ���� ��ü ����
		//////////////////////////////////////////////////////////////////////////
		int		GetMemoryPoolFullCount (void)
		{
			return MemoryPoolNodeCnt;
		}

		//////////////////////////////////////////////////////////////////////////
		// ���� ������� �� ������ ��´�.
		//
		// Parameters: ����.
		// Return: (int) ������� �� ����.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount (void)
		{
			return UseSize / sizeof(st_BLOCK_NODE);
		}

		///////////////////////////////////////////////////////////////////////////
		//���� ��������. ��밡���� �� ������ ��´�.
		//ps. �����ڿ� ���ڷ� Placement new�� true�� �س��� �ʾҴٸ� ��ȯ�� ��ġ���� ���� �� �� ����.
		// Parameters: ����.
		// Return: (int) ��밡�� �� ����.
		///////////////////////////////////////////////////////////////////////////
		int		GetFreeCount (void)
		{
			return MemoryPoolNodeCnt - (UseSize / sizeof(st_BLOCK_NODE));
		}

		void Lock (void)
		{
			AcquireSRWLockExclusive (&_CS);
		}
		void Free (void)
		{
			ReleaseSRWLockExclusive (&_CS);
		}
	};

}
=======

				free (Delete);
			}

			return;

		}


		//Placement new�� false�� ���



		st_BLOCK_NODE *p;
		p = HeadNode;

		//ó������ Maxġ�� �������� ���� ���
		if ( BlockFlag == true )
		{
			//����� ���鼭 �ı��� ���� ȣ��
			while ( p != NULL )
			{
				p->T.~DATA ();
				p = p->stpNextBlock;
			}
			free (DestroyPointer);
			return;
		}

		//�ڵ��Ҵ����� ���� ���



		while ( p != NULL )
		{
			st_BLOCK_NODE *Delete;

			Delete = p;
			p = p->stpNextBlock;
			Delete->T.~DATA ();

			free (Delete);
		}
		return;



	}


	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA	*Alloc (void)
	{
		//����尡 NULL�� ��� �ΰ����� ����.
		if ( HeadNode == NULL )
		{
			//���� �ִ�ġ�� �����Ǿ����� ���
			if ( BlockFlag == true )
			{
				return NULL;
			}

			//���� �ִ�ġ�� �����Ǿ����� ���� ��� ���� �Ҵ� �޾Ƽ� �������ش�.
			st_BLOCK_NODE *p = ( st_BLOCK_NODE * )malloc (sizeof (st_BLOCK_NODE));
			p->FrontSafeLine = SafeLine;
			new(&p->T) DATA ();
			p->stpNextBlock = NULL;
			p->LastSafeLine = SafeLine;

			MemoryPoolNodeCnt++;
			UseSize += sizeof (st_BLOCK_NODE);
			return &p->T;


		}


		//p�����Ϳ� ���� ��� �ӽ�����. ��� ���� ������带 ������.
		st_BLOCK_NODE *p = HeadNode;
		HeadNode = HeadNode->stpNextBlock;

		//���� �÷��̽���Ʈ ���� ������̶�� ���⼭ ���� ���� �����ڸ� �������ش�.
		if ( PlacementNew == true )
		{
			new(&p->T) DATA ();
		}

		//�������͸� ��ȯ����.
		UseSize += sizeof (st_BLOCK_NODE);

		return &p->T;
	}

	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free (DATA *pData)
	{
		//�� ���� �� �������� ���� ĳ����
		st_BLOCK_NODE *p = ( st_BLOCK_NODE * )(( char * )pData - 4);

		//���� �����Ͱ� �޸�Ǯ���� ���� �� �����Ͱ� �´��� Ȯ��. �ƴ϶�� false��ȯ
		if ( p->FrontSafeLine != SafeLine || p->LastSafeLine != SafeLine )
		{
			return false;
		}

		//���� �����Ͱ� �޸�Ǯ�� �������Ͱ� �´ٸ� PlacementnewȮ���� �ı��� ȣ��
		if ( PlacementNew == true )
		{
			p->T.~DATA ();
		}

		//���� ���� �ؽ�Ʈ �� ������ ���� �� ��忡 �ž� �ִ´�.
		p->stpNextBlock = HeadNode;
		HeadNode = p;

		UseSize -= sizeof (st_BLOCK_NODE);
		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (int) �޸� Ǯ ���� ��ü ����
	//////////////////////////////////////////////////////////////////////////
	int		GetMemoryPoolFullCount (void)
	{
		return MemoryPoolNodeCnt;
	}

	//////////////////////////////////////////////////////////////////////////
	// ���� ������� �� ������ ��´�.
	//
	// Parameters: ����.
	// Return: (int) ������� �� ����.
	//////////////////////////////////////////////////////////////////////////
	int		GetUseCount (void)
	{
		return UseSize / sizeof (st_BLOCK_NODE);
	}

	///////////////////////////////////////////////////////////////////////////
	//���� ��������. ��밡���� �� ������ ��´�.
	//ps. �����ڿ� ���ڷ� Placement new�� true�� �س��� �ʾҴٸ� ��ȯ�� ��ġ���� ���� �� �� ����.
	// Parameters: ����.
	// Return: (int) ��밡�� �� ����.
	///////////////////////////////////////////////////////////////////////////
	int		GetFreeCount (void)
	{
		return MemoryPoolNodeCnt - (UseSize / sizeof (st_BLOCK_NODE));
	}

	void LOCK (void)
	{
		AcquireSRWLockExclusive (&_CS);
	}
	void Free (void)
	{
		ReleaseSRWLockExclusive (&_CS);
	}

};
>>>>>>> new1

#pragma pack(4)

#endif