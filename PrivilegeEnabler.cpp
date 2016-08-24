#include "stdafx.h"
#include "PrivilegeEnabler.h"
#include <vector>


BOOL EnablePrivilege(LPCTSTR pszPrivilegeName, BOOL fEnable /*= TRUE*/)
{
	BOOL fRet = FALSE;
	HANDLE hToken = nullptr;

	// Try to open this process's access token
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		// privilege
		TOKEN_PRIVILEGES tp = { 1 };
		if (LookupPrivilegeValue(nullptr, pszPrivilegeName, &tp.Privileges[0].Luid))
		{
			tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);

			fRet = (GetLastError() == ERROR_SUCCESS);
		}
		CloseHandle(hToken);
	}

	return fRet;
}

BOOL IsDirectory(const CString& path)
{
	auto dwAttri = GetFileAttributes(path);
	return ((dwAttri != 0xffffffff
		    && (dwAttri & FILE_ATTRIBUTE_DIRECTORY)));
}

CPrivilegeEnabler::CPrivilegeEnabler()
{
	std::vector<LPCTSTR> privilegeNames
	{
		SE_BACKUP_NAME,			//	these two are required for the FILE_FLAG_BACKUP_SEMANTICS flag used in the call to 
		SE_RESTORE_NAME,		//  CreateFile() to open the directory handle for ReadDirectoryChangesW
		SE_CHANGE_NOTIFY_NAME	//	just to make sure...it's on by default for all users.
								//<others here as needed>
	};

	for (const auto& name : privilegeNames)
	{
		if (!EnablePrivilege(name), TRUE)
		{
			TRACE(_T("Unable to enable privilege: %s	--	GetLastError(): %d\n"), name, GetLastError());
			TRACE(_T("CDirectoryChangeWatcher notifications may not work as intended due to insufficient access rights/process privileges.\n"));
			TRACE(_T("File: %s Line: %d\n"), _T(__FILE__), __LINE__);
		}
	}
}

CPrivilegeEnabler::~CPrivilegeEnabler()
{
}

CPrivilegeEnabler& CPrivilegeEnabler::Instance()
{
	static CPrivilegeEnabler theInstance;//constructs this first time it's called.
	return theInstance;
}
