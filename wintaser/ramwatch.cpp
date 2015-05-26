/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x0500
// Windows Header Files:
#include <windows.h>

#pragma region The Score
#include <Score/Ram/Watch/WatcherCollection.h>
#pragma endregion The Score

#include "resource.h"
#include "ramsearch.h"
#include "ramwatch.h"
#include "Config.h"
#include <assert.h>
#include <windows.h>
#include <string>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

static HMENU ramwatchmenu;
static HMENU rwrecentmenu;
static HACCEL RamWatchAccels = nullptr;
char rw_recent_files[MAX_RECENT_WATCHES][1024];
//char Watch_Dir[1024]="";
const unsigned int RW_MENU_FIRST_RECENT_FILE = 600;
bool RWfileChanged = false; //Keeps track of whether the current watch file has been changed, if so, ramwatch will prompt to save changes
bool AutoRWLoad = false;    //Keeps track of whether Auto-load is checked
bool RWSaveWindowPos = false; //Keeps track of whether Save Window position is checked
char currentWatch[1024];
int ramw_x, ramw_y;			//Used to store ramwatch dialog window positions
//AddressWatcher rswatches[MAX_WATCH_COUNT];
//int WatchCount=0;

#pragma region The Score
Score::Ram::Watch::WatcherCollection theWatchers;
#pragma endregion The Score

extern HWND RamWatchHWnd;
extern HWND hWnd;
extern HINSTANCE hInst;
extern CRITICAL_SECTION g_processMemCS;
//extern char exefilename [MAX_PATH+1];
//extern char thisprocessPath [MAX_PATH+1];
//extern bool started;
static char Str_Tmp_RW[1024];

void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths); //initializes the ram search and/or ram watch listbox

#define MESSAGEBOXPARENT (RamWatchHWnd ? RamWatchHWnd : hWnd)

bool QuickSaveWatches();
bool ResetWatches();

//bool IsSameWatch(const AddressWatcher& l, const AddressWatcher& r)
//{
//    return ((l.Address == r.Address) && (l.Size == r.Size) && (l.Type == r.Type)/* && (l.WrongEndian == r.WrongEndian)*/);
//}

//bool VerifyWatchNotAlreadyAdded(const Score::Ram::Watch::Watcher& watch, int /*skipIndex*/ = -1)
//{
//    //for (int j = 0; j < WatchCount; j++)
//    //{
//    //    if(j == skipIndex)
//    //        continue;
//    //    if(IsSameWatch(rswatches[j], watch))
//    //    {
//    //        if(RamWatchHWnd)
//    //            SetForegroundWindow(RamWatchHWnd);
//    //        return false;
//    //    }
//    //}
//    //return true;
//    auto b = theWatchers.CheckIfAlreadyPresent(watch);
//    if (b && RamWatchHWnd)
//    {
//        SetForegroundWindow(RamWatchHWnd);
//    }
//    return !b;
//}

auto InsertWatch(const Score::Ram::Watch::Watcher& aTemplateWatcher, const char* aDescription) -> bool
{
    return theWatchers.Insert(aTemplateWatcher,
        []()
    {
        if (RamWatchHWnd)
        {
            SetForegroundWindow(RamWatchHWnd);
        }
    },
        nullptr,
        []()
    {
        ListView_SetItemCount(GetDlgItem(RamWatchHWnd, IDC_WATCHLIST), theWatchers.Count());
        RWfileChanged = true;
    },
        aDescription);
}

LRESULT CALLBACK PromptWatchNameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //Gets the description of a watched address
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        //Clear_Sound_Buffer();

        GetWindowRect(hWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, nullptr, max(0, r.left + (dx1 - dx2)), max(0, r.top + (dy1 - dy2)), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, nullptr, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        strcpy(Str_Tmp_RW, "Enter a name for this RAM address.");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT, WM_SETTEXT, 0, (LPARAM)Str_Tmp_RW);
        strcpy(Str_Tmp_RW, "");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT2, WM_SETTEXT, 0, (LPARAM)Str_Tmp_RW);
        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            GetDlgItemText(hDlg, IDC_PROMPT_EDIT, Str_Tmp_RW, 80);
            InsertWatch(Score::Ram::Watch::Watcher(), Str_Tmp_RW);
            EndDialog(hDlg, true);
            return true;
            break;
        }
        case ID_CANCEL:
        case IDCANCEL:
            EndDialog(hDlg, false);
            return false;
            break;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, false);
        return false;
        break;
    }

    return false;
}

bool InsertWatch(const Score::Ram::Watch::Watcher& aTemplateWatcher, HWND parent)
{
    return theWatchers.Insert(aTemplateWatcher,
        []()
    {
        if (RamWatchHWnd)
        {
            SetForegroundWindow(RamWatchHWnd);
        }
    },
        [&parent]()
    {
        if (!parent)
            parent = RamWatchHWnd;
        if (!parent)
            parent = hWnd;
    },
        [&parent]()
    {
        DialogBox(hInst, MAKEINTRESOURCE(IDD_PROMPT), parent, (DLGPROC)PromptWatchNameProc);
    },
        nullptr);
}

void Update_RAM_Watch()
{
    //bool watchChanged[MAX_WATCH_COUNT] = { 0 };

    //if (WatchCount)
    //{
    //    // update cached values and detect changes to displayed listview items

    //    EnterCriticalSection(&g_processMemCS);
    //    for (int i = 0; i < WatchCount; i++)
    //    {
    //        RSVal prevCurValue = rswatches[i].CurValue;
    //        RSVal newCurValue = GetCurrentValue(rswatches[i]);
    //        if (!prevCurValue.CheckBinaryEquality(newCurValue))
    //        {
    //            rswatches[i].CurValue = newCurValue;
    //            watchChanged[i] = TRUE;
    //        }
    //    }
    //    LeaveCriticalSection(&g_processMemCS);
    //}
    theWatchers.UpdateCachedValuesAndHasChangedFlags();

    // refresh any visible parts of the listview box that changed
    HWND lv = GetDlgItem(RamWatchHWnd, IDC_WATCHLIST);
    auto top = ListView_GetTopIndex(lv);
    auto bottom = top + ListView_GetCountPerPage(lv) + 1; // +1 is so we will update a partially-displayed last item
    if (top < 0)
    {
        top = 0;
    }
    if (bottom > int(theWatchers.Count()))
    {
        bottom = int(theWatchers.Count());
    }
    int start = -1;
    for (int i = top; i <= bottom; i++)
    {
        if (start == -1)
        {
            if (i != bottom && theWatchers[i].myHasChanged)
            {
                start = i;
                //somethingChanged = true;
            }
        }
        else
        {
            if (i == bottom || !theWatchers[i].myHasChanged)
            {
                ListView_RedrawItems(lv, start, i - 1);
                start = -1;
            }
        }
    }
}

bool AskSave()
{
    //This function asks to save changes if the watch file contents have changed
    //returns false only if a save was attempted but failed or was cancelled
    if (RWfileChanged && !theWatchers.Empty())
    {
        int answer = MessageBox(MESSAGEBOXPARENT, "Save Changes?", "Ram Watch", MB_YESNOCANCEL);
        if (answer == IDYES)
            if (!QuickSaveWatches())
                return false;
        return (answer != IDCANCEL);
    }
    return true;
}

void UpdateRW_RMenu(HMENU menu, unsigned int mitem, unsigned int baseid)
{
    MENUITEMINFO moo;
    int x;

    moo.cbSize = sizeof(moo);
    moo.fMask = MIIM_SUBMENU | MIIM_STATE;

    GetMenuItemInfo(GetSubMenu(ramwatchmenu, 0), mitem, FALSE, &moo);
    moo.hSubMenu = menu;
    moo.fState = strlen(rw_recent_files[0]) ? MFS_ENABLED : MFS_GRAYED;

    SetMenuItemInfo(GetSubMenu(ramwatchmenu, 0), mitem, FALSE, &moo);

    // Remove all recent files submenus
    for (x = 0; x < MAX_RECENT_WATCHES; x++)
    {
        RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
    }

    // Recreate the menus
    for (x = MAX_RECENT_WATCHES - 1; x >= 0; x--)
    {
        char tmp[128 + 5];

        // Skip empty strings
        if (!strlen(rw_recent_files[x]))
        {
            continue;
        }

        moo.cbSize = sizeof(moo);
        moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

        // Fill in the menu text.
        if (strlen(rw_recent_files[x]) < 128)
        {
            sprintf(tmp, "&%d. %s", (x + 1) % 10, rw_recent_files[x]);
        }
        else
        {
            sprintf(tmp, "&%d. %s", (x + 1) % 10, rw_recent_files[x] + strlen(rw_recent_files[x]) - 127);
        }

        // Insert the menu item
        moo.cch = strlen(tmp);
        moo.fType = 0;
        moo.wID = baseid + x;
        moo.dwTypeData = tmp;
        InsertMenuItem(menu, 0, 1, &moo);
    }
}

void UpdateRWRecentArray(const char* addString, unsigned int arrayLen, HMENU menu, unsigned int menuItem, unsigned int baseId)
{
    // Try to find out if the filename is already in the recent files list.
    for (unsigned int x = 0; x < arrayLen; x++)
    {
        if (strlen(rw_recent_files[x]))
        {
            if (!strcmp(rw_recent_files[x], addString))    // Item is already in list.
            {
                // If the filename is in the file list don't add it again.
                // Move it up in the list instead.

                int y;
                char tmp[1024];

                // Save pointer.
                strcpy(tmp, rw_recent_files[x]);

                for (y = x; y; y--)
                {
                    // Move items down.
                    strcpy(rw_recent_files[y], rw_recent_files[y - 1]);
                }

                // Put item on top.
                strcpy(rw_recent_files[0], tmp);

                // Update the recent files menu
                UpdateRW_RMenu(menu, menuItem, baseId);

                return;
            }
        }
    }

    // The filename wasn't found in the list. That means we need to add it.

    // Move the other items down.
    for (unsigned int x = arrayLen - 1; x; x--)
    {
        strcpy(rw_recent_files[x], rw_recent_files[x - 1]);
    }

    // Add the new item.
    strcpy(rw_recent_files[0], addString);

    // Update the recent files menu
    UpdateRW_RMenu(menu, menuItem, baseId);
}


void RWAddRecentFile(const char *filename)
{
    UpdateRWRecentArray(filename, MAX_RECENT_WATCHES, rwrecentmenu, RAMMENU_FILE_RECENT, RW_MENU_FIRST_RECENT_FILE);
}

void OpenRWRecentFile(int memwRFileNumber)
{
    if (!ResetWatches())
    {
        return;
    }

    int rnum = memwRFileNumber;
    if ((unsigned int)rnum >= MAX_RECENT_WATCHES)
        return; //just in case

    char* x;

    while (true)
    {
        x = rw_recent_files[rnum];
        if (!*x)
            return;		//If no recent files exist just return.  Useful for Load last file on startup (or if something goes screwy)

        if (rnum) //Change order of recent files if not most recent
        {
            RWAddRecentFile(x);
            rnum = 0;
        }
        else
        {
            break;
        }
    }

    strcpy(currentWatch, x);
    strcpy(Str_Tmp_RW, currentWatch);

    //loadwatches here
    if (!theWatchers.ReadFile(Str_Tmp_RW,
        [&rnum]()
    {
        int answer = MessageBox(MESSAGEBOXPARENT, "Error opening file.", "ERROR", MB_OKCANCEL);
        if (answer == IDOK)
        {
            rw_recent_files[rnum][0] = '\0';    //Clear file from list 
            if (rnum)                           //Update the ramwatch list
            {
                RWAddRecentFile(rw_recent_files[0]);
            }
            else
            {
                RWAddRecentFile(rw_recent_files[1]);
            }
        }
    }))
    {
        return;
    }

    if (RamWatchHWnd)
    {
        ListView_SetItemCount(GetDlgItem(RamWatchHWnd, IDC_WATCHLIST), theWatchers.Count());
    }
    RWfileChanged = false;
    return;
}

int Change_File_L(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
    OPENFILENAME ofn;

    SetCurrentDirectory(Config::thisprocessPath);

    if (!strcmp(Dest, ""))
    {
        strcpy(Dest, "default.");
        strcat(Dest, Ext);
    }

    memset(&ofn, 0, sizeof(OPENFILENAME));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.hInstance = hInst;
    ofn.lpstrFile = Dest;
    ofn.nMaxFile = 2047;
    ofn.lpstrFilter = Filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = Dir;
    ofn.lpstrTitle = Titre;
    ofn.lpstrDefExt = Ext;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn)) return 1;

    return 0;
}

int Change_File_S(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
    OPENFILENAME ofn;

    SetCurrentDirectory(Config::thisprocessPath);

    if (!strcmp(Dest, ""))
    {
        strcpy(Dest, "default.");
        strcat(Dest, Ext);
    }

    memset(&ofn, 0, sizeof(OPENFILENAME));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.hInstance = hInst;
    ofn.lpstrFile = Dest;
    ofn.nMaxFile = 2047;
    ofn.lpstrFilter = Filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = Dir;
    ofn.lpstrTitle = Titre;
    ofn.lpstrDefExt = Ext;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileName(&ofn)) return 1;

    return 0;
}

bool Save_Watches()
{
    char* slash = max(strrchr(Config::exefilename, '\\'), strrchr(Config::exefilename, '/'));
    strcpy(Str_Tmp_RW, slash ? slash + 1 : Config::exefilename);
    char* dot = strrchr(Str_Tmp_RW, '.');
    if (dot)
    {
        *dot = '\0';
    }
    strcat(Str_Tmp_RW, ".wch");
    if (Change_File_S(Str_Tmp_RW, Config::thisprocessPath, "Save Watches", "Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
    {
        strcpy(currentWatch, Str_Tmp_RW);
        RWAddRecentFile(currentWatch);

        theWatchers.WriteFile(Str_Tmp_RW);

        RWfileChanged = false;

        return true;
    }
    return false;
}

bool QuickSaveWatches()
{
    if (RWfileChanged == false) return true; //If file has not changed, no need to save changes
    if (currentWatch[0] == '\0') //If there is no currently loaded file, run to Save as and then return
    {
        return Save_Watches();
    }

    strcpy(Str_Tmp_RW, currentWatch);

    theWatchers.WriteFile(Str_Tmp_RW);

    RWfileChanged = false;
    return true;
}

bool Load_Watches(bool clear, const char* filename)
{
    if (clear && !ResetWatches())
    {
        return false;
    }
    if (!theWatchers.ReadFile(filename, []() { MessageBox(MESSAGEBOXPARENT, "Error opening file.", "ERROR", MB_OK); }))
    {
        return false;
    }
    strcpy(currentWatch, filename);
    RWAddRecentFile(currentWatch);

    if (RamWatchHWnd)
    {
        ListView_SetItemCount(GetDlgItem(RamWatchHWnd, IDC_WATCHLIST), theWatchers.Count());
    }
    RWfileChanged = false;
    return true;
}

bool Load_Watches(bool clear)
{
    char* slash = max(strrchr(Config::exefilename, '\\'), strrchr(Config::exefilename, '/'));
    strcpy(Str_Tmp_RW, slash ? slash + 1 : Config::exefilename);
    char* dot = strrchr(Str_Tmp_RW, '.');
    if (dot) *dot = 0;
    strcat(Str_Tmp_RW, ".wch");
    if (Change_File_L(Str_Tmp_RW, Config::thisprocessPath, "Load Watches", "Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
    {
        return Load_Watches(clear, Str_Tmp_RW);
    }
    return false;
}

bool ResetWatches()
{
    if (!AskSave())
    {
        return false;
    }
    theWatchers.Clear();

    if (RamWatchHWnd)
    {
        ListView_SetItemCount(GetDlgItem(RamWatchHWnd, IDC_WATCHLIST), 0);
    }
    RWfileChanged = false;
    currentWatch[0] = '\0';
    return true;
}

void RemoveWatch(int watchIndex)
{
    theWatchers.Remove(watchIndex);
}

void RemoveWatch(const Score::Ram::Watch::Watcher& aWatcher, int ignoreIndex)
{
#pragma message("...warning...")
    theWatchers.Remove(aWatcher);
}

LRESULT CALLBACK EditWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //Gets info for a RAM Watch, and then inserts it into the Watch List
{
    RECT r;
   // RECT r2;
    //int dx1, dy1, dx2, dy2;
    //static int index;
    //static char s, t = s = 0;
    static Score::Ram::Watch::Watcher editedWatcher;
    static Score::Ram::Watch::Watcher* watcherPatternPtr;

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        //Clear_Sound_Buffer();

        GetWindowRect(hWnd, &r);
        //dx1 = (r.right - r.left) / 2;
        //dy1 = (r.bottom - r.top) / 2;

        //GetWindowRect(hDlg, &r2);
        //dx2 = (r2.right - r2.left) / 2;
        //dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, nullptr, max(0, r.left + (dx1 - dx2)), max(0, r.top + (dy1 - dy2)), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, nullptr, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        watcherPatternPtr = reinterpret_cast<Score::Ram::Watch::Watcher*>(lParam);
        editedWatcher = watcherPatternPtr ? *watcherPatternPtr : Score::Ram::Watch::Watcher();

        sprintf(Str_Tmp_RW, "%08X", editedWatcher.myAddress);
        SetDlgItemText(hDlg, IDC_EDIT_COMPAREADDRESS, Str_Tmp_RW);

        SetDlgItemText(hDlg, IDC_PROMPT_EDIT, editedWatcher.myDescription.c_str());

        switch (editedWatcher.myTypeID)
        {
        case Score::Ram::TypeID::SignedDecimal:
            SendDlgItemMessage(hDlg, IDC_SIGNED, BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
            EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
            break;
        case Score::Ram::TypeID::UnsignedDecimal:
            SendDlgItemMessage(hDlg, IDC_UNSIGNED, BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
            EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
            break;
        case Score::Ram::TypeID::Hexadecimal:
            SendDlgItemMessage(hDlg, IDC_HEX, BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
            EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
            break;
        case Score::Ram::TypeID::Float:
            SendDlgItemMessage(hDlg, IDC_FLOAT, BM_SETCHECK, BST_CHECKED, 0);
            if (editedWatcher.mySizeID == Score::Ram::SizeID::_8bits || editedWatcher.mySizeID == Score::Ram::SizeID::_16bits)
            {
                editedWatcher.mySizeID = Score::Ram::SizeID::_32bits;
            }
            EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), false);
            EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), false);
            break;
        }

        switch (editedWatcher.mySizeID)
        {
        case Score::Ram::SizeID::_8bits:
            SendDlgItemMessage(hDlg, IDC_1_BYTE, BM_SETCHECK, BST_CHECKED, 0);
            break;
        case Score::Ram::SizeID::_16bits:
            SendDlgItemMessage(hDlg, IDC_2_BYTES, BM_SETCHECK, BST_CHECKED, 0);
            break;
        case Score::Ram::SizeID::_32bits:
            SendDlgItemMessage(hDlg, IDC_4_BYTES, BM_SETCHECK, BST_CHECKED, 0);
            break;
        case Score::Ram::SizeID::_64bits:
            SendDlgItemMessage(hDlg, IDC_8_BYTES, BM_SETCHECK, BST_CHECKED, 0);
            break;
        }

        return true;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SIGNED:    editedWatcher.myTypeID = Score::Ram::TypeID::SignedDecimal;      return true;
        case IDC_UNSIGNED:  editedWatcher.myTypeID = Score::Ram::TypeID::UnsignedDecimal;    return true;
        case IDC_HEX:       editedWatcher.myTypeID = Score::Ram::TypeID::Hexadecimal;        return true;
        case IDC_FLOAT:     editedWatcher.myTypeID = Score::Ram::TypeID::Float;              return true;

        case IDC_1_BYTE:    editedWatcher.mySizeID = Score::Ram::SizeID::_8bits;             return true;
        case IDC_2_BYTES:   editedWatcher.mySizeID = Score::Ram::SizeID::_16bits;            return true;
        case IDC_4_BYTES:   editedWatcher.mySizeID = Score::Ram::SizeID::_32bits;            return true;
        case IDC_8_BYTES:   editedWatcher.mySizeID = Score::Ram::SizeID::_64bits;            return true;

        case IDOK:
        {
            editedWatcher.myEndianness = Score::Ram::Endianness::Little;

            GetDlgItemText(hDlg, IDC_EDIT_COMPAREADDRESS, Str_Tmp_RW, 1024);
            char *addrstr = Str_Tmp_RW;
            if (strlen(Str_Tmp_RW) > 8)
            {
                addrstr = &(Str_Tmp_RW[strlen(Str_Tmp_RW) - 9]);
            }
            for (int i = 0; addrstr[i]; i++)
            {
                if (toupper(addrstr[i]) == 'O')
                    addrstr[i] = '0';
            }
            sscanf(addrstr, "%08X", &(editedWatcher.myAddress));

            //if((Temp.Address & ~0xFFFFFF) == ~0xFFFFFF)
            //	Temp.Address &= 0xFFFFFF;

            bool canceled = false;
            if (theWatchers.CheckIfAlreadyPresent(editedWatcher))
            {
                int result = MessageBox(hDlg, "Watch already exists. Replace it?", "REPLACE", MB_YESNO);
                if (result == IDYES)
                {
                    RemoveWatch(editedWatcher, -1);
                }
                else if (result == IDNO)
                {
                    canceled = true;
                }
            }
            if (!canceled)
            {
                if (editedWatcher.myAddress.IsValid() || !Config::started || (watcherPatternPtr && editedWatcher.myAddress == watcherPatternPtr->myAddress))
                {
                    GetDlgItemText(hDlg, IDC_PROMPT_EDIT, Str_Tmp_RW, 80);
                    if (watcherPatternPtr)
                    {
                        *watcherPatternPtr = editedWatcher;
                        watcherPatternPtr->myDescription = Str_Tmp_RW;
                    }
                    else
                    {
                        InsertWatch(editedWatcher, Str_Tmp_RW);
                    }
                    if (RamWatchHWnd)
                    {
                        ListView_SetItemCount(GetDlgItem(RamWatchHWnd, IDC_WATCHLIST), theWatchers.Count());
                    }
                    EndDialog(hDlg, true);
                }
                else
                {
                    MessageBox(hDlg, "Invalid Address", "ERROR", MB_OK);
                }
            }

            RWfileChanged = true;
            return true;
            break;
        }
        case ID_CANCEL:
        case IDCANCEL:
            EndDialog(hDlg, false);
            return false;
            break;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, false);
        return false;
        break;
    }

    return false;
}

LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;
    static int watchIndex = 0;

    switch (uMsg)
    {
    case WM_MOVE: {
        RECT wrect;
        GetWindowRect(hDlg, &wrect);
        ramw_x = wrect.left;
        ramw_y = wrect.top;
        break;
    };

    case WM_INITDIALOG: {

        GetWindowRect(hWnd, &r);  //Ramwatch window
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2); // TASer window
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;


        // push it away from the main window if we can
        const int width = (r.right - r.left);
        const int height = (r.bottom - r.top);
        const int width2 = (r2.right - r2.left);
        if (r.left + width2 + width < GetSystemMetrics(SM_CXSCREEN))
        {
            r.right += width;
            r.left += width;
        }
        else if ((int)r.left - (int)width2 > 0)
        {
            r.right -= width2;
            r.left -= width2;
        }

        //-----------------------------------------------------------------------------------
        //If user has Save Window Pos selected, override default positioning
        if (RWSaveWindowPos)
        {
            //If ramwindow is for some reason completely off screen, use default instead 
            if (ramw_x > (-width * 2) || ramw_x < (width * 2 + GetSystemMetrics(SM_CYSCREEN)))
                r.left = ramw_x;	  //This also ignores cases of windows -32000 error codes
            //If ramwindow is for some reason completely off screen, use default instead 
            if (ramw_y >(0 - height * 2) || ramw_y < (height * 2 + GetSystemMetrics(SM_CYSCREEN)))
                r.top = ramw_y;		  //This also ignores cases of windows -32000 error codes
        }
        //-------------------------------------------------------------------------------------
        SetWindowPos(hDlg, nullptr, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        ramwatchmenu = GetMenu(hDlg);
        rwrecentmenu = CreateMenu();
        UpdateRW_RMenu(rwrecentmenu, RAMMENU_FILE_RECENT, RW_MENU_FIRST_RECENT_FILE);

        const char* names[3] = { "Address", "Value", "Notes" };
        int widths[3] = { 62, 64, 64 + 51 + 53 };
        init_list_box(GetDlgItem(hDlg, IDC_WATCHLIST), names, 3, widths);
        if (!ResultCount)
            reset_address_info();
        else
            signal_new_frame();
        ListView_SetItemCount(GetDlgItem(hDlg, IDC_WATCHLIST), theWatchers.Count());
        //if(littleEndian) SendDlgItemMessage(hDlg, IDC_ENDIAN, BM_SETCHECK, BST_CHECKED, 0);

        RamWatchAccels = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

        Update_RAM_Watch();

        DragAcceptFiles(hDlg, TRUE);

        return true;
        break;
    }

    case WM_INITMENU:
        CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, AutoRWLoad ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, RWSaveWindowPos ? MF_CHECKED : MF_UNCHECKED);
        break;

    case WM_MENUSELECT:
    case WM_ENTERSIZEMOVE:
        //Clear_Sound_Buffer();
        break;

    case WM_NOTIFY:
    {
        LPNMHDR lP = (LPNMHDR)lParam;
        switch (lP->code)
        {
        case LVN_GETDISPINFO:
        {
            LV_DISPINFO *Item = (LV_DISPINFO *)lParam;
            Item->item.mask = LVIF_TEXT;
            Item->item.state = 0;
            Item->item.iImage = 0;
            const unsigned int iNum = Item->item.iItem;
            static char num[64];
            switch (Item->item.iSubItem)
            {
            case 0:
                sprintf(num, "%08X", theWatchers[iNum].myAddress);
                Item->item.pszText = num;
                return true;
            case 1:
            {
                theWatchers[iNum].sprint(num);
                Item->item.pszText = num;
                return true;
            }
            case 2:
#pragma message ("DrCos: const_cast... beurk!")
                Item->item.pszText = const_cast<LPSTR>(theWatchers[iNum].myDescription.c_str());
                return true;

            default:
                return false;
            }
        }
        case LVN_ODFINDITEM:
        {
            // disable search by keyboard typing,
            // because it interferes with some of the accelerators
            // and it isn't very useful here anyway
            SetWindowLong(hDlg, DWL_MSGRESULT, ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST)));
            return 1;
        }
        }
    }
    break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case RAMMENU_FILE_SAVE:
            QuickSaveWatches();
            break;

        case RAMMENU_FILE_SAVEAS:
            //case IDC_C_SAVE:
            return Save_Watches();
        case RAMMENU_FILE_OPEN:
            return Load_Watches(true);
        case RAMMENU_FILE_APPEND:
            //case IDC_C_LOAD:
            return Load_Watches(false);
        case RAMMENU_FILE_NEW:
            //case IDC_C_RESET:
            ResetWatches();
            return true;
        case IDC_C_WATCH_REMOVE:
            watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
            RemoveWatch(watchIndex);
            ListView_SetItemCount(GetDlgItem(hDlg, IDC_WATCHLIST), theWatchers.Count());
            RWfileChanged = true;
            SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
            return true;
        case IDC_C_WATCH_EDIT:
//#pragma message( "DrCos: Sorry 'Edit Watch' is broken!")
            watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
            DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC)EditWatchProc, (LPARAM)&theWatchers[watchIndex]);
            SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
            return true;
        case IDC_C_WATCH: // New watch
#pragma message( "DrCos: Sorry 'New Watch' is broken!")
            //rswatches[WatchCount].Address = rswatches[WatchCount].WrongEndian = 0;
            //rswatches[WatchCount].Size = 'b';
            //rswatches[WatchCount].Type = 's';
            //DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC)EditWatchProc, (LPARAM)nullptr);
            //SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
            return true;
        case IDC_C_WATCH_DUPLICATE:
#pragma message( "DrCos: Sorry 'Duplicate Watch' is broken!")
            //watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
            //rswatches[WatchCount].Address = rswatches[watchIndex].Address;
            //rswatches[WatchCount].WrongEndian = rswatches[watchIndex].WrongEndian;
            //rswatches[WatchCount].Size = rswatches[watchIndex].Size;
            //rswatches[WatchCount].Type = rswatches[watchIndex].Type;
            //DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC)EditWatchProc, (LPARAM)nullptr);
            //SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
            return true;
        case IDC_C_WATCH_UP:
        {
            //watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
            //if (watchIndex == 0 || watchIndex == -1)
            //    return true;
            //void *tmp = malloc(sizeof(AddressWatcher));
            //memcpy(tmp, &(rswatches[watchIndex]), sizeof(AddressWatcher));
            //memcpy(&(rswatches[watchIndex]), &(rswatches[watchIndex - 1]), sizeof(AddressWatcher));
            //memcpy(&(rswatches[watchIndex - 1]), tmp, sizeof(AddressWatcher));
            //free(tmp);
            //ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex - 1);
            //ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex - 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            //ListView_SetItemCount(GetDlgItem(hDlg, IDC_WATCHLIST), WatchCount);
            //RWfileChanged = true;
            return true;
        }
        case IDC_C_WATCH_DOWN:
        {
            //watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
            //if (watchIndex >= WatchCount - 1 || watchIndex == -1)
            //    return true;
            //void *tmp = malloc(sizeof(AddressWatcher));
            //memcpy(tmp, &(rswatches[watchIndex]), sizeof(AddressWatcher));
            //memcpy(&(rswatches[watchIndex]), &(rswatches[watchIndex + 1]), sizeof(AddressWatcher));
            //memcpy(&(rswatches[watchIndex + 1]), tmp, sizeof(AddressWatcher));
            //free(tmp);
            //ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex + 1);
            //ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex + 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            //ListView_SetItemCount(GetDlgItem(hDlg, IDC_WATCHLIST), WatchCount);
            //RWfileChanged = true;
            return true;
        }
        case RAMMENU_FILE_AUTOLOAD:
        {
            AutoRWLoad ^= 1;
            CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, AutoRWLoad ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        case RAMMENU_FILE_SAVEWINDOW:
        {
            RWSaveWindowPos ^= 1;
            CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, RWSaveWindowPos ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        case IDC_C_ADDCHEAT:
        {
            //					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST)) | (1 << 24);
            //					DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDITCHEAT), hDlg, (DLGPROC) EditCheatProc,(LPARAM) searchIndex);
        }
        case IDOK:
        case IDCANCEL:
            RamWatchHWnd = nullptr;
            DragAcceptFiles(hDlg, FALSE);
            EndDialog(hDlg, true);
            return true;
        default:
            if (LOWORD(wParam) >= RW_MENU_FIRST_RECENT_FILE && LOWORD(wParam) < RW_MENU_FIRST_RECENT_FILE + MAX_RECENT_WATCHES)
                OpenRWRecentFile(LOWORD(wParam) - RW_MENU_FIRST_RECENT_FILE);
        }
        break;

    case WM_KEYDOWN: // handle accelerator keys
    {
        SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
        MSG msg;
        msg.hwnd = hDlg;
        msg.message = uMsg;
        msg.wParam = wParam;
        msg.lParam = lParam;
        if (RamWatchAccels && TranslateAccelerator(hDlg, RamWatchAccels, &msg))
            return true;
    }	break;

    case WM_CLOSE:
        RamWatchHWnd = nullptr;
        DragAcceptFiles(hDlg, FALSE);
        EndDialog(hDlg, true);
        return true;

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        DragQueryFile(hDrop, 0, Str_Tmp_RW, 1024);
        DragFinish(hDrop);
        return Load_Watches(true, Str_Tmp_RW);
    }	break;
    }

    return false;
}
