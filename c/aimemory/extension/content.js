/*
 * AImemory - Browser Extension Content Script v0.5
 * Fixed: dedup injection, cleaner context, smarter auto-correct
 * Author: Jiayi Sun (SY115)
 */

const API = 'http://127.0.0.1:7890';
let isConnected = false;
let lastUserMessage = '';
let hallucinationCount = 0;
let correctionAttempted = false;
const MAX_HALLUCINATION = 3;
const STORE_MAX = 2000;
const sentMessages = new Set();
const processedResponses = new Set();
let isSending = false;
let lastSendTime = 0;
const SEND_COOLDOWN = 3000;

async function api(path, method, body) {
    try {
        const opts = {method: method || 'GET', headers: {'Content-Type':'application/json'}};
        if (body) opts.body = JSON.stringify(body);
        const r = await fetch(API + path, opts);
        return await r.json();
    } catch(e) { return null; }
}

async function checkHealth() {
    const r = await api('/health');
    isConnected = r && r.status === 'ok';
    return isConnected;
}

function getPlatform() {
    const h = location.hostname;
    if (h.includes('claude.ai')) return 'claude';
    if (h.includes('chat.openai.com') || h.includes('chatgpt.com')) return 'chatgpt';
    if (h.includes('gemini.google.com')) return 'gemini';
    return 'unknown';
}

function getInputField() {
    const p = getPlatform();
    if (p === 'claude') return document.querySelector('[contenteditable="true"], .ProseMirror');
    if (p === 'chatgpt') return document.querySelector('#prompt-textarea, textarea[data-id]');
    if (p === 'gemini') return document.querySelector('.ql-editor, [contenteditable="true"], textarea');
    return null;
}

function getInputText(field) {
    if (!field) return '';
    return (field.innerText || field.value || field.textContent || '').trim();
}

function setInputText(field, text) {
    if (!field) return;
    if (field.tagName === 'TEXTAREA' || field.tagName === 'INPUT') {
        field.value = text;
        field.dispatchEvent(new Event('input', {bubbles: true}));
    } else {
        field.innerText = text;
        field.dispatchEvent(new Event('input', {bubbles: true}));
    }
}

function doSend() {
    const btns = document.querySelectorAll('button');
    for (const btn of btns) {
        const label = (btn.getAttribute('aria-label') || '').toLowerCase();
        const testid = btn.getAttribute('data-testid') || '';
        if (label.includes('send') || testid.includes('send') || btn.type === 'submit') {
            isSending = true;
            btn.click();
            setTimeout(function() { isSending = false; }, 1000);
            return true;
        }
    }
    const field = getInputField();
    if (field) {
        isSending = true;
        field.dispatchEvent(new KeyboardEvent('keydown', {key:'Enter', code:'Enter', bubbles:true}));
        setTimeout(function() { isSending = false; }, 1000);
        return true;
    }
    return false;
}

function getLatestAIResponse() {
    const p = getPlatform();
    let msgs;
    if (p === 'claude') msgs = document.querySelectorAll('[data-is-ai-message="true"], .font-claude-message');
    else if (p === 'chatgpt') msgs = document.querySelectorAll('[data-message-author-role="assistant"]');
    else if (p === 'gemini') msgs = document.querySelectorAll('.model-response-text, .response-content');
    else msgs = [];
    if (msgs.length === 0) return null;
    const last = msgs[msgs.length - 1];
    return {element: last, text: (last.innerText || '').trim()};
}

/* ============================================================
 * Deduplicate helper
 * ============================================================ */
function dedup(arr, keyFn) {
    const seen = new Set();
    return arr.filter(function(item) {
        const k = keyFn(item);
        if (seen.has(k)) return false;
        seen.add(k);
        return true;
    });
}

/* ============================================================
 * Memory Injection (CLEAN, DEDUPED)
 * ============================================================ */
async function prepareMessage(userText) {
    if (!isConnected) return userText;
    
    const verify = await api('/verify', 'POST', {user_message: userText, ai_response: ''});
    if (!verify) return userText;
    
    /* Only inject if risk is meaningful */
    if (verify.hallucination_risk < 30) {
        /* Low risk: just store and pass through */
        storeUserMsg(userText);
        await api('/extract', 'POST', {user_message: userText, ai_response: ''});
        return userText;
    }
    
    /* Build clean, deduped context */
    let lines = [];
    
    /* Deduplicate warnings */
    if (verify.warnings && verify.warnings.length > 0) {
        const unique = [];
        const seen = new Set();
        verify.warnings.forEach(function(w) {
            if (!seen.has(w)) { seen.add(w); unique.push(w); }
        });
        unique.forEach(function(w) { lines.push(w); });
    }
    
    /* Add one key question if any */
    if (verify.questions && verify.questions.length > 0) {
        lines.push('If unsure, ask me before answering.');
    }
    
    storeUserMsg(userText);
    await api('/extract', 'POST', {user_message: userText, ai_response: ''});
    
    if (lines.length === 0) return userText;
    
    /* Build compact context block */
    let context = '[AImemory] ' + lines.join(' | ') + '\n\n';
    return context + userText;
}

async function storeUserMsg(text) {
    const hash = text.substring(0, 100);
    if (!sentMessages.has(hash)) {
        sentMessages.add(hash);
        await api('/store', 'POST', {
            layer: 'chat', key: 'user_' + Date.now(),
            value: text.substring(0, STORE_MAX), confidence: 90
        });
    }
}

/* ============================================================
 * Response Check (SMARTER)
 * ============================================================ */
async function checkResponse(aiText, aiElement) {
    if (!isConnected || !aiText || aiText.length < 10) return;
    
    const sig = aiText.substring(0, 100);
    if (processedResponses.has(sig)) return;
    processedResponses.add(sig);
    
    /* Store AI response */
    await api('/store', 'POST', {
        layer: 'chat', key: 'ai_' + Date.now(),
        value: aiText.substring(0, STORE_MAX), confidence: 70
    });
    
    const verify = await api('/verify', 'POST', {
        user_message: lastUserMessage, ai_response: aiText
    });
    if (!verify) return;
    
    console.log('[AImemory] Risk=' + verify.hallucination_risk + '% action=' + verify.action);
    
    /* Show badge if risk >= 30 */
    if (verify.hallucination_risk >= 30) {
        showBadge(aiElement, verify);
    }
    
    /* Only auto-correct if action is 'ask' AND risk >= 80 */
    if (verify.action === 'ask' && verify.hallucination_risk >= 80) {
        hallucinationCount++;
        if (hallucinationCount >= MAX_HALLUCINATION) {
            showNewWindowWarning();
        } else if (!correctionAttempted) {
            correctionAttempted = true;
            autoCorrect(verify);
        }
    } else if (verify.hallucination_risk < 30) {
        hallucinationCount = 0;
        correctionAttempted = false;
    }
}

/* ============================================================
 * Auto-Correction (CLEANER)
 * ============================================================ */
function autoCorrect(verify) {
    let parts = [];
    
    /* Deduplicate warnings */
    if (verify.warnings) {
        const seen = new Set();
        verify.warnings.forEach(function(w) {
            if (!seen.has(w)) { seen.add(w); parts.push(w); }
        });
    }
    
    if (parts.length === 0) return;
    
    let msg = 'Stop. Check these facts before continuing:\n';
    parts.forEach(function(p) { msg += '- ' + p + '\n'; });
    msg += 'Are you sure your response is correct?';
    
    const field = getInputField();
    if (field) {
        setTimeout(function() {
            setInputText(field, msg);
            setTimeout(doSend, 500);
        }, 2000);
    }
}

/* ============================================================
 * New Window Warning
 * ============================================================ */
async function showNewWindowWarning() {
    if (document.getElementById('aimemory-overlay')) return;
    
    const searchAll = await api('/search?q=' + encodeURIComponent(lastUserMessage));
    let ctx = '';
    if (searchAll && searchAll.results) {
        const unique = dedup(searchAll.results, function(r) { return r.key; });
        unique.slice(0, 5).forEach(function(r) {
            ctx += r.key + ': ' + r.value.substring(0, 150) + '\n';
        });
    }
    
    const overlay = document.createElement('div');
    overlay.id = 'aimemory-overlay';
    overlay.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.7);z-index:999999;display:flex;align-items:center;justify-content:center;';
    const box = document.createElement('div');
    box.style.cssText = 'background:#1a1a2e;color:#eee;padding:24px;border-radius:12px;max-width:500px;font-family:sans-serif;border:2px solid #ff4444;';
    box.innerHTML =
        '<h2 style="color:#ff4444;margin-bottom:12px;">AImemory Warning</h2>' +
        '<p style="margin-bottom:12px;">AI is hallucinating repeatedly. Continuing may be unreliable.</p>' +
        '<p style="margin-bottom:16px;color:#ffaa00;">Open a new conversation. Your context will be copied.</p>' +
        '<div style="display:flex;gap:12px;">' +
        '<button id="aim-copy" style="padding:8px 16px;background:#4fc3f7;color:#000;border:none;border-radius:6px;cursor:pointer;">Copy Context</button>' +
        '<button id="aim-close" style="padding:8px 16px;background:#333;color:#eee;border:1px solid #555;border-radius:6px;cursor:pointer;">Dismiss</button></div>';
    overlay.appendChild(box);
    document.body.appendChild(overlay);
    
    document.getElementById('aim-copy').onclick = function() {
        navigator.clipboard.writeText('[Context]\n' + ctx + '\n' + lastUserMessage);
        alert('Copied! Paste into new chat.');
        overlay.remove();
        hallucinationCount = 0;
        correctionAttempted = false;
    };
    document.getElementById('aim-close').onclick = function() { overlay.remove(); };
}

/* ============================================================
 * Warning Badge (DEDUPED)
 * ============================================================ */
function showBadge(element, verify) {
    if (!element || !element.parentElement) return;
    const old = element.parentElement.querySelector('.aimemory-badge');
    if (old) old.remove();
    
    let color = verify.hallucination_risk >= 60 ? '#ff4444' : verify.hallucination_risk >= 30 ? '#ffaa00' : '#44aaff';
    let label = verify.hallucination_risk >= 60 ? 'HIGH RISK' : verify.hallucination_risk >= 30 ? 'CAUTION' : 'NOTE';
    
    const badge = document.createElement('div');
    badge.className = 'aimemory-badge';
    badge.style.cssText = 'margin:4px 0;padding:8px 12px;background:' + color + '15;border-left:3px solid ' + color + ';border-radius:4px;font-size:12px;font-family:monospace;color:' + color + ';';
    
    let html = '<strong>AImemory ' + label + ' (' + verify.hallucination_risk + '%)</strong>';
    if (verify.warnings) {
        const seen = new Set();
        verify.warnings.forEach(function(w) {
            if (!seen.has(w)) {
                seen.add(w);
                html += '<br><span style="color:#888;font-size:11px;">' + w + '</span>';
            }
        });
    }
    badge.innerHTML = html;
    element.parentElement.insertBefore(badge, element.nextSibling);
}

/* ============================================================
 * Send Interception
 * ============================================================ */
function setupInterception() {
    document.addEventListener('click', async function(e) {
        if (isSending) return;
        if (Date.now() - lastSendTime < SEND_COOLDOWN) return;
        const btn = e.target.closest('button');
        if (!btn) return;
        const label = (btn.getAttribute('aria-label') || '').toLowerCase();
        const testid = btn.getAttribute('data-testid') || '';
        if (!label.includes('send') && !testid.includes('send') && btn.type !== 'submit') return;
        if (!isConnected) return;
        const field = getInputField();
        if (!field) return;
        const userText = getInputText(field);
        if (!userText || userText.length < 2) return;
        if (sentMessages.has(userText.substring(0, 100))) return;
        
        e.preventDefault();
        e.stopPropagation();
        e.stopImmediatePropagation();
        lastSendTime = Date.now();
        lastUserMessage = userText;
        
        const enhanced = await prepareMessage(userText);
        if (enhanced !== userText) setInputText(field, enhanced);
        setTimeout(doSend, 300);
    }, true);
    
    /* Note: Enter key interception removed to avoid conflicts with 
       platform UI (Gemini sidebar toggle). Only button clicks are intercepted. */
}

/* ============================================================
 * Response Monitor
 * ============================================================ */
function startMonitor() {
    let lastText = '';
    let stableCount = 0;
    setInterval(function() {
        if (!isConnected) return;
        const resp = getLatestAIResponse();
        if (!resp || !resp.text || resp.text.length < 10) return;
        if (resp.text === lastText) {
            stableCount++;
            if (stableCount === 2) checkResponse(resp.text, resp.element);
        } else {
            lastText = resp.text;
            stableCount = 0;
        }
    }, 1500);
}

/* ============================================================
 * Status + Permission + Init
 * ============================================================ */
function createStatus() {
    const el = document.createElement('div');
    el.id = 'aimemory-status';
    el.style.cssText = 'position:fixed;bottom:10px;right:10px;padding:4px 10px;border-radius:12px;font-size:11px;font-family:monospace;z-index:99999;cursor:pointer;background:#1a1a2e;border:1px solid #333;color:#4fc3f7;';
    el.textContent = isConnected ? 'AImemory ON' : 'AImemory OFF';
    el.onclick = async function() {
        await checkHealth();
        el.textContent = isConnected ? 'AImemory ON' : 'AImemory OFF';
        el.style.color = isConnected ? '#4fc3f7' : '#ff4444';
    };
    document.body.appendChild(el);
}

function requestPermission() {
    return new Promise(function(resolve) {
        try { if (localStorage.getItem('aimemory_ok') === '1') { resolve(true); return; } } catch(e) {}
        const overlay = document.createElement('div');
        overlay.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.8);z-index:999999;display:flex;align-items:center;justify-content:center;';
        const box = document.createElement('div');
        box.style.cssText = 'background:#1a1a2e;color:#eee;padding:24px;border-radius:12px;max-width:450px;font-family:sans-serif;border:2px solid #4fc3f7;';
        box.innerHTML =
            '<h2 style="color:#4fc3f7;margin-bottom:12px;">AImemory</h2>' +
            '<p style="margin-bottom:12px;">Allow AImemory to monitor AI conversations for hallucination detection?</p>' +
            '<p style="font-size:12px;color:#aaa;margin-bottom:16px;">All data stored locally. Nothing sent to cloud.</p>' +
            '<div style="display:flex;gap:12px;">' +
            '<button id="aim-yes" style="padding:8px 16px;background:#4fc3f7;color:#000;border:none;border-radius:6px;cursor:pointer;">Allow</button>' +
            '<button id="aim-no" style="padding:8px 16px;background:#333;color:#eee;border:1px solid #555;border-radius:6px;cursor:pointer;">Deny</button></div>';
        overlay.appendChild(box);
        document.body.appendChild(overlay);
        document.getElementById('aim-yes').onclick = function() {
            try { localStorage.setItem('aimemory_ok', '1'); } catch(e) {}
            overlay.remove(); resolve(true);
        };
        document.getElementById('aim-no').onclick = function() { overlay.remove(); resolve(false); };
    });
}

async function init() {
    const platform = getPlatform();
    if (platform === 'unknown') return;
    console.log('[AImemory] v0.5 on ' + platform);
    await checkHealth();
    if (!isConnected) { createStatus(); return; }
    const ok = await requestPermission();
    if (!ok) return;
    createStatus();
    setupInterception();
    startMonitor();
    setInterval(async function() {
        await checkHealth();
        const el = document.getElementById('aimemory-status');
        if (el) {
            el.textContent = isConnected ? 'AImemory ON' : 'AImemory OFF';
            el.style.color = isConnected ? '#4fc3f7' : '#ff4444';
        }
    }, 30000);
}

setTimeout(init, 1500);
