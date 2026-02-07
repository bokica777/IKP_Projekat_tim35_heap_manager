#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap_manager.h"
#include "hm_jobs.h"
#include "hm_results.h"
#include "hm_metrics.h"

#pragma comment(lib, "Ws2_32.lib")

int hm_pool_start(int workers);
void hm_pool_stop(void);

static uint32_t g_next_id = 1;

static hm_job_t* make_alloc_job(uint32_t id, int size)
{
    hm_job_t* j = (hm_job_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(hm_job_t));
    if (!j) return NULL;
    j->type = HM_JOB_ALLOC;
    j->request_id = id;
    j->size = size;
    return j;
}

static hm_job_t* make_free_job(uint32_t id, void* addr)
{
    hm_job_t* j = (hm_job_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(hm_job_t));
    if (!j) return NULL;
    j->type = HM_JOB_FREE;
    j->request_id = id;
    j->address = addr;
    return j;
}

static int send_line(SOCKET s, const char* line)
{
    int n = (int)strlen(line);
    int sent = 0;
    while (sent < n) {
        int r = send(s, line + sent, n - sent, 0);
        if (r <= 0) return -1;
        sent += r;
    }
    return 0;
}

static char* ltrim(char* p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static void rtrim(char* p)
{
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == '\r' || p[n-1] == '\n' || p[n-1] == ' ' || p[n-1] == '\t')) {
        p[n-1] = 0;
        n--;
    }
}

static void handle_command(SOCKET client, char* line)
{
    rtrim(line);
    char* p = ltrim(line);
    if (*p == 0) return;

    char cmd[32] = {0};
    if (sscanf(p, "%31s", cmd) != 1) return;

    if (_stricmp(cmd, "ALLOC") == 0)
    {
        int size = 0;
        if (sscanf(p + 5, "%d", &size) != 1 || size <= 0) {
            send_line(client, "ERR bad_alloc\n");
            return;
        }

        uint32_t id = g_next_id++;
        hm_job_t* job = make_alloc_job(id, size);
        if (!job) { send_line(client, "ERR oom\n"); return; }

        if (hm_results_put_pending(id, job) != 0) { HeapFree(GetProcessHeap(), 0, job); send_line(client, "ERR map_full\n"); return; }
        if (hm_jobs_push(job) != 0) { hm_results_remove(id); HeapFree(GetProcessHeap(), 0, job); send_line(client, "ERR queue_full\n"); return; }

        char out[64];
        sprintf(out, "OK %u\n", (unsigned)id);
        send_line(client, out);
        return;
    }

    if (_stricmp(cmd, "FREE") == 0)
    {
        char addr_str[64] = {0};
        if (sscanf(p + 4, "%63s", addr_str) != 1) {
            send_line(client, "ERR bad_free\n");
            return;
        }

        unsigned long long v = _strtoui64(addr_str, NULL, 0);
        void* addr = (void*)(uintptr_t)v;

        uint32_t id = g_next_id++;
        hm_job_t* job = make_free_job(id, addr);
        if (!job) { send_line(client, "ERR oom\n"); return; }

        if (hm_results_put_pending(id, job) != 0) { HeapFree(GetProcessHeap(), 0, job); send_line(client, "ERR map_full\n"); return; }
        if (hm_jobs_push(job) != 0) { hm_results_remove(id); HeapFree(GetProcessHeap(), 0, job); send_line(client, "ERR queue_full\n"); return; }

        char out[64];
        sprintf(out, "OK %u\n", (unsigned)id);
        send_line(client, out);
        return;
    }

    if (_stricmp(cmd, "POLL") == 0)
    {
        unsigned int idu = 0;
        if (sscanf(p + 4, "%u", &idu) != 1 || idu == 0) {
            send_line(client, "ERR bad_poll\n");
            return;
        }
        uint32_t id = (uint32_t)idu;

        hm_res_state_t st;
        void* res = NULL;
        void* jobp = NULL;

        if (hm_results_get(id, &st, &res, &jobp) != 0) {
            send_line(client, "ERR unknown_id\n");
            return;
        }

        if (st == HM_RES_PENDING) {
            send_line(client, "PENDING\n");
            return;
        }

        if (st == HM_RES_DONE) {
            char out[128];
            sprintf(out, "DONE %p\n", res);
            send_line(client, out);

            if (jobp) HeapFree(GetProcessHeap(), 0, jobp);
            hm_results_remove(id);
            return;
        }

        send_line(client, "ERR bad_state\n");
        return;
    }

    if (_stricmp(cmd, "STATS") == 0)
    {
        hm_metrics_t m = hm_get_metrics();
        char out[256];
        sprintf(out, "alloc=%llu free=%llu frag=%.4f total_free=%zu largest_free=%zu\n",
            (unsigned long long)m.alloc_count,
            (unsigned long long)m.free_count,
            m.external_fragmentation,
            m.total_free_bytes,
            m.largest_free_block
        );
        send_line(client, out);
        return;
    }

    if (_stricmp(cmd, "QUIT") == 0)
    {
        send_line(client, "BYE\n");
        return;
    }

    send_line(client, "ERR unknown_cmd\n");
}

int main(void)
{
    hm_config_t cfg = hm_default_config();
    cfg.segment_size = 256 * 1024;
    cfg.max_free_segments = 5;
    cfg.enable_thread_safety = 1;

    if (hm_init(cfg) != 0) { printf("hm_init FAILED\n"); return 1; }
    if (hm_jobs_init() != 0) { printf("hm_jobs_init FAILED\n"); return 1; }
    if (hm_results_init() != 0) { printf("hm_results_init FAILED\n"); return 1; }
    if (hm_pool_start(4) != 0) { printf("hm_pool_start FAILED\n"); return 1; }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { printf("WSAStartup FAILED\n"); return 1; }

    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { printf("socket FAILED\n"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) != 0) { printf("bind FAILED\n"); return 1; }
    if (listen(ls, 1) != 0) { printf("listen FAILED\n"); return 1; }

    printf("SERVER listening on 127.0.0.1:5555\n");
    printf("Waiting for one client...\n");

    SOCKET cs = accept(ls, NULL, NULL);
    if (cs == INVALID_SOCKET) { printf("accept FAILED\n"); return 1; }

    printf("Client connected.\n");

    char inbuf[4096];
    int inlen = 0;

    for (;;)
    {
        int r = recv(cs, inbuf + inlen, (int)sizeof(inbuf) - 1 - inlen, 0);
        if (r <= 0) break;
        inlen += r;
        inbuf[inlen] = 0;

        char* line_start = inbuf;
        for (;;)
        {
            char* nl = strchr(line_start, '\n');
            if (!nl) break;
            *nl = 0;

            char line[1024];
            strncpy(line, line_start, sizeof(line) - 1);
            line[sizeof(line) - 1] = 0;

            handle_command(cs, line);

            if (_stricmp(ltrim(line), "QUIT") == 0) {
                closesocket(cs);
                closesocket(ls);
                WSACleanup();
                hm_pool_stop();
                hm_results_shutdown();
                hm_jobs_shutdown();
                hm_shutdown();
                printf("Server stopped.\n");
                return 0;
            }

            line_start = nl + 1;
        }

        int remaining = (int)strlen(line_start);
        memmove(inbuf, line_start, remaining);
        inlen = remaining;
    }

    closesocket(cs);
    closesocket(ls);
    WSACleanup();

    hm_pool_stop();
    hm_results_shutdown();
    hm_jobs_shutdown();
    hm_shutdown();

    printf("Server stopped.\n");
    return 0;
}
