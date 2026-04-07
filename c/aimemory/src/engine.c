/*
 * AImemory - Core Engine v0.2
 * Added: Intent recognition, auto-learning, ask-don't-guess verification
 * Author: Jiayi Sun (SY115)
 */

#include "aimemory.h"
#include <ctype.h>
#include <errno.h>

/* Forward declaration */
static char to_lower(char c);

/* ============================================================
 * Cross-language synonym table (Chinese <-> English)
 * ============================================================ */
typedef struct {
    const char *zh;
    const char *en;
} LangPair;

static const LangPair SYNONYMS[] = {
    /* Security terms */
    {"\xe6\xbc\x8f\xe6\xb4\x9e",           "vulnerability"},    /* 漏洞 */
    {"\xe5\x86\x85\xe6\xa0\xb8",           "kernel"},           /* 内核 */
    {"\xe5\xb4\xa9\xe6\xba\x83",           "crash"},            /* 崩溃 */
    {"\xe8\x93\x9d\xe5\xb1\x8f",           "bsod"},             /* 蓝屏 */
    {"\xe6\x8f\x90\xe6\x9d\x83",           "privilege escalation"}, /* 提权 */
    {"\xe6\x8a\xa5\xe5\x91\x8a",           "report"},           /* 报告 */
    {"\xe6\x8f\x90\xe4\xba\xa4",           "submit"},           /* 提交 */
    {"\xe5\x8f\x91\xe9\x80\x81",           "send"},             /* 发送 */
    {"\xe5\x8f\x91\xe7\xbb\x99",           "send"},             /* 发给 */
    {"\xe5\x8f\x91",                        "send"},             /* 发 (single char) */
    {"\xe5\x88\xa0\xe9\x99\xa4",           "delete"},           /* 删除 */
    {"\xe4\xb8\x8b\xe8\xbd\xbd",           "download"},         /* 下载 */
    {"\xe4\xb8\x8a\xe4\xbc\xa0",           "upload"},           /* 上传 */
    {"\xe5\x88\x86\xe6\x9e\x90",           "analyze"},          /* 分析 */
    {"\xe6\x94\xbb\xe5\x87\xbb",           "attack"},           /* 攻击 */
    {"\xe9\x98\xb2\xe5\xbe\xa1",           "defense"},          /* 防御 */
    {"\xe5\xb7\xa5\xe5\x85\xb7",           "tool"},             /* 工具 */
    {"\xe4\xbb\xa3\xe7\xa0\x81",           "code"},             /* 代码 */
    {"\xe5\xae\x89\xe5\x85\xa8",           "security"},         /* 安全 */
    {"\xe9\xa9\xb1\xe5\x8a\xa8",           "driver"},           /* 驱动 */
    {"\xe6\x96\x87\xe4\xbb\xb6",           "file"},             /* 文件 */
    {"\xe7\xb3\xbb\xe7\xbb\x9f",           "system"},           /* 系统 */
    {"\xe5\x86\x85\xe5\xad\x98",           "memory"},           /* 内存 */
    {"\xe5\xb9\xbb\xe8\xa7\x89",           "hallucination"},    /* 幻觉 */
    {"\xe9\x94\x99\xe8\xaf\xaf",           "error"},            /* 错误 */
    {"\xe4\xbf\xae\xe5\xa4\x8d",           "fix"},              /* 修复 */
    {"\xe6\xb5\x8b\xe8\xaf\x95",           "test"},             /* 测试 */
    {"\xe7\x94\xa8\xe6\x88\xb7",           "user"},             /* 用户 */
    {"\xe5\xaf\x86\xe7\xa0\x81",           "password"},         /* 密码 */
    {"\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8", "server"},         /* 服务器 */
    {"\xe6\x8f\x92\xe4\xbb\xb6",           "plugin"},           /* 插件 */
    {"\xe6\x90\x9c\xe7\xb4\xa2",           "search"},           /* 搜索 */
    {"\xe8\xae\xb0\xe5\xbf\x86",           "memory"},           /* 记忆 */
    {"\xe9\xaa\x8c\xe8\xaf\x81",           "verify"},           /* 验证 */
    /* Common abbreviations */
    {"poc",                                  "poc"},
    {"ssd",                                  "ssd"},
    {"cve",                                  "cve"},
    {"api",                                  "api"},
    {NULL, NULL}
};

#define MAX_EXPANDED 8

/* Expand query with cross-language synonyms */
static int expand_query(const char *query, char expanded[][MAX_KEY_LEN]) {
    int count = 0;
    
    /* Always include original */
    strncpy(expanded[count], query, MAX_KEY_LEN - 1);
    expanded[count][MAX_KEY_LEN - 1] = '\0';
    count++;
    
    /* Check each synonym pair */
    for (int i = 0; SYNONYMS[i].zh != NULL && count < MAX_EXPANDED; i++) {
        /* Chinese in query -> add English */
        if (strstr(query, SYNONYMS[i].zh)) {
            strncpy(expanded[count], SYNONYMS[i].en, MAX_KEY_LEN - 1);
            expanded[count][MAX_KEY_LEN - 1] = '\0';
            count++;
        }
        /* English in query -> add Chinese */
        char q_lower[MAX_KEY_LEN];
        int j;
        for (j = 0; query[j] && j < MAX_KEY_LEN - 1; j++)
            q_lower[j] = to_lower(query[j]);
        q_lower[j] = '\0';
        
        if (strstr(q_lower, SYNONYMS[i].en)) {
            strncpy(expanded[count], SYNONYMS[i].zh, MAX_KEY_LEN - 1);
            expanded[count][MAX_KEY_LEN - 1] = '\0';
            count++;
        }
    }
    
    return count;
}

/* ============================================================
 * Internal helpers
 * ============================================================ */

static void layer_path(char *out, const char *base_dir, MemoryLayer layer) {
    snprintf(out, MAX_PATH_LEN, "%s/%s/%s", base_dir, DATA_DIR, LAYER_NAMES[layer]);
}

static void entry_path(char *out, const char *base_dir, MemoryLayer layer, const char *key) {
    char safe_key[MAX_KEY_LEN];
    int j = 0;
    for (int i = 0; key[i] && j < MAX_KEY_LEN - 1; i++) {
        if (key[i] == '/' || key[i] == '\\' || key[i] == ' ')
            safe_key[j++] = '_';
        else
            safe_key[j++] = key[i];
    }
    safe_key[j] = '\0';
    snprintf(out, MAX_PATH_LEN, "%s/%s/%s/%s.mem", base_dir, DATA_DIR, LAYER_NAMES[layer], safe_key);
}

static int mkdirs(const char *path) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

/* UTF-8 aware lowercase for ASCII portion */
static char to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int fuzzy_match(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    
    int score = 0;
    int needle_len = strlen(needle);
    int haystack_len = strlen(haystack);
    
    char h_lower[MAX_VALUE_LEN], n_lower[MAX_KEY_LEN];
    int i;
    for (i = 0; i < haystack_len && i < MAX_VALUE_LEN - 1; i++)
        h_lower[i] = to_lower(haystack[i]);
    h_lower[i] = '\0';
    
    for (i = 0; i < needle_len && i < MAX_KEY_LEN - 1; i++)
        n_lower[i] = to_lower(needle[i]);
    n_lower[i] = '\0';
    
    if (strcmp(h_lower, n_lower) == 0) return 100;
    if (strstr(h_lower, n_lower)) {
        score = 70;
        if (strncmp(h_lower, n_lower, needle_len) == 0) score = 85;
        return score;
    }
    
    /* UTF-8 byte sequence match (works for Chinese etc) */
    if ((unsigned char)needle[0] > 127) {
        if (strstr(haystack, needle)) return 80;
    }
    
    /* Word-by-word match */
    char needle_copy[MAX_KEY_LEN];
    strncpy(needle_copy, n_lower, MAX_KEY_LEN - 1);
    needle_copy[MAX_KEY_LEN - 1] = '\0';
    
    int words_matched = 0, total_words = 0;
    char *word = strtok(needle_copy, " \t\n");
    while (word) {
        total_words++;
        if (strstr(h_lower, word)) words_matched++;
        word = strtok(NULL, " \t\n");
    }
    
    if (total_words > 0 && words_matched > 0)
        score = (words_matched * 50) / total_words;
    
    return score;
}

static int write_entry(const char *filepath, MemEntry *entry) {
    FILE *f = fopen(filepath, "w");
    if (!f) return -1;
    fprintf(f, "KEY:%s\nLAYER:%d\nTIME:%ld\nCONF:%d\nVALUE:%s\n",
            entry->key, entry->layer, entry->timestamp, entry->confidence, entry->value);
    fclose(f);
    return 0;
}

static int read_entry(const char *filepath, MemEntry *entry) {
    FILE *f = fopen(filepath, "r");
    if (!f) return -1;
    
    char line[MAX_VALUE_LEN + 64];
    memset(entry, 0, sizeof(MemEntry));
    
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strncmp(line, "KEY:", 4) == 0)
            strncpy(entry->key, line + 4, MAX_KEY_LEN - 1);
        else if (strncmp(line, "LAYER:", 6) == 0)
            entry->layer = atoi(line + 6);
        else if (strncmp(line, "TIME:", 5) == 0)
            entry->timestamp = atol(line + 5);
        else if (strncmp(line, "CONF:", 5) == 0)
            entry->confidence = atoi(line + 5);
        else if (strncmp(line, "VALUE:", 6) == 0) {
            strncpy(entry->value, line + 6, MAX_VALUE_LEN - 1);
            int vlen = strlen(entry->value);
            while (fgets(line, sizeof(line), f) && vlen < MAX_VALUE_LEN - 2) {
                int llen = strlen(line);
                if (vlen + llen < MAX_VALUE_LEN - 1) {
                    entry->value[vlen] = '\n';
                    memcpy(entry->value + vlen + 1, line, llen);
                    vlen += llen + 1;
                    if (entry->value[vlen-1] == '\n') vlen--;
                }
            }
            entry->value[vlen] = '\0';
        }
    }
    fclose(f);
    return (entry->key[0] != '\0') ? 0 : -1;
}

static void insert_result(SearchResults *results, MemEntry *entry, int score) {
    if (score <= 0) return;
    int pos = results->count;
    for (int i = 0; i < results->count; i++) {
        if (score > results->results[i].score) { pos = i; break; }
    }
    if (pos >= MAX_RESULTS) return;
    if (results->count < MAX_RESULTS) {
        for (int i = results->count; i > pos; i--)
            results->results[i] = results->results[i-1];
        results->count++;
    } else {
        for (int i = MAX_RESULTS - 1; i > pos; i--)
            results->results[i] = results->results[i-1];
    }
    results->results[pos].entry = *entry;
    results->results[pos].score = score;
}

/* Check if text contains any of the given keywords */
static int contains_any(const char *text, const char **keywords, int count) {
    for (int i = 0; i < count; i++) {
        if (strstr(text, keywords[i])) return 1;
    }
    return 0;
}

/* Detect language (simple: check for CJK bytes) */
static void detect_lang(const char *text, char *lang) {
    for (int i = 0; text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        /* CJK Unified Ideographs: UTF-8 starts with 0xE4-0xE9 */
        if (c >= 0xE4 && c <= 0xE9) {
            strcpy(lang, "zh");
            return;
        }
        /* Japanese Hiragana/Katakana: 0xE3 */
        if (c == 0xE3) {
            strcpy(lang, "ja");
            return;
        }
        /* Korean: 0xEA-0xED */
        if (c >= 0xEA && c <= 0xED) {
            strcpy(lang, "ko");
            return;
        }
    }
    strcpy(lang, "en");
}

/* ============================================================
 * Public API - Core Memory
 * ============================================================ */

int mem_init(const char *base_dir) {
    char path[MAX_PATH_LEN];
    printf("[AImemory] Initializing memory system...\n");
    for (int i = 0; i < LAYER_COUNT; i++) {
        layer_path(path, base_dir, i);
        mkdirs(path);
        printf("  [+] Layer: %-12s -> %s\n", LAYER_NAMES[i], path);
    }
    printf("[AImemory] Ready. %d layers initialized.\n", LAYER_COUNT);
    return 0;
}

int mem_store(const char *base_dir, MemEntry *entry) {
    char path[MAX_PATH_LEN], dir[MAX_PATH_LEN];
    if (!entry->key[0]) return -1;
    if (entry->timestamp == 0) entry->timestamp = time(NULL);
    if (entry->confidence == 0) entry->confidence = 50;
    layer_path(dir, base_dir, entry->layer);
    mkdirs(dir);
    entry_path(path, base_dir, entry->layer, entry->key);
    if (write_entry(path, entry) != 0) return -1;
    printf("[+] Stored [%s] \"%s\" (confidence: %d%%)\n",
           LAYER_NAMES[entry->layer], entry->key, entry->confidence);
    return 0;
}

SearchResults mem_search(const char *base_dir, const char *query) {
    SearchResults results;
    memset(&results, 0, sizeof(results));
    
    /* Expand query with cross-language synonyms */
    char expanded[MAX_EXPANDED][MAX_KEY_LEN];
    int exp_count = expand_query(query, expanded);
    
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        /* Search with all expanded terms */
        for (int e = 0; e < exp_count; e++) {
            SearchResults lr = mem_search_layer(base_dir, expanded[e], layer);
            for (int i = 0; i < lr.count; i++)
                insert_result(&results, &lr.results[i].entry, lr.results[i].score);
        }
    }
    return results;
}

SearchResults mem_search_layer(const char *base_dir, const char *query, MemoryLayer layer) {
    SearchResults results;
    memset(&results, 0, sizeof(results));
    char dir[MAX_PATH_LEN];
    layer_path(dir, base_dir, layer);
    
#ifdef _WIN32
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, MAX_PATH_LEN, "%s\\*.mem", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return results;
    do {
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, MAX_PATH_LEN, "%s\\%s", dir, fd.cFileName);
        MemEntry entry;
        if (read_entry(filepath, &entry) != 0) continue;
        int ks = fuzzy_match(entry.key, query);
        int vs = fuzzy_match(entry.value, query);
        int score = ks > vs ? ks + 10 : vs;
        score = (score * entry.confidence) / 100;
        if (score > 0) insert_result(&results, &entry, score);
    } while (FindNextFile(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return results;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        int len = strlen(de->d_name);
        if (len < 5 || strcmp(de->d_name + len - 4, ".mem") != 0) continue;
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, MAX_PATH_LEN, "%s/%s", dir, de->d_name);
        MemEntry entry;
        if (read_entry(filepath, &entry) != 0) continue;
        int ks = fuzzy_match(entry.key, query);
        int vs = fuzzy_match(entry.value, query);
        int score = ks > vs ? ks + 10 : vs;
        score = (score * entry.confidence) / 100;
        if (score > 0) insert_result(&results, &entry, score);
    }
    closedir(d);
#endif
    return results;
}

int mem_delete(const char *base_dir, const char *key, MemoryLayer layer) {
    char path[MAX_PATH_LEN];
    entry_path(path, base_dir, layer, key);
    if (remove(path) == 0) {
        printf("[-] Deleted [%s] \"%s\"\n", LAYER_NAMES[layer], key);
        return 0;
    }
    return -1;
}

SearchResults mem_list_layer(const char *base_dir, MemoryLayer layer) {
    SearchResults results;
    memset(&results, 0, sizeof(results));
    char dir[MAX_PATH_LEN];
    layer_path(dir, base_dir, layer);
    
#ifdef _WIN32
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, MAX_PATH_LEN, "%s\\*.mem", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return results;
    do {
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, MAX_PATH_LEN, "%s\\%s", dir, fd.cFileName);
        MemEntry entry;
        if (read_entry(filepath, &entry) == 0 && results.count < MAX_RESULTS) {
            results.results[results.count].entry = entry;
            results.results[results.count].score = 100;
            results.count++;
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return results;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && results.count < MAX_RESULTS) {
        int len = strlen(de->d_name);
        if (len < 5 || strcmp(de->d_name + len - 4, ".mem") != 0) continue;
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, MAX_PATH_LEN, "%s/%s", dir, de->d_name);
        MemEntry entry;
        if (read_entry(filepath, &entry) == 0) {
            results.results[results.count].entry = entry;
            results.results[results.count].score = 100;
            results.count++;
        }
    }
    closedir(d);
#endif
    return results;
}

void mem_stats(const char *base_dir) {
    printf("\n=== AImemory Stats ===\n");
    int total = 0;
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        SearchResults lr = mem_list_layer(base_dir, layer);
        printf("  %-12s: %d entries\n", LAYER_NAMES[layer], lr.count);
        total += lr.count;
    }
    printf("  %-12s: %d entries\n", "TOTAL", total);
    printf("========================\n\n");
}

/* ============================================================
 * Intent Recognition
 * ============================================================ */

IntentResult mem_detect_intent(const char *message) {
    IntentResult result;
    memset(&result, 0, sizeof(result));
    result.type = INTENT_UNKNOWN;
    result.confidence = 30;
    
    detect_lang(message, result.lang);
    
    /* Build lowercase copy for English matching */
    char lower[MAX_VALUE_LEN];
    int i;
    for (i = 0; message[i] && i < MAX_VALUE_LEN - 1; i++)
        lower[i] = to_lower(message[i]);
    lower[i] = '\0';
    
    /* === English patterns === */
    const char *ask_en[] = {"what ", "how ", "why ", "when ", "where ", "who ",
                            "can you", "could you", "is it", "are there",
                            "do you know", "tell me", "explain", "?"};
    const char *action_en[] = {"please ", "make ", "create ", "build ", "write ",
                               "send ", "delete ", "fix ", "run ", "do ", "help me",
                               "generate", "deploy", "install", "download"};
    const char *correct_en[] = {"no,", "wrong", "that's not", "incorrect", "actually,",
                                "you're wrong", "mistake", "not right", "fix this"};
    const char *confirm_en[] = {"yes", "ok", "sure", "correct", "right", "agreed",
                                "exactly", "perfect", "good", "go ahead", "proceed"};
    const char *deny_en[] = {"no", "don't", "stop", "cancel", "never", "nope",
                             "disagree", "refuse", "won't"};
    const char *emotion_en[] = {"feel", "happy", "sad", "angry", "frustrated",
                                "worried", "scared", "excited", "tired", "hate",
                                "love", "afraid", "anxious", "hope"};

    /* === Chinese patterns === */
    const char *ask_zh[] = {"\xe4\xbb\x80\xe4\xb9\x88",    /* 什么 */
                            "\xe6\x80\x8e\xe4\xb9\x88",    /* 怎么 */
                            "\xe4\xb8\xba\xe4\xbb\x80\xe4\xb9\x88", /* 为什么 */
                            "\xe5\x93\xaa\xe9\x87\x8c",    /* 哪里 */
                            "\xe8\xb0\x81",                  /* 谁 */
                            "\xe5\x90\x97",                  /* 吗 */
                            "\xe5\x91\xa2",                  /* 呢 */
                            "\xef\xbc\x9f"};                 /* ？ */
    const char *action_zh[] = {"\xe5\xb8\xae\xe6\x88\x91",  /* 帮我 */
                               "\xe5\x81\x9a",              /* 做 */
                               "\xe5\x88\x9b\xe5\xbb\xba",  /* 创建 */
                               "\xe5\x86\x99",              /* 写 */
                               "\xe5\x8f\x91\xe9\x80\x81",  /* 发送 */
                               "\xe5\x88\xa0\xe9\x99\xa4",  /* 删除 */
                               "\xe4\xbf\xae\xe5\xa4\x8d"}; /* 修复 */
    const char *correct_zh[] = {"\xe4\xb8\x8d\xe5\xaf\xb9", /* 不对 */
                                "\xe9\x94\x99\xe4\xba\x86",  /* 错了 */
                                "\xe4\xb8\x8d\xe6\x98\xaf"}; /* 不是 */
    const char *confirm_zh[] = {"\xe5\xaf\xb9",              /* 对 */
                                "\xe5\xa5\xbd",              /* 好 */
                                "\xe6\x98\xaf\xe7\x9a\x84",  /* 是的 */
                                "\xe6\xb2\xa1\xe9\x97\xae\xe9\xa2\x98", /* 没问题 */
                                "\xe7\xbb\xa7\xe7\xbb\xad"};/* 继续 */
    const char *deny_zh[] = {"\xe4\xb8\x8d",                /* 不 */
                             "\xe4\xb8\x8d\xe8\xa6\x81",    /* 不要 */
                             "\xe5\x81\x9c",                /* 停 */
                             "\xe5\x8f\x96\xe6\xb6\x88"};   /* 取消 */

    /* Score each intent */
    int scores[8] = {0};
    
    if (contains_any(lower, ask_en, 14)) scores[INTENT_ASK_INFO] += 40;
    if (contains_any(message, ask_zh, 8)) scores[INTENT_ASK_INFO] += 40;
    
    if (contains_any(lower, action_en, 15)) scores[INTENT_REQUEST_ACTION] += 40;
    if (contains_any(message, action_zh, 7)) scores[INTENT_REQUEST_ACTION] += 40;
    
    if (contains_any(lower, correct_en, 9)) scores[INTENT_CORRECT_AI] += 50;
    if (contains_any(message, correct_zh, 3)) scores[INTENT_CORRECT_AI] += 50;
    
    if (contains_any(lower, confirm_en, 11)) scores[INTENT_CONFIRM] += 35;
    if (contains_any(message, confirm_zh, 5)) scores[INTENT_CONFIRM] += 35;
    
    if (contains_any(lower, deny_en, 8)) scores[INTENT_DENY] += 35;
    if (contains_any(message, deny_zh, 4)) scores[INTENT_DENY] += 35;
    
    if (contains_any(lower, emotion_en, 14)) scores[INTENT_EXPRESS_EMOTION] += 30;
    
    /* No strong signals = providing info */
    int max_score = 0;
    for (int j = 1; j < 8; j++) {
        if (scores[j] > max_score) {
            max_score = scores[j];
            result.type = j;
        }
    }
    
    if (max_score == 0) {
        result.type = INTENT_PROVIDE_INFO;
        result.confidence = 40;
    } else {
        result.confidence = max_score > 80 ? 90 : max_score + 30;
    }
    
    /* Extract topic: first significant words */
    const char *skip_words[] = {"what", "how", "why", "can", "you", "please",
                                "the", "a", "an", "is", "are", "do", "help", "me",
                                "i", "my", "to", "it", "this", "that"};
    char msg_copy[MAX_KEY_LEN];
    strncpy(msg_copy, lower, MAX_KEY_LEN - 1);
    msg_copy[MAX_KEY_LEN - 1] = '\0';
    
    char *tok = strtok(msg_copy, " \t\n?!.,");
    result.topic[0] = '\0';
    while (tok && strlen(result.topic) < MAX_KEY_LEN - 32) {
        int skip = 0;
        for (int j = 0; j < 20; j++) {
            if (strcmp(tok, skip_words[j]) == 0) { skip = 1; break; }
        }
        if (!skip && strlen(tok) > 2) {
            if (result.topic[0]) strcat(result.topic, " ");
            strcat(result.topic, tok);
        }
        tok = strtok(NULL, " \t\n?!.,");
    }
    
    return result;
}

/* ============================================================
 * Verification with Ask-Don't-Guess
 * ============================================================ */

VerifyResult mem_verify(const char *base_dir, const char *user_msg, const char *ai_resp) {
    VerifyResult vr;
    memset(&vr, 0, sizeof(vr));
    
    /* Detect intent */
    IntentResult intent = mem_detect_intent(user_msg);
    
    /* Expand queries for cross-language matching */
    char exp_user[MAX_EXPANDED][MAX_KEY_LEN];
    int exp_u_count = expand_query(user_msg, exp_user);
    char exp_ai[MAX_EXPANDED][MAX_KEY_LEN];
    int exp_a_count = expand_query(ai_resp, exp_ai);
    
    /* Search for context (mem_search already expands) */
    SearchResults ctx = mem_search(base_dir, user_msg);
    
    /* Search correction/verify/self layers with ALL expanded terms */
    SearchResults cor1, cor2, rules1, rules2, self_r, identity;
    memset(&cor1, 0, sizeof(cor1));
    memset(&cor2, 0, sizeof(cor2));
    memset(&rules1, 0, sizeof(rules1));
    memset(&rules2, 0, sizeof(rules2));
    memset(&self_r, 0, sizeof(self_r));
    memset(&identity, 0, sizeof(identity));
    
    for (int i = 0; i < exp_u_count; i++) {
        SearchResults r;
        r = mem_search_layer(base_dir, exp_user[i], LAYER_CORRECTION);
        for (int j = 0; j < r.count; j++) insert_result(&cor1, &r.results[j].entry, r.results[j].score);
        r = mem_search_layer(base_dir, exp_user[i], LAYER_VERIFY);
        for (int j = 0; j < r.count; j++) insert_result(&rules1, &r.results[j].entry, r.results[j].score);
        r = mem_search_layer(base_dir, exp_user[i], LAYER_SELF);
        for (int j = 0; j < r.count; j++) insert_result(&self_r, &r.results[j].entry, r.results[j].score);
        r = mem_search_layer(base_dir, exp_user[i], LAYER_IDENTITY);
        for (int j = 0; j < r.count; j++) insert_result(&identity, &r.results[j].entry, r.results[j].score);
    }
    for (int i = 0; i < exp_a_count; i++) {
        SearchResults r;
        r = mem_search_layer(base_dir, exp_ai[i], LAYER_CORRECTION);
        for (int j = 0; j < r.count; j++) insert_result(&cor2, &r.results[j].entry, r.results[j].score);
        r = mem_search_layer(base_dir, exp_ai[i], LAYER_VERIFY);
        for (int j = 0; j < r.count; j++) insert_result(&rules2, &r.results[j].entry, r.results[j].score);
    }
    
    vr.context_found = ctx.count;
    vr.corrections_found = cor1.count + cor2.count;
    vr.rules_matched = rules1.count + rules2.count;
    vr.self_warnings = self_r.count;
    
    /* Calculate risk */
    int risk = 0;
    if (vr.corrections_found > 0) risk += 40;
    if (vr.rules_matched > 0) risk += 30;
    if (vr.self_warnings > 0) risk += 20;
    if (vr.context_found == 0) risk += 10;
    
    /* If user is correcting AI, risk is high */
    if (intent.type == INTENT_CORRECT_AI) risk += 20;
    
    /* If user is just ASKING for info (not requesting action),
       reduce risk - they're not trying to DO something dangerous,
       they're just asking a question */
    if (intent.type == INTENT_ASK_INFO || intent.type == INTENT_PROVIDE_INFO) {
        risk = risk / 2;  /* Halve the risk for info questions */
    }
    
    if (risk > 100) risk = 100;
    vr.hallucination_risk = risk;
    
    /* Determine action */
    if (risk >= 70) {
        vr.action = ACTION_ASK;  /* Don't respond, ask first */
    } else if (risk >= 30) {
        vr.action = ACTION_WARN; /* Respond but warn */
    } else {
        vr.action = ACTION_PASS; /* Safe */
    }
    
    /* Generate warnings (deduplicated, with full content) */
    char seen_keys[MAX_QUESTIONS][MAX_KEY_LEN];
    int seen_count = 0;
    
    if (vr.context_found == 0) {
        snprintf(vr.warnings[vr.warning_count++], MAX_VALUE_LEN,
                 "No context found. AI may be guessing.");
    }
    for (int i = 0; i < cor1.count && vr.warning_count < MAX_QUESTIONS; i++) {
        int dup = 0;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen_keys[s], cor1.results[i].entry.key) == 0) { dup = 1; break; }
        }
        if (dup) continue;
        strncpy(seen_keys[seen_count++], cor1.results[i].entry.key, MAX_KEY_LEN - 1);
        /* Include full value so AI understands the actual mistake */
        char truncated[512];
        strncpy(truncated, cor1.results[i].entry.value, 500);
        truncated[500] = '\0';
        snprintf(vr.warnings[vr.warning_count++], MAX_VALUE_LEN,
                 "PAST MISTAKE: %s", truncated);
    }
    for (int i = 0; i < rules1.count && vr.warning_count < MAX_QUESTIONS; i++) {
        int dup = 0;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen_keys[s], rules1.results[i].entry.key) == 0) { dup = 1; break; }
        }
        if (dup) continue;
        strncpy(seen_keys[seen_count++], rules1.results[i].entry.key, MAX_KEY_LEN - 1);
        /* Include full rule content */
        char truncated[512];
        strncpy(truncated, rules1.results[i].entry.value, 500);
        truncated[500] = '\0';
        snprintf(vr.warnings[vr.warning_count++], MAX_VALUE_LEN,
                 "RULE: %s", truncated);
    }
    for (int i = 0; i < self_r.count && vr.warning_count < MAX_QUESTIONS; i++) {
        int dup = 0;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen_keys[s], self_r.results[i].entry.key) == 0) { dup = 1; break; }
        }
        if (dup) continue;
        strncpy(seen_keys[seen_count++], self_r.results[i].entry.key, MAX_KEY_LEN - 1);
        char truncated[512];
        strncpy(truncated, self_r.results[i].entry.value, 500);
        truncated[500] = '\0';
        snprintf(vr.warnings[vr.warning_count++], MAX_VALUE_LEN,
                 "SELF-CHECK: %s", truncated);
    }
    
    /* === ASK DON'T GUESS: Generate questions === */
    if (vr.action == ACTION_ASK || vr.context_found == 0) {
        /* No context: ask user to clarify */
        if (vr.context_found == 0) {
            snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                     "I don't have enough context about this topic. Can you give me more details?");
        }
        
        /* Past mistake found: ask user to confirm direction */
        if (vr.corrections_found > 0 && vr.question_count < MAX_QUESTIONS) {
            snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                     "I've made mistakes on similar topics before. Are you sure this is correct?");
        }
        
        /* Rule matched: ask for confirmation */
        if (vr.rules_matched > 0 && vr.question_count < MAX_QUESTIONS) {
            for (int i = 0; i < rules1.count && vr.question_count < MAX_QUESTIONS; i++) {
                snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                         "Verification needed: %s - Should I proceed?",
                         rules1.results[i].entry.value);
            }
        }
        
        /* Self-awareness triggered */
        if (vr.self_warnings > 0 && vr.question_count < MAX_QUESTIONS) {
            for (int i = 0; i < self_r.count && vr.question_count < MAX_QUESTIONS; i++) {
                snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                         "Self-check: %s - Do you want me to continue anyway?",
                         self_r.results[i].entry.value);
            }
        }
        
        /* Intent-based questions */
        if (intent.type == INTENT_ASK_INFO && vr.context_found == 0 && vr.question_count < MAX_QUESTIONS) {
            snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                     "I'm not sure about \"%s\". Can you provide a reference or more context?",
                     intent.topic);
        }
        
        if (intent.type == INTENT_REQUEST_ACTION && vr.question_count < MAX_QUESTIONS) {
            if (identity.count == 0) {
                snprintf(vr.questions[vr.question_count++], MAX_VALUE_LEN,
                         "I don't know enough about your situation to do this safely. Can you tell me more about your setup?");
            }
        }
    }
    
    return vr;
}

/* ============================================================
 * Auto-Learning
 * ============================================================ */

ExtractedFacts mem_extract_facts(const char *message, const char *context) {
    ExtractedFacts facts;
    memset(&facts, 0, sizeof(facts));
    (void)context;
    
    char lower[MAX_VALUE_LEN];
    int i;
    for (i = 0; message[i] && i < MAX_VALUE_LEN - 1; i++)
        lower[i] = to_lower(message[i]);
    lower[i] = '\0';
    
    /* Detect identity facts */
    const char *id_patterns_en[] = {"my name is", "i am ", "i'm ", "i work at",
                                     "i live in", "my job is", "i use "};
    const char *id_patterns_zh[] = {"\xe6\x88\x91\xe5\x8f\xab",     /* 我叫 */
                                     "\xe6\x88\x91\xe6\x98\xaf",     /* 我是 */
                                     "\xe6\x88\x91\xe5\x9c\xa8",     /* 我在 */
                                     "\xe6\x88\x91\xe7\x94\xa8"};    /* 我用 */
    
    for (int j = 0; j < 7 && facts.count < MAX_RESULTS; j++) {
        char *pos = strstr(lower, id_patterns_en[j]);
        if (pos) {
            MemEntry *e = &facts.entries[facts.count];
            int plen = strlen(id_patterns_en[j]);
            snprintf(e->key, MAX_KEY_LEN, "auto_%s", id_patterns_en[j]);
            /* Replace spaces in key */
            for (char *k = e->key; *k; k++) if (*k == ' ') *k = '_';
            
            /* Extract the value after the pattern */
            char *start = (char*)message + (pos - lower) + plen;
            int vlen = 0;
            while (start[vlen] && start[vlen] != '.' && start[vlen] != ',' 
                   && start[vlen] != '\n' && vlen < MAX_VALUE_LEN - 1)
                vlen++;
            strncpy(e->value, start, vlen);
            e->value[vlen] = '\0';
            e->layer = LAYER_IDENTITY;
            e->confidence = 70;
            e->timestamp = time(NULL);
            facts.count++;
        }
    }
    
    for (int j = 0; j < 4 && facts.count < MAX_RESULTS; j++) {
        if (strstr(message, id_patterns_zh[j])) {
            MemEntry *e = &facts.entries[facts.count];
            snprintf(e->key, MAX_KEY_LEN, "auto_identity_%ld", time(NULL));
            strncpy(e->value, message, MAX_VALUE_LEN - 1);
            e->layer = LAYER_IDENTITY;
            e->confidence = 65;
            e->timestamp = time(NULL);
            facts.count++;
            break;
        }
    }
    
    /* Detect correction facts */
    const char *cor_en[] = {"that's wrong", "you're wrong", "incorrect",
                            "that's not right", "no, it should be", "actually,"};
    const char *cor_zh[] = {"\xe9\x94\x99\xe4\xba\x86",    /* 错了 */
                            "\xe4\xb8\x8d\xe5\xaf\xb9"};   /* 不对 */
    
    for (int j = 0; j < 6 && facts.count < MAX_RESULTS; j++) {
        if (strstr(lower, cor_en[j])) {
            MemEntry *e = &facts.entries[facts.count];
            snprintf(e->key, MAX_KEY_LEN, "correction_%ld", time(NULL));
            strncpy(e->value, message, MAX_VALUE_LEN - 1);
            e->layer = LAYER_CORRECTION;
            e->confidence = 85;
            e->timestamp = time(NULL);
            facts.count++;
            break;
        }
    }
    for (int j = 0; j < 2 && facts.count < MAX_RESULTS; j++) {
        if (strstr(message, cor_zh[j])) {
            MemEntry *e = &facts.entries[facts.count];
            snprintf(e->key, MAX_KEY_LEN, "correction_%ld", time(NULL));
            strncpy(e->value, message, MAX_VALUE_LEN - 1);
            e->layer = LAYER_CORRECTION;
            e->confidence = 85;
            e->timestamp = time(NULL);
            facts.count++;
            break;
        }
    }
    
    /* Detect work/project facts */
    const char *work_en[] = {"working on", "my project", "the codebase",
                              "the repo", "our system", "the server", "the database"};
    for (int j = 0; j < 7 && facts.count < MAX_RESULTS; j++) {
        if (strstr(lower, work_en[j])) {
            MemEntry *e = &facts.entries[facts.count];
            snprintf(e->key, MAX_KEY_LEN, "work_%ld", time(NULL));
            strncpy(e->value, message, MAX_VALUE_LEN - 1);
            e->layer = LAYER_WORK;
            e->confidence = 60;
            e->timestamp = time(NULL);
            facts.count++;
            break;
        }
    }
    
    return facts;
}

int mem_auto_learn(const char *base_dir, const char *user_msg, const char *ai_resp) {
    int stored = 0;
    
    /* Extract facts from user message */
    ExtractedFacts facts = mem_extract_facts(user_msg, ai_resp);
    
    for (int i = 0; i < facts.count; i++) {
        if (mem_store(base_dir, &facts.entries[i]) == 0)
            stored++;
    }
    
    if (stored > 0)
        printf("[auto-learn] Extracted and stored %d facts\n", stored);
    
    return stored;
}
