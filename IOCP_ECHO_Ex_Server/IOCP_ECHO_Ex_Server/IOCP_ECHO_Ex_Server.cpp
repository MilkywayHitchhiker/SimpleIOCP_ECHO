// IOCP_ECHO_Ex_Server.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define ServerIP "127.0.0.1"
#define ServerPort 5000

#define BUFSIZE 1024

typedef struct
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
	OVERLAPPED RecvOverlapped;
	OVERLAPPED SendOverlapped;
	char bufferSend[BUFSIZE];
	char bufferRecv[BUFSIZE];
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

/*
typedef struct
{
	OVERLAPPED overlapped;
	char buffer[BUFSIZE];
	WSABUF wsaBuf;
} PER_IO_DATA, *LPPER_IO_DATA;
*/
unsigned int __stdcall CompletionThread (LPVOID pComPort);
void ErrorHandling (char *message);

#pragma comment(lib, "ws2_32.lib")

int main (int argc, char** argv)
{
	WSADATA wsaData;
	if ( WSAStartup (MAKEWORD (2, 2), &wsaData) != 0 )
		ErrorHandling ("WSAStartup() error!");

	HANDLE hCompletionPort = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, 1);

	SYSTEM_INFO SystemInfo;
	GetSystemInfo (&SystemInfo);
	for ( int i = 0; i<1; ++i )
		_beginthreadex (NULL, 0, CompletionThread, ( LPVOID )hCompletionPort, 0, NULL);

	SOCKET hServSock = WSASocket (AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr (ServerIP);
	servAddr.sin_port = htons (ServerPort);

	bind (hServSock, ( SOCKADDR* )&servAddr, sizeof (servAddr));
	listen (hServSock, 5);

	//LPPER_IO_DATA PerIoData;
	LPPER_HANDLE_DATA PerHandleData;

	int RecvBytes;
	int Flags;

	while ( TRUE )
	{
		SOCKADDR_IN clntAddr;
		int addrLen = sizeof (clntAddr);

		SOCKET hClntSock = accept (hServSock, ( SOCKADDR* )&clntAddr, &addrLen);

		PerHandleData = ( LPPER_HANDLE_DATA )malloc (sizeof (PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy (&(PerHandleData->clntAddr), &clntAddr, addrLen);

		CreateIoCompletionPort (( HANDLE )hClntSock, hCompletionPort, ( DWORD )PerHandleData, 0);

		//PerIoData = ( LPPER_IO_DATA )malloc (sizeof (PER_IO_DATA));
		//memset (&(PerIoData->overlapped), 0, sizeof (OVERLAPPED));
		memset (&PerHandleData->RecvOverlapped, 0, sizeof (PerHandleData->RecvOverlapped));

		WSABUF buf;
		buf.len = BUFSIZE;
		buf.buf = PerHandleData->bufferRecv;
		Flags = 0;

		WSARecv (PerHandleData->hClntSock, &buf, 1, ( LPDWORD )&RecvBytes, ( LPDWORD )&Flags, &PerHandleData->RecvOverlapped, NULL);
	}

	return 0;
}

unsigned int __stdcall CompletionThread (LPVOID pComPort)
{
	HANDLE hCompletionPort = ( HANDLE )pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	//LPPER_IO_DATA PerIoData;
	OVERLAPPED *p;
	DWORD flags;

	while ( 1 )
	{
		GetQueuedCompletionStatus (hCompletionPort, &BytesTransferred, ( LPDWORD )&PerHandleData, ( LPOVERLAPPED* )&p, INFINITE);

		if ( BytesTransferred == 0 )
		{
			closesocket (PerHandleData->hClntSock);
			free (PerHandleData);
			continue;
		}

		//완료통지가 RecvOver일경우
		if ( p == &PerHandleData->RecvOverlapped )
		{
			printf ("RecvOver\n");

			memcpy (PerHandleData->bufferSend, PerHandleData->bufferRecv, BytesTransferred);

			PerHandleData->bufferRecv[BytesTransferred] = '\0';

			printf ("Recv[%s]\n", PerHandleData->bufferRecv);

			memset (&PerHandleData->SendOverlapped, 0, sizeof (PerHandleData->SendOverlapped));

			WSABUF buf;

			buf.len = BytesTransferred;
			buf.buf = PerHandleData->bufferSend;

			WSASend (PerHandleData->hClntSock, &buf, 1, NULL, 0, &PerHandleData->SendOverlapped, NULL);

			memset (&PerHandleData->RecvOverlapped, 0, sizeof (OVERLAPPED));

			WSABUF Recvbuf;
			Recvbuf.len = BUFSIZE;
			Recvbuf.buf = PerHandleData->bufferRecv;

			flags = 0;

			WSARecv (PerHandleData->hClntSock, &Recvbuf, 1, NULL, &flags, &PerHandleData->RecvOverlapped, NULL);
			continue;
		}

		//완료통지가 SendOver일경우
		if ( p == &PerHandleData->SendOverlapped )
		{
			printf ("SendOver\n");
			continue;
		}


	}

	return 0;
}

void ErrorHandling (char *message)
{
	fputs (message, stderr);
	fputc ('\n', stderr);
	exit (1);
}
