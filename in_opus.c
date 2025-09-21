/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * in_opus.c by Raymond GILLIBERT, from template in_raw.c.                 *
 *                                                                         *
 * This is a WINAMP 2+ Plugin to play .opus files and radio streams.       *
 * I think you can do whatever you want with the code if you find it.      *
 * The code depends on libopus, libogg and libopusfile                     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <math.h>
#include <windows.h>
#include <string.h>
#include <opus/opusfile.h>
#include <limits.h>
#include "resource.h"
#include "infobox.h"
#include "in2.h"
#include "wa_ipc.h"
#include "resample.h"

// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF WM_USER+2
#define IN_UNICODE 0x0F000000

// raw configuration.
#define NCH 2
#define SR 48000
#define VERBOSE 0
#define BIG_LENGTH INT_MAX
#define BIG_LENGTH_MULT 60
#define DECODE_BUFF_SIZE (20*(SR/1000)*NCH)// Smaller one 20ms in // in SAMPLES

// FUNCTION DECLARATION
static void first_init(char *font_str, int *font_height);
void setvolume(int volume);
void pause();
char *utf16_to_utf8(const wchar_t *input);
wchar_t *utf8_to_utf16(const char *utfs);
DWORD WINAPI DecodeThread(LPVOID b); // the decode thread procedure

// GLOBAL VARIABLES
static In_Module mod;  // the output module (filled in near the bottom of this file)
char lastfn[4*MAX_PATH]; // currently playing file

opus_int64 decode_pos_ms; // current decoding position, in milliseconds.
                          // Used for correcting DSP plug-in pitch changes
volatile opus_int64 seek_needed; // if != -1, it is the point that the decode
                                 // thread should seek to, in ms.
int paused;               // are we paused?
HANDLE thread_handle=INVALID_HANDLE_VALUE; // the handle to the decode thread

volatile char killDecodeThread=0;    // the kill switch for the decode thread

char TAGS_DISPLAY_MODE, isNT; // 0: raw, 1: ANSI, 2: Unicode, 3: AUTO
char UNICODE_FILE;
char TARGET_LUFS;
char USE_REPLAY_GAIN=0; // 0 : no, 1: Album, 2: Track.
char UNIFIED_DIALOG;
HFONT TAGS_FONT=NULL;

static OggOpusFile *_of=NULL;
static int THREAD_PRIORITY;
static opus_int32 PRE_GAIN;
static opus_int32 RADIO_GAIN;
static int OP_CURRENT_GAIN;
static int SAMPLERATE;
static char RESAMPLE_Q;
static char BPS;
static char INTERNAL_VOLUME;
static char VIS_BROKEN; // Winamp version < 5.11
static char HOURS_MODE_ON; // to fix the 24days 20h 31min 24s and 647ms bug
static char USE_DITHERING;
static char RADIO;
static char OGGEXT_HACK;
static char INSTANT_BR;
static char FORMAT_TITLE;

/////////////////////////////////////////////////////////////////////////////
// This function is mendatory when linking with MinGW to be able to load dll
// Uner windows 95. You have to built everithing with the following flags
// -mdll -e _DllMain@12 -fno-stack-check -fno-stack-protector
// -mno-stack-arg-probe -nostdlib -lgcc -lkernel32 -lmsvcrt -luser32 -lgdi32
// I Know that is a lot.... (I am using TDM GCC 5.1, [2015])
BOOL WINAPI DllMain(HANDLE hInst, ULONG reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Configuration box we'd want to write it here (using DialogBox, etc)
void config(HWND hwndParent)
{
    int font_height;
    char monstring[1024];
    char font_str[LF_FACESIZE];

    // To read the config so we show up to date data
    // This also makes a way not to restart winamp...
    first_init(font_str, &font_height);

    sprintf(monstring,
        "Open \"winamp.ini\" with notepad and write the options you want to change\n"
        "Current Config:\n\n"

        "[IN_OPUS]\n"
        "USE_REPLAY_GAIN=%d\n"
        "PRE_GAIN=%.2f\n"
        "RADIO_GAIN=%.2f\n"
        "TAGS_DISPLAY_MODE=%d\n"
        "TAGS_FONT=%d %s\n"
        "OUT_SAMPLERATE=%d\n"
        "OUT_BITS=%d\n"
        "INTERNAL_VOLUME=%d\n"
        "TARGET_LUFS=%d\n\n"

        "USE_REPLAY_GAIN values:\n"
        "0: Disable (default), 1: Album gain 2: Track gain \n"
        "3: Auto track/album gain when shuffle on/off\n"
        "4: Raw gain (not even header gain)\n"
        "PRE_GAIN is a preamplification factor in dB, applied before RG.\n"
        "TAGS_DISPLAY_MODE: 0: Raw, 1: ANSI, 2: Force Unicode 3: Auto\n"
        "OUT_SAMPLERATE to set the output samplerate\n"
        "INTERNAL_VOLUME set to 1 to enable internal handling of volume.\n"
        "TARGET_LUFS, is the Loudness in Units of Full Scale that you want (default -23)"

        , USE_REPLAY_GAIN, ((float)PRE_GAIN)/256.F, ((float)RADIO_GAIN)/256.F
        , TAGS_DISPLAY_MODE, font_height, font_str, SAMPLERATE, BPS
        , INTERNAL_VOLUME, TARGET_LUFS
    );
    MessageBox(hwndParent, monstring, "This is not the configuration you are looking for", MB_OK);
}
/////////////////////////////////////////////////////////////////////////////
// About Dialog box...
void about(HWND hwndParent)
{
    MessageBox(hwndParent,
    "OPUS File Player v0.913, by Raymond Gillibert (*.opus, *.opu files).\n"
    "Using libopus 1.5.2, libogg 1.3.2 and libopusfile 0.12.\n"
    "You can write me at raymond_gillibert@yahoo.fr if necessary."
    , "About Winamp OPUS Player",MB_OK);
}

/////////////////////////////////////////////////////////////////////////////
// To read the internal config
static void readinternalconfig(char *font_str, int *font_height)
{
    float pre_gain_float=0.0f, radio_gain_float=0.0f;
    int UNICODE_FILE_i=0, RESAMPLE_Q_i=0, USE_REPLAY_GAIN_i=0, INTERNAL_VOLUME_i=0
       , TAGS_DISPLAY_MODE_i=0, USE_DITHERING_i=0, TARGET_LUFS_i=0, bps_i=0
       , OGGEXT_HACK_i=0, UNIFIED_DIALOG_i=0, INSTANT_BR_i=0, FORMAT_TITLE_i=0;

    THREAD_PRIORITY = THREAD_PRIORITY_ABOVE_NORMAL;

    sscanf( "Out=48000, 2, 16 Uni=2, Gain=0, +0.0000, -3.0000, -23, 0, 3, 1, 0, 1, 1, 1, 0 "
            "Times New Roman or other font with a super long name",
            "Out=%d, %d, %d Uni=%d, Gain=%d, %f, %f, %d, %d, %d, %d, %d, %d, %d, %d, %d %[^\n]s"
            , &SAMPLERATE, &RESAMPLE_Q_i, &bps_i
            , &UNICODE_FILE_i
            , &USE_REPLAY_GAIN_i, &pre_gain_float, &radio_gain_float
            , &TARGET_LUFS_i
            , &INTERNAL_VOLUME_i
            , &TAGS_DISPLAY_MODE_i
            , &USE_DITHERING_i
            , &OGGEXT_HACK_i
            , &UNIFIED_DIALOG_i
            , &INSTANT_BR_i
            , &FORMAT_TITLE_i
            , font_height, font_str);

    RESAMPLE_Q        = (char) RESAMPLE_Q_i;
    UNICODE_FILE      = (char) UNICODE_FILE_i;
    BPS               = (char) bps_i;
    USE_REPLAY_GAIN   = (char) USE_REPLAY_GAIN_i;
    TAGS_DISPLAY_MODE = (char) TAGS_DISPLAY_MODE_i;
    USE_DITHERING     = (char) USE_DITHERING_i;
    INTERNAL_VOLUME   = (char) INTERNAL_VOLUME_i;
    TARGET_LUFS       = (char) TARGET_LUFS_i;
    OGGEXT_HACK       = (char) OGGEXT_HACK_i;
    UNIFIED_DIALOG    = (char) UNIFIED_DIALOG_i;
    INSTANT_BR        = (char) INSTANT_BR_i;
    FORMAT_TITLE      = (char) FORMAT_TITLE_i;
    PRE_GAIN          = lrintf(pre_gain_float*256.F);
    RADIO_GAIN        = lrintf(radio_gain_float*256.F);
}
/////////////////////////////////////////////////////////////////////////////
// To read the config
static void readconfig(char *ini_name, char *rez, char *font_str, int *font_height)
{
    if(!ini_name || *ini_name=='\0' || !rez || !font_str || !font_height) return;
    if(INVALID_FILE_ATTRIBUTES == GetFileAttributes(ini_name)) return;

    USE_REPLAY_GAIN  =GetPrivateProfileInt("IN_OPUS", "USE_REPLAY_GAIN", USE_REPLAY_GAIN, ini_name);
    THREAD_PRIORITY  =GetPrivateProfileInt("IN_OPUS", "THREAD_PRIORITY", THREAD_PRIORITY, ini_name);
    TAGS_DISPLAY_MODE=GetPrivateProfileInt("IN_OPUS", "TAGS_DISPLAY_MODE", TAGS_DISPLAY_MODE, ini_name);
    USE_DITHERING    =GetPrivateProfileInt("IN_OPUS", "USE_DITHERING", USE_DITHERING, ini_name);
    UNICODE_FILE     =GetPrivateProfileInt("IN_OPUS", "UNICODE_FILE", UNICODE_FILE, ini_name);
    SAMPLERATE       =GetPrivateProfileInt("IN_OPUS", "OUT_SAMPLERATE", SAMPLERATE, ini_name);
    RESAMPLE_Q       =GetPrivateProfileInt("IN_OPUS", "RESAMPLE_Q", RESAMPLE_Q, ini_name);
    INTERNAL_VOLUME  =GetPrivateProfileInt("IN_OPUS", "INTERNAL_VOLUME", INTERNAL_VOLUME, ini_name);
    TARGET_LUFS      =GetPrivateProfileInt("IN_OPUS", "TARGET_LUFS", TARGET_LUFS, ini_name);
    BPS              =GetPrivateProfileInt("IN_OPUS", "OUT_BITS", BPS, ini_name);
    OGGEXT_HACK      =GetPrivateProfileInt("IN_OPUS", "OGGEXT_HACK", OGGEXT_HACK, ini_name);
    UNIFIED_DIALOG   =GetPrivateProfileInt("IN_OPUS", "UNIFIED_DIALOG", UNIFIED_DIALOG, ini_name);
    INSTANT_BR       =GetPrivateProfileInt("IN_OPUS", "INSTANT_BR", INSTANT_BR, ini_name);
    FORMAT_TITLE     =GetPrivateProfileInt("IN_OPUS", "FORMAT_TITLE", FORMAT_TITLE, ini_name);

    if(BPS!=8 && BPS!=24 && BPS!=32) BPS=16;

    if(RESAMPLE_Q < 0) RESAMPLE_Q=0;    else if (RESAMPLE_Q > 4)      RESAMPLE_Q=4;
    if(SAMPLERATE < 1) SAMPLERATE = SR; else if (SAMPLERATE > 192000) SAMPLERATE=192000;

    if(GetPrivateProfileString("IN_OPUS", "PRE_GAIN", "666.0", rez, 255, ini_name)){
        float tmpfloatgain;
        sscanf(rez, "%f", &tmpfloatgain);
        if(tmpfloatgain < 665.0F) PRE_GAIN = lrintf(tmpfloatgain*256.F);
    }
    if(GetPrivateProfileString("IN_OPUS", "RADIO_GAIN", "666.0", rez, 255, ini_name)){
        float tmpfloatgain;
        sscanf(rez, "%f", &tmpfloatgain);
        if (tmpfloatgain < 665.0F) RADIO_GAIN = lrintf(tmpfloatgain*256.F);
    }

    ///// CUSTOM FONTS /////
    if(GetPrivateProfileString("IN_OPUS", "TAGS_FONT", "", rez, 255, ini_name)){
        sscanf(rez, "%d %[^\n]s", font_height, font_str);
    }
}
/////////////////////////////////////////////////////////////////////////////
//
static HFONT applyglobalfont(char *font_str, int font_height)
{
    HDC hdc;
    long lfHeight;
    HFONT tags_font=NULL; // To set the font

    if(font_str == NULL || font_height == 0 || *font_str == '\0'){
        if(VERBOSE)MessageBox(NULL,"No font substitution","in_opus",MB_OK);
        tags_font = NULL;
    } else {
        hdc = GetDC(NULL);
        lfHeight = -MulDiv(font_height, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(NULL, hdc);
        tags_font = CreateFont(lfHeight,0,0,0,0,FALSE,0,0,0,0,0,0,0,font_str);
        if(VERBOSE)MessageBox(NULL,font_str,"in_opus: using font",MB_OK);
    }

    return tags_font;
}
/////////////////////////////////////////////////////////////////////////////
// We need config to be read already.
void applyglobal_unicode_fn(char *ini_name, int ininamelength, char *rez)
{
    if(UNICODE_FILE == 2 && isNT){ // Auto mode and WinNT
        FILE *Whatsnew=NULL;
        float version=0;
        char *p;

        p = ini_name + ininamelength;
        while (p >= ini_name && *p != '\\' && *p != '/') p--;
        ++p;
        size_t remaining = ini_name + ininamelength - p;
        lstrcpy_sA(p, remaining, "wacup.exe");
        if(INVALID_FILE_ATTRIBUTES != GetFileAttributes(ini_name)){
            if(VERBOSE)MessageBox(mod.hMainWindow,"You are using WACUP dude!","in_opus",MB_OK);
            return;
        }
        lstrcpy_sA(p, remaining, "MediaMonkey.exe");
        if(INVALID_FILE_ATTRIBUTES != GetFileAttributes(ini_name)){
            if(VERBOSE)MessageBox(mod.hMainWindow,"You are using WACUP dude!","in_opus",MB_OK);
            UNICODE_FILE=3; // WE ARE USING MEDIA MONKEY
            return;
        }
        lstrcpy_sA(p, remaining, "whatsnew.txt");
        // NOW ini_name contains the path towards "whatsnew.txt"
        if(INVALID_FILE_ATTRIBUTES != GetFileAttributes(ini_name))
            Whatsnew=fopen(ini_name, "r");
        if(Whatsnew){
            if(fgets(rez, 255, Whatsnew)){
                sscanf(rez, "Winamp %f", &version);
                if(VERBOSE){
                    sprintf(rez, "You are using winamp version %f", version);
                    MessageBox(mod.hMainWindow, rez, "in_opus", MB_OK);
                }
            }
            if(version > 5.295) UNICODE_FILE=2;      // if Auto and WinNT and Winamp 5.3+
            else UNICODE_FILE=0;                     // if Winamp 5.29-
        }else{
            if(VERBOSE) MessageBox(mod.hMainWindow,"We could not determine WINAMP's version!","in_opus",MB_OK);
            UNICODE_FILE=0; // if we could not find the "whatsnew.txt"
        }
        if(Whatsnew) fclose(Whatsnew);
    } else if(UNICODE_FILE == 2) { // In AUTO mode and not NT.
        if(VERBOSE) MessageBox(mod.hMainWindow,"You are using Windows 9x\n"
                              "No UNICODE support dude!","in_opus",MB_OK);
        UNICODE_FILE=0; // if we are not under NT based OS
    }
    // Here if UNICODE_FILE=1 it means it was before.
    // if UNICODE_FILE=0 it means that it was set to 0 befort or that the system does
    // not meet the requirements.
    // Finally if UNICODE_FILE=2 it means that Auto mode was enabled requirements are met.
}
/////////////////////////////////////////////////////////////////////////////
// Initialisation called at startup...
void init()
{
    HOURS_MODE_ON=0;
    char ini_name[MAX_PATH], rez[256], font_str[LF_FACESIZE];
    int font_height=0;
    int winamp_version;
    ini_name[0] = rez[0] = font_str[0] = '\0';

    VIS_BROKEN=1;
    winamp_version = SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GETVERSION);

    if(winamp_version >= 0x5011){
        if(winamp_version >= 0x5012) VIS_BROKEN=0; // Winamp 5.12 allows 24/32b visualisation.
        // this gets the string of the full ini file path
        char *newini = (char *)SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GETINIFILE);
        if(!newini) return;
        lstrcpy_sA(ini_name, countof(ini_name), newini);

        // Lastfn here contains the winamp.ini path, we read the file only if
        // we have not read it already.
        if(_stricmp(lastfn, ini_name)){
            if(VERBOSE)MessageBox(NULL, ini_name,"in_opus: Winamp ini_name bis",MB_OK);
            readconfig(ini_name, rez, font_str, &font_height); //winamp.ini
            applyglobalfont(font_str, font_height);
        }
    }
}

char *ffilestart(const char *str)
{
    const char *p = strrchr(str, '\\');
    if (!p) p = strrchr(str, '/');
    return (char *)p;
}

/////////////////////////////////////////////////////////////////////////////
// Initialisation called before winamp loads the module.
static void first_init(char *Fstr, int *Fnth)
{
    if(VERBOSE)MessageBox(NULL,"first_init start","in_opus",MB_OK);

    int font_height=0;

    char ini_name[MAX_PATH], rez[MAX_PATH], font_str[MAX_PATH];
    int ininamelength;
    char *p;
    ini_name[0] = rez[0] = font_str[0] = '\0';

    isNT = !(GetVersion() & 0x80000000); /* To Use Unicode stuff on NT */

    readinternalconfig(font_str, &font_height);

    //// Find in_opus.ini in plugin folder ////
    GetModuleFileName(NULL, ini_name, countof(ini_name));
    ininamelength = strlen(ini_name);
    p = ffilestart(ini_name);
    if (!p) p = ini_name;
    *p = '\0';
    lstrcat_sA(p, countof(ini_name), "\\Plugins\\in_opus.ini");
    if(VERBOSE)MessageBox(NULL, ini_name,"in_opus: in_opus.ini path",MB_OK);

    readconfig(ini_name, rez, font_str, &font_height); //in_opus.ini

    //// Find winamp.ini IN Winamp's folder ////
    GetModuleFileName(NULL, ini_name, sizeof(ini_name));
    ininamelength = strlen(ini_name);
    p = strrchr(ini_name, '.');
    if (!p) p = ini_name + ininamelength;
    strcpy(p, ".ini");
    if(VERBOSE)MessageBox(NULL, ini_name,"in_opus: Winamp.ini path",MB_OK);

    readconfig(ini_name, rez, font_str, &font_height); //winamp.ini

    TAGS_FONT = applyglobalfont(font_str, font_height);

    applyglobal_unicode_fn(ini_name, ininamelength, rez);
    if (Fstr && Fnth) {
        if(font_height!=0) lstrcpy_sA(Fstr, LF_FACESIZE,  font_str);
        else lstrcpy_sA(Fstr, LF_FACESIZE, "(Not user set)");
        *Fnth=font_height;
    } else {
        lstrcpy_sA(lastfn, countof(lastfn), ini_name);
    }
} // END OF INIT
/////////////////////////////////////////////////////////////////////////////
// Called when winanmp quits...
void quit()
{
    op_free(_of); // Just in case
}

/////////////////////////////////////////////////////////////////////////////
// Used for detecting URL streams...
int isourfile(const char *const fn)
{
    int err = -132;
    int ret=0;

    const char *ffn = fn;
    if (UNICODE_FILE) {
        char *fnUTF8 = utf16_to_utf8((const wchar_t *)fn);
        if (fnUTF8)
            ffn = fnUTF8;
    }

    const char *p=ffn;
    p+= strlen(p) - 4 ;
    if (isURL(ffn)) {

        if (!_strnicmp(p,".opu", 4)
        ||  !_strnicmp(--p,".opus", 5) ){
           ret=1;
        }
    } else if (OGGEXT_HACK && !_strnicmp(p, ".ogg", 4)){
        // Maybe an opus file with .ogg ext
        OggOpusFile *_tmp = op_test_file(ffn, &err);

        if (err == 0 && _tmp) {
            op_free(_tmp);
            //MessageBox(NULL, ".OGG OPUS file", "isourfile", MB_OK);
            ret = 1; // This is actually an opus file.
        } else {
            //MessageBox(NULL, ".OGG NOT OPUS file", "isourfile", MB_OK);
            ret = 0;
        }
    }

    if (ffn!=fn) free((char *)ffn);

    return ret;
}
/////////////////////////////////////////////////////////////////////////////
// This is for the current file only. We could add more features like
// handelig of peak tag info,
static void apply_replaygain(void)
{
    int lufs_offset;

    // We apply the gain to reach desired LUFs only if RG is enabled
    // AND RG info is availabes in file
    if(USE_REPLAY_GAIN) lufs_offset = (isRGavailable(_of))? (TARGET_LUFS+23)*256 : 0;
    else lufs_offset = 0;

    switch (USE_REPLAY_GAIN) {
    case 1: OP_CURRENT_GAIN = OP_ALBUM_GAIN; break;
    case 2: OP_CURRENT_GAIN = OP_TRACK_GAIN; break;
    case 3: OP_CURRENT_GAIN = SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GET_SHUFFLE)
                              ? OP_TRACK_GAIN
                              : OP_ALBUM_GAIN;
                              break;
     // NO LUFS in case of absolute gain
    case 4: OP_CURRENT_GAIN = OP_ABSOLUTE_GAIN; lufs_offset = 0; break;

    // By default we use only header gain
    default: OP_CURRENT_GAIN = OP_HEADER_GAIN;
    }
    op_set_gain_offset(_of, OP_CURRENT_GAIN, lufs_offset+(RADIO? RADIO_GAIN: PRE_GAIN));
}
void TryResolvePath(char *fn)
{
    // Nothing to do on Windows 9x
    // Or in unicode mode
    if (!isNT || UNICODE_FILE || isURL(fn)) return;
    if (strchr(fn, '?')) {
        // We got an invalid full path name
        // Try to resolve the short path name
        WIN32_FIND_DATA finddat, finddat2;
        HANDLE f1 = FindFirstFile(fn, &finddat);
        if (f1 != INVALID_HANDLE_VALUE) {
            // We found only one file that can match the pattern
            if (!FindNextFile(f1, &finddat2)) {
                char *p = ffilestart(fn);
                if (p) {
                    strcpy(p+1, finddat.cAlternateFileName);
                    return; // Sucess!
                }
            }
            FindClose(f1);
        }
    }
    if (INVALID_FILE_ATTRIBUTES != GetFileAttributes(fn))
        return; // the file name is valid, nothing to do.
    // We failed it mean that we can try to list all files in the
    // file's directorry and match them
    char *lastbks = ffilestart(fn);
    if (!lastbks) return;

    // Get ANSI file name only.
    char * const filenameA = lastbks+1;
    // get file extension
    const char * const extA = strrchr(filenameA, '.');
    if (!extA) return; // no extension, we got a problem!

    { // Check if path exist
    char oldlastbs = *lastbks;
    *lastbks = '\0'; // get fn = path only.
    DWORD attrib = GetFileAttributes(fn);
    *lastbks = oldlastbs; // restore fn
    // Path does not exist, we stop here.
    if (attrib == 0xFFFFFFFF || !(attrib&FILE_ATTRIBUTE_DIRECTORY)) return;
    }

    // Generate unicode path name (path only).
    wchar_t wfnsearch[MAX_PATH];
    wchar_t extW[8];
    int ret = MultiByteToWideChar(CP_ACP, 0, extA, -1, extW, 8);
    if (!ret) return;
    ret = MultiByteToWideChar(CP_ACP, 0, fn, -1, wfnsearch, MAX_PATH);
    if (!ret) return;

    // Find last backslash
    wchar_t *lastbsW = wcsrchr(wfnsearch, L'\\');
    if (!lastbsW) lastbsW = wcsrchr(wfnsearch, L'/');
    if (!lastbsW) return;

    // We must look for files in the form "x:\path\to\file\*.ext"
    lastbsW[1] = '\0'; // Null terminate the path after the backslash
    lstrcat_sW(wfnsearch, countof(wfnsearch), L"*");
    lstrcat_sW(wfnsearch, countof(wfnsearch), extW);
    // here wfnsearch is in the x:\path\to\file\*.ext form.

    unsigned matches=0;
    char foundfnShortA[16]=""; // short file name...
    WIN32_FIND_DATAW finddat;
    HANDLE f1 = FindFirstFileW(wfnsearch, &finddat);
    if (f1) {
        do {
            //finddat.cAlternateFileName
            //MessageBoxW(NULL, finddat.cFileName, finddat.cAlternateFileName, 0);

            // filenameA: input ansi file name
            char foundfilenameA[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, finddat.cFileName, -1, foundfilenameA, MAX_PATH, NULL, NULL);
            if (!strcmp(filenameA, foundfilenameA)
            && finddat.cAlternateFileName[0]) {
                // Copy short name in foundfnShortA.
                WideCharToMultiByte(CP_ACP, 0, finddat.cAlternateFileName, -1, foundfnShortA, 16, NULL, NULL);
                matches++;
            }

        } while (FindNextFileW(f1, &finddat));
        FindClose(f1);

        if (matches == 1) {
            // If we only found a single match!
            filenameA[0] = '\0';
            strcpy(filenameA, foundfnShortA);
        }
    }
}
static OggOpusFile *op_open_file_TR(char *path, int *err)
{
    TryResolvePath(path);
    return op_open_file(path, err);
}
/////////////////////////////////////////////////////////////////////////////
// Called when winamp wants to play a file
int play(char *fn)
{
    //MessageBox(NULL, "Play", NULL, 0);
    if (_of) MessageBox(NULL, "File already opened!", "in_opus error", 0);
    int maxlatency, err;
    DWORD thread_id;
    char *p;
    paused=0;
    decode_pos_ms=0;
    seek_needed=-1;

    // Convert file name in UTF8 if needed.
    char *ffn=fn;
    if (UNICODE_FILE) {
        char *fnUTF8 = utf16_to_utf8((wchar_t *)fn);
        if(!fnUTF8)
            return 1;
        ffn = fnUTF8;
    }
    if(VERBOSE) MessageBox(mod.hMainWindow, ffn
                    , "in_opus: play function, going to OPEN FILE",MB_OK);
    if (isURL(ffn)) {
        RADIO = 1;
        mod.is_seekable = 0;

        if(VERBOSE) MessageBox(mod.hMainWindow, fn , "in_opus: This is an URL",MB_OK);
        p = ffn;
        p += strlen(p) - 6;
        if(!_strnicmp(p,">.opus", 6)) *p ='\0';
        _of = op_open_url(ffn, &err, NULL);

    } else {
        _of = op_open_file_TR(ffn, &err);
        RADIO = 0;
        mod.is_seekable = 1;
    }
    if (_of == NULL || err) { // error opening file
        if (VERBOSE) MessageBox(mod.hMainWindow, ffn, "in_opus: unable to open file",MB_OK);
        // we return error. 1 means to keep going in the playlist, -1
        // means to stop the playlist.
        if (RADIO) lstrcpy_sA(lastfn, countof(lastfn), ffn);

        if (ffn!=fn) free(ffn);
        return RADIO? 0 : 1;
    }

    lstrcpy_sA(lastfn, countof(lastfn), ffn);
    if (ffn!=fn) free(ffn);

    // -1 and -1 are to specify buffer and prebuffer lengths. -1 means
    // to use the default, which all input plug-ins should really do.
    maxlatency = mod.outMod->Open(SAMPLERATE,NCH,BPS, -1,-1);

    // maxlatency is the maxium latency between a outMod->Write() call and
    // when you hear those samples. In ms. Used primarily by the visualization
    // system if < 0 means error opening device.
    if (maxlatency < 0) {
        if(VERBOSE) MessageBox(mod.hMainWindow,"Unable to open devide (maxlatency < 0)"
                              , "in_opus error:",MB_OK);
        op_free(_of); _of=NULL;
        return 1;
    }
    // initialize visualization stuff
    mod.SAVSAInit(maxlatency,SAMPLERATE);
    mod.VSASetInfo(SAMPLERATE,NCH);

    // Set Dithering and RG
    op_set_dither_enabled(_of, USE_DITHERING);
    apply_replaygain();

    // set the output plug-ins default volume.
    // volume is 0-255, -666 is a token for current volume.
    setvolume(-666); // mod.outMod->SetVolume(-666);

    // LAUNCH DECODE THREAD
    if(VERBOSE >= 2) MessageBox(NULL,"Going to Lunch DecodeThread", "in_opus", MB_OK);
    killDecodeThread=0;
    thread_handle = (HANDLE) CreateThread(NULL, 0, DecodeThread, NULL, 0, &thread_id);
    SetThreadPriority (thread_handle, THREAD_PRIORITY);

    return 0;
} // END OF play()

/////////////////////////////////////////////////////////////////////////////
// Standard pause implementation
void pause()   { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }

/////////////////////////////////////////////////////////////////////////////
// Stop playing.
void stop()
{
    //MessageBox(NULL, "Stop", NULL, 0);
    if (thread_handle != INVALID_HANDLE_VALUE) {
        killDecodeThread=1;
        if (WaitForSingleObject(thread_handle, 5000) == WAIT_TIMEOUT){ // 5 sec
            MessageBox(mod.hMainWindow
                ,"Error, DecodeThread not responding...\nKilling decode thread!"
                ,"in_opus: error", 0);
            TerminateThread(thread_handle, 0);
        }
        CloseHandle(thread_handle);
        thread_handle = INVALID_HANDLE_VALUE;
    }

    // close output system
    mod.outMod->Close();

    // deinitialize visualization
    mod.SAVSADeInit();

    // Write your own file closing code here
    HOURS_MODE_ON=0;
    op_free(_of);
    _of=NULL;
} // END OF stop()

/////////////////////////////////////////////////////////////////////////////
// Returns length of playing track in ms
int getlength()
{
    opus_int64 length_in_ms;   // -1000 means unknown

    length_in_ms = -1000;
    if(HOURS_MODE_ON!=2) HOURS_MODE_ON=0;

    if(!RADIO && _of) length_in_ms = op_pcm_total(_of, -1)/(opus_int64)48;

    if(length_in_ms > BIG_LENGTH){ // Could be INT_MAX here or 1 000 min
        length_in_ms = length_in_ms/BIG_LENGTH_MULT;
        HOURS_MODE_ON=1;
    }

    return length_in_ms; // Will be 60x smaller in hour mode
}

/////////////////////////////////////////////////////////////////////////////
// Returns current output position, in ms. you could just use
// return mod.outMod->GetOutputTime(), but the dsp plug-ins that do tempo
// changing tend to make that wrong.
int getoutputtime()
{
      if(!HOURS_MODE_ON){
          return decode_pos_ms + ( mod.outMod->GetOutputTime()-mod.outMod->GetWrittenTime() );
      } else {
          return decode_pos_ms/BIG_LENGTH_MULT;
      }
}

/////////////////////////////////////////////////////////////////////////////
// Called when the user releases the seek scroll bar.
// Usually we use it to set seek_needed to the seek
// point (seek_needed is -1 when no seek is needed)
// and the decode thread checks seek_needed.
void setoutputtime(int time_in_ms) // Or mili-minutes when in HOURS_MODE
{
    seek_needed = HOURS_MODE_ON? ((opus_int64)time_in_ms*(opus_int64)BIG_LENGTH_MULT): time_in_ms;
}

/////////////////////////////////////////////////////////////////////////////
// Standard volume/pan functions
void setvolume(int volume)
{
    mod.outMod->SetVolume(volume);
    static int cvol;
    int current_pregain =RADIO? RADIO_GAIN: PRE_GAIN;
    if(volume != -666)cvol=volume;
    if(INTERNAL_VOLUME==1 || (INTERNAL_VOLUME==2 && RADIO)){
        if(_of)op_set_gain_offset(_of, OP_CURRENT_GAIN, (cvol>0)? current_pregain - 20*(255-cvol): -32768);
    } else {
        mod.outMod->SetVolume(volume);
    }
}
void setpan(int pan)
{
    mod.outMod->SetPan(pan);
}

/////////////////////////////////////////////////////////////////////////////
// This gets called when the use hits Alt+3 to get the file info.
int infoDlg(char *fn, HWND hwnd)
{
    TryResolvePath(fn);
    DoInfoBox(mod.hDllInstance, hwnd, fn);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Returns length of opus file track in ms or mili-minutes
static int get_opus_length_auto(OggOpusFile *_tmp, char *hours_mode_on)
{
    opus_int64 length_in_ms;   // -1000 means unknown

    if(hours_mode_on) *hours_mode_on = 0;

    length_in_ms = _tmp ? op_pcm_total(_tmp, -1)/48 : -1000;

    if(length_in_ms > BIG_LENGTH) { // Could be INT_MAX here it is 1 000 min
        length_in_ms = length_in_ms/BIG_LENGTH_MULT;
        if(hours_mode_on) *hours_mode_on = 1;
    }

    return (int)length_in_ms; // Will be 60x smaller in hour mode
}

static const char *GetTTitleA(const char *fn, char *buf)
{
    // get non-path portion of lastfn
    const char *p=fn+strlen(fn);
    while (p >= fn && *p != '\\') p--;
    p++;

    const char *ttl=p;
    if (buf && FORMAT_TITLE) {
        if( winampGetExtendedFileInfo(fn, "Artist", buf, MAX_PATH/3)
        &&  winampGetExtendedFileInfo(fn, "Title", lstrcat_sA(buf, MAX_PATH, " - "), MAX_PATH/3) )
            ttl = buf;
    }
    if (!strcmp(ttl, " - ")) ttl = p;

    return ttl;
}

static void internal_getfileinfoA(char *fn, char *title, int *length_in_ms)
{
    int err = 0;
    char hours_mode_on = 0;
    OggOpusFile *_tmp = NULL;

    char is_fn_url = isURL(fn);
    if(!is_fn_url) {
        _tmp = op_open_file_TR(fn, &err);
    }
    if (length_in_ms) {
        *length_in_ms = get_opus_length_auto(_tmp, &hours_mode_on);
    }
    if (title) { // Set the track title in *title
        char buf[MAX_PATH*2];
        const char *ttl = GetTTitleA(fn, buf);

        if(err && !is_fn_url && !UNICODE_FILE)
            wsprintfA(title,"[in_opus err %d %.220s] %.220s",err,TranslateOpusErr(err), ttl);
        else if(hours_mode_on)
            wsprintfA(title,"%.500 [h:min]", ttl);
        else
            lstrcpy_sA(title, 512, ttl);
    }
    op_free(_tmp);
}
/////////////////////////////////////////////////////////////////////////////
// This is an odd function. it is used to get the title and/or
// length of a track.
// If filename is either NULL or of length 0, it means you should
// return the info of lastfn. Otherwise, return the information
// for the file in filename.
// If title is NULL, no title is copied into it.
// If length_in_ms is NULL, no length is copied into it.
void getfileinfo(char *filename, char *title, int *length_in_ms)
{

    if (!filename || !*filename) {  // currently playing file => lastfn
        internal_getfileinfoA(lastfn, title, length_in_ms);
    } else { // some other file => filename
        internal_getfileinfoA(filename, title, length_in_ms);
    }
}

// fn must be UTF8!
static void internal_getfileinfoW(const char* fn, wchar_t *title, int *length_in_ms)
{
    int err=0;
    OggOpusFile *_tmp=NULL;
    char hours_mode_on=0;

    char is_fn_url = isURL(fn);
    if (!is_fn_url)_tmp = op_open_file(fn, &err);
    if (length_in_ms) {
        *length_in_ms = get_opus_length_auto(_tmp, &hours_mode_on);
    }
    if (title){ // get non-path portion of fn which is UTF8 already
        char *p=lastfn+strlen(lastfn);
        while (p >= lastfn && *p != '\\') p--;
        p++;
        wchar_t *tmpW = utf8_to_utf16(p);

        if(err && !is_fn_url)  wsprintfW(title, L"[in_opus err %d %.220hs] %.220s", err, TranslateOpusErr(err), tmpW);
        else if (hours_mode_on) wsprintfW(title, L"%.500s [h:min]", tmpW);
        else lstrcpy_sW(title, 512, tmpW);

        free(tmpW);
    }
    op_free(_tmp);
}

/////////////////////////////////////////////////////////////////////////////
void getfileinfoW(char *filenamew, char *titlew, int *length_in_ms)
{
    wchar_t *filename = (wchar_t *)filenamew;
    if (!filename || !*filename) {  // currently playing file
        internal_getfileinfoW(lastfn, (wchar_t *)titlew, length_in_ms);
    } else {// some other file => filename
        char *fnUTF8  = utf16_to_utf8(filename);
        if(fnUTF8) {
            internal_getfileinfoW(fnUTF8, (wchar_t *)titlew, length_in_ms);
            free(fnUTF8);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// Winamp2 visuals have problems accepting sample sizes larger than
// 16 bits, so we reduce it to 8 bits if in 24 or 32 bit mode.
// Function cp/pasted and modified from in_flac.c (libflac-1.2.1)
static inline void do_vis(char *__restrict data, char *__restrict vis_buffer
                        , long long pos, unsigned samples, char resolution)
{
    char *ptr;
    unsigned size, count;
    int position;

    position =  HOURS_MODE_ON? pos/BIG_LENGTH_MULT: pos;

    if(!vis_buffer) goto DEFAULT; // If we did not allocate buffer it means
                                 // that ther is no deed to reduce BPS.
    switch(resolution) {
        case 32:
        case 24:
            size  = resolution / 8;
            count = samples;
            data += size - 1;
            ptr = vis_buffer;

            while(count--) {
                *ptr++ = data[0] ^ 0x80;
                data += size;
            }
            data = vis_buffer;
            resolution = 8;
            /* fall through */
        case 16:
        case 8:
        default:
        DEFAULT:
            mod.SAAddPCMData (data, NCH, resolution, position);
            mod.VSAAddPCMData(data, NCH, resolution, position);
    }
}

/////////////////////////////////////////////////////////////////////////////
// if you CAN do EQ with your format, each data byte is 0-63 (+20db to -20db)
// and preamp is the same.
void eq_set(int on, char data[10], int preamp)
{

}

/////////////////////////////////////////////////////////////////////////////
// Render 576 samples into buf. This function is only used by DecodeThread.
// Note that if you adjust the size of sample_buffer, for say, 1024
// sample blocks, it will still work, but some of the visualization
// might not look as good as it could. Stick with 576 sample blocks
// if you can, and have an additional auxiliary (overflow) buffer if
// necessary...  Buff size should be 576*NCH*(BPS/8) in bytes
static inline int get_576_samples(char *__restrict buf, float *__restrict Ibuff)
{
    int l;

    // op_read_stereo always output 16b 48kHz stereo output, the return
    // value is the number of samples per channel.
    if(BPS!=16){
        l = op_read_float_stereo(_of, Ibuff, DECODE_BUFF_SIZE);
        float2int_dither(buf, Ibuff, l*NCH, BPS, USE_DITHERING);
    }else{
        l = op_read_stereo(_of, (opus_int16*) buf, DECODE_BUFF_SIZE); // (2ch * 60ms at 48KHz = 11520)
    }

    return NCH*(BPS/8)*l; // = Number of bytes
}
/////////////////////////////////////////////////////////////////////////////
// Alternate version that down sample to SAMPLERATE.
static inline int get_samples44(
    char *__restrict buf,
    float *__restrict Ibuff, float *__restrict Obuff,
    SpeexResamplerState *StResampler,
    spx_uint32_t max_out_samples)
{
    spx_uint32_t l, lout;
    lout = max_out_samples;

    l = op_read_float_stereo(_of, Ibuff, DECODE_BUFF_SIZE); // (2ch * 60ms at 48KHz = 11520)

    int lbck=l;
    int err = speex_resampler_process_interleaved_float(StResampler,
                                                  Ibuff, &l,
                                                  Obuff, &lout);
    float2int_dither(buf, Obuff, lout*NCH, BPS, USE_DITHERING);

    if (VERBOSE >= 3){
        char tmp[128];
        sprintf(tmp, "We are after resampling, in=%d/%d, out=%d/%d\nERROR: %d"
              , l, lbck, lout, max_out_samples, err);
        MessageBoxA(NULL,tmp, "in_opus", MB_OK);
    }

    return NCH*(BPS/8)*lout; // number of bytes
}

/////////////////////////////////////////////////////////////////////////////
DWORD WINAPI DecodeThread(LPVOID b)
{
    float *Ibuff=NULL, *Obuff=NULL;
    char *sample_buffer=NULL;
    char *vis_buffer=NULL;
    SpeexResamplerState *StResampler=NULL;
    int err, l, max_out_length;
    int done=0, avbitrate=0; // set to TRUE if decoding has finished

    if(VERBOSE >= 3) MessageBox(NULL,"Decode Thread Start", "in_opus", MB_OK);

    max_out_length = (DECODE_BUFF_SIZE*(BPS/8)*SAMPLERATE)/SR; // in byte! not samples
    sample_buffer = malloc(max_out_length);
    if(!sample_buffer) goto QUIT;

    if(SAMPLERATE != SR){
        StResampler = speex_resampler_init(NCH, SR, SAMPLERATE, RESAMPLE_Q, &err);
        if(VERBOSE >= 3) MessageBox(NULL,"Speex Resampler started", "in_opus", MB_OK);
        Ibuff= malloc(   (DECODE_BUFF_SIZE)*sizeof(float));
        Obuff= malloc( ( (DECODE_BUFF_SIZE)*sizeof(float)*SAMPLERATE )/SR);
        if(!StResampler || !Ibuff || !Obuff) goto QUIT;
    } else if(BPS!=16) {
        Ibuff= malloc(   (DECODE_BUFF_SIZE)*sizeof(float));
    }
    if(VIS_BROKEN && (BPS==24 || BPS==32)) vis_buffer=malloc( (DECODE_BUFF_SIZE*SAMPLERATE)/SR ); //always 8b

    if(!INSTANT_BR) avbitrate = op_bitrate(_of, -1)/1000;

    mod.SetInfo(avbitrate, SAMPLERATE/1000, NCH, 1);

    while (!killDecodeThread) {
        if(VERBOSE >= 3) MessageBox(NULL,"Decode Thread not dead", "in_opus", MB_OK);

        if (seek_needed != -1){ // seek is needed.
            if(VERBOSE >= 2) MessageBox(NULL,"Going to seek in file", "in_opus", MB_OK);
            decode_pos_ms = seek_needed;
            seek_needed=-1;
            done=0;
            mod.outMod->Flush(HOURS_MODE_ON?decode_pos_ms/BIG_LENGTH_MULT:decode_pos_ms);
            // flush output device and set
            // output position to the seek position
            op_pcm_seek (_of, decode_pos_ms*48); //sample to seek to.
        }

        if (done){ // done was set to TRUE during decoding, signaling eof
            mod.outMod->CanWrite();     // some output drivers need CanWrite
                                        // to be called on a regular basis.
            if (!mod.outMod->IsPlaying()) {
                // we're done playing, so tell Winamp and quit the thread.
                PostMessage(mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
                goto QUIT;    // quit thread
            }
            Sleep(10);        // give a little CPU time back to the system.

        } else if (mod.outMod->CanWrite() >= ((max_out_length)*(mod.dsp_isactive()?2:1)))
            // CanWrite() returns the number of bytes you can write, so we check that
            // to the block size. the reason we multiply the block size by two if
            // mod.dsp_isactive() is that DSP plug-ins can change it by up to a
            // factor of two (for tempo adjustment).
        {
            if(VERBOSE >= 3) MessageBox(NULL,"Going to get samples", "in_opus", MB_OK);
            if (StResampler) {
                l = get_samples44(sample_buffer, Ibuff, Obuff, StResampler, max_out_length/(NCH*(BPS/8)));
            } else {
                l = get_576_samples(sample_buffer, Ibuff);
            }

            if (!l) { // no samples means we're at eof
                if (VERBOSE >= 2) MessageBox(NULL,"No samples found, we are at OEF", "in_opus", MB_OK);
                done = 1;
            } else {    // we got samples!
                if (VERBOSE >= 3) MessageBox(NULL,"We got samples", "in_opus", MB_OK);

                do_vis(sample_buffer, vis_buffer, decode_pos_ms, max_out_length/(BPS/8), BPS);

                // adjust decode position variable
                decode_pos_ms+=(l*500)/(SAMPLERATE*(BPS/8));

                // if we have a DSP plug-in, then call it on our samples
                if (mod.dsp_isactive()) {

                    l=mod.dsp_dosamples(  (short *)sample_buffer     // dsp_dosamples
                                        , l/(NCH*(BPS/8)), BPS, NCH
                                        , SAMPLERATE) *(NCH*(BPS/8));
                }
                if (VERBOSE >= 3) MessageBox(NULL,"going to write pcm data to the output system"
                                           , "in_opus", MB_OK);

                // write the pcm data to the output system
                mod.outMod->Write(sample_buffer, l);

                // Write informations in the winamp windows
                // dividing by 1000 for the first parameter of setinfo makes it
                // display 'H'... for hundred.. i.e. 14H Kbps.
                if(INSTANT_BR) {
                    int br=op_bitrate_instant(_of)/1000;
                    mod.SetInfo((br> 0)? br:-1,-1,-1,-1);
                }
            }
        } else {
            // if we can't write data, wait a little bit. Otherwise, continue
            // through the loop writing more data (without sleeping)
            Sleep(20);
            if(VERBOSE >= 3) MessageBox(NULL,"Unable to write samples,\n Sleeping 20 ms"
                                       , "in_opus", MB_OK);
        }
    if(decode_pos_ms > BIG_LENGTH) HOURS_MODE_ON=2;
    } // END of While !Kill decode thread

    QUIT:
    if (StResampler) { speex_resampler_destroy(StResampler); StResampler=NULL; }
    free(Obuff);
    free(Ibuff);
    free(sample_buffer);
    free(vis_buffer);

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// module definition.
static In_Module mod =
{
    IN_VER, // defined in IN2.H
    "OPUS Player v0.912 by Raymond",
    0,      // hMainWindow (filled in by winamp)
    0,      // hDllInstance (filled in by winamp)
    "OPUS\0OPUS Audio File (*.OPUS)\0OPU\0Opus Audio File (*.OPU)\0",
    // this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
    1,      // is_seekable
    1,      // uses output plug-in system
    config,
    about,
    init,
    quit,
    getfileinfo,
    infoDlg,
    isourfile,
    play,
    pause,
    unpause,
    ispaused,
    stop,

    getlength,
    getoutputtime,
    setoutputtime,

    setvolume,
    setpan,

    0,0,0,0,0,0,0,0,0, // visualization calls filled in by winamp

    0,0, // dsp calls filled in by winamp

    eq_set, //eq_set

    NULL,        // setinfo call filled in by winamp

    0 // out_mod filled in by winamp
};
/////////////////////////////////////////////////////////////////////////////
// If we want to support unicode filenmes we need to modify these functions:
// void GetFileInfo(char *filename, char *title, int *length_in_ms)* DONE
// int infoDlg(char *fn, HWND hwnd) * DONE
// IsOurFile()* DONE
// Play() * DONE
/////////////////////////////////////////////////////////////////////////////
// exported symbol. Returns a pointer to the output module.
__declspec( dllexport ) In_Module * winampGetInModule2()
{
    first_init(NULL, NULL);

    if(isNT && UNICODE_FILE){
        if(VERBOSE) MessageBox(NULL ,"Unicode filename support enabled" ,"in_opus",MB_OK);
        mod.version=(IN_VER | IN_UNICODE);
        if(UNICODE_FILE != 3) mod.GetFileInfo=getfileinfoW; // Except For MediaMonkey
    }
    return &mod;
}

#ifdef LOCAL_LRINTF
long __cdecl lrintf(float x)
{
    long ll = 0;
    asm volatile("fistpl %0"  : "=m" (ll) : "t" (x) : "st");
    return ll;
}
#endif

//void *__cdecl malloc(size_t sz) { return HeapAlloc(GetProcessHeap(), 0, sz); }
//void *__cdecl calloc(size_t n, size_t sz) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n*sz); }
//void __cdecl free(void *x) { HeapFree(GetProcessHeap(), 0, x); }
//void * __cdecl realloc(void *x, size_t sz)
//{
//    if(!sz) { if(x)HeapFree(GetProcessHeap(), 0, x); return NULL; }
//    if(!x) return HeapAlloc(GetProcessHeap(), 0, sz);
//    return HeapReAlloc(GetProcessHeap(), 0, x, sz);
//}
