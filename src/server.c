/*
 * AImemory - HTTP Server v0.2
 * Added: /intent, /extract, updated /verify with ask-don't-guess
 * Author: Jiayi Sun (SY115)
 */

#include "aimemory.h"
#include <ctype.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <signal.h>
#endif

#define PORT 7890
#define BUFFER_SIZE 65536
#define BASE_DIR "."

static volatile int running = 1;

#ifndef _WIN32
static void handle_signal(int sig) { (void)sig; running = 0; }
#endif

static void url_decode(char *dst, const char *src, int dst_size) {
    int i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') { dst[j++] = ' '; i++; }
        else { dst[j++] = src[i++]; }
    }
    dst[j] = '\0';
}

static int json_get_string(const char *json, const char *key, char *out, int out_size) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *pos = strstr(json, pattern);
    if (!pos) return -1;
    pos += strlen(pattern);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t')) pos++;
    if (*pos != '"') return -1;
    pos++;
    int i = 0;
    while (*pos && *pos != '"' && i < out_size - 1) {
        if (*pos == '\\' && *(pos+1)) {
            pos++;
            switch(*pos) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                default: out[i++] = *pos; break;
            }
        } else { out[i++] = *pos; }
        pos++;
    }
    out[i] = '\0';
    return 0;
}

static void json_escape(char *dst, const char *src, int dst_size) {
    int j = 0;
    for (int i = 0; src[i] && j < dst_size - 2; i++) {
        if (src[i] == '"') { dst[j++] = '\\'; dst[j++] = '"'; }
        else if (src[i] == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (src[i] == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
        else { dst[j++] = src[i]; }
    }
    dst[j] = '\0';
}

static void send_response(int client, int status, const char *body) {
    char header[512];
    int body_len = body ? strlen(body) : 0;
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        status, status == 200 ? "OK" : "Error", body_len);
    send(client, header, strlen(header), 0);
    if (body) send(client, body, body_len, 0);
}

static void handle_request(int client) {
    char buf[BUFFER_SIZE];
    int n = recv(client, buf, BUFFER_SIZE - 1, 0);
    if (n <= 0) { close(client); return; }
    buf[n] = '\0';
    
    char method[16], path[512];
    sscanf(buf, "%15s %511s", method, path);
    
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client, 200, "");
        close(client); return;
    }
    
    char *body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;
    
    char response[BUFFER_SIZE];
    
    /* GET /health */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        send_response(client, 200, "{\"status\":\"ok\",\"engine\":\"aimemory\",\"version\":\"0.2\"}");
    }
    
    /* GET /stats */
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/stats") == 0) {
        int total = 0, len = 0;
        len += snprintf(response + len, sizeof(response) - len, "{\"layers\":{");
        for (int i = 0; i < LAYER_COUNT; i++) {
            SearchResults lr = mem_list_layer(BASE_DIR, i);
            if (i > 0) len += snprintf(response + len, sizeof(response) - len, ",");
            len += snprintf(response + len, sizeof(response) - len, "\"%s\":%d", LAYER_NAMES[i], lr.count);
            total += lr.count;
        }
        snprintf(response + len, sizeof(response) - len, "},\"total\":%d}", total);
        send_response(client, 200, response);
    }
    
    /* GET /search?q= */
    else if (strcmp(method, "GET") == 0 && strncmp(path, "/search?q=", 10) == 0) {
        char query[MAX_KEY_LEN];
        url_decode(query, path + 10, MAX_KEY_LEN);
        SearchResults results = mem_search(BASE_DIR, query);
        
        int len = 0;
        len += snprintf(response + len, sizeof(response) - len, "{\"count\":%d,\"results\":[", results.count);
        for (int i = 0; i < results.count && len < BUFFER_SIZE - 1024; i++) {
            char ek[MAX_KEY_LEN*2], ev[MAX_VALUE_LEN*2];
            json_escape(ek, results.results[i].entry.key, sizeof(ek));
            json_escape(ev, results.results[i].entry.value, sizeof(ev));
            if (i > 0) len += snprintf(response + len, sizeof(response) - len, ",");
            len += snprintf(response + len, sizeof(response) - len,
                "{\"key\":\"%s\",\"value\":\"%s\",\"layer\":\"%s\",\"score\":%d,\"confidence\":%d}",
                ek, ev, LAYER_NAMES[results.results[i].entry.layer],
                results.results[i].score, results.results[i].entry.confidence);
        }
        snprintf(response + len, sizeof(response) - len, "]}");
        send_response(client, 200, response);
        printf("[search] q=\"%s\" -> %d results\n", query, results.count);
    }
    
    /* POST /store */
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/store") == 0) {
        if (!body) { send_response(client, 400, "{\"error\":\"no body\"}"); }
        else {
            MemEntry e;
            memset(&e, 0, sizeof(e));
            char layer_str[32];
            json_get_string(body, "layer", layer_str, sizeof(layer_str));
            json_get_string(body, "key", e.key, MAX_KEY_LEN);
            json_get_string(body, "value", e.value, MAX_VALUE_LEN);
            char conf_str[16];
            if (json_get_string(body, "confidence", conf_str, sizeof(conf_str)) == 0)
                e.confidence = atoi(conf_str);
            if (e.confidence <= 0) e.confidence = 80;
            e.layer = -1;
            for (int i = 0; i < LAYER_COUNT; i++)
                if (strcmp(layer_str, LAYER_NAMES[i]) == 0) { e.layer = i; break; }
            if (e.layer < 0 || !e.key[0])
                send_response(client, 400, "{\"error\":\"invalid\"}");
            else {
                mem_store(BASE_DIR, &e);
                send_response(client, 200, "{\"status\":\"stored\"}");
            }
        }
    }
    
    /* POST /intent */
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/intent") == 0) {
        if (!body) { send_response(client, 400, "{\"error\":\"no body\"}"); }
        else {
            char message[MAX_VALUE_LEN];
            json_get_string(body, "message", message, MAX_VALUE_LEN);
            IntentResult ir = mem_detect_intent(message);
            char et[MAX_KEY_LEN*2];
            json_escape(et, ir.topic, sizeof(et));
            snprintf(response, sizeof(response),
                "{\"intent\":\"%s\",\"confidence\":%d,\"topic\":\"%s\",\"lang\":\"%s\"}",
                INTENT_NAMES[ir.type], ir.confidence, et, ir.lang);
            send_response(client, 200, response);
            printf("[intent] type=%s conf=%d topic=\"%s\" lang=%s\n",
                   INTENT_NAMES[ir.type], ir.confidence, ir.topic, ir.lang);
        }
    }
    
    /* POST /verify (updated with ask-don't-guess) */
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/verify") == 0) {
        if (!body) { send_response(client, 400, "{\"error\":\"no body\"}"); }
        else {
            char user_msg[MAX_VALUE_LEN], ai_resp[MAX_VALUE_LEN];
            json_get_string(body, "user_message", user_msg, MAX_VALUE_LEN);
            json_get_string(body, "ai_response", ai_resp, MAX_VALUE_LEN);
            
            VerifyResult vr = mem_verify(BASE_DIR, user_msg, ai_resp);
            
            int len = 0;
            len += snprintf(response + len, sizeof(response) - len,
                "{\"action\":\"%s\",\"hallucination_risk\":%d,"
                "\"context_found\":%d,\"corrections_found\":%d,"
                "\"rules_matched\":%d,\"self_warnings\":%d,",
                ACTION_NAMES[vr.action], vr.hallucination_risk,
                vr.context_found, vr.corrections_found,
                vr.rules_matched, vr.self_warnings);
            
            /* Warnings */
            len += snprintf(response + len, sizeof(response) - len, "\"warnings\":[");
            for (int i = 0; i < vr.warning_count; i++) {
                char ew[MAX_VALUE_LEN*2];
                json_escape(ew, vr.warnings[i], sizeof(ew));
                if (i > 0) len += snprintf(response + len, sizeof(response) - len, ",");
                len += snprintf(response + len, sizeof(response) - len, "\"%s\"", ew);
            }
            
            /* Questions (ask-don't-guess) */
            len += snprintf(response + len, sizeof(response) - len, "],\"questions\":[");
            for (int i = 0; i < vr.question_count; i++) {
                char eq[MAX_VALUE_LEN*2];
                json_escape(eq, vr.questions[i], sizeof(eq));
                if (i > 0) len += snprintf(response + len, sizeof(response) - len, ",");
                len += snprintf(response + len, sizeof(response) - len, "\"%s\"", eq);
            }
            snprintf(response + len, sizeof(response) - len, "]}");
            
            send_response(client, 200, response);
            printf("[verify] action=%s risk=%d%% questions=%d\n",
                   ACTION_NAMES[vr.action], vr.hallucination_risk, vr.question_count);
        }
    }
    
    /* POST /extract (auto-learn) */
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/extract") == 0) {
        if (!body) { send_response(client, 400, "{\"error\":\"no body\"}"); }
        else {
            char user_msg[MAX_VALUE_LEN], ai_resp[MAX_VALUE_LEN];
            json_get_string(body, "user_message", user_msg, MAX_VALUE_LEN);
            json_get_string(body, "ai_response", ai_resp, MAX_VALUE_LEN);
            int stored = mem_auto_learn(BASE_DIR, user_msg, ai_resp);
            snprintf(response, sizeof(response), "{\"extracted\":%d}", stored);
            send_response(client, 200, response);
        }
    }
    
    else {
        send_response(client, 404, "{\"error\":\"not found\"}");
    }
    
    close(client);
}

int main(void) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SetConsoleOutputCP(65001);  /* UTF-8 console output */
#else
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
#endif
    
    mem_init(BASE_DIR);
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    
    listen(server_fd, 10);
    
    printf("\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║  AImemory Server v0.2                    ║\n");
    printf("  ║  http://127.0.0.1:%d                   ║\n", PORT);
    printf("  ║  by Jiayi Sun (SY115)                    ║\n");
    printf("  ║  Ctrl+C to stop                          ║\n");
    printf("  ╚══════════════════════════════════════════╝\n\n");
    printf("  API Endpoints:\n");
    printf("    GET  /health          Health check\n");
    printf("    GET  /stats           Memory statistics\n");
    printf("    GET  /search?q=...    Search all layers\n");
    printf("    POST /store           Store memory entry\n");
    printf("    POST /intent          Detect user intent\n");
    printf("    POST /verify          Verify + ask-don't-guess\n");
    printf("    POST /extract         Auto-learn from conversation\n\n");
    
    while (running) {
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int c = accept(server_fd, (struct sockaddr*)&ca, &cl);
        if (c < 0) continue;
        handle_request(c);
    }
    
    close(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    printf("\nServer stopped.\n");
    return 0;
}
