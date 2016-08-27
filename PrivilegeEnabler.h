#pragma once

// Helper functions
static BOOL EnablePrivilege(LPCTSTR pszPrivilegeName, BOOL fEnable = TRUE);

class CPrivilegeEnabler
{
private:
	CPrivilegeEnabler();

public:
	~CPrivilegeEnabler();

	static CPrivilegeEnabler& Instance();
};

