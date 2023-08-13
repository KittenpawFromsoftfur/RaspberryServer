#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "core.h"

CCore::CCore()
{
	m_sTimeStart = {0};
}

// makes a dir if not yet existing
int CCore::Mkdir(const char *pPath)
{
	struct stat sStat = {0};
	int retval = 0;

	// check if folders exist already
	retval = stat(pPath, &sStat);
	if (retval != 0)
	{
		// create folders
		retval = mkdir(pPath, 0777);
		if (retval != 0)
			return ERROR;
	}

	return OK;
}

int CCore::RemoveFile(const char *pFilename)
{
	int retval = 0;

	retval = remove(pFilename);
	if (retval != 0)
		return ERROR;

	return OK;
}

// removes all files in a directory
int CCore::RemoveFilesDirectory(const char *pDirname)
{
	DIR *pDir = 0;
	struct dirent *psDirent = 0;
	int retval = 0;
	char aFilepath[CCORE_MAXLEN_FILEPATH] = {0};

	// check if directory exists
	pDir = opendir(pDirname);
	if (!pDir)
		return ERROR;

	// read files
	while (1)
	{
		psDirent = readdir(pDir);

		if (!psDirent)
			break;

		// skip dir names
		if (strncmp(psDirent->d_name, ".", 2) == 0 ||
			strncmp(psDirent->d_name, "..", 3) == 0)
			continue;

		// make filepath
		memset(aFilepath, 0, ARRAYSIZE(aFilepath)); // zero as good measure
		snprintf(aFilepath, ARRAYSIZE(aFilepath), "%s/%s", pDirname, psDirent->d_name);

		// remove file
		retval = RemoveFile(aFilepath);
		if (retval != OK)
		{
			closedir(pDir);
			return ERROR;
		}
	}

	closedir(pDir);
	return OK;
}

// true=exists, false=not-exist, ERROR=error
int CCore::CountFilesDirectory(const char *pDirname)
{
	DIR *pDir = 0;
	struct dirent *psDirent = 0;
	int amount = 0;

	// check if directory exists
	pDir = opendir(pDirname);
	if (!pDir)
		return ERROR;

	// read files
	while (1)
	{
		psDirent = readdir(pDir);

		if (!psDirent)
			break;

		// skip dir names
		if (strncmp(psDirent->d_name, ".", 2) == 0 ||
			strncmp(psDirent->d_name, "..", 3) == 0)
			continue;

		amount++;
	}

	closedir(pDir);
	return amount;
}

// true=exists, false=does not exist, ERROR=error
int CCore::CheckFileExists(const char *pDirname, const char *pFilename, size_t LenFilename)
{
	DIR *pDir = 0;
	struct dirent *psDirent = 0;

	// check if already existing
	pDir = opendir(pDirname);
	if (!pDir)
		return ERROR;

	// read files
	while (1)
	{
		psDirent = readdir(pDir);

		if (!psDirent)
			break;

		if (strncmp(psDirent->d_name, pFilename, LenFilename) == 0)
		{
			closedir(pDir);
			return true;
		}
	}

	closedir(pDir);
	return false;
}

int CCore::StringCompareNocase(const char *pSource, const char *pDest, size_t Len)
{
	signed char diff = 0;

	for (int i = 0; i < Len; ++i)
	{
		if (!pSource[i] && !pDest[i])
			break;

		diff = tolower(pSource[i]) - tolower(pDest[i]);

		if (diff != 0)
			return diff;
	}

	return 0;
}

void CCore::StringCopyIgnore(char *pDest, const char *pSource, size_t Len, const char *pIgnore)
{
	int destIndex = 0;
	int ignLen = strnlen(pIgnore, CCORE_MAXLEN_STRINGIGNORE);
	int ignore = false;

	for (int i = 0; i < Len; ++i)
	{
		if (!pSource[i])
			break;

		// check if next source char should be ignored
		ignore = false;

		for (int ign = 0; ign < ignLen; ++ign)
		{
			if (pSource[i] == pIgnore[ign])
			{
				ignore = true;
				break;
			}
		}

		if (ignore)
		{
			destIndex++;
			continue;
		}

		pDest[destIndex] = pSource[i];
		destIndex++;
	}
}

void CCore::StringRemove(char *pSource, size_t Len, const char *pRem)
{
	int remLen = strnlen(pRem, Len);
	int remove = false;
	int i = 0;

	while (1)
	{
		if (!pSource[i])
			break;

		// check if next source char can be removed
		remove = false;

		for (int rem = 0; rem < remLen; ++rem)
		{
			if (pSource[i] == pRem[rem])
			{
				remove = true;
				break;
			}
		}

		if (remove)
		{
			for (int index = i; index < Len - 1; ++index)
			{
				pSource[index] = pSource[index + 1];

				if (!pSource[index])
					break;
			}
		}
		else
		{
			i++;
		}
	}

	// null terminate string
	pSource[i] = '\0';
}

void CCore::StringToUpper(char *pSource, size_t Len)
{
	for (int i = 0; i < Len; ++i)
	{
		if (!pSource[i])
			break;

		pSource[i] = toupper(pSource[i]);
	}
}

void CCore::StringToLower(char *pSource, size_t Len)
{
	for (int i = 0; i < Len; ++i)
	{
		if (!pSource[i])
			break;

		pSource[i] = tolower(pSource[i]);
	}
}

int CCore::CheckStringAscii(const char *pString, size_t Len)
{
	char ch = 0;

	for (int i = 0; i < Len; ++i)
	{
		ch = pString[i];

		if (!ch)
			break;

		// allow 0-9, A-Z, a-z
		if (ch < 48 || (ch > 57 && ch < 65) || (ch > 90 && ch < 97) || ch > 122)
			return false;
	}

	return true;
}

int CCore::IsLetter(char Char)
{
	if (Char < 65 || (Char > 90 && Char < 97) || Char > 122)
		return false;

	return true;
}

int CCore::IsNumber(char Char)
{
	if (Char < 48 || Char > 57)
		return false;

	return true;
}

int CCore::DetachThreadSafely(std::thread *pThread)
{
	if (!pThread)
		return ERROR;

	if (pThread->joinable())
		pThread->detach();

	return OK;
}

int CCore::MeasureStart()
{
	if (clock_gettime(CLOCK_REALTIME, &m_sTimeStart) != 0)
		return ERROR;

	return OK;
}

double CCore::MeasureStop(int Print)
{
	struct timespec sTimeStop = {0};
	double diff = 0;
	int seconds = 0;
	int millies = 0;
	int micros = 0;

	if (clock_gettime(CLOCK_REALTIME, &sTimeStop) != 0)
		return ERROR;

	diff = (sTimeStop.tv_sec - m_sTimeStart.tv_sec) + (double)(sTimeStop.tv_nsec - m_sTimeStart.tv_nsec) / 1000000000L;

	// output elapsed time if wanted
	if (Print)
	{
		seconds = diff;
		millies = (diff - seconds) * 1000;
		micros = (diff - seconds - (double)millies / 1000) * 1000000;
		printf("*** Time passed: %ds %dms %dµs (%0.6fs)\n", seconds, millies, micros, diff);
	}

	return diff;
}