#pragma once
#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"
#include <WinBase.h>
#include <string>
#include <algorithm>

std::unique_ptr<g3::LogWorker>	gLogWorker;

void CheckLogDir(const std::string& logpath)
{
	auto attr = GetFileAttributes(logpath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES || attr & FILE_ATTRIBUTE_DIRECTORY)
	{
		CreateDirectory(logpath.c_str(), nullptr);
	}
}

std::string GetLogDir()
{
	TCHAR szModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(nullptr, szModulePath, MAX_PATH);

	std::string logpath, fullpath(std::begin(szModulePath), std::end(szModulePath));
	auto exePos = fullpath.find_last_of("\\");
	if (exePos != std::string::npos)
	{
		logpath.assign(fullpath.begin(), fullpath.begin() + exePos);
	}
	logpath.append("\\log");
	CheckLogDir(logpath);

	return logpath;
}

std::string GetModuleName()
{
	TCHAR szModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(nullptr, szModulePath, MAX_PATH);

	std::string fullpath(std::begin(szModulePath), std::end(szModulePath));
	auto exePos = fullpath.find_last_of("\\");
	auto lastDot = fullpath.find_last_of(".");

	return std::string(fullpath.begin() + exePos + 2, fullpath.begin() + lastDot);
}

static void LogInit()
{
	auto logpath = GetLogDir();
	auto exeName = GetModuleName();
	gLogWorker = g3::LogWorker::createLogWorker();
	gLogWorker->addDefaultLogger(exeName, logpath, "");
	g3::initializeLogging(gLogWorker.get());
}