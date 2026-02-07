#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

static int send_all(SOCKET s, const char* buf, int n)
{
    int sent = 0;
    while (sent < n) {
        int r = send(s, buf + sent, n - sent, 0);
        if (r <= 0) return -1;
        sent += r;
    }
    return 0;
}

static int recv_line(SOCKET s, char* out, int cap)
{
    int len = 0;
    while (len < cap - 1) {
        char c;
        int r = recv(s, &c, 1, 0);
        if (r <= 0) return -1;
        out[len++] = c;
        if (c == '\n') break;
    }
    out[len] = 0;
    return len;
}

static char* ltrim(char* p) { while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++; return p; }
static void rtrim(char* p) { size_t n = strlen(p); while (n && (p[n-1]=='\r'||p[n-1]=='\n'||p[n-1]==' '||p[n-1]=='\t')) p[--n]=0; }

#define IDMAP_CAP 4096
typedef struct { uint32_t id; void* ptr; int used; } idmap_entry_t;
static idmap_entry_t g_idmap[IDMAP_CAP];

static size_t hash_u32(uint32_t x) { return (size_t)(x * 2654435761u); }

static void idmap_set(uint32_t id, void* ptr)
{
    size_t start = hash_u32(id) & (IDMAP_CAP - 1);
    for (size_t k = 0; k < IDMAP_CAP; k++) {
        size_t i = (start + k) & (IDMAP_CAP - 1);
        if (!g_idmap[i].used || g_idmap[i].id == id) {
            g_idmap[i].used = 1;
            g_idmap[i].id = id;
            g_idmap[i].ptr = ptr;
            return;
        }
    }
}

static int idmap_get(uint32_t id, void** out_ptr)
{
    size_t start = hash_u32(id) & (IDMAP_CAP - 1);
    for (size_t k = 0; k < IDMAP_CAP; k++) {
        size_t i = (start + k) & (IDMAP_CAP - 1);
        if (!g_idmap[i].used) return -1;
        if (g_idmap[i].id == id) { *out_ptr = g_idmap[i].ptr; return 0; }
    }
    return -1;
}

static void idmap_remove(uint32_t id)
{
    size_t start = hash_u32(id) & (IDMAP_CAP - 1);
    for (size_t k = 0; k < IDMAP_CAP; k++) {
        size_t i = (start + k) & (IDMAP_CAP - 1);
        if (!g_idmap[i].used) return;
        if (g_idmap[i].id == id) { g_idmap[i].used = 0; g_idmap[i].id = 0; g_idmap[i].ptr = NULL; return; }
    }
}

static int cmd_alloc(SOCKET s, int size, uint32_t* out_id)
{
    char buf[128];
    sprintf(buf, "ALLOC %d\n", size);
    if (send_all(s, buf, (int)strlen(buf)) != 0) return -1;

    char resp[512];
    if (recv_line(s, resp, (int)sizeof(resp)) <= 0) return -1;

    printf("%s", resp);

    unsigned int idu = 0;
    if (sscanf(resp, "OK %u", &idu) == 1) {
        *out_id = (uint32_t)idu;
        return 0;
    }
    return -1;
}

static int cmd_poll(SOCKET s, uint32_t id, int* out_is_done, void** out_ptr)
{
    char buf[128];
    sprintf(buf, "POLL %u\n", (unsigned)id);
    if (send_all(s, buf, (int)strlen(buf)) != 0) return -1;

    char resp[512];
    if (recv_line(s, resp, (int)sizeof(resp)) <= 0) return -1;

    printf("%s", resp);

    if (strncmp(resp, "PENDING", 7) == 0) {
        *out_is_done = 0;
        *out_ptr = NULL;
        return 0;
    }

    if (strncmp(resp, "DONE", 4) == 0) {
        void* p = NULL;
        sscanf(resp + 4, "%p", &p);
        *out_is_done = 1;
        *out_ptr = p;
        return 0;
    }

    return -1;
}

static int cmd_wait(SOCKET s, uint32_t id, void** out_ptr)
{
    for (;;) {
        int done = 0;
        void* p = NULL;
        if (cmd_poll(s, id, &done, &p) != 0) return -1;
        if (done) { *out_ptr = p; return 0; }
        Sleep(1);
    }
}

static int cmd_free_ptr(SOCKET s, void* ptr, uint32_t* out_id)
{
    char buf[128];
    sprintf(buf, "FREE %p\n", ptr);
    if (send_all(s, buf, (int)strlen(buf)) != 0) return -1;

    char resp[512];
    if (recv_line(s, resp, (int)sizeof(resp)) <= 0) return -1;

    printf("%s", resp);

    unsigned int idu = 0;
    if (sscanf(resp, "OK %u", &idu) == 1) {
        *out_id = (uint32_t)idu;
        return 0;
    }
    return -1;
}

static int cmd_stats(SOCKET s)
{
    const char* q = "STATS\n";
    if (send_all(s, q, (int)strlen(q)) != 0) return -1;

    char resp[512];
    if (recv_line(s, resp, (int)sizeof(resp)) <= 0) return -1;
    printf("%s", resp);
    return 0;
}

static int cmd_quit(SOCKET s)
{
    const char* q = "QUIT\n";
    if (send_all(s, q, (int)strlen(q)) != 0) return -1;

    char resp[512];
    if (recv_line(s, resp, (int)sizeof(resp)) <= 0) return -1;
    printf("%s", resp);
    return 0;
}

static int run_script(SOCKET s, FILE* in)
{
    char line[256];
    uint32_t last_id = 0;

    while (fgets(line, sizeof(line), in)) {
        rtrim(line);
        char* p = ltrim(line);
        if (*p == 0) continue;
        if (*p == '#') continue;

        char cmd[32] = {0};
        if (sscanf(p, "%31s", cmd) != 1) continue;

        if (_stricmp(cmd, "ALLOC") == 0) {
            int sz = 0;
            if (sscanf(p + 5, "%d", &sz) != 1) return -1;
            if (cmd_alloc(s, sz, &last_id) != 0) return -1;
            continue;
        }

        if (_stricmp(cmd, "ALLOCWAIT") == 0) {
            int sz = 0;
            if (sscanf(p + 9, "%d", &sz) != 1) return -1;
            uint32_t id = 0;
            if (cmd_alloc(s, sz, &id) != 0) return -1;
            void* ptr = NULL;
            if (cmd_wait(s, id, &ptr) != 0) return -1;
            idmap_set(id, ptr);
            last_id = id;
            continue;
        }

        if (_stricmp(cmd, "POLL") == 0) {
            unsigned int idu = 0;
            if (sscanf(p + 4, "%u", &idu) != 1) return -1;
            int done = 0; void* ptr = NULL;
            if (cmd_poll(s, (uint32_t)idu, &done, &ptr) != 0) return -1;
            if (done) idmap_set((uint32_t)idu, ptr);
            continue;
        }

        if (_stricmp(cmd, "WAIT") == 0) {
            unsigned int idu = 0;
            if (sscanf(p + 4, "%u", &idu) != 1) return -1;
            void* ptr = NULL;
            if (cmd_wait(s, (uint32_t)idu, &ptr) != 0) return -1;
            idmap_set((uint32_t)idu, ptr);
            continue;
        }

        if (_stricmp(cmd, "FREEID") == 0) {
            unsigned int idu = 0;
            if (sscanf(p + 6, "%u", &idu) != 1) return -1;
            void* ptr = NULL;
            if (idmap_get((uint32_t)idu, &ptr) != 0 || !ptr) return -1;
            uint32_t fid = 0;
            if (cmd_free_ptr(s, ptr, &fid) != 0) return -1;
            void* z = NULL;
            if (cmd_wait(s, fid, &z) != 0) return -1;
            idmap_remove((uint32_t)idu);
            continue;
        }

        if (_stricmp(cmd, "FREEPTR") == 0) {
            char xs[64] = {0};
            if (sscanf(p + 7, "%63s", xs) != 1) return -1;
            unsigned long long v = _strtoui64(xs, NULL, 0);
            void* ptr = (void*)(uintptr_t)v;
            uint32_t fid = 0;
            if (cmd_free_ptr(s, ptr, &fid) != 0) return -1;
            void* z = NULL;
            if (cmd_wait(s, fid, &z) != 0) return -1;
            continue;
        }

        if (_stricmp(cmd, "STATS") == 0) {
            if (cmd_stats(s) != 0) return -1;
            continue;
        }

        if (_stricmp(cmd, "SLEEP") == 0) {
            int ms = 0;
            if (sscanf(p + 5, "%d", &ms) != 1) return -1;
            Sleep((DWORD)ms);
            continue;
        }

        if (_stricmp(cmd, "QUIT") == 0) {
            if (cmd_quit(s) != 0) return -1;
            return 0;
        }

        if (_stricmp(cmd, "LAST") == 0) {
            printf("LAST %u\n", (unsigned)last_id);
            continue;
        }

        return -1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("connect failed\n");
        return 1;
    }

    if (argc >= 2) {
        FILE* f = fopen(argv[1], "r");
        if (!f) { printf("cannot open script\n"); return 1; }
        int rc = run_script(s, f);
        fclose(f);
        closesocket(s);
        WSACleanup();
        return rc == 0 ? 0 : 1;
    }

    printf("Connected. Use: hm_client.exe tests\\demo_basic.txt\n");
    closesocket(s);
    WSACleanup();
    return 0;
}
