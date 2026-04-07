/*
 * AImemory - Interactive CLI
 * Test and demonstrate the six-layer memory system
 * Author: Jiayi Sun (SY115)
 */

#include "aimemory.h"

#define BASE_DIR "."

static void print_help(void) {
    printf("\n");
    printf("  AImemory - AI Anti-Hallucination Memory Engine\n");
    printf("  by Jiayi Sun (SY115)\n");
    printf("  ============================================\n\n");
    printf("  Commands:\n");
    printf("    store <layer> <key> <value>   Store a memory\n");
    printf("    search <query>                Search ALL layers\n");
    printf("    list <layer>                  List entries in a layer\n");
    printf("    delete <layer> <key>          Delete an entry\n");
    printf("    stats                         Show memory stats\n");
    printf("    demo                          Load demo data\n");
    printf("    help                          Show this help\n");
    printf("    quit                          Exit\n\n");
    printf("  Layers: identity work chat correction self verify\n\n");
}

static MemoryLayer parse_layer(const char *name) {
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (strcmp(name, LAYER_NAMES[i]) == 0) return i;
    }
    /* Try short names */
    if (strcmp(name, "id") == 0) return LAYER_IDENTITY;
    if (strcmp(name, "cor") == 0) return LAYER_CORRECTION;
    if (strcmp(name, "ver") == 0) return LAYER_VERIFY;
    return -1;
}

static void print_results(SearchResults *results) {
    if (results->count == 0) {
        printf("  (no results)\n");
        return;
    }
    
    for (int i = 0; i < results->count; i++) {
        SearchResult *r = &results->results[i];
        char timebuf[64];
        struct tm *tm = localtime(&r->entry.timestamp);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm);
        
        printf("  [%d] score=%d layer=%-10s conf=%d%% time=%s\n",
               i+1, r->score, LAYER_NAMES[r->entry.layer],
               r->entry.confidence, timebuf);
        printf("      key: %s\n", r->entry.key);
        
        /* Truncate long values */
        if (strlen(r->entry.value) > 80) {
            printf("      val: %.77s...\n", r->entry.value);
        } else {
            printf("      val: %s\n", r->entry.value);
        }
        printf("\n");
    }
}

static void load_demo(void) {
    printf("[*] Loading demo data...\n\n");
    
    /* Identity layer */
    MemEntry e;
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "user_name", MAX_KEY_LEN);
    strncpy(e.value, "Jiayi Sun, 22 years old, independent security researcher from Qingdao", MAX_VALUE_LEN);
    e.layer = LAYER_IDENTITY;
    e.confidence = 95;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "user_skills", MAX_KEY_LEN);
    strncpy(e.value, "Python, C basics, reverse engineering with dnSpy, WinDbg kernel debugging, bug bounty on HackerOne", MAX_VALUE_LEN);
    e.layer = LAYER_IDENTITY;
    e.confidence = 85;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "user_preference", MAX_KEY_LEN);
    strncpy(e.value, "Prefers direct communication, no repetition, no patronizing. Wants practical next steps.", MAX_VALUE_LEN);
    e.layer = LAYER_IDENTITY;
    e.confidence = 90;
    mem_store(BASE_DIR, &e);
    
    /* Work layer */
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "ntfs_vulnerability", MAX_KEY_LEN);
    strncpy(e.value, "Found ntfs.sys kernel vuln. BugCheck 0x4E PFN_LIST_CORRUPT. VHDX with MFT cluster=0xFFFFFFFFFFFFFFFF. Double-click triggers BSOD. Zero privilege. Win11 25H2.", MAX_VALUE_LEN);
    e.layer = LAYER_WORK;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "ssd_submission", MAX_KEY_LEN);
    strncpy(e.value, "Initial disclosure email sent to SSD 2026-04-06. Summary only, no PoC attached. Waiting for response.", MAX_VALUE_LEN);
    e.layer = LAYER_WORK;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    /* Correction layer */
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "verifier_log_mistake", MAX_KEY_LEN);
    strncpy(e.value, "AI claimed crash log was clean but it contained VRF (Driver Verifier) markers. User caught this via cross-checking with Gemini. Must always verify log cleanliness before submission.", MAX_VALUE_LEN);
    e.layer = LAYER_CORRECTION;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "poc_giveaway_mistake", MAX_KEY_LEN);
    strncpy(e.value, "AI suggested sending full PoC and report to SSD immediately. User correctly pushed back - should send summary first, wait for offer, then provide details.", MAX_VALUE_LEN);
    e.layer = LAYER_CORRECTION;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    /* Self-awareness layer */
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "hallucination_risk", MAX_KEY_LEN);
    strncpy(e.value, "AI cannot detect its own hallucinations. Tends to generate confident answers even when uncertain. Should say 'I don't know' more often.", MAX_VALUE_LEN);
    e.layer = LAYER_SELF;
    e.confidence = 95;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "business_judgment", MAX_KEY_LEN);
    strncpy(e.value, "AI has poor business instincts. User has better commercial judgment. Defer to user on pricing, negotiation, and disclosure strategy.", MAX_VALUE_LEN);
    e.layer = LAYER_SELF;
    e.confidence = 90;
    mem_store(BASE_DIR, &e);
    
    /* Verify layer */
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "rule_verify_logs", MAX_KEY_LEN);
    strncpy(e.value, "RULE: Before claiming a crash log is clean, verify no Driver Verifier markers (VRF, 'Driver Verifier: Applied') exist in the log.", MAX_VALUE_LEN);
    e.layer = LAYER_VERIFY;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    memset(&e, 0, sizeof(e));
    strncpy(e.key, "rule_ask_before_send", MAX_KEY_LEN);
    strncpy(e.value, "RULE: Never suggest sending full vulnerability details before receiving a formal offer. Send summary first, negotiate, then deliver.", MAX_VALUE_LEN);
    e.layer = LAYER_VERIFY;
    e.confidence = 100;
    mem_store(BASE_DIR, &e);
    
    printf("\n[*] Demo data loaded. Try: search ntfs\n\n");
}

int main(void) {
    mem_init(BASE_DIR);
    print_help();
    
    char line[MAX_VALUE_LEN];
    char cmd[64], arg1[MAX_KEY_LEN], arg2[MAX_KEY_LEN], arg3[MAX_VALUE_LEN];
    
    while (1) {
        printf("aimemory> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        /* Remove newline */
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (len == 0 || line[0] == '\0') continue;
        
        /* Parse command */
        cmd[0] = arg1[0] = arg2[0] = arg3[0] = '\0';
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) {
            printf("Bye.\n");
            break;
        }
        
        if (strcmp(line, "help") == 0 || strcmp(line, "h") == 0) {
            print_help();
            continue;
        }
        
        if (strcmp(line, "stats") == 0) {
            mem_stats(BASE_DIR);
            continue;
        }
        
        if (strcmp(line, "demo") == 0) {
            load_demo();
            continue;
        }
        
        /* Parse multi-arg commands */
        int n = sscanf(line, "%63s %255s %255s %4095[^\n]", cmd, arg1, arg2, arg3);
        
        if (strcmp(cmd, "search") == 0 || strcmp(cmd, "s") == 0) {
            if (n < 2) {
                printf("  Usage: search <query>\n");
                continue;
            }
            /* Reconstruct full query from remaining args */
            char query[MAX_VALUE_LEN];
            char *q = line + strlen(cmd) + 1;
            strncpy(query, q, MAX_VALUE_LEN - 1);
            
            printf("\n  Searching all layers for: \"%s\"\n\n", query);
            SearchResults results = mem_search(BASE_DIR, query);
            print_results(&results);
            
        } else if (strcmp(cmd, "store") == 0) {
            if (n < 4) {
                printf("  Usage: store <layer> <key> <value>\n");
                continue;
            }
            int layer = parse_layer(arg1);
            if (layer < 0) {
                printf("  Unknown layer: %s\n", arg1);
                continue;
            }
            MemEntry e;
            memset(&e, 0, sizeof(e));
            strncpy(e.key, arg2, MAX_KEY_LEN - 1);
            strncpy(e.value, arg3, MAX_VALUE_LEN - 1);
            e.layer = layer;
            e.confidence = 80;
            mem_store(BASE_DIR, &e);
            
        } else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
            if (n < 2) {
                printf("  Usage: list <layer>\n");
                continue;
            }
            int layer = parse_layer(arg1);
            if (layer < 0) {
                printf("  Unknown layer: %s\n", arg1);
                continue;
            }
            printf("\n  Layer: %s\n\n", LAYER_NAMES[layer]);
            SearchResults results = mem_list_layer(BASE_DIR, layer);
            print_results(&results);
            
        } else if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "del") == 0) {
            if (n < 3) {
                printf("  Usage: delete <layer> <key>\n");
                continue;
            }
            int layer = parse_layer(arg1);
            if (layer < 0) {
                printf("  Unknown layer: %s\n", arg1);
                continue;
            }
            mem_delete(BASE_DIR, arg2, layer);
            
        } else {
            printf("  Unknown command: %s (type 'help')\n", cmd);
        }
    }
    
    return 0;
}
