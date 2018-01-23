#pragma once


#ifndef __listStack__
#define __listStack__

template<class DATA>
class CStack
{
private:
	struct node
	{
		node *next;
		DATA data;
	};

	SRWLOCK _CS;
public:
	node *head;
	int Cnt;
	CStack ()
	{
		head = NULL;
		Cnt = 0;
		InitializeSRWLock (&_CS);
	}
	~CStack ()
	{

	}
	void push (DATA data)
	{
		node *temp = new node;
		temp->data = data;
		temp->next = head;
		head = temp;
		Cnt++;
	}
	DATA pop ()
	{
		if ( head == NULL )
		{
			return NULL;
		}
		node *temp = head;
		head = head->next;
		DATA data = temp->data;
		delete temp;
		Cnt--;
		return data;

	}
	bool empty (void)
	{
		if ( Cnt == 0 )
		{
			return true;
		}
		return false;
	}
	int NodeCnt (void)
	{
		return Cnt;
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

#endif