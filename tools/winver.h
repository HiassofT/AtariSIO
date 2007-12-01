#ifndef WINVER_H
#define WINVER_H

#ifdef WINVER

#define snprintf(str, size, args...) sprintf(str, args)
#define vsnprintf(str, size, args...) vsprintf(str, args)

#endif

#endif
