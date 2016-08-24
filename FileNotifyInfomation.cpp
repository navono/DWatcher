#include "stdafx.h"
#include "FileNotifyInfomation.h"


CFileNotifyInfomation::CFileNotifyInfomation(BYTE* lpFileNotifyInfoBuffer, DWORD dwBuffSize)
	: _pBuffer(lpFileNotifyInfoBuffer)
	, _dwBufferSize(dwBuffSize)
{
	_pCurrentRecord = (PFILE_NOTIFY_INFORMATION)_pBuffer;
}

/***************
Sets the m_pCurrentRecord to the next FILE_NOTIFY_INFORMATION record.

Even if this return FALSE, (unless m_pCurrentRecord is NULL)
m_pCurrentRecord will still point to the last record in the buffer.
****************/
BOOL CFileNotifyInfomation::GetNextNotifyInformation()
{
	if (_pCurrentRecord != nullptr
		&& _pCurrentRecord->NextEntryOffset != 0UL)
	{
		// set the current record to point to the 'next' record
		PFILE_NOTIFY_INFORMATION pOld = _pCurrentRecord;
		_pCurrentRecord = (PFILE_NOTIFY_INFORMATION)
			((LPBYTE)_pCurrentRecord + _pCurrentRecord->NextEntryOffset);

		ASSERT((DWORD)((BYTE*)_pCurrentRecord - _pBuffer) < _dwBufferSize);

		if ((DWORD)((BYTE*)_pCurrentRecord - _pBuffer) > _dwBufferSize)
		{
			// we've gone too far.... this data is hosed.
			//
			// This sometimes happens if the watched directory becomes deleted... 
			// remove the FILE_SHARE_DELETE flag when using CreateFile() 
			// to get the handle to the directory...
			_pCurrentRecord = pOld;
		}

		return (BOOL)(_pCurrentRecord != pOld);
	}

	return FALSE;
}

/*****************************************
Copies the FILE_NOTIFY_INFORMATION record to the beginning of the buffer
specified in the constructor.

The size of the current record is returned in DWORD & dwSizeOfCurrentRecord.

*****************************************/
BOOL CFileNotifyInfomation::CopyCurrentRecordToBeginningOfBuffer(OUT DWORD& dwSizeOfCurrentRecord)
{
	if (_pCurrentRecord == nullptr)
	{
		return FALSE;
	}

	BOOL bRetVal = TRUE;

	// determine the size of the current record
	dwSizeOfCurrentRecord = sizeof(FILE_NOTIFY_INFORMATION);

	// substract out sizeof FILE_NOTIFY_INFOMATION::FileName[1]
	TCHAR FileName[1];	// same as is defined for FILE_NOTIFY_INFOMATION::FileName
	UNREFERENCED_PARAMETER(FileName);
	
	// and replace it w/ value of FILE_NOTIFY_INFOMATION::FileNameLength
	dwSizeOfCurrentRecord += _pCurrentRecord->FileNameLength;

	if ((void*)_pBuffer != (void*)_pCurrentRecord)
	{
		// copy the _pCurrentRecord to the beginning og _pBuffer;

		try
		{
			memcpy(_pBuffer, _pCurrentRecord, dwSizeOfCurrentRecord);
			bRetVal = TRUE;
		}
		catch (...)
		{
			LOGF(INFO, "EXCEPTION! probably because bytes overlapped in a call to memcpy()");
		}
	}
}

DWORD CFileNotifyInfomation::GetAction() const
{
	if (_pCurrentRecord != nullptr)
	{
		return _pCurrentRecord->Action;
	}

	return 0UL;
}

CString CFileNotifyInfomation::GetFileName() const
{
	if (_pCurrentRecord != nullptr)
	{
		TCHAR FileName[MAX_PATH + 1] = { 0 };
		memcpy(FileName,
			_pCurrentRecord->FileName,
			min((MAX_PATH * sizeof(TCHAR)), _pCurrentRecord->FileNameLength));

		return CString(FileName);
	}

	return "";
}

static inline bool HasTrailingBackslash(const CString& str)
{
	if (str.GetLength() > 0
		&& str[str.GetLength() - 1] == _T('\\'))
	{
		return true;
	}

	return false;
}

CString CFileNotifyInfomation::GetFileNameWithPath(const CString& rootPath) const
{
	auto strFileName(rootPath);
	if (!HasTrailingBackslash(rootPath))
	{
		strFileName.Append(_T("\\"));
	}

	strFileName += GetFileName();
	return strFileName;
}
