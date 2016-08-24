#include "stdafx.h"
#include "DirectoryChangeHandler.h"


CDirectoryChangeHandler::CDirectoryChangeHandler()
{
}

CDirectoryChangeHandler::~CDirectoryChangeHandler()
{
}

long CDirectoryChangeHandler::AddRef()
{

}

long CDirectoryChangeHandler::Release()
{

}

long CDirectoryChangeHandler::CurrentRefCount() const
{

}

BOOL CDirectoryChangeHandler::UnWatchDirectory()
{

}


//////////////////////////////////////////////////////////////////////////
void CDirectoryChangeHandler::On_FileAdded(const CString& strFileName)
{

}

void CDirectoryChangeHandler::On_FileRemoved(const CString& strFileName)
{

}

void CDirectoryChangeHandler::On_FileNameChanged(const CString& strFileName, const CString& strNewFileName)
{

}

void CDirectoryChangeHandler::On_FileModified(const CString& strFileName)
{

}

void CDirectoryChangeHandler::On_ReadDirectoryChangesError(DWORD dwError, const CString& strDirectoryName)
{

}

void CDirectoryChangeHandler::On_WatchStarted(DWORD dwError, const CString & strDirectoryName)
{

}

void CDirectoryChangeHandler::On_WatchStopped(const CString & strDirectoryName)
{

}

bool CDirectoryChangeHandler::On_FilterNotification(DWORD dwNotifyAction, LPCTSTR szFileName, LPCTSTR szNewFileName)
{

}

void CDirectoryChangeHandler::SetChangedDirectoryName(const CString & strChangedDirName)
{

}

long CDirectoryChangeHandler::_ReferencesWatcher(CDirectoryChangeWatcher *pDirChangeWatcher)
{

}

long CDirectoryChangeHandler::_ReleaseReferenceToWatcher(CDirectoryChangeWatcher *pDirChangeWatcher)
{

}
