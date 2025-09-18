/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * in_opus info box by Gillibert Raymond,                                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <opus/opusfile.h>
#include "resource.h"
#include "infobox.h"

extern HFONT TAGS_FONT; // To set the font
extern char USE_REPLAY_GAIN; // 0 : no, 1: Album, 2: Track.
extern char TAGS_DISPLAY_MODE, isNT;
extern char UNICODE_FILE;
extern char TARGET_LUFS;
extern char UNIFIED_DIALOG;

#include "utf_ansi.c"

/***************************************
 *  SHARED HELPERS                     *
 ***************************************/
const char *lstrcpy_sA(char *__restrict__ dest, size_t N, const char *src)
{
    char *orig=dest;
    char *dmax=dest+N-1; /* keep space for a terminating NULL */
    for (; dest<dmax && (*dest=*src); ++src,++dest);  /* then append from src */
    *dest='\0'; /* ensure result is NULL terminated */
    return orig;
}
__cdecl char *strncpy(char *dest, const char *src, size_t N)
{
    char *orig=dest;
    char *dmax=dest+N-1; /* keep space for a terminating NULL */
    for (; dest<dmax && (*dest=*src); ++src,++dest);  /* then append from src */
    *dest='\0'; /* ensure result is NULL terminated */
    return orig;
}
static const wchar_t *lstrcpy_sW(wchar_t *__restrict__ dest, size_t N, const wchar_t *src)
{
    wchar_t *orig=dest;
    wchar_t *dmax=dest+N-1; /* keep space for a terminating NULL */
    for (; dest<dmax && (*dest=*src); ++src,++dest);  /* then append from src */
    *dest='\0'; /* ensure result is NULL terminated */
    return orig;
}

static int has_opus_ext(const char *p)
{
    p+=strlen(p); p-=4;
    if(!_strnicmp(p, ".opu", 4) || !_strnicmp(--p, ".opus", 5)) {
        return 1;
    } else {
        return 0;
    }
}
static int has_opus_extW(const wchar_t *p)
{
    p+=wcslen(p); p = &p[-4];
    if(!_wcsnicmp(p, L".opu", 4) || !_wcsnicmp(--p, L".opus", 5)) {
        return 1;
    } else {
        return 0;
    }
}

char isURL(const char *const fn)
{
    return fn && fn[0] == 'h' && fn[1] == 't' && fn[2] == 't'
          && fn[3] == 'p' && fn[4] == ':' && fn[5] == '/' && fn[6] == '/';
}

char isRGavailable(const OggOpusFile *_of)
{
    if(_of == NULL) return 0;
    if(op_get_header_gain(_of)) return 1; // Il y as un gain...

    const OpusTags *_tags=NULL;
    _tags = op_tags(_of, -1);
    if(_tags){
        int gain;
        if(opus_tags_get_track_gain(_tags, &gain) == 0) { return 1;}
        if(opus_tags_get_album_gain(_tags, &gain) == 0) { return 1;}
    }
    return 0;
}

const char *TranslateOpusErr(int err)
{
     switch(err){
         case OP_FALSE      : return "Fail";
         case OP_EOF        : return "End of file";
         case OP_HOLE       : return "Corupt page";
         case OP_EREAD      : return "Read failed";
         case OP_EFAULT     : return "Offline";
         case OP_EIMPL      : return "Unsupported";
         case OP_EINVAL     : return "Invalid param";
         case OP_ENOTFORMAT : return "Not an opus stream";
         case OP_EBADHEADER : return "Bad header";
         case OP_EVERSION   : return "Bad version";
         case OP_ENOTAUDIO  : return "Not audio?";
         case OP_EBADPACKET : return "Packet failed to decode";
         case OP_EBADLINK   : return "Bad link";
         case OP_ENOSEEK    : return "Unseekable stream";
         case OP_EBADTIMESTAMP: return "Bad timestamp";
         default: return "?";
     }
}

/***************************************
 *  Infobox helpers                    *
 ***************************************/
static void SetTextStr(HWND hwnd, int IDC, const char *z)
{
    char *tmpstr;
    wchar_t *tmpW;
    if(!z || *z == '\0') return;

    if (TAGS_DISPLAY_MODE == 0){ //RAW UTF-8
        SetDlgItemText(hwnd, IDC, z);
    } else if (TAGS_DISPLAY_MODE == 2 || (TAGS_DISPLAY_MODE == 3 && isNT)){ // Full Unicode
        tmpW = utf8_to_utf16((char *)z);
        if (tmpW && *tmpW != '\0')SetDlgItemTextW(hwnd, IDC, tmpW);
        free(tmpW);
    } else { // Convert to ANSI...
        tmpstr = utf8_to_ansi((char *)z);
        if (tmpstr && *tmpstr != '\0') SetDlgItemText(hwnd, IDC, tmpstr);
        free(tmpstr);
    }
}

static void SetText(HWND hwnd, int IDC, const char *TagN, const OpusTags *_tags)
{
    const char *z;
    z = opus_tags_query(_tags, TagN, 0);

    SetTextStr(hwnd, IDC, z);

}

static void SetTextRaw(HWND  hwnd, int IDC, const char *TagN, const OpusTags *_tags)
{
    const char *z;
    z = opus_tags_query(_tags, TagN, 0);
    if(z && *z != '\0') SetDlgItemText(hwnd, IDC, z);
}

static void SetTextML(HWND hwnd, int IDC, const char *TagN, const OpusTags *_tags)
{
    char *tmpstr;
    const char *z;

    z = opus_tags_query(_tags, TagN, 0);

    if(z && *z != '\0'){
        tmpstr = unix2dos(z);
        SetTextStr(hwnd, IDC, tmpstr);
        free(tmpstr); // this one we have to free, it aint const
    }
}

static void SetFonts(HWND hwnd,HFONT fonte)
{
    if (fonte != NULL && hwnd != NULL) {
        WPARAM newfont = (WPARAM)fonte;
        SendDlgItemMessage(hwnd, IDC_NAME,       WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_TITLE,      WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_ARTIST,     WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_ALBUM,      WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_COMMENT,    WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_GENRE,      WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_COMPOSER,   WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_YEAR,       WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_TRACK,      WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_URL,        WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_PERFORMER,  WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_DESCRIPTION,WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_COPYRIGHT,  WM_SETFONT, newfont, TRUE);
        SendDlgItemMessage(hwnd, IDC_TRACKTOTAL, WM_SETFONT, newfont, TRUE);
    }
}
static const char *GetRGmode(char use_replay_gain)
{
    static char const * const table[] =
        { "Off", "Album", "Track", "Auto", "Null", "Off" };
    return table[ (size_t)use_replay_gain < countof(table) ? use_replay_gain : 0 ];
}
/*************************************
 * Infobox Initialisation for files  *
 *************************************/
static BOOL InitInfoboxInfo(HWND hwnd, const char *file)
{
    OpusServerInfo info_real, *_info=NULL;
    LONGLONG filesize=0;
    OggOpusFile *_tmp=NULL;
    const OpusTags *_tags=NULL;
    char *fnUTF8=NULL;
    int err = 0;
    char file_is_url;
    BOOL ret;
    char buffer[300];

    /* write file name as given by Winamp */
    const char *ffn = file;
    if (UNICODE_FILE) {
        fnUTF8 = utf16_to_utf8((wchar_t *)file);
        if (fnUTF8) {
            ffn = fnUTF8;
            if(TAGS_DISPLAY_MODE == 2 || (TAGS_DISPLAY_MODE == 3 && isNT)){
                SetDlgItemTextW(hwnd, IDC_NAME, (wchar_t *)file); // UNICODE
            } else if(TAGS_DISPLAY_MODE == 1){
                char *tmpstr=utf8_to_ansi(fnUTF8);
                if(tmpstr) SetDlgItemText(hwnd, IDC_NAME, tmpstr); // ANSI
                free(tmpstr);
            } else if(TAGS_DISPLAY_MODE == 0){
                SetDlgItemText(hwnd, IDC_NAME, fnUTF8); // raw utf-8 dump
            }
        }
    } else { // no UNICODE_FILE
        SetDlgItemText(hwnd, IDC_NAME, file); // file is ANSI
    }
    file_is_url = isURL(ffn);

    /* stream data and vorbis comment */
    if (file_is_url) {
        int L;
        _info = &info_real;
        if(_info == NULL) goto FAIL;
        opus_server_info_init(_info);

        const char *p = ffn;
        L = strlen(p) - 6;
        if(!_strnicmp(p+L, ">.opus", 6)){
            lstrcpy_sA(buffer, countof(buffer), p);
            buffer[L]='\0';
            L=0;
        }
        _tmp = op_open_url(!L? buffer: p, &err,OP_GET_SERVER_INFO(_info),NULL);

    } else { // NOT an URL
        if (ffn)
            _tmp = op_open_file(ffn, &err);
    }

    if (!_tmp) {
        if (file_is_url && err == OP_ENOTFORMAT) {
            sprintf(buffer,"Cannot open file: \n\n%s\n\n"
                   "Error No. -132: Not an opus stream\n"
                   "Try renaming the stream adding ?.mp3 or ?.ogg at the end."
                   , ffn);

        } else {
            sprintf(buffer, "Cannot open file: \n\n%s\n\n Error No. %d: %s"
                   , ffn, err, TranslateOpusErr(err));
        }
        MessageBox(hwnd, buffer, "OPUS Stream Error", MB_OK);

        goto FAIL;
    }
    free(fnUTF8); fnUTF8=NULL;


    /* Get Bitrate, Gain and duration info */
    if (!file_is_url) {
        _tags = op_tags(_tmp, -1);
        if (!_tags) goto FAIL;

        /* Repaly gains. */
        int tgain=0, again=0, lufs_offset=0;
        int retT = opus_tags_get_track_gain(_tags, &tgain); // Track Gain
        int retA = opus_tags_get_album_gain(_tags, &again); // Album Gain
        int hgain = op_get_header_gain(_tmp); // Header gain, all in 256th dB
        if(hgain != 0 || retT == 0 || retA == 0)
            lufs_offset = (23 + TARGET_LUFS)*256;

        /* Fill small Encoder/RG info at the bottom */
        sprintf(buffer
           ,"Encoder: %s\nTrk %+.1f dB, Alb %+.1f dB, Hed %+.1f dB, (%s)"
           , opus_tags_query(_tags, "ENCODER", 0)
           ,((float)(tgain))*(1.F/256.F)
           ,((float)(again))*(1.F/256.F)
           ,((float)(hgain+lufs_offset))*(1.F/256.F)
           ,GetRGmode(USE_REPLAY_GAIN)  );

        SetDlgItemText(hwnd, IDC_ENCODER, buffer);

        /* File size / duration / bps */
        filesize = op_raw_total (_tmp, -1);              // In Bites
        unsigned length = (unsigned)(op_pcm_total(_tmp, -1)/48000);// File length in seconds
        float bps = (float)op_bitrate(_tmp, -1);        // In bits per seconds
        sprintf(buffer, "%I64d bytes (%d ch) for %u h %u min %u s at %.1f Kbps"
                      , filesize, op_channel_count(_tmp, -1) ,(length/3600)
                      , (length/60)%60, length%60, bps/1000.F);
    } else {
        sprintf(buffer, "Radio Stream: %s (%d ch) at %d kbps"
                      , _info->server, op_channel_count(_tmp, -1)
                      , _info->bitrate_kbps);
    }
    /* Print size / duration / bps  */
    SetDlgItemText(hwnd, IDC_INFO, buffer);


    /* OPUS TAGS */
    SetFonts(hwnd, TAGS_FONT); // Setting spetial font if needed

    if(!file_is_url){ // Normal TAGS
        SetText    (hwnd, IDC_TITLE,      "TITLE", _tags);
        SetText    (hwnd, IDC_ARTIST,     "ARTIST", _tags);
        SetText    (hwnd, IDC_ALBUM,      "ALBUM", _tags);
        SetText    (hwnd, IDC_COMMENT,    "COMMENT", _tags);
        SetText    (hwnd, IDC_GENRE,      "GENRE", _tags);
        SetText    (hwnd, IDC_COMPOSER,   "COMPOSER", _tags);
        SetText    (hwnd, IDC_PERFORMER,  "PERFORMER", _tags);
        SetText    (hwnd, IDC_COPYRIGHT,  "COPYRIGHT", _tags);
        SetTextRaw (hwnd, IDC_YEAR,       "DATE", _tags);
        SetTextRaw (hwnd, IDC_TRACK,      "TRACKNUMBER", _tags);
        SetTextRaw (hwnd, IDC_TRACKTOTAL, "TRACKTOTAL", _tags);
        SetTextRaw (hwnd, IDC_URL,        "PURL", _tags);
        SetTextML  (hwnd, IDC_DESCRIPTION,"DESCRIPTION", _tags); //MultiLine
    } else if (_info){ // RADIO mode
        SetTextStr (hwnd, IDC_TITLE,       _info->name);
        SetTextStr (hwnd, IDC_DESCRIPTION, _info->description);
        SetTextStr (hwnd, IDC_GENRE,       _info->genre);
        SetTextStr (hwnd, IDC_URL,         _info->url);
        SetTextStr (hwnd, IDC_COMMENT,     _info->content_type);
        SetTextStr (hwnd, IDC_COPYRIGHT, (_info->is_public==1)?"Public server"
                                       : (_info->is_public==-1)?"": "Private server");
        opus_server_info_clear(_info);
    }

    /* END OF TAGS */

    ret=TRUE; goto FREALL; // if we are here everything is good.

    FAIL: ret=FALSE;

    FREALL: /* Free all mem and exit */
    free(fnUTF8);
    op_free(_tmp);

    return ret;
}

static INT_PTR CALLBACK InfoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) { // Big Switch de msg

        /* Initialisation */
        case WM_INITDIALOG:
            SetWindowText(hwnd, isURL((char *)lParam)? "OPUS Radio Info": "OPUS File Info");
            /* init fields using our funcion */
            if (!InitInfoboxInfo(hwnd, (const char*)lParam))
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            return TRUE;

        /* If Destroy message is sent */
        case WM_DESTROY: break;

        /* A Command is sent */
        case WM_COMMAND:
           switch (LOWORD(wParam)) { /* OK / Cancel */
               case IDOK:
                  /* int SUCESS = write_tags_in_file(hwnd, (const char*)lParam));
                   * EndDialog(hwnd, LOWORD(wParam));
                   * return SUCESS; */
               case IDCANCEL:
                   EndDialog(hwnd, LOWORD(wParam));
                   return TRUE;
           } break;

    } // END Big Switch de msg

    return 0;
}

void DoInfoBox(HINSTANCE inst, HWND hwnd, const char *filename)
{
    int ret;

    if(TAGS_DISPLAY_MODE == 2 || (TAGS_DISPLAY_MODE == 3 && isNT)){
        SetLastError(0);
        ret = DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_INFOBOX), hwnd, InfoProc, (LONG)filename);

        if (ret == 0 || GetLastError() == 120) {
            /*MessageBox(hwnd, "Error: cannot create Unicode Dialog Box,\n"
                        "Your system does not handle 'DialogBoxParamW' function\n"
                        "Go in wianmp.ini and set TAGS_DISPLAY_MODE=0, 1 or 3\n"
                        "in the [IN_OPUS] section\n\n"
                        "Disabling Unicode support for this session"
                      , "in_opus error", MB_OK);
            */
            TAGS_DISPLAY_MODE=1;
        }
    }

    if (TAGS_DISPLAY_MODE <= 1 || (TAGS_DISPLAY_MODE == 3 && !isNT)) {
        ret = DialogBoxParam(inst, MAKEINTRESOURCE(IDD_INFOBOX), hwnd, InfoProc, (LONG)filename);
    }
}


/////////////////////////////////////////////////////////////////////////////
// This is called for every metadata item to be saved to a file and has a unicode and ansi
// version. The unicode version is the prefered version on unicode compatible Winamp clients
// though the ansi function will be called if the unicode is not present or being used on an
// older style client without internal unicode support for metadata.
// data is the metadata tag being queried which typically consists of the id3v1 fields but also includes some
// additional ones which Winamp uses for determining how to handle files or display different things to the user.
// They can consist of, but are not limited to the following (and plug-in specific tags can be handled):
// "track", "title", "artist", "album", "year", "comment", "genre", "length",
// "type", "family", "formatinformation", "gain", "bitrate", "vbr", "stereo" and more
int winampGetExtendedFileInfo_utf8(const char *fn, const char *data, char *dest, size_t destlen)
{
    const char *z = NULL;
    int err=0;
    const OpusTags *_tags=NULL;
    OggOpusFile *_of;


    //MessageBoxA(NULL, data, fn, 0);
    if (!_stricmp(data, "type")) {
        dest[0] = '0'; // audio format
        return 1;

    } else if (!_stricmp(data, "family")) {
//        strcpy(dest, "Ogg/Opus Audio File");
        return 0;

    } else if (!_stricmp(data, "length")) {
        return 0;

    }
    // opening file
    _of = op_open_file(fn, &err);
    if(_of && !err){ // if file opened correctly, open tags.
        _tags = op_tags(_of, -1);
    } else if (err != -132 || has_opus_ext(fn)) { // if the file could not be opened and is opus.
        op_free(_of); // free it in case.
        if(!_stricmp(data, "title")) {
            // send error message...
            const char *p;
            if ( (p = ffilestart(fn)) ) p++; else p = (char *)fn;

            sprintf(dest, "[in_opus err %d %s] %s"
                , err, TranslateOpusErr(err), p);
            return 1;
        } else {
            return 0;
        }
    }
    // if file opened correctly
    if(!_tags) goto fail;

    // FROM HERE TAGS HAS TO BE OK!

    if (!_stricmp(data, "bitrate")) {
        sprintf(dest, "%1.f", (float)op_bitrate(_of, -1));
        goto finish;

    } else if (!_stricmp(data, "formatinformation")) {
        sprintf(dest,
            "Size: %I64d bites\n"
            "Length: %I64d s\n"
            "Bitrate: %.2f kbps\n"
            "Channels: %d\n"
            "Header gain: %.2f dB\n"
            "Input samplerate: %d Hz\n"
            "Encoder: %s\n"
           , op_raw_total(_of, -1)
           , op_pcm_total(_of, -1)/48000
           , (float)op_bitrate(_of, -1)/1000.F
           , op_channel_count(_of, -1)
           , (float)(op_get_header_gain(_of)/256.F)
           , op_get_input_sample_rate(_of)
           , opus_tags_query(_tags, "ENCODER", 0)
        );

        goto finish;

    } else if (!_stricmp(data, "replaygain_track_gain")) {
        int tgain=0;
        opus_tags_get_track_gain(_tags, &tgain);
        sprintf(dest, "%.2f", (float)tgain/256.f);
        goto finish;

    } else if (!_stricmp(data, "replaygain_album_gain")) {
        int again=0;
        opus_tags_get_album_gain (_tags, &again);
        sprintf(dest, "%.2f", (float)again/256.f);
        goto finish;

    } else if (!_stricmp(data, "comment")) { // convert to DOS
        char *dos;
        z = opus_tags_query(_tags, "DESCRIPTION", 0);
        dos = unix2dos(z);
        if(!dos) goto fail;
        lstrcpy_sA(dest, destlen-1, dos);
        free(dos);
        goto finish;

    } else if (!_stricmp(data, "year")) {
        z = opus_tags_query(_tags, "DATE", 0);
    } else if (!_stricmp(data, "track")) {
        z = opus_tags_query(_tags, "TRACKNUMBER", 0);
    } else if (!_stricmp(data, "disc")) {
        z = opus_tags_query(_tags, "DISCNUMBER", 0);
    } else {
        z = opus_tags_query(_tags, data, 0);
    }
    if (z) {
       lstrcpy_sA(dest, destlen-1, z);
       goto finish;
    }
    fail:
    op_free(_of);
    return 0;

    finish: // sucess!
    op_free(_of);
    return 1;
}

__declspec(dllexport) int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, size_t destlen)
{
    char *dest_utf8, *dest_ansi;
    int ret = 0;

    dest_utf8=calloc(destlen, sizeof(char)); if(!dest_utf8) return 0;

    if (winampGetExtendedFileInfo_utf8(fn, data, dest_utf8, destlen)) {
        dest_ansi = utf8_to_ansi(dest_utf8); if (!dest_ansi) goto fail;
        dest[0] = '\0';
        lstrcpy_sA(dest, destlen-1, dest_ansi);
        free(dest_ansi);
        ret = 1;
    } else {
        ret = 0;
    }
    fail:
    free(dest_utf8);
    return ret;
}

__declspec(dllexport) int winampGetExtendedFileInfoW(const wchar_t *fn, const char *data, wchar_t *dest, size_t destlen)
{
    char *dest_utf8 = NULL, *fnUTF8 = NULL;
    int ret =0;
    fnUTF8 = utf16_to_utf8(fn);                if(!fnUTF8)    goto fail;
    dest_utf8 = calloc(destlen, sizeof(char)); if(!dest_utf8) goto fail;

    if (winampGetExtendedFileInfo_utf8(fnUTF8, data, dest_utf8, destlen)) {
        wchar_t *dest_w = utf8_to_utf16(dest_utf8); if(!dest_w) goto fail;
        dest[0] = '\0';
        lstrcpy_sW(dest, destlen-1, dest_w);
        free(dest_w);
        ret = 1;
    } else {
        ret = 0;
    }

    fail:
    free(dest_utf8);
    free(fnUTF8);

    return ret;
}
__declspec(dllexport) int winampUseUnifiedFileInfoDlg(const wchar_t *fn)
{
    int err=-132;
    wchar_t *p=(wchar_t *)fn;
//    MessageBox(NULL,"START","winampUseUnifiedFileInfoDlg",MB_OK);

    if(UNIFIED_DIALOG){
        p += wcslen(p);

        if (!_wcsnicmp(fn,L"http://", 7)){
            // if URL;
//            MessageBox(NULL,"URL return 0","winampUseUnifiedFileInfoDlg",MB_OK);
            return 0;

        } else if(has_opus_extW(fn)) {
            // extension is .opus or .opu
//            MessageBox(NULL,"OPUS file and return 1","winampUseUnifiedFileInfoDlg",MB_OK);
            return 1;

        } else if((p[-1]=='g'&&p[-2]=='g'&&p[-3]=='o'&&p[-4]=='.')) {
            // extenson is .ogg
            char *fnUTF8;
            OggOpusFile *_tmp;

            fnUTF8 = utf16_to_utf8(fn); if(!fnUTF8) return 0;
            _tmp = op_test_file(fnUTF8, &err);
            free(fnUTF8);
            op_free(_tmp);

            if(err == 0){
//                MessageBox(NULL,"OGG file and return 1","winampUseUnifiedFileInfoDlg",MB_OK);
                return 1;
            } else {
//                MessageBox(NULL,"OGG file and return 0","winampUseUnifiedFileInfoDlg",MB_OK);
                return 0;
            }
        }

    } else {
        return 0;
    }
    return 0;
}
__declspec(dllexport) int winampSetExtendedFileInfoW(const wchar_t *fn, const char *data, const wchar_t *val)
{
  return 0;
}
__declspec(dllexport) int winampSetExtendedFileInfo(const char *fn, const char *data, const char *val)
{
  return 0;
}
__declspec(dllexport) int winampWriteExtendedFileInfo()
{
  return 0;
}
