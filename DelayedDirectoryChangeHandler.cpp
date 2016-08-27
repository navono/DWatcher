#include "stdafx.h"
#include "DelayedDirectoryChangeHandler.h"


CDelayedDirectoryChangeHandler::CDelayedDirectoryChangeHandler(std::shared_ptr<CDirectoryChangeHandler> pRealHandler, 
	bool bAppHasGUI, const std::string& strIncludeFilter, const std::string& strExcludeFilter, DWORD dwFilterFlags)
{

}

CDelayedDirectoryChangeHandler::~CDelayedDirectoryChangeHandler()
{
}

void CDelayedDirectoryChangeHandler::PostNotification(std::shared_ptr<CDirChangeNotification> pNotification)
{

}

void CDelayedDirectoryChangeHandler::DispatchNotificationFunction(std::shared_ptr<CDirChangeNotification> pNotification)
{

}

void CDelayedDirectoryChangeHandler::On_FileAdd(const CString& strFileName)
{

}

void CDelayedDirectoryChangeHandler::On_FileRemoved(const CString& strFileName)
{
	
}

void CDelayedDirectoryChangeHandler::On_FileModified(const CString& strFileName)
{

}

void CDelayedDirectoryChangeHandler::On_FileNameChanged(const CString& strFileName)
{

}

void CDelayedDirectoryChangeHandler::On_ReadDiretoryChangesError(DWORD dwError, const CString& strDirName)
{

}

void CDelayedDirectoryChangeHandler::On_WatchStarted(DWORD dwError, const CString& strDirName)
{

}

void CDelayedDirectoryChangeHandler::On_WatchStopped(const CString& strDirName)
{

}

void CDelayedDirectoryChangeHandler::SetChangeDirectoryName(const CString& strDirName)
{

}

const CString& CDelayedDirectoryChangeHandler::GetChangedDirectoryName() const
{

}

BOOL CDelayedDirectoryChangeHandler::WaitForOnWatchStoppedDispatched()
{

}

bool CDelayedDirectoryChangeHandler::NotifyClientOfFileChanged(std::shared_ptr<CDirChangeNotification> pNotify)
{

}

bool CDelayedDirectoryChangeHandler::IncludeThisNotification(const std::string& strFileName)
{

}

bool CDelayedDirectoryChangeHandler::ExcludeThisNotification(const std::string& strFileName)
{

}

std::shared_ptr<CDirChangeNotification> CDelayedDirectoryChangeHandler::GetNotificationObj()
{

}

void CDelayedDirectoryChangeHandler::DisposeOfNotification(std::shared_ptr<CDirChangeNotification> pNotify)
{

}

void CDelayedDirectoryChangeHandler::SetPartialPathOffset(const CString& strWatchedDirname)
{

}


//////////////////////////////////////////////////////////////////////////
BOOL CDelayedDirectoryChangeHandler::_PathMatchSpec(const std::string& strPath)
{

}

BOOL CDelayedDirectoryChangeHandler::_InitPathMatchFunc(const std::string& strIncludeFilter, const std::string& strExcludeFilter)
{

}

BOOL CDelayedDirectoryChangeHandler::_InitPatterns(const std::string& strIncludeFilter, const std::string& strExcludeFilter)
{

}

void CDelayedDirectoryChangeHandler::_UninitPathMatchFunc()
{

}

bool CDelayedDirectoryChangeHandler::_UseRealPathMatchSpec() const
{

}
