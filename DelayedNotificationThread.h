#pragma once
#include "DelayedNotifier.h"
class CDelayedNotificationThread :
	public CDelayedNotifier
{
public:
	CDelayedNotificationThread();
	virtual ~CDelayedNotificationThread();
};

