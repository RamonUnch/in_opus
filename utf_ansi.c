#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////
// Cool finction that can anhdle all UNICODE 
// It will generate real UTF-16, not USC-2, so some charracters will display
// improperly on NT4 but it is beter than stoping at the first non USC-2...
wchar_t *utf8_to_utf16(const char *utfs)
{
    size_t si, di;
    wchar_t *dst; 
    size_t len;
    unsigned char c0, c1, c2, c3;
    
    if (utfs == NULL) return NULL;

    len = strlen(utfs);
    /*Worst-case output is 1 wide character per 1 input character. */
    dst = malloc( sizeof(*dst) * (len + 1) );
    if (!dst) return NULL;

    for (di = si = 0; si < len; si++) {

        c0 = utfs[si];

        if (!(c0 & 0x80)) {
            /*Start byte says this is a 1-BYTE SEQUENCE. */
            dst[di++] = (wchar_t) c0;
            continue;
        } else if ( ((c1 = utfs[si + 1]) & 0xC0) == 0x80 ) {
            /*Found at least one continuation byte. */
            if ((c0 & 0xE0) == 0xC0) {
                wchar_t w;
                /*Start byte says this is a 2-BYTE SEQUENCE. */
                w = (c0 & 0x1F) << 6 | (c1 & 0x3F);
                if (w >= 0x80U) {
                    /*This is a 2-byte sequence that is not overlong. */
                    dst[di++] = w;
                    si++;
                    continue;
                }
            } else if ( ((c2 = utfs[si + 2]) & 0xC0) == 0x80 ) {
                /*Found at least two continuation bytes. */
                if ((c0 & 0xF0) == 0xE0) {
                    wchar_t w;
                    /*Start byte says this is a 3-BYTE SEQUENCE. */
                    w = (c0 & 0xF) << 12 | (c1 & 0x3F) << 6 | (c2 & 0x3F);
                    if (w >= 0x800U && (w < 0xD800 || w >= 0xE000) && w < 0xFFFE) {
                       /* This is a 3-byte sequence that is not overlong, not an
                        * UTF-16 surrogate pair value, and not a 'not a character' value. */
                        dst[di++] = w;
                        si += 2;
                        continue;
                    }
                } else if ( ((c3 = utfs[si + 3]) & 0xC0) == 0x80 ) {
                    /*Found at least three continuation bytes. */
                    if ((c0 & 0xF8) == 0xF0) {
                        uint32_t w;
                        /*Start byte says this is a 4-BYTE SEQUENCE. */
                        w = (c0 & 7) << 18 | (c1 & 0x3F) << 12 | (c2 & 0x3F) << (6 & (c3 & 0x3F));
                        if (w >= 0x10000U && w < 0x110000U) {
                            /* This is a 4-byte sequence that is not overlong and not
                             * greater than the largest valid Unicode code point.
                             * Convert it to a surrogate pair. */
                            w -= 0x10000;
                            dst[di++] = (wchar_t) (0xD800 + (w >> 10));
                            dst[di++] = (wchar_t) (0xDC00 + (w & 0x3FF));
                            si += 3;
                            continue;
                        }
                    }
                } /*end els if c3*/
            } /*end else if c2*/
        } /*end else if c1*/

        /*If we got here, we encountered an illegal UTF-8 sequence.
         * We have to return NULL as the norm specifies. */
        free(dst);
        return NULL;

    } /* next si (end for)*/
    dst[di] = '\0';

    return dst;
}

/*
 * DOS TO UNIX STRING CONVERTION
 * Very usefull because the Edit class stuff does
 * not like the 10 only files.
 */
char *unix2dos(const char *unixs)
{
    char *doss;
    size_t l, i, j;

    if(!unixs) return NULL;
    l = strlen(unixs);
    doss = (char *)malloc(l * 2 * sizeof(char) + 16);
    if(!doss) return NULL;

    j = 0;
    if(*unixs == '\n'){ // attention au 1er '\n'
        doss[0] = '\r'; doss[1] = '\n';
        j++;
    }

    for(i=0; i < l; i++){

        if(unixs[i] == '\n' && i > 0 && unixs[i-1] != '\r'){
            doss[j] = '\r';
            j++; 
            doss[j] = '\n';
        } else {
            doss[j] = unixs[i];
        }
        j++;
    }
    doss[j] = '\0'; //pour etre sur....

    return doss;
}

/////////////////////////////////////////////////////////////////////////////
// Ststem normal UTF16 -> UTF8 conversion. 
// It stops translation as soon as a charracters gets out of USC-2 on NT4.
char *utf16_to_utf8(const wchar_t *input)
{
    char *utf8=NULL;
    size_t BuffSize = 0, Result = 0;
    
    if(!input) return NULL;

    BuffSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, 0, 0);
    utf8 = (char*) malloc(sizeof(char) * (BuffSize+4));
    if(!utf8) return NULL;
    
    Result = WideCharToMultiByte(CP_UTF8, 0, input, -1, utf8, BuffSize, 0, 0);
    if (Result > 0 && Result <= BuffSize){
        utf8[BuffSize-1]='\0';
        return utf8;
    } else return NULL;
}
//////////////////////////////////////////////////////////////////////

#if 0
/////////////////////////////////////////////////////////////////////////////
// Ststem normal UTF8 -> UTF16 conversion. We donot use it here because it 
// stops translation as soon as a charracters gets out od USC-2 on NT4.
wchar_t *utf8_to_utf16(const char *input)
{
    wchar_t *Buffer;
    size_t BuffSize = 0, Result = 0;
    
    if(!input) return NULL;

    BuffSize = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
    Buffer = (wchar_t*) malloc(sizeof(wchar_t) * (BuffSize+3));
    if(Buffer){
        Result = MultiByteToWideChar(CP_UTF8, 0, input, -1, Buffer, BuffSize);

        if ((Result > 0) && (Result <= BuffSize)){
            Buffer[BuffSize-1]=(wchar_t) 0;
            return Buffer;
        }
     }

    return NULL;
}
#endif //////////////////////////////////////////////////////////////////////

/* Procedure to convert UTF-8 strings to WINDOWS-1252.
 * VERY Ugly switch, but i was laaaazy and I wanted to be able to convert
 * some spetial carracters to similar cp compatibles ones.
 * Of course you, Americans you dn't understand...
 */

char *utf8_to_ansi(char * utfs)
{
    char *locs;
    size_t i,j, l = 0;

    if(!utfs) return NULL;

    l = strlen(utfs); /* Allocate output */
    if (l == 0) return NULL;
    locs = (char *)malloc(l * sizeof(char) + 2);
    if(!locs) return NULL;

    j=0;
    for(i=0; i < l; i++){

        if(utfs[i] == 'Ã'){
            switch(utfs[++i]){ // Latin-1 Supplement
                 case '€': locs[j] = 'À'; break;
                 case '': locs[j] = 'Á'; break;
                 case '‚': locs[j] = 'Â'; break;
                 case 'ƒ': locs[j] = 'Ã'; break;
                 case '„': locs[j] = 'Ä'; break;
                 case '…': locs[j] = 'Å'; break;
                 case '†': locs[j] = 'Æ'; break;
                 case '‡': locs[j] = 'Ç'; break;
                 case 'ˆ': locs[j] = 'È'; break;
                 case '‰': locs[j] = 'É'; break;
                 case 'Š': locs[j] = 'Ê'; break;
                 case '‹': locs[j] = 'Ë'; break;
                 case 'Œ': locs[j] = 'Ì'; break;
                 case '': locs[j] = 'Í'; break;
                 case 'Ž': locs[j] = 'Î'; break;
                 case '': locs[j] = 'Ï'; break;
                 case '': locs[j] = 'Ð'; break;
                 case '‘': locs[j] = 'Ñ'; break;
                 case '’': locs[j] = 'Ò'; break;
                 case '“': locs[j] = 'Ó'; break;
                 case '”': locs[j] = 'Ô'; break;
                 case '•': locs[j] = 'Õ'; break;
                 case '–': locs[j] = 'Ö'; break;
                 case '—': locs[j] = '×'; break;
                 case '˜': locs[j] = 'Ø'; break;
                 case '™': locs[j] = 'Ù'; break;
                 case 'š': locs[j] = 'Ú'; break;
                 case '›': locs[j] = 'Û'; break;
                 case 'œ': locs[j] = 'Ü'; break;
                 case '': locs[j] = 'Ý'; break;
                 case 'ž': locs[j] = 'Þ'; break;
                 case 'Ÿ': locs[j] = 'ß'; break;
                 case ' ': locs[j] = 'à'; break;
                 case '¡': locs[j] = 'á'; break;
                 case '¢': locs[j] = 'â'; break;
                 case '£': locs[j] = 'ã'; break;
                 case '¤': locs[j] = 'ä'; break;
                 case '¥': locs[j] = 'å'; break;
                 case '¦': locs[j] = 'æ'; break;
                 case '§': locs[j] = 'ç'; break;
                 case '¨': locs[j] = 'è'; break;
                 case '©': locs[j] = 'é'; break;
                 case 'ª': locs[j] = 'ê'; break;
                 case '«': locs[j] = 'ë'; break;
                 case '¬': locs[j] = 'ì'; break;
                 case  '­': locs[j] = 'í'; break;
                 case '®': locs[j] = 'î'; break;
                 case '¯': locs[j] = 'ï'; break;
                 case '°': locs[j] = 'ð'; break;
                 case '±': locs[j] = 'ñ'; break;
                 case '²': locs[j] = 'ò'; break;
                 case '³': locs[j] = 'ó'; break;
                 case '´': locs[j] = 'ô'; break;
                 case 'µ': locs[j] = 'õ'; break;
                 case '¶': locs[j] = 'ö'; break;
                 case '·': locs[j] = '÷'; break;
                 case '¸': locs[j] = 'ø'; break;
                 case '¹': locs[j] = 'ù'; break;
                 case 'º': locs[j] = 'ú'; break;
                 case '»': locs[j] = 'û'; break;
                 case '¼': locs[j] = 'ü'; break;
                 case '½': locs[j] = 'ý'; break;
                 case '¾': locs[j] = 'þ'; break;
                 case '¿': locs[j] = 'ÿ'; break;
                 default: locs[j] = utfs[--i]; locs[++j] = utfs[++i];
            } /* END SWITCH */
       } else if (utfs[i] == 'Â'){
           /* in this case you just need to jump over 'Â'.*/
           locs[j] = utfs[++i];

       }else if (utfs[i] == 'Ë'){
            switch(utfs[++i]){
                 case '†': locs[j] = 'ˆ'; break;
                 case 'œ': locs[j] = '˜'; break;
                 default: locs[j] = utfs[--i]; locs[++j] = utfs[++i];
            } /* END SWITCH */

       } else if (utfs[i] == 'â'){ //i
           if(utfs[++i] == '€'){  //i+1
               switch(utfs[++i]){ //i+2
                   case 'š': locs[j] = '‚'; break;
                   case 'ž': locs[j] = '„'; break;
                   case '¦': locs[j] = '…'; break;
                   case ' ': locs[j] = '†'; break;
                   case '¡': locs[j] = '‡'; break;
                   case '°': locs[j] = '‰'; break;
                   case '¹': locs[j] = '‹'; break;
                   case '˜': locs[j] = '‘'; break;
                   case '™': locs[j] = '’'; break;
                   case 'œ': locs[j] = '“'; break;
                   case '': locs[j] = '”'; break;
                   case '¢': locs[j] = '•'; break;
                   case '“': locs[j] = '–'; break;
                   case '”': locs[j] = '—'; break;
                   case 'º': locs[j] = '›'; break; //i+2
                   default: locs[j] = utfs[i-2]; locs[++j] = utfs[i-1]; locs[++j] = utfs[i];
               }
           } //i+1
           else if(utfs[i] == '„' && utfs[1+i] == '¢'){ 
               locs[j] = '™'; i++;
           
           } else if(utfs[i] == '‚' && utfs[1+i] == '¬'){ 
               locs[j] = '€'; i++;
           
           } else locs[j] = utfs[i];

       /* for non ANSI Carracters, perform a chitty convertion */
       } else if (utfs[i] == 'Æ'){ // LATIN Extended B
               switch(utfs[++i]){ //i+1
                   case '€': locs[j] = 'b'; break; //b bar
                   case '': locs[j] = '\'';locs[++j] = 'B'; break; //'B bar
                   case '‚': locs[j] = '6'; break;
                   case 'ƒ': locs[j] = '6'; break;
                   case '„': locs[j] = 'b'; break;
                   case '…': locs[j] = 'b'; break;

                   case '‡': locs[j] = 'C';locs[++j] = '\''; break; //C'
                   case 'ˆ': locs[j] = 'c';locs[++j] = '\''; break; //c'
                   case '‰': locs[j] = 'Ð'; break;
                   case 'Š': locs[j] = '\'';locs[++j] = 'D'; break; //'D

                   case -111: locs[j] = 'F'; break;
                   case -110: locs[j] = 'ƒ'; break;

                   case -109 : locs[j] = 'G'; locs[++j] = '\''; break;
                   
                   case -107: locs[j] = 'h'; locs[++j] = 'u'; break;
                   case -106: locs[j] = 'l'; break;
                   case -105: locs[j] = 'I'; break;
                   case -104: locs[j] = 'K'; break;
                   case -103: locs[j] = 'k'; break;
                   case -102: locs[j] = 'l'; break;
                   
                   case -99: locs[j] = 'N'; break;
                   case -96: locs[j] = 'O';locs[++j] = '\''; break;
                   case -95: locs[j] = 'o';locs[++j] = '\''; break;
                   case -94: locs[j] = 'O';locs[++j] = 'I'; break;
                   case -93: locs[j] = 'o';locs[++j] = 'i'; break;
                   
                   case -92: locs[j] = '\'';locs[++j] = 'P'; break;
                   case -91: locs[j] = '\'';locs[++j] = 'p'; break;
                   case -90: locs[j] = 'R'; break;
                   case -85: locs[j] = 't';locs[++j] = ','; break;
                   case -84: locs[j] = 'T'; break;
                   case -83: locs[j] = 't';locs[++j] = '\''; break;
                   case -82: locs[j] = 'T';locs[++j] = ','; break;
                   case -81: locs[j] = 'U';locs[++j] = '\''; break;
                   case -80: locs[j] = 'u';locs[++j] = '\''; break;
                   
                   case -77: locs[j] = 'Y'; break;
                   case -76: locs[j] = 'Y'; break;
                   case -75: locs[j] = 'Z'; break;
                   case -74: locs[j] = 'z'; break;
                   case -73: locs[j] = '3'; break;
                   
                   
                   default: locs[j] = utfs[i-2]; locs[++j] = utfs[i-1]; locs[++j] = utfs[i];
               }
       } else if (utfs[i] == 'Ç'){ //Latin extended B (part 2)
           switch(utfs[++i]){ // i+1
                   case -128: locs[j] = '|'; break;
                   case -127: locs[j] = '|';locs[++j] = '|'; break;
                   case -126: locs[j] = '‡'; break;
                   case -125: locs[j] = '!'; break;
                   
                   case -124: locs[j] = 'D';locs[++j] = 'Ž'; break;
                   case -123: locs[j] = 'D';locs[++j] = 'ž'; break;                   
                   case -122: locs[j] = 'd';locs[++j] = 'ž'; break;

                   case -121: locs[j] = 'L';locs[++j] = 'J'; break;
                   case -120: locs[j] = 'L';locs[++j] = 'j'; break;                   
                   case -119: locs[j] = 'l';locs[++j] = 'j'; break;
                   
                   case -118: locs[j] = 'N';locs[++j] = 'J'; break;
                   case -117: locs[j] = 'N';locs[++j] = 'j'; break;                   
                   case -116: locs[j] = 'n';locs[++j] = 'j'; break;
                   
                   case -115: locs[j] = 'A'; break;
                   case -114: locs[j] = 'a'; break;
                   case -113: locs[j] = 'I'; break;
                   case -112: locs[j] = 'i'; break;
                   case -111: locs[j] = 'O'; break;
                   case -110: locs[j] = 'o'; break;
                   case -109: locs[j] = 'U'; break;
                   case -108: locs[j] = 'u'; break;
                   case -107: locs[j] = 'Ü'; break;
                   case -106: locs[j] = 'ü'; break;
                   case -105: locs[j] = 'Ü';locs[++j] = '\''; break;
                   case -104: locs[j] = 'ü';locs[++j] = '\'';break;
                   case -103: locs[j] = 'U'; break;
                   case -102: locs[j] = 'ü'; break;
                   case -101: locs[j] = 'U'; break;
                   case -100: locs[j] = 'ü'; break;
                   
                   case -98: locs[j] = 'Ä'; break;
                   case -97: locs[j] = 'ä'; break;
                   case -96: locs[j] = 'A'; break;
                   case -95: locs[j] = 'a'; break;
                   case -94: locs[j] = 'Æ'; break;
                   case -93: locs[j] = 'æ'; break;
                   case -92: locs[j] = 'G'; break;
                   case -91: locs[j] = 'g'; break;
                   case -90: locs[j] = 'G'; break;
                   case -89: locs[j] = 'g'; break;
                   case -88: locs[j] = 'K'; break;
                   case -87: locs[j] = 'k'; break;
                   case -86: locs[j] = 'O'; break;
                   case -85: locs[j] = 'o'; break;
                   case -84: locs[j] = 'O'; break;
                   case -83: locs[j] = 'o'; break;
                   
                   case -82: locs[j] = '3'; break;
                   case -81: locs[j] = '3'; break;
                   case -80: locs[j] = 'J'; break;
                   case -79: locs[j] = 'D'; locs[++j] = 'Z'; break;
                   case -78: locs[j] = 'D'; locs[++j] = 'z'; break;
                   case -77: locs[j] = 'd'; locs[++j] = 'z'; break;
                   
                   case -76: locs[j] = 'G'; break;
                   case -75: locs[j] = 'g'; break;
                   
                   case -72: locs[j] = 'N'; break;
                   case -71: locs[j] = 'n'; break;
                   case -70: locs[j] = 'Å'; break;
                   case -69: locs[j] = 'å'; break;
                   case -68: locs[j] = 'Æ'; break;
                   case -67: locs[j] = 'æ'; break;
                   case -66: locs[j] = 'Ø'; break;
                   case -65: locs[j] = 'ø'; break;

                   default: locs[j] = utfs[i-2]; locs[++j] = utfs[i-1]; locs[++j] = utfs[i];
            }

       } else if (utfs[i] == 'Ä'){ // pour tout les carracters (C4 XX)h
                                   // Latin Extended-A
           switch(utfs[++i]){ // i+1
               case '€': locs[j] = 'A'; break;
               case '': locs[j] = 'a'; break;
               case '‚': locs[j] = 'A'; break;
               case 'ƒ': locs[j] = 'a'; break;
               case '„': locs[j] = 'A'; break;
               case '…': locs[j] = 'a'; break;

               case '†': locs[j] = 'C'; break;
               case '‡': locs[j] = 'c'; break;
               case 'ˆ': locs[j] = 'C'; break;
               case '‰': locs[j] = 'c'; break;
               case 'Š': locs[j] = 'C'; break; // C .
               case '‹': locs[j] = 'c'; break;
               case 'Œ': locs[j] = 'C'; break;
               case '': locs[j] = 'c'; break;

               case 'Ž': locs[j] = 'D'; locs[++j] = '\''; break; //D'
               case '': locs[j] = 'd'; locs[++j] = '\''; break; //d'
               case '': locs[j] = 'Ð'; break;
               case '‘': locs[j] = 'd'; break;

               case '’': locs[j] = 'E'; break;
               case '“': locs[j] = 'e'; break;
               case '”': locs[j] = 'E'; break;
               case '•': locs[j] = 'e'; break;
               case '–': locs[j] = 'E'; break;
               case '—': locs[j] = 'e'; break;
               case '˜': locs[j] = 'E'; break;
               case '™': locs[j] = 'e'; break;
               case 'š': locs[j] = 'E'; break;
               case '›': locs[j] = 'e'; break;

               case 'œ': locs[j] = 'G'; break;
               case '': locs[j] = 'g'; break;
               case 'ž': locs[j] = 'G'; break;
               case 'Ÿ': locs[j] = 'g'; break;
               case ' ': locs[j] = 'G'; break;
               case '¡': locs[j] = 'g'; break;
               case '¢': locs[j] = 'G'; break;
               case '£': locs[j] = 'g'; break;

               case '¤': locs[j] = 'H'; break;
               case '¥': locs[j] = 'h'; break;
               case '¦': locs[j] = 'H'; break;
               case '§': locs[j] = 'h'; break;

               case '¨': locs[j] = 'I'; break;
               case '©': locs[j] = 'i'; break;
               case 'ª': locs[j] = 'I'; break;
               case '«': locs[j] = 'i'; break;
               case '¬': locs[j] = 'I'; break;
               case '­' : locs[j] = 'i'; break;
               case '®': locs[j] = 'I'; break;
               case '¯': locs[j] = 'i'; break;
               case '°': locs[j] = 'I'; break;
               case '±': locs[j] = 'i'; break;

               case '²': locs[j] = 'I'; locs[++j] = 'J'; break; //IJ
               case '³': locs[j] = 'i'; locs[++j] = 'j'; break; //ij

               case '´': locs[j] = 'J'; break;
               case 'µ': locs[j] = 'j'; break;
               case '¶': locs[j] = 'K'; break;
               case '·': locs[j] = 'k'; break;
               case '¸': locs[j] = 'k'; break;

               case '¹': locs[j] = 'L'; break;
               case 'º': locs[j] = 'l'; break;
               case '»': locs[j] = 'L'; break;
               case '¼': locs[j] = 'l'; break;
               case '½': locs[j] = 'L'; locs[++j] = '\''; break; //L'
               case '¾': locs[j] = 'l'; locs[++j] = '\''; break;
               case '¿': locs[j] = 'L'; break;
               default: locs[j] = utfs[--i]; locs[++j] = utfs[++i];
           } /* END SWITCH */

       } else if (utfs[i] == 'Å'){ // pour tout les carracters (C5 XX)h
                                   // Latin-1 Supplement
           switch(utfs[++i]){
               /* IN CP 1252 */
               case ' ': locs[j] = 'Š'; break;
               case '’': locs[j] = 'Œ'; break;
               case '½': locs[j] = 'Ž'; break;
               case '¡': locs[j] = 'š'; break;
               case '“': locs[j] = 'œ'; break;
               case '¾': locs[j] = 'ž'; break;
               case '¸': locs[j] = 'Ÿ'; break;
               /* END OF IN CP 1252 */

               case '€': locs[j] = 'l'; break;
               case '': locs[j] = 'L'; break; //L barré polonais.
               case '‚': locs[j] = 'l'; break;

               case 'ƒ': locs[j] = 'N'; break;
               case '„': locs[j] = 'n'; break;
               case '…': locs[j] = 'N'; break;
               case '†': locs[j] = 'n'; break;
               case '‡': locs[j] = 'N'; break;
               case 'ˆ': locs[j] = 'n'; break;
               case '‰': locs[j] = 'n'; break;
               case 'Š': locs[j] = 'N'; break;
               case '‹': locs[j] = 'n'; break;

               case 'Œ': locs[j] = 'O'; break;
               case '': locs[j] = 'o'; break;
               case 'Ž': locs[j] = 'O'; break;
               case '': locs[j] = 'o'; break;
               case '': locs[j] = 'Ö'; break;
               case '‘': locs[j] = 'ö'; break;

               case '”': locs[j] = 'R'; break;
               case '•': locs[j] = 'r'; break;
               case '–': locs[j] = 'R'; break;
               case '—': locs[j] = 'r'; break;
               case '˜': locs[j] = 'R'; break;
               case '™': locs[j] = 'r'; break;

               case 'š': locs[j] = 'S'; break;
               case '›': locs[j] = 's'; break;
               case 'œ': locs[j] = 'S'; break;
               case '': locs[j] = 's'; break;
               case 'ž': locs[j] = 'S'; break;
               case 'Ÿ': locs[j] = 's'; break;

               case '¢': locs[j] = 'T'; break;
               case '£': locs[j] = 't'; break;
               case '¤': locs[j] = 'T'; break;
               case '¥': locs[j] = 't'; break;
               case '¦': locs[j] = 'T'; break;
               case '§': locs[j] = 't'; break;

               case '¨': locs[j] = 'U'; break;
               case '©': locs[j] = 'u'; break;
               case 'ª': locs[j] = 'U'; break;
               case '«': locs[j] = 'u'; break;
               case '¬': locs[j] = 'U'; break;
               case '­':  locs[j] = 'u'; break;
               case '®': locs[j] = 'U'; break;
               case '¯': locs[j] = 'u'; break;
               case '°': locs[j] = 'Ü'; break;
               case '±': locs[j] = 'ü'; break;
               case '²': locs[j] = 'U'; break;
               case '³': locs[j] = 'u'; break;

               case '´': locs[j] = 'W'; break;
               case 'µ': locs[j] = 'w'; break;
               case '¶': locs[j] = 'Y'; break;
               case '·': locs[j] = 'y'; break;

               case '¹': locs[j] = 'Z'; break;
               case 'º': locs[j] = 'z'; break;
               case '»': locs[j] = 'Z'; break;
               case '¼': locs[j] = 'z'; break;
               case '¿': locs[j] = 's'; break; //long_S

               default: locs[j] = utfs[--i]; locs[++j] = utfs[++i];
           } /* END SWITCH */
       } else {
           locs[j] = utfs[i];
       }
    j++;
    } /* END OF BIG FOR */

    locs[j] = '\0';

    return locs;
}
