#ifndef FT_CORE_H
#define FT_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Connect and basic I/O
int  ft_connect(const char* host, int port);     // returns socket fd or -1
void ft_close(int s);

// Simple request/response lines
int  ft_send_line(int s, const char* line);      // sends "line\n"
int  ft_recv_line(int s, char* buf, size_t n);   // reads a line without CRLF

// Server commands implemented in current server.c
int  ft_spwd(int s, char* out, size_t n);        // "spwd" -> path
int  ft_scd(int s, const char* path);            // "scd <path>"
int  ft_sls(int s, char*** names, int* count);   // "sls" -> list; uses short read-timeout
void ft_sls_free(char** names, int count);

int  ft_smkdir(int s, const char* name);         // "smkdir <name>"
int  ft_srename(int s, const char* oldp, const char* newp); // "srename <old> <new>"
int  ft_srm(int s, const char* path);            // "srm <path>" (DANGEROUS)

// File transfer (upload to server). Matches server "write_file".
int  ft_put_file(int s, const char* local_path, const char* remote_dir);

// Helper
const char* ft_basename(const char* p);

#ifdef __cplusplus
}
#endif
#endif
