// Exercise the shipped inline controller with a small DOM/media adapter.
// Real rendering, seeking and decoding are checked separately in a browser.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../main/memo_portal.html'), 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];

function setup({supported = true, reply, playDenied = false} = {}) {
  const elements = [], urls = [], revoked = [], requests = [], events = {};
  class Element {
    constructor(tag) {
      this.tag = tag; this.children = []; this.className = ''; this.attributes = {};
      elements.push(this);
      this.classList = {
        contains: c => this.className.split(' ').includes(c),
        add: c => this.className += ` ${c}`,
        remove: c => this.className = this.className.split(' ').filter(x => x !== c).join(' '),
      };
    }
    append(...children) { this.children.push(...children); }
    setAttribute(k, v) { this.attributes[k] = v; }
    removeAttribute(k) { delete this.attributes[k]; delete this[k]; }
    canPlayType() { return supported ? 'probably' : ''; }
    async play() { if (playDenied) throw Error('gesture required'); this.paused = false; }
    pause() { this.paused = true; }
    load() {}
  }
  const controls = new Map();
  const ctx = vm.createContext({
    document: {
      querySelector: s => {
        if (!controls.has(s)) controls.set(s, new Element('div'));
        return controls.get(s);
      },
      createElement: tag => new Element(tag),
      querySelectorAll: tag => elements.filter(e => e.tag === tag),
    },
    window: {addEventListener: (name, callback) => events[name] = callback},
    URL: {
      createObjectURL: blob => { const u = `blob:demo/${urls.length}`; urls.push({u, blob}); return u; },
      revokeObjectURL: url => revoked.push(url),
    },
    fetch: async (url, init) => {
      requests.push({url, init});
      if (reply) return reply(url, init);
      return {ok: true, blob: async () => new Blob([new Uint8Array(235)], {type: 'audio/ogg'})};
    },
    setTimeout, clearTimeout, setInterval: () => 0, AbortController, Blob,
  });
  vm.runInContext(source, ctx);
  vm.runInContext('key = "12345678"; setConnected(true);', ctx);
  const box = vm.runInContext('audioPreview({id:42,audio_duration_ms:20}, "Demo")', ctx);
  const audio = box.children.find(x => x.tag === 'audio');
  const button = box.children.find(x => x.tag === 'button');
  const hint = box.children.find(x => x.tag === 'p');
  const link = box.children.find(x => x.tag === 'a');
  return {ctx, audio, button, hint, link, urls, revoked, requests, events};
}
(async () => {
  const p = setup({playDenied:true});
  assert.equal(p.requests.length, 0, 'never fetch audio on page load');
  await p.button.onclick();
  assert.equal(p.requests[0].url, '/api/audio?id=42');
  assert.equal(p.requests[0].init.headers['X-Memo-Key'], '12345678');
  assert.equal(p.requests[0].init.cache, 'no-store');
  assert.equal(p.audio.classList.contains('hidden'), false, 'gesture rejection retains controls');
  assert.equal(p.link.href, p.audio.src);
  vm.runInContext('setConnected(false)', p.ctx);
  assert.match(p.hint.textContent, /已载入/);
  assert.equal(p.audio.src, p.urls[0].u, 'disconnect keeps the in-page clip');
  p.events.pagehide({persisted:true});
  assert.equal(p.revoked.length, 0, 'back/forward cache keeps its document URLs');
  assert.equal(p.audio.paused, true);
  p.events.pagehide({persisted:false});
  assert.deepEqual(p.revoked, [p.urls[0].u]);
  assert.equal(p.audio.src, undefined);

  const offline = setup();
  vm.runInContext('setConnected(false)', offline.ctx);
  await offline.button.onclick();
  assert.equal(offline.requests.length, 0);
  assert.equal(offline.button.disabled, true);
  vm.runInContext('setConnected(true)', offline.ctx);
  assert.equal(offline.button.disabled, false);

  for (const status of [401,404,500]) {
    const error = setup({reply:async () => ({ok:false,status})});
    await error.button.onclick();
    assert.equal(error.urls.length, 0);
    assert.equal(error.button.disabled, false, 'failure permits retry');
    assert.match(error.hint.textContent, status === 401 ? /密码已失效/ : status === 404 ? /替换或删除/ : /读取失败/);
  }
  const unsupported = setup({supported:false});
  await unsupported.button.onclick();
  assert.equal(unsupported.audio.classList.contains('hidden'), true);
  assert.equal(unsupported.link.classList.contains('hidden'), false);
  assert.match(unsupported.hint.textContent, /不支持/);

  const invalid = setup({reply:async () => ({ok:true,blob:async () => new Blob(['invalid'],{type:'text/html'})})});
  await invalid.button.onclick();
  assert.equal(invalid.urls.length, 0);
  assert.match(invalid.hint.textContent, /不完整/);

  let finish;
  const pending = setup({reply: () => new Promise(resolve => { finish = resolve; })});
  const job = pending.button.onclick();
  vm.runInContext('disposePreviews()', pending.ctx);
  assert.equal(pending.requests[0].init.signal.aborted, true);
  finish({ok:true,blob:async () => new Blob([new Uint8Array(235)],{type:'audio/ogg'})});
  await job;
  assert.equal(pending.urls.length, 0, 'late response cannot resurrect a disposed player');
  console.log('Portal audio: auth headers, offline playback, retries, unsupported media, cancellation and URL lifetime PASS');
})().catch(error => { console.error(error); process.exitCode = 1; });
