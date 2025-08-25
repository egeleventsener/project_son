#include "ft_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define CLOSESOCK closesocket
  static int _wsa_init_once(void){ static int inited=0; if(!inited){ WSADATA w; if(WSAStartup(MAKEWORD(2,2), &w)!=0) return -1; inited=1; } return 0; }
#else
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #define CLOSESOCK close
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int send_all(int s, const void *b, size_t n){
    const char *p=(const char*)b; size_t off=0;
    while(off<n){ 
#ifdef _WIN32
        int r = send(s, p+off, (int)(n-off), 0);
#else
        ssize_t r = send(s, p+off, n-off, 0);
#endif
        if(r<=0) return -1; off += (size_t)r;
    }
    return 0;
}

int ft_connect(const char* host, int port){
#ifdef _WIN32
    if (_wsa_init_once()<0) return -1;
#endif
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((unsigned short)port);
#ifdef _WIN32
    a.sin_addr.s_addr = inet_addr(host);
    if (a.sin_addr.s_addr == INADDR_NONE){
        struct addrinfo hints={0}, *res=0;
        hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
        char pbuf[16]; _snprintf(pbuf, 16, "%d", port);
        if (getaddrinfo(host, pbuf, &hints, &res)==0){
            memcpy(&a, res->ai_addr, res->ai_addrlen);
            freeaddrinfo(res);
        }
    }
#else
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1){ CLOSESOCK(s); return -1; }
#endif
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0){ CLOSESOCK(s); return -1; }
    return s;
}

void ft_close(int s){ if (s>=0) CLOSESOCK(s); }

int ft_send_line(int s, const char* line){
    if (!line) return -1;
    size_t n = strlen(line);
    if (send_all(s, line, n) < 0) return -1;
    return send_all(s, "\n", 1);
}

int ft_recv_line(int s, char* buf, size_t n){
    if (!buf || n==0) return -1;
    size_t u=0;
    for(;;){
#ifdef _WIN32
        char ch; int r = recv(s, &ch, 1, 0);
#else
        char ch; ssize_t r = recv(s, &ch, 1, 0);
#endif
        if (r<=0) return -1;
        if (ch=='\n'){ buf[u]='\0'; return (int)u; }
        if (ch!='\r' && u+1<n){ buf[u++]=ch; }
        if (u+1>=n){ buf[n-1]='\0'; return (int)u; }
    }
}

const char* ft_basename(const char* p){
    if (!p) return "";
    const char* s = strrchr(p, '/');
#ifdef _WIN32
    const char* b = strrchr(p, '\\');
    if (b && (!s || b>s)) s=b;
#endif
    return s ? s+1 : p;
}

// --- commands ---

int ft_spwd(int s, char* out, size_t n){
    if (ft_send_line(s, "spwd")<0) return -1;
    return ft_recv_line(s, out, n) < 0 ? -1 : 0;
}

int ft_scd(int s, const char* path){
    char line[PATH_MAX+8]; snprintf(line, sizeof(line), "scd %s", path?path:"");
    if (ft_send_line(s, line)<0) return -1;
    char resp[256]; if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
    return strcmp(resp, "Directory changed")==0 ? 0 : -1;
}

int ft_smkdir(int s, const char* name){
    char line[PATH_MAX+8]; snprintf(line, sizeof(line), "smkdir %s", name?name:"");
    if (ft_send_line(s, line)<0) return -1;
    char resp[256]; if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
    return strcmp(resp, "Directory created")==0 ? 0 : -1;
}

int ft_srename(int s, const char* oldp, const char* newp){
    char line[PATH_MAX*2+16]; snprintf(line, sizeof(line), "srename %s %s", oldp?oldp:"", newp?newp:"");
    if (ft_send_line(s, line)<0) return -1;
    char resp[256]; if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
    return strcmp(resp, "Renamed")==0 ? 0 : -1;
}

int ft_srm(int s, const char* path){
    char line[PATH_MAX+8]; snprintf(line, sizeof(line), "srm %s", path?path:"");
    if (ft_send_line(s, line)<0) return -1;
    char resp[256]; if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
    return strcmp(resp, "Deleted")==0 ? 0 : -1;
}

// sls has no explicit terminator in server.c.
// Strategy: read lines with select() + short timeout; stop when timeout fires.
int ft_sls(int s, char*** names, int* count){
    if (!names || !count) return -1;
    *names = NULL; *count = 0;
    if (ft_send_line(s, "sls")<0) return -1;

    int cap = 64;
    char **arr = (char**)calloc((size_t)cap, sizeof(char*));
    if (!arr) return -1;

    for(;;){
        // 150ms idle timeout
#ifdef _WIN32
        fd_set rfds; FD_ZERO(&rfds); FD_SET((SOCKET)s, &rfds);
#else
        fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
#endif
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 150000;
        int sel = select(s+1, &rfds, NULL, NULL, &tv);
        if (sel <= 0) break; // timeout or error -> assume list finished

        char line[PATH_MAX];
        if (ft_recv_line(s, line, sizeof(line))<0) break;
        if (strcmp(line,"(empty)")==0){
            // empty dir sentinel produced by server for no entries
            break;
        }
        if (line[0]=='\0') break;
        if (*count >= cap){
            cap *= 2;
            char **tmp = (char**)realloc(arr, (size_t)cap*sizeof(char*));
            if (!tmp) break;
            arr = tmp;
        }
        arr[*count] = _strdup(line);
#ifndef _WIN32
        if (!arr[*count]) arr[*count] = strdup(line);
#endif
        (*count)++;
    }
    *names = arr;
    return 0;
}

void ft_sls_free(char** names, int count){
    if (!names) return;
    for(int i=0;i<count;i++) free(names[i]);
    free(names);
}

// --- upload ---
// Protocol from server.c:
//   write_file\n
//   <filename>\n
//   SIZE <bytes>\n
//   <raw bytes...>
//   server replies with one line: "OK" or error text

static long long file_size_of_path(const char* path){
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return -1;
    LARGE_INTEGER li; li.HighPart = fad.nFileSizeHigh; li.LowPart = fad.nFileSizeLow; 
    return (long long)li.QuadPart;
#else
    FILE* fp = fopen(path, "rb"); if (!fp) return -1;
    if (fseeko(fp, 0, SEEK_END)!=0){ fclose(fp); return -1; }
    off_t sz = ftello(fp);
    fclose(fp);
    return (long long)sz;
#endif
}

int ft_put_file(int s, const char* local_path, const char* remote_dir){
    if (!local_path) return -1;
    char fname[PATH_MAX];
    const char* base = ft_basename(local_path);
    snprintf(fname, sizeof(fname), "%s", base?base:"file");

    if (remote_dir && remote_dir[0] && strcmp(remote_dir, ".")!=0){
        char cdline[PATH_MAX+8];
        snprintf(cdline, sizeof(cdline), "scd %s", remote_dir);
        if (ft_send_line(s, cdline) < 0) return -1;
        char resp[256]; if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
        // ignore failure here; server will write into current dir
    }

    if (ft_send_line(s, "write_file")<0) return -1;
    if (ft_send_line(s, fname)<0) return -1;

    long long fsz = file_size_of_path(local_path);
    if (fsz < 0) return -1;
    char sizeln[64]; snprintf(sizeln, sizeof(sizeln), "SIZE %lld", fsz);
    if (ft_send_line(s, sizeln)<0) return -1;

    // stream file
    FILE* fp = fopen(local_path, "rb");
    if (!fp) return -1;
    char buf[8192]; size_t n;
    while ((n = fread(buf,1,sizeof(buf),fp)) > 0){
        if (send_all(s, buf, n) < 0){ fclose(fp); return -1; }
    }
    fclose(fp);

    // read server final line
    char resp[256];
    if (ft_recv_line(s, resp, sizeof(resp))<0) return -1;
    return (strcmp(resp, "OK")==0) ? 0 : -1;
}
