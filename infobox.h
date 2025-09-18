#define countof(array) (sizeof(array)/sizeof(array[0]))

void DoInfoBox(HINSTANCE inst, HWND hwnd, const char *filename);

opus_int32 op_get_header_gain(const OggOpusFile *_of);
opus_int32  op_get_input_sample_rate(const OggOpusFile *_of);
const char *TranslateOpusErr(int err);

char isURL(const char *const fn);
char isRGavailable(const OggOpusFile *_of);
const char *lstrcpy_sA(char *__restrict__ dest, size_t N, const char *src);
char *ffilestart(const char *str);
int isourfile(const char *const fn);

__declspec(dllexport) int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, size_t destlen);
__declspec(dllexport) int winampGetExtendedFileInfoW(const wchar_t *fn, const char *data, wchar_t *dest, size_t destlen);

//int has_opus_ext(const char *p);
//int has_opus_extW(const wchar_t *p);
