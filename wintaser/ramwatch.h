/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

#pragma once

#include <Score/Ram/Watch/Watcher.h>

#include "ramsearch.h"

bool ResetWatches();
void OpenRWRecentFile(int memwRFileNumber);
extern bool AutoRWLoad;
extern bool RWSaveWindowPos;
#define MAX_RECENT_WATCHES 5
extern char rw_recent_files[MAX_RECENT_WATCHES][1024];
extern bool AskSave();
extern int ramw_x;
extern int ramw_y;
extern bool RWfileChanged;

//enum class CCHHEECCKK;
// AddressWatcher is self-contained now
//struct AddressWatcher
//{
//    unsigned int Address; // hardware address
//    char/*/CCHHEECCKK*/ Size;
//    char Type;
//    char* comment; // nullptr means no comment, non-nullptr means allocated comment
//    bool WrongEndian;
//    RSVal CurValue;
//};
//#define MAX_WATCH_COUNT 256
//extern AddressWatcher rswatches[MAX_WATCH_COUNT];
//extern int WatchCount; // number of valid items in rswatches

extern char Watch_Dir[1024];

bool InsertWatch(const Score::Ram::Watch::Watcher& Watch, const char *Comment);
void RemoveWatch(const Score::Ram::Watch::Watcher& Watch, int ignoreIndex = -1);
bool InsertWatch(const Score::Ram::Watch::Watcher& Watch, HWND parent = nullptr); // asks user for comment
void Update_RAM_Watch();
bool Load_Watches(bool clear, const char* filename);
