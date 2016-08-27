#pragma once
#include "DirectoryChangeHandler.h"
#include "DirChangeNotification.h"
#include "DelayedNotifier.h"


typedef BOOL(STDAPICALLTYPE * Func_PatternMatchSpec)
	(const std::string& strFile, const std::string& strSpec);

//
//	Decorates an instance of a CDirectoryChangeHandler object.
//	Intercepts notification function calls and posts them to 
//	another thread through a method implemented by a class derived from 
//	CDelayedNotifier
//	
//
//	This class implements dispatching the notifications to a thread
//	other than CDirectoryChangeWatcher::MonitorDirectoryChanges()
//
//	Also supports the include and exclude filters for each directory
//
class CDelayedDirectoryChangeHandler : public CDirectoryChangeHandler
{
public:
	CDelayedDirectoryChangeHandler() = delete;

	CDelayedDirectoryChangeHandler(std::shared_ptr<CDirectoryChangeHandler> pRealHandler, 
		bool bAppHasGUI, const std::string& strIncludeFilter, 
		const std::string& strExcludeFilter, DWORD dwFilterFlags);
	virtual ~CDelayedDirectoryChangeHandler();

	std::shared_ptr<CDirectoryChangeHandler> GetRealChangeHandler() const { return _pRealHandler; }
	//CDirectoryChangeHandler*& GetRealChangeHandler() { return _pRealHandler; }//FYI: PCLint will give a warning that this exposes a private/protected member& defeats encapsulation.  

	void	PostNotification(std::shared_ptr<CDirChangeNotification> pNotification);
	void	DispatchNotificationFunction(std::shared_ptr<CDirChangeNotification> pNotification);

protected:
	//These functions are called when the directory to watch has had a change made to it
	void	On_FileAdd(const CString& strFileName);
	void	On_FileRemoved(const CString& strFileName);
	void	On_FileModified(const CString& strFileName);
	void	On_FileNameChanged(const CString& strFileName);
	void	On_ReadDiretoryChangesError(DWORD dwError, const CString& strDirName);

	void	On_WatchStarted(DWORD dwError, const CString& strDirName);
	void	On_WatchStopped(const CString& strDirName);

	void	SetChangeDirectoryName(const CString& strDirName);
	const CString& GetChangedDirectoryName() const;

	BOOL	WaitForOnWatchStoppedDispatched();

	bool	NotifyClientOfFileChanged(std::shared_ptr<CDirChangeNotification> pNotify);

	bool	IncludeThisNotification(const std::string& strFileName);
	bool	ExcludeThisNotification(const std::string& strFileName);

	std::shared_ptr<CDirChangeNotification>	GetNotificationObj();
	void	DisposeOfNotification(std::shared_ptr<CDirChangeNotification> pNotify);

	void	SetPartialPathOffset(const CString& strWatchedDirname);

protected:
	std::shared_ptr<CDelayedNotifier>			_pDelayNotifier;
	std::shared_ptr<CDirectoryChangeHandler>	_pRealHandler;
	
	bool	_bAppHasGUI;
	DWORD	_dwFilterFlags;
	DWORD	_dwPartialPathOffset;	//helps support FILTERS_CHECK_PARTIAL_PATH

	friend	class CDirectoryChangeWatcher;
	friend	class CDirectoryChangeWatcher::CDirWatchInfo;

private:
	BOOL	_PathMatchSpec(const std::string& strPath);
	BOOL	_InitPathMatchFunc(const std::string& strIncludeFilter, const std::string& strExcludeFilter);
	BOOL	_InitPatterns(const std::string& strIncludeFilter, const std::string& strExcludeFilter);
	void	_UninitPathMatchFunc();

	bool	_UseRealPathMatchSpec() const;

private:
	HANDLE		_hWatchStoppedDispatchedEvent;

	std::string	_strIncludeFilter;
	std::string	_strExcludeFilter;

	//
	//	Load PathMatchSpec dynamically because it's only supported if IE 4.0 or greater is
	//	installed.
	static HMODULE	_hShlwapi_dll;
	static BOOL		_hShlwapi_dllExists;
	static long		_nRefCnt_hShlwapi;
	static Func_PatternMatchSpec	_fpPatternMatchSpec;

	//note: if the PathMatchSpec function isn't found, wildcmp() is used instead.
	//
	//	to support multiple file specs separated by a semi-colon,
	//	the include and exclude filters that are passed into the 
	//	the constructor are parsed into separate strings
	//	which are all checked in a loop.
	//
	int _nNumIncludeFilterSpecs;
	int _nNumExcludeFilterSpecs;
};

