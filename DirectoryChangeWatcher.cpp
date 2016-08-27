#include "stdafx.h"
#include "DirectoryChangeWatcher.h"
#include "PrivilegeEnabler.h"
#include "DWatcher.h"	// IsDirectory


CDirectoryChangeWatcher::CDirectoryChangeWatcher(bool bAppHasGUI /*= true*/, 
	DWORD dwFilterFlags /*= FILTERS_DEFAULT_BEHAVIOR*/)
	: _hCompPort(nullptr)
	, _hThread(nullptr)
	, _dwThreadID(0UL)
	, _bAppHasGUI(bAppHasGUI)
	, _dwFilterFlags(dwFilterFlags == 0 ? FILTERS_DEFAULT_BEHAVIOR : dwFilterFlags)
{
	//NOTE:  
	//	The bAppHasGUI variable indicates that you have a message pump associated
	//	with the main thread(or the thread that first calls CDirectoryChangeWatcher::WatchDirectory() ).
	//	Directory change notifications are dispatched to your main thread.
	//	
	//	If your app doesn't have a gui, then pass false.  Doing so causes a worker thread
	//	to be created that implements a message pump where it dispatches/executes the notifications.
	//  It's ok to pass false even if your app does have a GUI.
	//	Passing false is required for Console applications, or applications without a message pump.
	//	Note that notifications are fired in a worker thread.
	//
}

CDirectoryChangeWatcher::~CDirectoryChangeWatcher()
{
	UnWatchAllDirectory();
	if (_hCompPort != nullptr)
	{
		CloseHandle(_hCompPort);
		_hCompPort = nullptr;
	}
}

/*************************************************************
FUNCTION:	WatchDirectory(const CString & strDirToWatch,   --the name of the directory to watch
DWORD dwChangesToWatchFor, --the changes to watch for see disk docs..for ReadDirectoryChangesW
CDirectoryChangeHandler * pChangeHandler -- handles changes in specified directory
BOOL bWatchSubDirs      --specified to watch sub directories of the directory that you want to watch
)

PARAMETERS:
const CString & strDirToWatch -- specifies the path of the directory to watch.
DWORD dwChangesToWatchFor	-- specifies flags to be passed to ReadDirectoryChangesW()
CDirectoryChangeHandler *	-- ptr to an object which will handle notifications of file changes.
BOOL bWatchSubDirs			-- specifies to watch subdirectories.
LPCTSTR szIncludeFilter		-- A file pattern string for files that you wish to receive notifications
for. See Remarks.
LPCTSTR szExcludeFilter		-- A file pattern string for files that you do not wish to receive notifications for. See Remarks

Starts watching the specified directory(and optionally subdirectories) for the specified changes

When specified changes take place the appropriate CDirectoryChangeHandler::On_Filexxx() function is called.

dwChangesToWatchFor can be a combination of the following flags, and changes map out to the
following functions:
FILE_NOTIFY_CHANGE_FILE_NAME    -- CDirectoryChangeHandler::On_FileAdded()
CDirectoryChangeHandler::On_FileNameChanged,
CDirectoryChangeHandler::On_FileRemoved
FILE_NOTIFY_CHANGE_DIR_NAME     -- CDirectoryChangeHandler::On_FileNameAdded(),
CDirectoryChangeHandler::On_FileRemoved
FILE_NOTIFY_CHANGE_ATTRIBUTES   -- CDirectoryChangeHandler::On_FileModified
FILE_NOTIFY_CHANGE_SIZE         -- CDirectoryChangeHandler::On_FileModified
FILE_NOTIFY_CHANGE_LAST_WRITE   -- CDirectoryChangeHandler::On_FileModified
FILE_NOTIFY_CHANGE_LAST_ACCESS  -- CDirectoryChangeHandler::On_FileModified
FILE_NOTIFY_CHANGE_CREATION     -- CDirectoryChangeHandler::On_FileModified
FILE_NOTIFY_CHANGE_SECURITY     -- CDirectoryChangeHandler::On_FileModified?


Returns ERROR_SUCCESS if the directory will be watched,
or a windows error code if the directory couldn't be watched.
The error code will most likely be related to a call to CreateFile(), or
from the initial call to ReadDirectoryChangesW().  It's also possible to get an
error code related to being unable to create an io completion port or being unable
to start the worker thread.

This function will fail if the directory to be watched resides on a
computer that is not a Windows NT/2000/XP machine.


You can only have one watch specified at a time for any particular directory.
Calling this function with the same directory name will cause the directory to be
unwatched, and then watched again(w/ the new parameters that have been passed in).

**************************************************************/
DWORD CDirectoryChangeWatcher::WatchDirectory(const CString & strDirToWatch, DWORD dwChangesToWatchFor, 
	CDirectoryChangeHandler * pChangeHandler, BOOL bWatchSubDirs /*= FALSE*/, 
	const std::string& strIncludeFilter /*= nullptr*/, const std::string& strExcludeFilter /*= nullptr*/)
{
	ASSERT(dwChangesToWatchFor != 0);

	if (strDirToWatch.IsEmpty()
		|| dwChangesToWatchFor == 0
		|| pChangeHandler == nullptr)
	{
		LOGF(FATAL, _T("ERROR: You've passed invalid parameters to CDirectoryChangeWatcher::WatchDirectory()\n"));
		::SetLastError(ERROR_INVALID_PARAMETER);
		return ERROR_INVALID_PARAMETER;
	}

	// double check that it's really a directory
	if (!IsDirectory(strDirToWatch))
	{
		LOGF(FATAL, _T("ERROR: CDirectoryChangeWatcher::WatchDirectory() -- %s is not a directory!\n"), strDirToWatch);
		::SetLastError(ERROR_BAD_PATHNAME);
		return ERROR_BAD_PATHNAME;
	}

	// double check that this directory is not already being watched
	// if it is, the unwatch it before restarting it
	if (IsWatchingDirectory(strDirToWatch))
	{
		UnWatchDirectory(strDirToWatch);
	}

	//
	//	Reference this singleton so that privileges for this process are enabled 
	//	so that it has required permissions to use the ReadDirectoryChangesW API, etc.
	//
	CPrivilegeEnabler::Instance();

	// open the directory to watch
	auto hDir = CreateFile(strDirToWatch,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | //<- the required privileges for this flag are: SE_BACKUP_NAME and SE_RESTORE_NAME
		FILE_FLAG_OVERLAPPED,
		nullptr);
	if (hDir == INVALID_HANDLE_VALUE)
	{
		auto dwError = GetLastError();
		LOGF(FATAL, _T("CDirectoryChangeWatcher::WatchDirectory() -- Couldn't open directory for monitoring. %d\n"), dwError);
		::SetLastError(dwError);
		return dwError;
	}

	CDirWatchInfo *pDirInfo = new CDirWatchInfo(hDir, strDirToWatch, pChangeHandler,
		dwChangesToWatchFor, bWatchSubDirs, _bAppHasGUI, strIncludeFilter,
		strExcludeFilter, _dwFilterFlags);
	
	// Create a IO completion port/or associate this key with
	// the existing IO completion port
	_hCompPort = CreateIoCompletionPort(hDir,
		_hCompPort, // if m_hCompPort is NULL, hDir is associated with a NEW completion port,
					// if m_hCompPort is NON-NULL, hDir is associated with the existing completion port that the handle m_hCompPort references
		(DWORD)pDirInfo, // the completion 'key'... this ptr is returned from GetQueuedCompletionStatus() 
						 // when one of the events in the dwChangesToWatchFor filter takes place
		0);
	if (_hCompPort == nullptr)
	{
		LOGF(FATAL, _T("ERROR -- Unable to create I/O Completion port! GetLastError(): %d File: %s Line: %d"), GetLastError(), _T(__FILE__), __LINE__);
		auto dwError = GetLastError();
		pDirInfo->DeleteSelf(nullptr);
		::SetLastError(dwError);//who knows what the last error will be after i call pDirInfo->DeleteSelf(), so set it just to make sure
		return dwError;
	}
	else
	{
		// Completion port created/directory associated w/ it successfully

		// If the thread isn't running, start it....
		// when the thread starts, it will call ReadDirectoryChangesW and wait 
		// for changes to take place

		if (_hThread == nullptr)
		{
			auto pThread = AfxBeginThread(_MonitorDirectoryChanges, this);
			if (pThread == nullptr)
			{
				LOGF(FATAL, _T("CDirectoryChangeWatcher::WatchDirectory()-- AfxBeginThread failed!\n"));
				pDirInfo->DeleteSelf(nullptr);
				return (GetLastError() == ERROR_SUCCESS) ? ERROR_MAX_THRDS_REACHED : GetLastError();
			}
			else
			{
				_hThread = pThread->m_hThread;
				_dwThreadID = pThread->m_nThreadID;
				pThread->m_bAutoDelete = TRUE;
			}
		}

		if (_hThread != nullptr)
		{
			// Signal the thread to issue the initial call to
			// ReadDirectoryChangesW()
			auto dwStarted = pDirInfo->StartMonitor(_hCompPort);
			if (dwStarted != ERROR_SUCCESS)
			{
				LOGF(FATAL, _T("Unable to watch directory: %s -- GetLastError(): %d\n"), dwStarted);
				pDirInfo->DeleteSelf(nullptr);
				::SetLastError(dwStarted);//I think this'll set the Err object in a VB app.....
				return dwStarted;
			}
			else
			{
				// ReadDirectoryChangesW was successful!
				// add the directory info to the first empty slot in the array
				
				pChangeHandler->_ReferencesWatcher(GetSharedPtr());
				AddToWatchInfo(pDirInfo);

				return dwStarted;
			}
		}
		else
		{
			ASSERT(FALSE);//control path shouldn't get here
			::SetLastError(ERROR_MAX_THRDS_REACHED);
			return ERROR_MAX_THRDS_REACHED;
		}
	}
	ASSERT(FALSE);//shouldn't get here.
}

BOOL CDirectoryChangeWatcher::IsWatchingDirectory(const CString& strDirName) const
{
	std::lock_guard<std::mutex> lock(_mutDirWatchInfo);
	//std::unique_lock<std::mutex> lk(_mutDirWatchInfo);

	int i;
	if (GetDirWatchInfo(strDirName, i) != nullptr)
	{
		return TRUE;
	}
	return FALSE;
}

int CDirectoryChangeWatcher::NumWatchedDirectories() const
{
	std::lock_guard<std::mutex> lock(_mutDirWatchInfo);
	int cnt(0), max = _directoriesToWatchVec.size();
	for (auto i(0UL); i < max; ++i)
	{
		if (_directoriesToWatchVec.at(i) != nullptr)
		{
			++cnt;
		}
	}

	return cnt;
}

BOOL CDirectoryChangeWatcher::UnWatchDirectory(const CString& strDirName)
{
	BOOL bRetVal = FALSE;
	if (_hCompPort != nullptr)
	{
		std::lock_guard<std::mutex> lock(_mutDirWatchInfo);
		int nIdx = -1;
		auto pDirInfo = GetDirWatchInfo(strDirName, nIdx);
		if (pDirInfo != nullptr && nIdx != -1)
		{
			pDirInfo->UnwatchDirectory(_hCompPort);
			_directoriesToWatchVec[nIdx] = nullptr;
			pDirInfo->DeleteSelf(this);
			bRetVal = TRUE;
		}
	}

	return bRetVal;
}

BOOL CDirectoryChangeWatcher::UnWatchAllDirectory()
{
	if (_hThread == nullptr)
	{
		std::lock_guard<std::mutex> lock(_mutDirWatchInfo);

		// Unwatch each of the watched directories
		// and delete the CDirWatchInfo associated w/ that directory...
		auto max = _directoriesToWatchVec.size();
		for (auto i (0UL); i < max; ++i)
		{
			auto pDirInfo = _directoriesToWatchVec[i];
			if (pDirInfo != nullptr)
			{
				pDirInfo->UnwatchDirectory(_hCompPort);
				_directoriesToWatchVec[i].reset();
				_directoriesToWatchVec[i] = nullptr;
				pDirInfo->DeleteSelf(this);
			}
		}

		_directoriesToWatchVec.clear();

		PostQueuedCompletionStatus(_hCompPort, 0, 0, nullptr);
		WaitForSingleObject(_hThread, INFINITE);
		_hThread = nullptr;
		_dwThreadID = 0UL;

		CloseHandle(_hCompPort);
		_hCompPort = nullptr;

		return TRUE;
	}
	else
	{
#ifdef _DEBUG
		//make sure that there aren't any 
		//CDirWatchInfo objects laying around... they should have all been destroyed 
		//and removed from the array m_DirectoriesToWatch
		for (auto i = 0UL; i < _directoriesToWatchVec.size(); ++i)
		{
			_directoriesToWatchVec[i].reset();
			_directoriesToWatchVec[i] = nullptr;
		}
#endif

		return FALSE;
	}
}

DWORD CDirectoryChangeWatcher::SetFilterFlags(DWORD dwFilterFlags)
{
	auto dwOld = _dwFilterFlags;
	_dwFilterFlags = dwFilterFlags;
	if (_dwFilterFlags == 0)
	{
		_dwFilterFlags - FILTERS_DEFAULT_BEHAVIOR;
	}

	return dwOld;
}

void CDirectoryChangeWatcher::ProcessChangeNotifications(IN CFileNotifyInformation & notify_info, 
	IN CDirWatchInfo * pdi, OUT DWORD & ref_dwReadBuffer_Offset)
{

}

long CDirectoryChangeWatcher::ReleaseReferenceToWatcher(CDirectoryChangeHandler * pChangeHandler)
{

}

BOOL CDirectoryChangeWatcher::UnwatchDirectoryBecauseOfError(CDirWatchInfo * pWatchInfo)
{

}

int CDirectoryChangeWatcher::AddToWatchInfo(CDirWatchInfo * pWatchInfo)
{

}

CDirectoryChangeWatcher::CDirWatchInfo* CDirectoryChangeWatcher::GetDirWatchInfo(
	IN const CString& strDirName, OUT int& ref_nIdx) const
{

}

CDirectoryChangeWatcher::CDirWatchInfo* CDirectoryChangeWatcher::GetDirWatchInfo(
	IN CDirWatchInfo* pWatchInfo, OUT int& ref_nIdx) const
{

}

CDirectoryChangeWatcher::CDirWatchInfo* CDirectoryChangeWatcher::GetDirWatchInfo(
	IN CDirectoryChangeHandler * pChangeHandler, OUT int& ref_nIdx) const
{

}

BOOL CDirectoryChangeWatcher::_UnWatchDirectory(std::shared_ptr<CDirectoryChangeHandler> pDirCH)
{

}

UINT CDirectoryChangeWatcher::_MonitorDirectoryChanges(LPVOID lpThis)
{

}



//////////////////////////////////////////////////////////////////////////
CDirectoryChangeWatcher::CDirWatchInfo::CDirWatchInfo(HANDLE hDir, const CString & strDirectoryName, 
	CDirectoryChangeHandler * pChangeHandler, DWORD dwChangeFilter, BOOL bWatchSubDir, bool bAppHasGUI, 
	LPCTSTR szIncludeFilter, LPCTSTR szExcludeFilter, DWORD dwFilterFlags)
{

}

CDirectoryChangeWatcher::CDirWatchInfo::~CDirWatchInfo()
{

}

void CDirectoryChangeWatcher::CDirWatchInfo::DeleteSelf(CDirectoryChangeWatcher * pWatcher)
{

}

DWORD CDirectoryChangeWatcher::CDirWatchInfo::StartMonitor(HANDLE hCompPort)
{

}

BOOL CDirectoryChangeWatcher::CDirWatchInfo::UnwatchDirectory(HANDLE hCompPort)
{

}

BOOL CDirectoryChangeWatcher::CDirWatchInfo::SignalShutdown(HANDLE hCompPort)
{

}

BOOL CDirectoryChangeWatcher::CDirWatchInfo::WaitForShutdown()
{

}

CDelayedDirectoryChangeHandler* CDirectoryChangeWatcher::CDirWatchInfo::GetChangeHandler() const
{

}

CDirectoryChangeHandler * CDirectoryChangeWatcher::CDirWatchInfo::GetRealChangeHandler() const
{

}

CDirectoryChangeHandler * CDirectoryChangeWatcher::CDirWatchInfo::SetRealDirectoryChangeHandler(CDirectoryChangeHandler * pChangeHandler)
{

}

BOOL CDirectoryChangeWatcher::CDirWatchInfo::CloseDirectoryHandle()
{

}
