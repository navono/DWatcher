#include "stdafx.h"
#include "DirectoryChangeHandler.h"


CDirectoryChangeHandler::CDirectoryChangeHandler()
	: _nRefCount(0)
	, _nWatcherRefCount(0)
	, _strChangedDirectoryName("")
{
}

CDirectoryChangeHandler::~CDirectoryChangeHandler()
{
}

long CDirectoryChangeHandler::AddRef()
{
	return InterlockedIncrement(&_nRefCount);
}

long CDirectoryChangeHandler::Release()
{
	long refCnt = -1;
	if ((refCnt = InterlockedDecrement(&_nRefCount)) == 0)
	{
		delete this;
	}

	return refCnt;
}

long CDirectoryChangeHandler::CurrentRefCount() const
{
	return _nRefCount;
}

BOOL CDirectoryChangeHandler::UnWatchDirectory()
{
	std::lock_guard<std::mutex> lock(_mutWatcher);

	if (_pDirChangeWatcher != nullptr)
	{
		return _pDirChangeWatcher->UnwatichDirectory(this);
	}

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
void CDirectoryChangeHandler::On_FileAdded(const CString& strFileName)
{
	LOGF(INFO, _T("The following file was added: %s\n"), strFileName);
}

void CDirectoryChangeHandler::On_FileRemoved(const CString& strFileName)
{
	LOGF(INFO, _T("The following file was removed: %s\n"), strFileName);
}

void CDirectoryChangeHandler::On_FileNameChanged(const CString& strFileName, const CString& strNewFileName)
{
	LOGF(INFO, _T("The file %s was RENAMED to %s\n"), strFileName, strNewFileName);
}

void CDirectoryChangeHandler::On_FileModified(const CString& strFileName)
{
	LOGF(INFO, _T("The following file was modified: %s\n"), strFileName);
}

void CDirectoryChangeHandler::On_ReadDirectoryChangesError(DWORD dwError, const CString& strDirectoryName)
{
	LOGF(FATAL, _T("An error has occurred on a watched directory!\n, This directory has become unwatched! -- %s \n"), strDirectoryName);
	LOGF(FATAL, _T("ReadDirectoryChangesW has failed! %d"), dwError);
}

void CDirectoryChangeHandler::On_WatchStarted(DWORD dwError, const CString & strDirectoryName)
{
	if (dwError == 0)
	{
		LOGF(INFO, _T("A watch has begun on the following directory: %s\n"), strDirectoryName);
	}
	else
	{
		LOGF(INFO, _T("A watch failed to start on the following directory: (Error: %d) %s\n"), dwError, strDirectoryName);
	}
}

void CDirectoryChangeHandler::On_WatchStopped(const CString & strDirectoryName)
{
	LOGF(INFO, _T("The watch on the following directory has stopped: %s\n"), strDirectoryName);
}

//	This function gives your class a chance to filter unwanted notifications.
//
//	PARAMETERS: 
//			DWORD	dwNotifyAction	-- specifies the event to filter
//			LPCTSTR szFileName		-- specifies the name of the file for the event.
//			LPCTSTR szNewFileName	-- specifies the new file name of a file that has been renamed.
//
//	RETURN VALUE:
//			return true from this function, and you will receive the notification.
//			return false from this function, and your class will NOT receive the notification.
//
//	Valid values of dwNotifyAction:
//		FILE_ACTION_ADDED			-- On_FileAdded() is about to be called.
//		FILE_ACTION_REMOVED			-- On_FileRemoved() is about to be called.
//		FILE_ACTION_MODIFIED		-- On_FileModified() is about to be called.
//		FILE_ACTION_RENAMED_OLD_NAME-- On_FileNameChanged() is about to be call.
//
//	  
//	NOTE:  When the value of dwNotifyAction is FILE_ACTION_RENAMED_OLD_NAME,
//			szFileName will be the old name of the file, and szNewFileName will
//			be the new name of the renamed file.
//
//  The default implementation always returns true, indicating that all notifications will 
//	be sent.
//	
bool CDirectoryChangeHandler::On_FilterNotification(DWORD dwNotifyAction, LPCTSTR szFileName, LPCTSTR szNewFileName)
{
	return true;
}

void CDirectoryChangeHandler::SetChangedDirectoryName(const CString & strChangedDirName)
{
	_strChangedDirectoryName = strChangedDirName;
}

// TODO	
long CDirectoryChangeHandler::_ReferencesWatcher(std::shared_ptr<CDirectoryChangeWatcher> pDirChangeWatcher)
{
	std::lock_guard<std::mutex> lock(_mutWatcher);

	if (_pDirChangeWatcher != nullptr
		&& _pDirChangeWatcher != pDirChangeWatcher)
	{
		LOGF(INFO, _T("CDirectoryChangeHandler...is becoming used by a different CDirectoryChangeWatcher!\n"));
		LOGF(INFO, _T("Directories being handled by this object will now be unwatched.\nThis object is now being used to "));
		LOGF(INFO, _T("handle changes to a directory watched by different CDirectoryChangeWatcher object, probably on a different directory"));

		if (UnWatchDirectory())
		{
			_pDirChangeWatcher = pDirChangeWatcher;
			_nWatcherRefCount = 1; //when this reaches 0, set m_pDirChangeWatcher to NULL
			return _nWatcherRefCount;
		}
		else
		{
			ASSERT(FALSE);//shouldn't get here!
			LOGF(FATAL, _T("CDirectoryChangeHandler...is becoming used by a different CDirectoryChangeWatcher!\n"));
		}
	}
	else
	{
		_pDirChangeWatcher = pDirChangeWatcher;
		if (_pDirChangeWatcher != nullptr)
		{
			return InterlockedIncrement(&_nWatcherRefCount);
		}
	}

	return _nWatcherRefCount;
}

long CDirectoryChangeHandler::_ReleaseReferenceToWatcher(std::shared_ptr<CDirectoryChangeWatcher> pDirChangeWatcher)
{
	std::lock_guard<std::mutex> lock(_mutWatcher);

	long nRef = 0;
	if ((nRef = InterlockedDecrement(&_nWatcherRefCount)) <= 0UL)
	{
		_pDirChangeWatcher.reset();
		_nWatcherRefCount = 0;
	}

	return nRef;
}
