/*
 * AImemory - AI Anti-Hallucination Memory Engine
 * Core header file v0.2
 * Author: Jiayi Sun (SY115)
 * License: Free use with attribution. Commercial use over $1M revenue: 5% fee.
 */

#ifndef AIMEMORY_H
#define AIMEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* Platform compatibility */
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define mkdir(p, m) _mkdir(p)
    #define PATH_SEP '\\'
#else
    #include <dirent.h>
    #include <unistd.h>
    #define PATH_SEP '/'
#endif

#define MAX_KEY_LEN      256
#define MAX_VALUE_LEN    4096
#define MAX_RESULTS      32
#define MAX_PATH_LEN     512
#define MAX_QUESTIONS    5
#define DATA_DIR         "data"

/* Six memory layers */
typedef enum {
    LAYER_IDENTITY   = 0,  /* Who is the user */
    LAYER_WORK       = 1,  /* Project context, technical details */
    LAYER_CHAT       = 2,  /* Conversation history */
    LAYER_CORRECTION = 3,  /* Mistakes AI has made */
    LAYER_SELF       = 4,  /* AI self-awareness, capability boundaries */
    LAYER_VERIFY     = 5,  /* Verification rules and patterns */
    LAYER_COUNT      = 6
} MemoryLayer;

/* Intent types */
typedef enum {
    INTENT_UNKNOWN       = 0,
    INTENT_ASK_INFO      = 1,  /* User asking for information */
    INTENT_REQUEST_ACTION= 2,  /* User wants something done */
    INTENT_PROVIDE_INFO  = 3,  /* User telling AI something */
    INTENT_EXPRESS_EMOTION= 4, /* User expressing feelings */
    INTENT_CORRECT_AI    = 5,  /* User correcting AI's mistake */
    INTENT_CONFIRM       = 6,  /* User confirming/agreeing */
    INTENT_DENY          = 7   /* User denying/disagreeing */
} IntentType;

/* Verification action */
typedef enum {
    ACTION_PASS  = 0,   /* Safe to respond normally */
    ACTION_WARN  = 1,   /* Respond but show warning */
    ACTION_ASK   = 2,   /* Don't respond, ask user first */
    ACTION_STOP  = 3    /* Block response, known bad pattern */
} VerifyAction;

/* Single memory entry */
typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    MemoryLayer layer;
    time_t timestamp;
    int confidence;                /* 0-100 */
} MemEntry;

/* Search result */
typedef struct {
    MemEntry entry;
    int score;
} SearchResult;

/* Search results collection */
typedef struct {
    SearchResult results[MAX_RESULTS];
    int count;
} SearchResults;

/* Intent analysis result */
typedef struct {
    IntentType type;
    int confidence;                /* 0-100 */
    char topic[MAX_KEY_LEN];       /* Detected topic/subject */
    char lang[8];                  /* Detected language: "en", "zh", etc */
} IntentResult;

/* Verification result */
typedef struct {
    VerifyAction action;
    int hallucination_risk;        /* 0-100 */
    int context_found;
    int corrections_found;
    int rules_matched;
    int self_warnings;
    char questions[MAX_QUESTIONS][MAX_VALUE_LEN];  /* Suggested questions to ask */
    int question_count;
    char warnings[MAX_QUESTIONS][MAX_VALUE_LEN];
    int warning_count;
} VerifyResult;

/* Auto-extracted facts */
typedef struct {
    MemEntry entries[MAX_RESULTS];
    int count;
} ExtractedFacts;

/* Layer name strings */
static const char *LAYER_NAMES[] = {
    "identity",
    "work",
    "chat",
    "correction",
    "self",
    "verify"
};

static const char *INTENT_NAMES[] = {
    "unknown",
    "ask_info",
    "request_action",
    "provide_info",
    "express_emotion",
    "correct_ai",
    "confirm",
    "deny"
};

static const char *ACTION_NAMES[] = {
    "pass",
    "warn",
    "ask",
    "stop"
};

/* === Core Memory API === */
int mem_init(const char *base_dir);
int mem_store(const char *base_dir, MemEntry *entry);
SearchResults mem_search(const char *base_dir, const char *query);
SearchResults mem_search_layer(const char *base_dir, const char *query, MemoryLayer layer);
int mem_delete(const char *base_dir, const char *key, MemoryLayer layer);
SearchResults mem_list_layer(const char *base_dir, MemoryLayer layer);
void mem_stats(const char *base_dir);

/* === Intent Recognition === */
IntentResult mem_detect_intent(const char *message);

/* === Verification with Ask-Don't-Guess === */
VerifyResult mem_verify(const char *base_dir, const char *user_msg, const char *ai_resp);

/* === Auto-Learning === */
ExtractedFacts mem_extract_facts(const char *message, const char *context);
int mem_auto_learn(const char *base_dir, const char *user_msg, const char *ai_resp);

#endif /* AIMEMORY_H */
