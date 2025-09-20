/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _WSPIAPI_H_
#define _WSPIAPI_H_

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <windows.h>

#ifdef _MSC_VER
#define WINBOOL BOOL
#endif

#define _WSPIAPI_STRCPY_S(_Dst,_Size,_Src) strcpy((_Dst),(_Src))
#define _WSPIAPI_STRCAT_S(_Dst,_Size,_Src) strcat((_Dst),(_Src))
#define _WSPIAPI_STRNCPY_S(_Dst,_Size,_Src,_Count) strncpy((_Dst),(_Src),(_Count)); (_Dst)[(_Size) - 1] = 0
#define _WSPIAPI_SPRINTF_S_1(_Dst,_Size,_Format,_Arg1) sprintf((_Dst),(_Format),(_Arg1))

#ifndef _WSPIAPI_COUNTOF
#ifndef __cplusplus
#define _WSPIAPI_COUNTOF(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#else
template <typename __CountofType,size_t __wspiapi_countof_helper_N> char (&__wspiapi_countof_helper(__CountofType (&_Array)[__wspiapi_countof_helper_N]))[__wspiapi_countof_helper_N];
#define _WSPIAPI_COUNTOF(_Array) sizeof(__wspiapi_countof_helper(_Array))
#endif
#endif

#define WspiapiMalloc(tSize) calloc(1,(tSize))
#define WspiapiFree(p) free(p)
#define WspiapiSwap(a,b,c) { (c) = (a); (a) = (b); (b) = (c); }
#define getaddrinfo WspiapiLegacyGetAddrInfo
#define getnameinfo WspiapiLegacyGetNameInfo
#define freeaddrinfo WspiapiLegacyFreeAddrInfo

#ifndef WSA_NOT_ENOUGH_MEMORY
#define WSA_NOT_ENOUGH_MEMORY 8
#endif

#ifndef WSATYPE_NOT_FOUND
#define WSATYPE_NOT_FOUND 10109
#endif

#ifndef AF_INET6
#define AF_INET6      23
#endif

#define INET6_ADDRSTRLEN 46

/* getnameinfo constants */ 
#define NI_MAXHOST	1025

#define NI_NOFQDN 	0x01
#define NI_NUMERICHOST	0x02
#define NI_NAMEREQD	0x04
#define NI_NUMERICSERV	0x08
#define NI_DGRAM	0x10

/* getaddrinfo constants */
#define AI_PASSIVE	1
#define AI_CANONNAME	2
#define AI_NUMERICHOST	4

/* getaddrinfo error codes */
#define EAI_AGAIN	WSATRY_AGAIN
#define EAI_BADFLAGS	WSAEINVAL
#define EAI_FAIL	WSANO_RECOVERY
#define EAI_FAMILY	WSAEAFNOSUPPORT
#define EAI_MEMORY	WSA_NOT_ENOUGH_MEMORY
#define EAI_NODATA	WSANO_DATA
#define EAI_NONAME	WSAHOST_NOT_FOUND
#define EAI_SERVICE	WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE	WSAESOCKTNOSUPPORT

#ifdef __cplusplus
extern "C" {
#endif
typedef int socklen_t;

struct addrinfo {
	int     ai_flags;
	int     ai_family;
	int     ai_socktype;
	int     ai_protocol;
	size_t  ai_addrlen;
	char   *ai_canonname;
	struct sockaddr  *ai_addr;
	struct addrinfo  *ai_next;
};

struct in6_addr {
    union {
        u_char	_S6_u8[16];
        u_short	_S6_u16[8];
        u_long	_S6_u32[4];
        } _S6_un;
};

/* These are used in some MS code */
#define in_addr6	in6_addr
#define _s6_bytes	_S6_un._S6_u8
#define _s6_words	_S6_un._S6_u16

typedef struct in6_addr IN6_ADDR,  *PIN6_ADDR, *LPIN6_ADDR;

struct sockaddr_in6 {
	short sin6_family;	/* AF_INET6 */
	u_short sin6_port; 	/* transport layer port # */
	u_long sin6_flowinfo;	/* IPv6 traffic class & flow info */
	struct in6_addr sin6_addr;  /* IPv6 address */
	u_long sin6_scope_id;	/* set of interfaces for a scope */
};
#ifdef __cplusplus
}
#endif

typedef int (WINAPI *WSPIAPI_PGETADDRINFO)(const char *nodename,const char *servname,const struct addrinfo *hints,struct addrinfo **res);
typedef int (WINAPI *WSPIAPI_PGETNAMEINFO)(const struct sockaddr *sa,socklen_t salen,char *host,size_t hostlen,char *serv,size_t servlen,int flags);
typedef void (WINAPI *WSPIAPI_PFREEADDRINFO)(struct addrinfo *ai);

#ifdef __cplusplus
extern "C" {
#endif
  typedef struct {
    char const *pszName;
    FARPROC pfAddress;
  } WSPIAPI_FUNCTION;

#define WSPIAPI_FUNCTION_ARRAY { { "getaddrinfo",(FARPROC) WspiapiLegacyGetAddrInfo }, \
  { "getnameinfo",(FARPROC) WspiapiLegacyGetNameInfo }, \
  { "freeaddrinfo",(FARPROC) WspiapiLegacyFreeAddrInfo } }

  char *WINAPI WspiapiStrdup (const char *pszString);
  WINBOOL WINAPI WspiapiParseV4Address (const char *pszAddress,PDWORD pdwAddress);
  struct addrinfo * WINAPI WspiapiNewAddrInfo (int iSocketType,int iProtocol,WORD wPort,DWORD dwAddress);
  int WINAPI WspiapiQueryDNS (const char *pszNodeName,int iSocketType,int iProtocol,WORD wPort,char pszAlias[NI_MAXHOST],struct addrinfo **pptResult);
  int WINAPI WspiapiLookupNode (const char *pszNodeName,int iSocketType,int iProtocol,WORD wPort,WINBOOL bAI_CANONNAME,struct addrinfo **pptResult);
  int WINAPI WspiapiClone (WORD wPort,struct addrinfo *ptResult);
  void WINAPI WspiapiLegacyFreeAddrInfo (struct addrinfo *ptHead);
  int WINAPI WspiapiLegacyGetAddrInfo(const char *pszNodeName,const char *pszServiceName,const struct addrinfo *ptHints,struct addrinfo **pptResult);
  int WINAPI WspiapiLegacyGetNameInfo(const struct sockaddr *ptSocketAddress,socklen_t tSocketLength,char *pszNodeName,size_t tNodeLength,char *pszServiceName,size_t tServiceLength,int iFlags);
  FARPROC WINAPI WspiapiLoad(WORD wFunction);
  int WINAPI WspiapiGetAddrInfo(const char *nodename,const char *servname,const struct addrinfo *hints,struct addrinfo **res);
  int WINAPI WspiapiGetNameInfo (const struct sockaddr *sa,socklen_t salen,char *host,size_t hostlen,char *serv,size_t servlen,int flags);
  void WINAPI WspiapiFreeAddrInfo (struct addrinfo *ai);

static __inline char*
gai_strerrorA(int ecode)
{
	static char message[1024+1];
	DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM
	              | FORMAT_MESSAGE_IGNORE_INSERTS
		      | FORMAT_MESSAGE_MAX_WIDTH_MASK;
	DWORD dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
  	FormatMessageA(dwFlags, NULL, ecode, dwLanguageId, (LPSTR)message, 1024, NULL);
	return message;
}
static __inline WCHAR*
gai_strerrorW(int ecode)
{
	static WCHAR message[1024+1];
	DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM
	              | FORMAT_MESSAGE_IGNORE_INSERTS
		      | FORMAT_MESSAGE_MAX_WIDTH_MASK;
	DWORD dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
  	FormatMessageW(dwFlags, NULL, ecode, dwLanguageId, (LPWSTR)message, 1024, NULL);
	return message;
}
#ifdef UNICODE
#define gai_strerror gai_strerrorW
#else
#define gai_strerror gai_strerrorA
#endif

#ifdef __cplusplus
}
#endif

#endif
