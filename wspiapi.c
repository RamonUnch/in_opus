#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock.h>
#include "wspiapi.h"

int WINAPI WspiapiQueryDNS(const char *pszNodeName, int iSocketType, int iProtocol,
                WORD wPort, char pszAlias[NI_MAXHOST], struct addrinfo **pptResult)
{
  struct addrinfo **paddrinfo = pptResult;
  struct hostent *phost = NULL;
  char **h;

  *paddrinfo = NULL;
  pszAlias[0] = 0;
  phost = gethostbyname (pszNodeName);
  if (phost){
      if (phost->h_addrtype == AF_INET && phost->h_length == sizeof(struct in_addr)){
            for (h = phost->h_addr_list; *h != NULL; h++){
                *paddrinfo = WspiapiNewAddrInfo (iSocketType, iProtocol, wPort,
                                                 ((struct in_addr *) *h)->s_addr);
                if (!*paddrinfo)
                  return EAI_MEMORY;
                paddrinfo = &((*paddrinfo)->ai_next);
            }
      }
      _WSPIAPI_STRCPY_S(pszAlias, NI_MAXHOST, phost->h_name);
      return 0;
  }
  switch(WSAGetLastError()) {
      case WSAHOST_NOT_FOUND: break;
      case WSATRY_AGAIN: return EAI_AGAIN;
      case WSANO_RECOVERY: return EAI_FAIL;
      case WSANO_DATA: return EAI_NODATA;
      default: break;
  }
  return EAI_NONAME;
}

void WINAPI WspiapiLegacyFreeAddrInfo (struct addrinfo *ptHead)
{
  struct addrinfo *p;

  for (p = ptHead; p != NULL; p = ptHead){
        if (p->ai_canonname)
          WspiapiFree (p->ai_canonname);
        if (p->ai_addr)
          WspiapiFree (p->ai_addr);
        ptHead = p->ai_next;
        WspiapiFree (p);
  }
}

int WINAPI WspiapiClone (WORD wPort, struct addrinfo *ptResult)
{
  struct addrinfo *p = NULL;
  struct addrinfo *n = NULL;

  for (p = ptResult; p != NULL;){
      n = WspiapiNewAddrInfo (SOCK_DGRAM, p->ai_protocol, wPort,
                 ((struct sockaddr_in *) p->ai_addr)->sin_addr.s_addr);
      if (!n) break;
      n->ai_next = p->ai_next;
      p->ai_next = n;
      p = n->ai_next;
  }
  if (p != NULL) return EAI_MEMORY;

  return 0;
}

int WINAPI WspiapiLookupNode (const char *pszNodeName,
                   int iSocketType, int iProtocol,
                   WORD wPort, WINBOOL bAI_CANONNAME,
                   struct addrinfo **pptResult)
{
  int err = 0, cntAlias = 0;
  char name[NI_MAXHOST]; name[0] = '\0';
  char alias[NI_MAXHOST]; alias[0] = '\0';
  char *pname = name, *palias = alias, *tmp = NULL;

  _WSPIAPI_STRCPY_S(pname, NI_MAXHOST, pszNodeName);
  for (;;){
      err = WspiapiQueryDNS (pszNodeName, iSocketType, iProtocol, wPort, palias, pptResult);
      if (err) break;
      if (*pptResult) break;
      cntAlias++;
      if (strlen (palias) == 0 || !strcmp (pname, palias) || cntAlias == 16){
          err = EAI_FAIL;
          break;
      }
      WspiapiSwap(pname, palias, tmp);
  }
  if (!err && bAI_CANONNAME){
      (*pptResult)->ai_canonname = WspiapiStrdup (palias);
      if (!(*pptResult)->ai_canonname)
          err = EAI_MEMORY;
  }
  return err;
}

char * WINAPI WspiapiStrdup (const char *pszString)
{
  char *rstr;
  size_t szlen;

  if(!pszString) return NULL;
  szlen = strlen(pszString) + 1;
  rstr = (char *) WspiapiMalloc (szlen);
  if (!rstr) return NULL;
  strcpy (rstr, pszString);
  return rstr;
}

struct addrinfo * WINAPI WspiapiNewAddrInfo (int iSocketType, int iProtocol, WORD wPort, DWORD dwAddress)
{
  struct addrinfo *n;
  struct sockaddr_in *pa;

  if ((n = (struct addrinfo *) WspiapiMalloc (sizeof (struct addrinfo))) == NULL)
    return NULL;
  if ((pa = (struct sockaddr_in *) WspiapiMalloc (sizeof(struct sockaddr_in))) == NULL){
        WspiapiFree(n);
        return NULL;
  }
  pa->sin_family = AF_INET;
  pa->sin_port = wPort;
  pa->sin_addr.s_addr = dwAddress;
  n->ai_family = PF_INET;
  n->ai_socktype = iSocketType;
  n->ai_protocol = iProtocol;
  n->ai_addrlen = sizeof (struct sockaddr_in);
  n->ai_addr = (struct sockaddr *) pa;
  return n;
}

WINBOOL WINAPI WspiapiParseV4Address (const char *pszAddress, PDWORD pdwAddress)
{
  DWORD dwAddress = 0;
  const char *h = NULL;
  int cnt;

  for (cnt = 0,h = pszAddress; *h != 0; h++)
    if (h[0] == '.') cnt++;
  if (cnt != 3) return FALSE;
  dwAddress = inet_addr (pszAddress);
  if (dwAddress == INADDR_NONE) return FALSE;
  *pdwAddress = dwAddress;

  return TRUE;
}

int WINAPI WspiapiLegacyGetAddrInfo(const char *pszNodeName,
                         const char *pszServiceName,
                         const struct addrinfo *ptHints,
                         struct addrinfo **pptResult)
{
  int err = 0, iFlags = 0, iFamily = PF_UNSPEC, iSocketType = 0, iProtocol = 0;
  struct in_addr inAddress;
  struct servent *svc = NULL;
  char *pc = NULL;
  WINBOOL isCloned = FALSE;
  WORD tcpPort = 0, udpPort = 0, port = 0;

  *pptResult = NULL;
  if (!pszNodeName && !pszServiceName)
    return EAI_NONAME;
  if (ptHints) {
        if (ptHints->ai_addrlen != 0 || ptHints->ai_canonname != NULL
            || ptHints->ai_addr!=NULL || ptHints->ai_next != NULL)
          return EAI_FAIL;
        iFlags = ptHints->ai_flags;
        if ((iFlags & AI_CANONNAME) != 0 && !pszNodeName)
          return EAI_BADFLAGS;
        iFamily = ptHints->ai_family;
        if (iFamily != PF_UNSPEC && iFamily != PF_INET)
          return EAI_FAMILY;
        iSocketType = ptHints->ai_socktype;
        if (iSocketType != 0 && iSocketType != SOCK_STREAM && iSocketType != SOCK_DGRAM
            && iSocketType != SOCK_RAW)
          return EAI_SOCKTYPE;
        iProtocol = ptHints->ai_protocol;
  }

  if (pszServiceName) {
      port = (WORD) strtoul (pszServiceName, &pc, 10);
      if(*pc == 0) {
          port = tcpPort = udpPort = htons (port);
          if (iSocketType == 0) {
              isCloned = TRUE;
              iSocketType = SOCK_STREAM;
          }
      } else {
          if (iSocketType == 0 || iSocketType == SOCK_DGRAM) {
              svc = getservbyname(pszServiceName, "udp");
              if (svc)
                port = udpPort = svc->s_port;
          }
          if (iSocketType == 0 || iSocketType == SOCK_STREAM) {
              svc = getservbyname(pszServiceName, "tcp");
              if (svc)
                port = tcpPort = svc->s_port;
              else {
                if(_stricmp(pszServiceName,"https")==0)
                  port = tcpPort = 443;
                else if(_stricmp(pszServiceName,"http")==0)
                  port = tcpPort = 80;
              }
          }
          if (port == 0)
            return (iSocketType ? EAI_SERVICE : EAI_NONAME);
          if (iSocketType==0) {
              iSocketType = (tcpPort) ? SOCK_STREAM : SOCK_DGRAM;
              isCloned = (tcpPort && udpPort);
          }
      }
  }
  if (!pszNodeName || WspiapiParseV4Address(pszNodeName,&inAddress.s_addr)) {
      if (!pszNodeName) {
            inAddress.s_addr = htonl ((iFlags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK);
      }
      *pptResult = WspiapiNewAddrInfo(iSocketType, iProtocol, port, inAddress.s_addr);
      if (!(*pptResult))
          err = EAI_MEMORY;
      if (!err && pszNodeName)  {
          (*pptResult)->ai_flags |= AI_NUMERICHOST;
          if (iFlags & AI_CANONNAME) {
              (*pptResult)->ai_canonname = WspiapiStrdup (inet_ntoa (inAddress));
              if (!(*pptResult)->ai_canonname)
                err = EAI_MEMORY;
          }
      }
  } else if (iFlags & AI_NUMERICHOST)
      err = EAI_NONAME;
  else
    err = WspiapiLookupNode (pszNodeName, iSocketType, iProtocol, port,
                               (iFlags & AI_CANONNAME), pptResult);
  if (!err && isCloned)
    err = WspiapiClone(udpPort, *pptResult);
  if (err) {
        WspiapiLegacyFreeAddrInfo (*pptResult);
        *pptResult = NULL;
  }
  return err;
}

int WINAPI WspiapiLegacyGetNameInfo(const struct sockaddr *ptSocketAddress,
  socklen_t tSocketLength, char *pszNodeName, size_t tNodeLength,
  char *pszServiceName, size_t tServiceLength, int iFlags)
{
  struct servent *ptService;
  WORD wPort;
  char szBuffer[] = "65535";
  char *pszService = szBuffer;
  struct hostent *ptHost;
  struct in_addr tAddress;
  char *pszNode = NULL;
  char *pc = NULL;

  if ((!ptSocketAddress) || (tSocketLength < (int)sizeof(struct sockaddr))) return EAI_FAIL;
  if (ptSocketAddress->sa_family != AF_INET) return EAI_FAMILY;
  if (tSocketLength < (int)sizeof(struct sockaddr_in)) return EAI_FAIL;
  if (!(pszNodeName && tNodeLength) && !(pszServiceName && tServiceLength)) {
    return EAI_NONAME;
  }
  if ((iFlags & NI_NUMERICHOST) && (iFlags & NI_NAMEREQD)) {
    return EAI_BADFLAGS;
  }
  if (pszServiceName && tServiceLength) {
    wPort = ((struct sockaddr_in *) ptSocketAddress)->sin_port;
    if (iFlags & NI_NUMERICSERV) {
      _WSPIAPI_SPRINTF_S_1(szBuffer, _WSPIAPI_COUNTOF(szBuffer), "%u", ntohs(wPort));
    }
    else {
      ptService = getservbyport(wPort, (iFlags & NI_DGRAM) ? "udp" : NULL);
      if (ptService && ptService->s_name) {
        pszService = ptService->s_name;
      }
      else {
        _WSPIAPI_SPRINTF_S_1(szBuffer, _WSPIAPI_COUNTOF(szBuffer), "%u", ntohs(wPort));
      }
    }
    if (tServiceLength > strlen(pszService))
      _WSPIAPI_STRCPY_S(pszServiceName, tServiceLength, pszService);
    else return EAI_FAIL;
  }
  if (pszNodeName && tNodeLength) {
    tAddress = ((struct sockaddr_in *) ptSocketAddress)->sin_addr;
    if (iFlags & NI_NUMERICHOST) {
      pszNode = inet_ntoa(tAddress);
    }
    else {
      ptHost = gethostbyaddr((char *) &tAddress, sizeof(struct in_addr), AF_INET);
      if (ptHost && ptHost->h_name) {
        pszNode = ptHost->h_name;
        if ((iFlags & NI_NOFQDN) && ((pc = strchr(pszNode, '.')) != NULL)) *pc = '\0';
      }
      else {
        if (iFlags & NI_NAMEREQD) {
          switch (WSAGetLastError()) {
            case WSAHOST_NOT_FOUND: return EAI_NONAME;
            case WSATRY_AGAIN: return EAI_AGAIN;
            case WSANO_RECOVERY: return EAI_FAIL;
            default: return EAI_NONAME;
          }
        }
        else pszNode = inet_ntoa(tAddress);
      }
    }
    if (tNodeLength > strlen(pszNode)) _WSPIAPI_STRCPY_S(pszNodeName, tNodeLength, pszNode);
    else return EAI_FAIL;
  }

  return 0;
}

char *inet_ntop(int af, const void *src, char *dst, size_t cnt)
{
	if (af == AF_INET)
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in),
					dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	else if (af == AF_INET6)
	{
		struct sockaddr_in6 in;
		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6),
					dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	return NULL;
}
