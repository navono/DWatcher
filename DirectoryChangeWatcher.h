#pragma once
#include "DirectoryChangeHandler.h"
#include "FileNotifyInformation.h"
#include <mutex>
#include <vector>
#include <memory>


#define READ_DIR_CHANGE_BUFFER_SIZE 4096


class CDirectoryChangeWatcher : public std::enable_shared_from_this<CDirectoryChangeWatcher>
{
public:
	enum {  //options for determining the behavior of the filter tests.
			//
		FILTERS_DONT_USE_FILTERS = 1, //never test the include/exclude filters. CDirectoryChangeHandler::On_FilterNotification() is still called.
		FILTERS_CHECK_FULL_PATH = 2, //For the file path: "C:\FolderName\SubFolder\FileName.xyz", the entire path is checked for the filter pattern.
		FILTERS_CHECK_PARTIAL_PATH = 4, //For the file path: "C:\FolderName\SubFolder\FileName.xyz", only "SubFolder\FileName.xyz" is checked against the filter pattern, provided that you are watching the folder "C:\FolderName", and you are also watching subfolders.
		FILTERS_CHECK_FILE_NAME_ONLY = 8, //For the file path: "C:\FolderName\SubFolder\FileName.xyz", only "FileName.xyz" is checked against the filter pattern.
		FILTERS_TEST_HANDLER_FIRST = 16, //test CDirectoryChangeHandler::On_FilterNotification() before checking the include/exclude filters. the default is to check the include/exclude filters first.
		FILTERS_DONT_USE_HANDLER_FILTER = 32, //CDirectoryChangeHander::On_FilterNotification() won't be called.
		FILTERS_NO_WATCHSTART_NOTIFICATION = 64,//CDirectoryChangeHander::On_WatchStarted() won't be called.
		FILTERS_NO_WATCHSTOP_NOTIFICATION = 128,//CDirectoryChangeHander::On_WatchStopped() won't be called.
		FILTERS_DEFAULT_BEHAVIOR = (FILTERS_CHECK_FILE_NAME_ONLY),
		FILTERS_DONT_USE_ANY_FILTER_TESTS = (FILTERS_DONT_USE_FILTERS | FILTERS_DONT_USE_HANDLER_FILTER),
		FILTERS_NO_WATCH_STARTSTOP_NOTIFICATION = (FILTERS_NO_WATCHSTART_NOTIFICATION | FILTERS_NO_WATCHSTOP_NOTIFICATION)
	};

	CDirectoryChangeWatcher(bool bAppHasGUI = true, DWORD dwFilterFlags = FILTERS_DEFAULT_BEHAVIOR);
	virtual ~CDirectoryChangeWatcher();

	std::shared_ptr<CDirectoryChangeWatcher> GetSharedPtr()
	{
		return std::shared_ptr<CDirectoryChangeWatcher>(this);
	}

	DWORD	WatchDirectory(const CString & strDirToWatch,
		DWORD dwChangesToWatchFor,
		CDirectoryChangeHandler * pChangeHandler,
		BOOL bWatchSubDirs = FALSE,
		const std::string& strIncludeFilter = nullptr,
		const std::string& strExcludeFilter = nullptr);

	BOOL	IsWatchingDirectory(const CString& strDirName) const;
	int		NumWatchedDirectories() const;

	BOOL	UnWatchDirectory(const CString& strDirName);
	BOOL	UnWatchAllDirectory();

	DWORD	SetFilterFlags(DWORD dwFilterFlags);
	DWORD	GetFilterFlags() const { return _dwFilterFlags;  }

public:
	// this class is used internally by CDirectoryChangeWatcher
	// to help manage the watched directories
	class CDirWatchInfo
	{
	public:
		CDirWatchInfo() = delete;
		CDirWatchInfo& operator=(const CDirWatchInfo&) = delete;

		CDirWatchInfo(HANDLE hDir, const CString & strDirectoryName,
			CDirectoryChangeHandler * pChangeHandler,
			DWORD dwChangeFilter, BOOL bWatchSubDir,
			bool bAppHasGUI,
			LPCTSTR szIncludeFilter,
			LPCTSTR szExcludeFilter,
			DWORD dwFilterFlags);

	private:
		~CDirWatchInfo();//only I can delete myself....use DeleteSelf()
	public:
		void	DeleteSelf(CDirectoryChangeWatcher * pWatcher);

		DWORD	StartMonitor(HANDLE hCompPort);
		BOOL	UnwatchDirectory(HANDLE hCompPort);
	protected:
		BOOL	SignalShutdown(HANDLE hCompPort);
		BOOL	WaitForShutdown();

	public:
		BOOL	LockProperties() { return m_cs.Lock(); }
		BOOL	UnlockProperties() { return m_cs.Unlock(); }

		CDelayedDirectoryChangeHandler* GetChangeHandler() const;
		CDirectoryChangeHandler * GetRealChangeHandler() const;//the 'real' change handler is your CDirectoryChangeHandler derived class.
		CDirectoryChangeHandler * SetRealDirectoryChangeHandler(CDirectoryChangeHandler * pChangeHandler);

		BOOL CloseDirectoryHandle();

		//CDirectoryChangeHandler * m_pChangeHandler;
		CDelayedDirectoryChangeHandler * m_pChangeHandler;
		HANDLE      m_hDir;//handle to directory that we're watching
		DWORD		m_dwChangeFilter;
		BOOL		m_bWatchSubDir;
		CString     m_strDirName;//name of the directory that we're watching
		CHAR        m_Buffer[READ_DIR_CHANGE_BUFFER_SIZE];//buffer for ReadDirectoryChangesW
		DWORD       m_dwBufLength;//length or returned data from ReadDirectoryChangesW -- ignored?...
		OVERLAPPED  m_Overlapped;
		DWORD		m_dwReadDirError;//indicates the success of the call to ReadDirectoryChanges()
		CCriticalSection m_cs;
		CEvent		m_StartStopEvent;
		enum eRunningState {
			RUNNING_STATE_NOT_SET,
			RUNNING_STATE_START_MONITORING,
			RUNNING_STATE_STOP,
			RUNNING_STATE_STOP_STEP2,
			RUNNING_STATE_STOPPED,
			RUNNING_STATE_NORMAL
		};
		eRunningState m_RunningState;

	};

	//so that CDirWatchInfo can call the following function.
	friend class CDirWatchInfo;

	void	ProcessChangeNotifications(
		IN CFileNotifyInformation & notify_info,
		IN CDirWatchInfo * pdi,
		OUT DWORD & ref_dwReadBuffer_Offset);

	long	ReleaseReferenceToWatcher(CDirectoryChangeHandler * pChangeHandler);

	BOOL	UnwatchDirectoryBecauseOfError(CDirWatchInfo * pWatchInfo);//called in case of error.
	int		AddToWatchInfo(CDirWatchInfo * pWatchInfo);

	//
	//	functions for retrieving the directory watch info based on different parameters
	//
	CDirWatchInfo*	GetDirWatchInfo(IN const CString& strDirName, OUT int& ref_nIdx) const;
	CDirWatchInfo*	GetDirWatchInfo(IN CDirWatchInfo* pWatchInfo, OUT int& ref_nIdx) const;
	CDirWatchInfo*	GetDirWatchInfo(IN CDirectoryChangeHandler * pChangeHandler, OUT int& ref_nIdx) const;

protected:
	//All file change notifications has taken place in the context of 
	// a worker thread...do any thread initialization here..
	virtual void	On_ThreadInitialize() {}
	
	//do thread cleanup here
	virtual void	On_ThreadExit() {}

private:
	BOOL		_UnWatchDirectory(std::shared_ptr<CDirectoryChangeHandler> pDirCH);
	
	UINT static _MonitorDirectoryChanges(LPVOID lpThis);

private:
	friend	class CDirectoryChangeHandler;

	HANDLE	_hCompPort;	//i/o completion port
	HANDLE	_hThread;	//MonitorDirectoryChanges() thread handle
	DWORD	_dwThreadID;
	std::vector<std::shared_ptr<CDirWatchInfo>>	_directoriesToWatchVec;
	std::mutex _mutDirWatchInfo;
	bool	_bAppHasGUI;
	DWORD	_dwFilterFlags;
};

