#ifndef CGI_HANDLERS_H
#define CGI_HANDLERS_H

// Forward declaration for tCGI struct, as it's part of lwIP httpd
struct tCGI;

// CGI Handler function signature
typedef const char* (*tCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

// Declarations for our CGI handlers
const char* cgi_handler_status(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* cgi_handler_pokemon_list(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* cgi_handler_trade_start(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

// Initialization function to register CGI handlers with the HTTPD server
void cgi_init(void);

#endif // CGI_HANDLERS_H
