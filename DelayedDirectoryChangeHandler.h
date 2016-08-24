#pragma once
#include "DirectoryChangeHandler.h"


class CDelayedDirectoryChangeHandler : public CDirectoryChangeHandler
{
public:
	CDelayedDirectoryChangeHandler();
	virtual ~CDelayedDirectoryChangeHandler();
};

