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
	ASSERT(AfxIsValidAddress(pdi, sizeof(CDirectoryChangeWatcher::CDirWatchInfo)));

	if (pdi == nullptr)
	{
		LOGF(FATAL, ("Invalid arguments to CDirectoryChangeWatcher::ProcessChangeNotifications() -- pdi is invalid!\n"));
		return;
	}

	DWORD dwLastAction = 0;
	ref_dwReadBuffer_Offset = 0UL;

	CDirectoryChangeHandler *pChangerHandler = pdi->GetChangeHandler();
	if (pChangerHandler == nullptr)
	{
		LOGF(FATAL, _T("CDirectoryChangeWatcher::ProcessChangeNotifications() Unable to continue, pdi->GetChangeHandler() returned NULL!\n"));
		return;
	}

	//
	//	go through and process the notifications contained in the
	//	CFileChangeNotification object( CFileChangeNotification is a wrapper for the FILE_NOTIFY_INFORMATION structure
	//									returned by ReadDirectoryChangesW)
	//
	do 
	{
		//The FileName member of the FILE_NOTIFY_INFORMATION
		//structure contains the NAME of the file RELATIVE to the 
		//directory that is being watched...
		//ie, if watching C:\Temp and the file C:\Temp\MyFile.txt is changed,
		//the file name will be "MyFile.txt"
		//If watching C:\Temp, AND you're also watching subdirectories
		//and the file C:\Temp\OtherFolder\MyOtherFile.txt is modified,
		//the file name will be "OtherFolder\MyOtherFile.txt

		switch (notify_info.GetAction())
		{
		case FILE_ACTION_ADDED:
			pChangerHandler->On_FileAdd(notify_info.GetFileNameWithPath(pdi->m_strDirName));
		case FILE_ACTION_REMOVED:
			pChangerHandler->On_FileRemoved(notify_info.GetFileNameWithPath(pdi->m_strDirName));
		case FILE_ACTION_MODIFIED:
			pChangerHandler->On_FileModified(notify_info.GetFileNameWithPath(pdi->m_strDirName));
		case FILE_ACTION_RENAMED_OLD_NAME:
		{
			auto strOldFileName = notify_info.GetFileNameWithPath(pdi->m_strDirName);
			if (notify_info.GetNextNotifyInformation())
			{
				// there is another PFILE_NOTIFY_INFORMATION record following the one we're working on now...
				// it will be the record for the FILE_ACTION_RENAMED_NEW_NAME record

				ASSERT(notify_info.GetAction() == FILE_ACTION_RENAMED_NEW_NAME);//making sure that the next record after the OLD_NAME record is the NEW_NAME record

				auto strNewFileName = notify_info.GetFileNameWithPath(pdi->m_strDirName);
				pChangerHandler->On_FileNameChanged(strOldFileName, strNewFileName);
			}
			else
			{
				//this OLD_NAME was the last record returned by ReadDirectoryChangesW
				//I will have to call ReadDirectoryChangesW again so that I will get 
				//the record for FILE_ACTION_RENAMED_NEW_NAME

				//Adjust an offset so that when I call ReadDirectoryChangesW again,
				//the FILE_NOTIFY_INFORMATION will be placed after 
				//the record that we are currently working on.

				/***************
				Let's say that 200 files all had their names changed at about the same time
				There will be 400 FILE_NOTIFY_INFORMATION records (one for OLD_NAME and one for NEW_NAME for EACH file which had it's name changed)
				that ReadDirectoryChangesW will have to report to
				me.   There might not be enough room in the buffer
				and the last record that we DID get was an OLD_NAME record,
				I will need to call ReadDirectoryChangesW again so that I will get the NEW_NAME
				record.    This way I'll always have to strOldFileName and strNewFileName to pass
				to CDirectoryChangeHandler::On_FileRenamed().

				After ReadDirecotryChangesW has filled out our buffer with
				FILE_NOTIFY_INFORMATION records,
				our read buffer would look something like this:
				End Of Buffer
				|
				\-/
				|_________________________________________________________________________
				|																		  |
				|file1 OLD name record|file1 NEW name record|...|fileX+1 OLD_name record| |(the record we want would be here, but we've ran out of room, so we adjust an offset and call ReadDirecotryChangesW again to get it)
				|_________________________________________________________________________|

				Since the record I need is still waiting to be returned to me,
				and I need the current 'OLD_NAME' record,
				I'm copying the current FILE_NOTIFY_INFORMATION record
				to the beginning of the buffer used by ReadDirectoryChangesW()
				and I adjust the offset into the read buffer so the the NEW_NAME record
				will be placed into the buffer after the OLD_NAME record now at the beginning of the buffer.

				Before we call ReadDirecotryChangesW again,
				modify the buffer to contain the current OLD_NAME record...

				|_______________________________________________________
				|														|
				|fileX old name record(saved)|<this is now garbage>.....|
				|_______________________________________________________|
				/-\
				|
				Offset for Read
				Re-issue the watch command to get the rest of the records...

				ReadDirectoryChangesW(..., pBuffer + (an Offset),

				After GetQueuedCompletionStatus() returns,
				our buffer will look like this:

				|__________________________________________________________________________________________________________
				|																										   |
				|fileX old name record(saved)|fileX new name record(the record we've been waiting for)| <other records>... |
				|__________________________________________________________________________________________________________|

				Then I'll be able to know that a file name was changed
				and I will have the OLD and the NEW name of the file to pass to CDirectoryChangeHandler::On_FileNameChanged

				****************/
				//NOTE that this case has never happened to me in my testing
				//so I can only hope that the code works correctly.
				//It would be a good idea to set a breakpoint on this line of code:
				notify_info.CopyCurrentRecordToBeginningOfBuffer(ref_dwReadBuffer_Offset);
			}
		}
		case FILE_ACTION_RENAMED_NEW_NAME:
		{
			//This should have been handled in FILE_ACTION_RENAMED_OLD_NAME
			ASSERT(FALSE);//this shouldn't get here
		}
		default:
			LOGF(WARNING, ("CDirectoryChangeWatcher::ProcessChangeNotifications() -- unknown FILE_ACTION_ value! : %d\n"), notify_info.GetAction());
			break;
		}

		dwLastAction = notify_info.GetAction();

	} while (notify_info.GetNextNotifyInformation());

}

long CDirectoryChangeWatcher::ReleaseReferenceToWatcher(CDirectoryChangeHandler * pChangeHandler)
{
	if (pChangeHandler != nullptr)
	{
		pChangeHandler->_ReleaseReferenceToWatcher(GetSharedPtr());
	}
}

BOOL CDirectoryChangeWatcher::UnwatchDirectoryBecauseOfError(CDirWatchInfo * pWatchInfo)
{
	BOOL bRetVal = FALSE;
	if (pWatchInfo != nullptr)
	{
		std::lock_guard<std::mutex> lk(_mutDirWatchInfo);
		int nIdx = -1;
		if (GetDirWatchInfo(pWatchInfo, nIdx) == pWatchInfo)
		{
			_directoriesToWatchVec.at(nIdx).reset();
			pWatchInfo->DeleteSelf(this);
			bRetVal = TRUE;
		}
	}

	return bRetVal;
}

int CDirectoryChangeWatcher::AddToWatchInfo(std::shared_ptr<CDirWatchInfo> pWatchInfo)
{
	std::lock_guard<std::mutex> lk(_mutDirWatchInfo);
	auto count = _directoriesToWatchVec.size();
	auto i = 0U;
	for (i = 0U; i < count; ++i)
	{
		if (_directoriesToWatchVec.at(i) == nullptr)
		{
			_directoriesToWatchVec.at(i) = pWatchInfo;
			break;
		}
	}

	if (i == count)
	{
		try 
		{
			_directoriesToWatchVec.push_back(pWatchInfo);
		}
		catch (CMemoryException * e) 
		{
			e->ReportError();
			e->Delete();//??? delete this? I thought CMemoryException objects where pre allocated in mfc? -- sample code in msdn does, so will i
		}
	}
}

std::shared_ptr<CDirectoryChangeWatcher::CDirWatchInfo> CDirectoryChangeWatcher::GetDirWatchInfo(
	IN const CString& strDirName, OUT int& ref_nIdx) const
{
	if (strDirName.IsEmpty())
	{
		return nullptr;
	}
	
	std::lock_guard<std::mutex> lk(_mutDirWatchInfo);
	auto count = _directoriesToWatchVec.size();
	std::shared_ptr<CDirWatchInfo> pWatchInfo = nullptr;
	if (auto i = 0U; i < count; ++i)
	{
		pWatchInfo = _directoriesToWatchVec.at(i);
		if (pWatchInfo != nullptr
			&& pWatchInfo->m_strDirName.CompareNoCase(strDirName) == 0)
		{
			ref_nIdx = i;
			return pWatchInfo;
		}
	}

	return nullptr;
}

std::shared_ptr<CDirectoryChangeWatcher::CDirWatchInfo>  CDirectoryChangeWatcher::GetDirWatchInfo(
	IN std::shared_ptr<CDirWatchInfo> pWatchInfo, OUT int& ref_nIdx) const
{
	std::lock_guard<std::mutex> lk(_mutDirWatchInfo);
	auto count = _directoriesToWatchVec.size();
	std::shared_ptr<CDirWatchInfo> pTmpWatchInfo = nullptr;
	if (auto i = 0U; i < count; ++i)
	{
		pTmpWatchInfo = _directoriesToWatchVec.at(i);
		if (pTmpWatchInfo != nullptr
			&& pTmpWatchInfo == pWatchInfo)
		{
			ref_nIdx = i;
			return pTmpWatchInfo;
		}
	}

	return nullptr;
}

std::shared_ptr<CDirectoryChangeWatcher::CDirWatchInfo>  CDirectoryChangeWatcher::GetDirWatchInfo(
	IN std::shared_ptr<CDirectoryChangeHandler> pChangeHandler, OUT int& ref_nIdx) const
{
	std::lock_guard<std::mutex> lk(_mutDirWatchInfo);
	auto count = _directoriesToWatchVec.size();
	std::shared_ptr<CDirWatchInfo> pTmpWatchInfo = nullptr;
	if (auto i = 0U; i < count; ++i)
	{
		pTmpWatchInfo = _directoriesToWatchVec.at(i);
		if (pTmpWatchInfo != nullptr
			&& pTmpWatchInfo->GetRealChangeHandler() == pChangeHandler.get())
		{
			ref_nIdx = i;
			return pTmpWatchInfo;
		}
	}

	return nullptr;
}

/************************************
This function is called from the dtor of CDirectoryChangeHandler automatically,
but may also be called by a programmer because it's public...

A single CDirectoryChangeHandler may be used for any number of watched directories.

Unwatch any directories that may be using this
CDirectoryChangeHandler * pChangeHandler to handle changes to a watched directory...

The CDirWatchInfo::m_pChangeHandler member of objects in the m_DirectoriesToWatch
array will == pChangeHandler if that handler is being used to handle changes to a directory....
************************************/
BOOL CDirectoryChangeWatcher::_UnWatchDirectory(std::shared_ptr<CDirectoryChangeHandler> pDirCH)
{
	std::lock_guard<std::mutex> lk(_mutDirWatchInfo);

	int nUnwatched = 0;
	int nIdx = -1;

	std::shared_ptr<CDirWatchInfo>	pDirInfo;

	while ((pDirInfo = GetDirWatchInfo(pDirCH, nIdx)) != nullptr)
	{
		pDirInfo->UnwatchDirectory(_hCompPort);

		++nUnwatched;
		_directoriesToWatchVec.at(nIdx).reset();
		pDirInfo->DeleteSelf(this);
	}

	return (BOOL)(nUnwatched != 0)
}

UINT CDirectoryChangeWatcher::_MonitorDirectoryChanges(LPVOID lpThis)
{
	DWORD numBytes;
	CDirWatchInfo *pdi;
	std::shared_ptr<CDirWatchInfo> pDI;
	LPOVERLAPPED lpOverlapped;

	auto *pThis = reinterpret_cast<CDirectoryChangeWatcher*>(lpThis);
	pThis->On_ThreadInitialize();

	do 
	{
		// Retrieve the directory info for this directory
		// through the io port's completion key
		if (!GetQueuedCompletionStatus(pThis->_hCompPort,
			&numBytes, (LPDWORD)&pdi,
			&lpOverlapped, INFINITE))
		{
			// The io completion request failed...
			// probably because the handle to the directory that
			// was used in a call to ReadDirectoryChangesW has been closed.
			if (pdi != nullptr && pdi->m_hDir != INVALID_HANDLE_VALUE)
			{
				// the directory handle is still open! (we expect this when after we close the directory handle )
				LOGF(FATAL, _T("GetQueuedCompletionStatus() returned FALSE\nGetLastError(): %d Completion Key: %p lpOverlapped: %p\n"), GetLastError(), pdi, lpOverlapped);
			}
		}

		if (pdi != nullptr)
		{
			/***********************************
			The CDirWatchInfo::m_RunningState is pretty much the only member
			of CDirWatchInfo that can be modified from the other thread.
			The functions StartMonitor() and UnwatchDirecotry() are the functions that
			can modify that variable.

			So that I'm sure that I'm getting the right value,
			I'm using a critical section to guard against another thread modifying it when I want
			to read it...

			************************************/

			bool bObjShouldBeOk = true;
			try
			{
				pdi->LockProperties();
			}
			catch(...)
			{
				LOG(WARNING, ("CDirectoryChangeWatcher::MonitorDirectoryChanges() -- pdi->LockProperties() raised an exception!\n"));
				bObjShouldBeOk = false;
			}

			if (bObjShouldBeOk)
			{
				auto runState = pdi->m_RunningState;
				pdi->UnlockProperties();
				/***********************************
				Unlock it so that there isn't a DEADLOCK if
				somebody tries to call a function which will
				cause CDirWatchInfo::UnwatchDirectory() to be called
				from within the context of this thread (eg: a function called because of
				the handler for one of the CDirectoryChangeHandler::On_Filexxx() functions)

				************************************/
				
				pdi->GetChangeHandler();
				switch (runState)
				{
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_NOT_SET:
					break;
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_START_MONITORING:
				{
					//Issue the initial call to ReadDirectoryChangesW()

					if (!ReadDirectoryChangesW(pdi->m_hDir,
						pdi->m_Buffer,	//<--FILE_NOTIFY_INFORMATION records are put into this buffer
						READ_DIR_CHANGE_BUFFER_SIZE,
						pdi->m_bWatchSubDir,
						pdi->m_dwChangeFilter,
						&pdi->m_dwBufLength,		//this var not set when using asynchronous mechanisms...
						&pdi->m_Overlapped,
						nullptr))
					{
						pdi->m_dwReadDirError = GetLastError();
						if (pdi->GetChangeHandler() != nullptr)
						{
							pdi->GetChangeHandler()->On_WatchStarted(pdi->m_dwReadDirError, pdi->m_strDirName);
						}
					}
					else
					{
						// read directory changes was successful!
						// allow it to run normally
						pdi->m_RunningState = CDirWatchInfo::RUNNING_STATE_NORMAL;
						pdi->m_dwReadDirError = ERROR_SUCCESS;
						if (pdi->GetChangeHandler() != nullptr)
						{
							pdi->GetChangeHandler()->On_WatchStarted(ERROR_SUCCESS, pdi->m_strDirName);
						}
					}
				}
				break;
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_STOP:
				{
					if (pdi->m_hDir != INVALID_HANDLE_VALUE)
					{
						// Since I've previously called ReadDirectoryChangesW() asynchronously, I am waiting
						// for it to return via GetQueuedCompletionStatus().  When I close the
						// handle that ReadDirectoryChangesW() is waiting on, it will
						// cause GetQueuedCompletionStatus() to return again with this pdi object....
						// Close the handle, and then wait for the call to GetQueuedCompletionStatus()
						// to return again by breaking out of the switch, and letting GetQueuedCompletionStatus()
						// get called again
						pdi->CloseDirectoryHandle();

						// back up step...GetQueuedCompletionStatus() will still need to return from the last time that ReadDirectoryChangesW() was called.....
						pdi->m_RunningState = CDirWatchInfo::RUNNING_STATE_STOP_STEP2;

					}
					else
					{
						// either we weren't watching this directory in the first place,
						// or we've already stopped monitoring it....

						// set the event that ReadDirectoryChangesW has returned and no further calls to it will be made...
						pdi->m_StartStopEvent.SetEvent();
					}
				}
				break;
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_STOP_STEP2:
				{
					// GetQueuedCompletionStatus() has returned from the last
					// time that ReadDirectoryChangesW was called...
					// Using CloseHandle() on the directory handle used by
					// ReadDirectoryChangesW will cause it to return via GetQueuedCompletionStatus()....
					if (pdi->m_hDir == INVALID_HANDLE_VALUE)
					{
						// signal that no further calls to ReadDirectoryChangesW will be made
						// and this pdi can be deleted
						pdi->m_StartStopEvent.SetEvent();
					}
					else
					{
						pdi->CloseDirectoryHandle();

						//wait for GetQueuedCompletionStatus() to return this pdi object again
					}
				}
				break;
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_STOPPED:
				{
					auto pChangeHandler = pdi->GetChangeHandler();
					if (pChangeHandler != nullptr)
					{
						pChangeHandler->SetChangeDirectoryName(pdi->m_strDirName);
					}

					DWORD dwReadBuffOffset = 0UL;

					// process the FILE_NOTIFY_INFORMATION records:
					CFileNotifyInformation notifyInfo((LPBYTE)pdi->m_Buffer, READ_DIR_CHANGE_BUFFER_SIZE);
					pThis->ProcessChangeNotifications(notifyInfo, pdi, dwReadBuffOffset);

					//	Changes have been processed,
					//	Reissue the watch command
					//
					if (!ReadDirectoryChangesW(pdi->m_hDir,
						pdi->m_Buffer + dwReadBuffOffset,	//<--FILE_NOTIFY_INFORMATION records are put into this buffer 
						READ_DIR_CHANGE_BUFFER_SIZE - dwReadBuffOffset,
						pdi->m_bWatchSubDir,
						pdi->m_dwChangeFilter,
						&pdi->m_dwBufLength,		//this var not set when using asynchronous mechanisms...
						&pdi->m_Overlapped,
						nullptr))//no completion routine!
					{
						//
						//	NOTE:  
						//		In this case the thread will not wake up for 
						//		this pdi object because it is no longer associated w/
						//		the I/O completion port...there will be no more outstanding calls to ReadDirectoryChangesW
						//		so I have to skip the normal shutdown routines(normal being what happens when CDirectoryChangeWatcher::UnwatchDirectory() is called.
						//		and close this up, & cause it to be freed.
						//
						LOGF(WARNING, ("WARNING: ReadDirectoryChangesW has failed during normal operations...failed on directory: %s\n"), pdi->m_strDirName);

						//
						//	To help insure that this has been unwatched by the time
						//	the main thread processes the On_ReadDirectoryChangesError() notification
						//	bump the thread priority up temporarily.  The reason this works is because the notification
						//	is really posted to another thread's message queue,...by setting this thread's priority
						//	to highest, this thread will get to shutdown the watch by the time the other thread has a chance
						//	to handle it. *note* not technically guaranteed 100% to be the case, but in practice it'll work.
						int nOldThreadPriority = GetThreadPriority(GetCurrentThread());
						SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

						//
						//	Notify the client object....(a CDirectoryChangeHandler derived class)
						//
						try
						{
							pdi->m_dwReadDirError = GetLastError();
							pChangeHandler->On_ReadDiretoryChangesError(pdi->m_dwReadDirError, pdi->m_strDirName);

							//Do the shutdown
							pThis->UnwatchDirectoryBecauseOfError(pdi);
							//pdi = NULL; <-- DO NOT set this to NULL, it will cause this worker thread to exit.
							//pdi is INVALID at this point!!
						}
						catch(...)
						{
							//LOGF(WARNING, );
						}

						SetThreadPriority(GetCurrentThread(), nOldThreadPriority);
					}
					else
					{
						// success, continue as normal
						pdi->m_dwReadDirError = ERROR_SUCCESS;
					}
				}
				break;
				case CDirectoryChangeWatcher::CDirWatchInfo::RUNNING_STATE_NORMAL:
				{
				}
				break;
				default:
					LOGF(FATAL, ("MonitorDirectoryChanges() -- how did I get here?\n"));
					break;
				}
			}
		}

	} while (pdi != nullptr);

	pThis->On_ThreadExit();
	return 0;
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
