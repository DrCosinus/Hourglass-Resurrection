/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

#pragma once

extern int ResultCount;


void reset_address_info();
void signal_new_frame();

//bool IsHardwareAddressValid(HWAddressType address);

void ReopenRamWindows(); //Reopen them when a new Rom is loaded
void Update_RAM_Search(); //keeps RAM values up to date in the search and watch windows
void InitRamSearch(); // call only once at program startup
void DeallocateRamSearch();


#define ALIGN16 __declspec(align(16)) // 16-byte alignment speeds up memcpy for size >= 0x100 (as of VS2005, if SSE2 is supported at runtime)


