#pragma once

// Helper functions
static BOOL EnablePrivilege(LPCTSTR pszPrivilegeName, BOOL fEnable = TRUE);
static BOOL IsDirectory(const CString& path);

class CPrivilegeEnabler
{
private:
	CPrivilegeEnabler();

public:
	~CPrivilegeEnabler();

	static CPrivilegeEnabler& Instance();
};

