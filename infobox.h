// Just for this man..

void DoInfoBox(HINSTANCE inst, HWND hwnd, const char *filename);

opus_int32 op_get_header_gain(OggOpusFile *_of);
opus_int32  op_get_input_sample_rate(OggOpusFile *_of);
const char *TranslateOpusErr(int err);
const wchar_t *TranslateOpusErrW(int err);

char isURL(char *fn);
char isRGavailable(OggOpusFile *_of);

//int has_opus_ext(const char *p);
//int has_opus_extW(const wchar_t *p);
