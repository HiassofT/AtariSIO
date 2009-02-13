#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "winver.h"
#include "OS.h"
#include "AtariDebug.h"

char *realpath(const char *path, char *resolved_path)
{
	if (strlen(path) >= PATH_MAX) {
		return 0;
	}
	strcpy(resolved_path, path);
	return resolved_path;
}
