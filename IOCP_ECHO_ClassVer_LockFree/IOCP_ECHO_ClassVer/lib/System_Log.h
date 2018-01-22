#pragma once
#include "Library.h"
enum en_LOG_LEVEL
{
	LOG_DEBUG = 0,
	LOG_WARNING,
	LOG_ERROR,
	LOG_SYSTEM,
};


namespace hiker
{

	class CSystemLog
	{
	private:

		CSystemLog (en_LOG_LEVEL Level)
		{
			InitializeSRWLock (&_srwLock);
			setlocale (LC_ALL, "");
			_SaveLogLevel = Level;
			_LogNo = 0;
			return;
		}
		~CSystemLog (void)
		{

		}

		static CSystemLog *pLog;
	public:

		//------------------------------------------------------
		// �̱��� Ŭ����, 
		//------------------------------------------------------
		static CSystemLog *GetInstance (en_LOG_LEVEL Level)
		{
			if ( pLog == NULL )
			{
				pLog = new CSystemLog(Level);
			}
			return pLog;
		}

		//------------------------------------------------------
		// �ܺο��� �α׷��� ����
		//------------------------------------------------------
		void SetLogLevel (en_LOG_LEVEL LogLevel)
		{
			_SaveLogLevel = LogLevel;
			return;
		}

		//------------------------------------------------------
		// �α� ��� ����.
		//------------------------------------------------------
		void SetLogDirectory (WCHAR *szDirectory)
		{
			_wmkdir (szDirectory);
			wsprintf (_SaveDirectory, L"%s\\", szDirectory);
		}

		//------------------------------------------------------
		// ���� �α� ����� �Լ�.
		//------------------------------------------------------
		void Log (WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...)
		{
			time_t Time;
			struct tm t;

			WCHAR FileName[128];
			WCHAR buf[128] = { 0, };
			WCHAR Logstr[512] = { 0, };
			va_list va;
			FILE *fp;

			if ( LogLevel < _SaveLogLevel )
			{
				return;
			}


			Time = time (NULL);
			localtime_s (&t,&Time);

			wsprintf (FileName, L"%s\\%-4d_%-1d_%ls.txt", _SaveDirectory, t.tm_year + 1900, t.tm_mon + 1, szType);

			__int64 No = InterlockedIncrement64 (&_LogNo);


			wsprintf (buf, L"\n[%08I64d] [ %-5s ] [%04d-%02d-%02d %02d:%02d:%02d", No, szType, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

			switch ( LogLevel )
			{
			case LOG_DEBUG :
				StringCbCat (buf,128, L" / DEBUG ]  ");
				break;

			case LOG_WARNING:
				StringCbCat (buf, 128, L" / WARNING ]  ");
				break;
			case LOG_ERROR :
				StringCbCat (buf, 128, L" / ERROR ]  ");
				break;
			case LOG_SYSTEM :
				StringCbCat (buf, 128, L" / SYSTEM ]  ");
				break;
			}

			va_start (va, szStringFormat);
			StringCchVPrintf (Logstr, 512, szStringFormat, va);
			va_end (va);


			AcquireSRWLockExclusive (&_srwLock);

			_wfopen_s (&fp, FileName, L"a+t,ccs=UNICODE");

			fwprintf_s (fp,buf);
			fwprintf_s (fp,Logstr);

			fclose (fp);

			ReleaseSRWLockExclusive (&_srwLock);
			

		}
		/*
		//------------------------------------------------------
		// BYTE ���̳ʸ��� ���� �α� ���
		//------------------------------------------------------
		void LogHex (WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen)
		{

		}

		//------------------------------------------------------
		// SessionKey 64�� ��� ����. �̴� ���ڿ��� �ƴ϶� �������� ���� ����.
		//------------------------------------------------------
		void LogSessionKey (WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pSessionKey)
		{

		}
		*/

	private:

		__int64	_LogNo;
		SRWLOCK			_srwLock;

		en_LOG_LEVEL	_SaveLogLevel;
		WCHAR			_SaveDirectory[25];
	};


}
