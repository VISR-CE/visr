/* Each scene supplies the same shaped-video, still, mask, and poster filenames. */
(() => {
'use strict';
const stage = document.getElementById('look-stage');
if (!stage) return;
const box = document.getElementById('look-box'), canvas = document.getElementById('look-gl');
const scenes = JSON.parse(box.dataset.scenes), scene = scenes[Math.floor(Math.random() * scenes.length)];
const sceneAsset = name => `${box.dataset.assetBase}${scene.folder}/${name}`;
const v = document.getElementById('look-video'), poster = document.getElementById('look-poster');
const reduced = matchMedia('(prefers-reduced-motion: reduce)');
const TOUCH = matchMedia('(pointer: coarse)').matches, FOCAL = 700;
// Narrow views shorten the focal length so a phone still sees about 70° across instead of a 30° slice.
const focal = (w, h) => Math.min(FOCAL, Math.max(w * .72, h * .55));
let sideways = 0, driftDirection = 1, dragVelocity = 0, movedAt = 0, inputAt = -Infinity, driftAt = 0, drift = 0, pitchDrift = 0, motion = false, head = null, yawOff = 0, pitchOff = 0, vx = 0, vy = 0;
let fresh = true, downAt = 0, moved = 0, hoverX = 0, glareX = 40, near = false, visible = false, loaded = false, running = false;
let yaw = scene.yaw ?? 0, pitch = scene.pitch ?? 22, last = null, mode = 'idle', idle, cx = 0, cy = 0, tcx = null, tcy = null;
let hasStill = false, hasMask = false, videoReady = false;
const onScreen = () => visible || mode !== 'idle';
const autoplay = () => !reduced.matches && !navigator.connection?.saveData;
// Video stays detached from layout, which avoids Safari's offscreen pause loop.
v.muted = true;
v.crossOrigin = 'anonymous';
const sources = [
  ...(v.canPlayType('video/mp4; codecs="hvc1"') ? ['shape.mp4'] : []),
  'shape-h264.mp4'
];
let sourceIndex = 0;
function loadVideo() {
  if (scene.still || !autoplay() || !gl || !near || v.hasAttribute('src')) return;
  v.src = sceneAsset(sources[sourceIndex]);
  v.load();
  playVideo();
}
function playVideo() {
  if (onScreen() && autoplay() && v.hasAttribute('src') && !document.hidden) v.play().catch(() => {});
}
v.addEventListener('error', () => {
  videoReady = false;
  if (++sourceIndex < sources.length) { v.removeAttribute('src'); loadVideo(); }
});
v.addEventListener('loadeddata', () => { fresh = true; videoReady = true; });
reduced.addEventListener('change', () => { if (!autoplay()) v.pause(); else { loadVideo(); playVideo(); } });
navigator.connection?.addEventListener('change', () => { if (!autoplay()) v.pause(); else { loadVideo(); playVideo(); } });
let gl;
try { gl = canvas.getContext('webgl2', { antialias: false, alpha: false }); } catch (_) {}
let u, videoTexture;
function texture(unit) {
  const t = gl.createTexture();
  gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, t);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([5, 14, 20, 255]));
  return t;
}
function shader(type, source) {
  const s = gl.createShader(type); gl.shaderSource(s, source); gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s));
  return s;
}
if (gl) {
  try {
    const p = gl.createProgram();
    gl.attachShader(p, shader(gl.VERTEX_SHADER, `#version 300 es
      void main(){vec2 c=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(c*2.-1.,0,1);}`));
    gl.attachShader(p, shader(gl.FRAGMENT_SHADER, `#version 300 es
      precision highp float;
      uniform sampler2D t, s, m; uniform bool moving; uniform float band;
      uniform vec2 center; uniform float focal, cy, sy, cp, sp; out vec4 o;
      void main(){
        vec3 d=normalize(vec3(gl_FragCoord.xy-center,-focal));
        d=vec3(d.x,cp*d.y-sp*d.z,sp*d.y+cp*d.z);
        d=vec3(cy*d.x-sy*d.z,d.y,sy*d.x+cy*d.z);
        vec2 uv=vec2(atan(d.x,-d.z)/6.2831853+.5,acos(clamp(d.y,-1.,1.))/3.14159265);
        o=texture(s,uv);
        if(moving && uv.y<band) o=mix(o,texture(t,vec2(uv.x,uv.y/band)),texture(m,uv).r);
      }`));
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
    gl.useProgram(p); u = n => gl.getUniformLocation(p, n);
    gl.uniform1i(u('t'), 0); gl.uniform1i(u('s'), 1); gl.uniform1i(u('m'), 2);
    videoTexture = texture(0);
  } catch (_) { gl = null; }
}
if (!gl) box.classList.add('look-static');
canvas.addEventListener('webglcontextlost', e => { e.preventDefault(); gl = null; box.classList.add('look-static'); canvas.style.visibility = 'hidden'; v.pause(); });
function loadScene() {
  if (loaded) return;
  loaded = true;
  poster.src = `look/${scene.folder}/poster.jpg`;
  stage.setAttribute('aria-label', `${scene.label}. Use arrow keys to look around, Enter to expand, and Escape to close.`);
  if (!gl) { stage.setAttribute('aria-label', `${scene.label}, still image. Enter to expand and Escape to close.`); return; }
  for (const [unit, name] of [[1, 'still8k.jpg'], [2, 'mask.png']]) {
    const t = texture(unit), pic = new Image();
    pic.onload = () => {
      if (!gl) return;
      gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, t);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, pic);
      if (unit === 1) { hasStill = true; canvas.style.visibility = 'visible'; } else hasMask = true;
      gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, videoTexture);
    };
    pic.crossOrigin = 'anonymous';
    pic.src = sceneAsset(name);
  }
  gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, videoTexture);
  loadVideo();
}
function size() {
  const dpr = Math.min(devicePixelRatio || 1, 2), r = stage.getBoundingClientRect();
  const w = mode === 'idle' ? r.width : innerWidth, h = mode === 'idle' ? r.height : innerHeight;
  canvas.style.width = w + 'px'; canvas.style.height = h + 'px';
  const W = Math.round(w * dpr), H = Math.round(h * dpr);
  if (canvas.width !== W || canvas.height !== H) { canvas.width = W; canvas.height = H; }
  return { dpr, w, h };
}
function driftView() {
  const now = performance.now(), dt = Math.min(.1, (now - driftAt) / 1000); driftAt = now;
  if (last) { drift = pitchDrift = 0; return; }
  const on = onScreen() && !reduced.matches && !motion && now - inputAt > 600, speed = scene.drift ?? { yaw: 10, pitch: 0 };
  const room = speed.stop === undefined ? 1 : Math.max(0, Math.min(1, (speed.stop - pitch) * Math.sign(speed.pitch) / 8));
  const want = on ? speed.yaw * driftDirection : 0, wantPitch = on && room > .02 ? speed.pitch * room : 0;
  drift += (want - drift) * Math.min(1, dt * 1.5);
  pitchDrift += (wantPitch - pitchDrift) * Math.min(1, dt * 1.5);
  if (want || Math.abs(drift) > .01) yaw += drift * dt; else drift = 0;
  if (wantPitch || Math.abs(pitchDrift) > .01) pitch = Math.min(88, Math.max(-88, pitch + pitchDrift * dt)); else pitchDrift = 0;
}
function moveGlare() {
  if (mode !== 'idle') return;
  const r = box.getBoundingClientRect(), want = 15 + 70 * (r.top + r.height / 2) / innerHeight + 30 * hoverX;
  glareX += (want - glareX) * (reduced.matches ? 1 : .08);
  document.getElementById('look-glare').style.setProperty('--glare', `${glareX.toFixed(1)}%`);
}
function render() {
  driftView();
  moveGlare();
  if (motion && head) {
    const ty = head[0] + yawOff;
    yaw += (((ty - yaw + 540) % 360 + 360) % 360 - 180) * .25;
    pitch += (Math.min(88, Math.max(-88, head[1] + pitchOff)) - pitch) * .25;
  } else if (TOUCH && !last && (Math.abs(vx) > .01 || Math.abs(vy) > .01)) {
    if (mode === 'full') pitch = Math.min(88, Math.max(-88, pitch + vy));
    vx *= .93; vy *= .93;
  }
  if (gl && hasStill) {
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, videoTexture);
    if (v.readyState >= 2 && fresh) { gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, v); fresh = !v.requestVideoFrameCallback; }
    const { dpr, w, h } = size();
    if (mode === 'idle') { tcx = w / 2; tcy = h / 2; cx = tcx; cy = tcy; }
    else { cx += (tcx - cx) * .18; cy += (tcy - cy) * .18; }
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.uniform1i(u('moving'), videoReady && hasMask && autoplay());
    gl.uniform1f(u('band'), scene.videoBand ?? .5);
    gl.uniform2f(u('center'), cx * dpr, (h - cy) * dpr); gl.uniform1f(u('focal'), focal(w, h) * dpr);
    const a = yaw * Math.PI / 180, b = pitch * Math.PI / 180;
    gl.uniform1f(u('cy'), Math.cos(a)); gl.uniform1f(u('sy'), Math.sin(a)); gl.uniform1f(u('cp'), Math.cos(b)); gl.uniform1f(u('sp'), Math.sin(b));
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  const deg = ((Math.round(yaw) % 360) + 360) % 360;
  document.getElementById('look-heading').textContent = String(deg).padStart(3, '0') + '°';
  document.getElementById('look-ticks').style.backgroundPosition = (-yaw * 4) + 'px 0';
}
function frame() {
  if ((!near && mode === 'idle') || document.hidden) { running = false; return; }
  render(); requestAnimationFrame(frame);
}
function startFrames() { if (!running) { running = true; requestAnimationFrame(frame); } }
let previousOverflow = '', scrollLocked = false, persistentFull = false;
function lockScroll() {
  if (scrollLocked) return;
  previousOverflow = document.documentElement.style.overflow; scrollLocked = true;
  document.documentElement.style.overflow = 'hidden';
}
function unlockScroll() {
  if (scrollLocked) document.documentElement.style.overflow = previousOverflow;
  scrollLocked = false; persistentFull = false;
}
function exitFull() {
  if (motion) document.getElementById('look-motion').click();
  unlockScroll(); vx = vy = 0; last = null;
  collapse(); stage.focus({ preventScroll: true });
}
document.addEventListener('visibilitychange', () => { if (!document.hidden && near) startFrames(); else v.pause(); });
function visorPath(x, y, w, h, k) {
  const cr = k * Math.min(.16 * w, .42 * h), a = k * .05 * h, cx = x + w / 2, n = v => +v.toFixed(1);
  const p = [
    'M', x + cr, y + a,
    'C', x + w * .33, y, x + w * .67, y, x + w - cr, y + a,
    'C', x + w - cr * .4, y + a * 1.6, x + w, y + k * .16 * h, x + w, y + .42 * h * (k ? 1 : 1),
    'C', x + w, y + h - k * .24 * h, x + w - k * .11 * w, y + h, x + w - k * .25 * w, y + h,
    'C', x + w - k * .37 * w, y + h, cx + k * .11 * w, y + h - k * .21 * h, cx, y + h - k * .21 * h,
    'C', cx - k * .11 * w, y + h - k * .21 * h, x + k * .37 * w, y + h, x + k * .25 * w, y + h,
    'C', x + k * .11 * w, y + h, x, y + h - k * .24 * h, x, y + .42 * h,
    'C', x, y + k * .16 * h, x + cr * .4, y + a * 1.6, x + cr, y + a, 'Z'];
  return p.map(t => typeof t === 'number' ? n(t) : t).join(' ');
}
let cur = null, anim = 0;
function setShape(c) { cur = c; stage.style.clipPath = `path("${visorPath(c.x, c.y, c.w, c.h, c.k)}")`; }
function idleShape() {
  const r = box.getBoundingClientRect();
  setShape({ x: 0, y: 0, w: r.width, h: r.height, k: 1 });
  const f = document.getElementById('look-frame'); f.setAttribute('viewBox', `0 0 ${r.width} ${r.height}`);
  document.getElementById('look-frame-path').setAttribute('d', visorPath(0, 0, r.width, r.height, 1));
  document.getElementById('look-glare-rim').setAttribute('viewBox', `0 0 ${r.width} ${r.height}`);
  document.getElementById('look-glare-rim').style.filter = `blur(${(r.width * .0125).toFixed(1)}px)`;
  document.getElementById('look-glare-edge').setAttribute('d', visorPath(0, 0, r.width, r.height, 1));
  document.getElementById('look-glare-edge').setAttribute('stroke-width', (r.width * .03).toFixed(1));
}
function morph(to, ms, end) {
  ms = reduced.matches ? 0 : ms;
  const id = ++anim, from = { ...cur }, t0 = performance.now();
  (function step(now) {
    if (id !== anim) return;
    const t = Math.min(1, ms ? (now - t0) / ms : 1), e = 1 - Math.pow(1 - t, 3), c = {};
    for (const key in to) c[key] = from[key] + (to[key] - from[key]) * e;
    setShape(c);
    if (t < 1) requestAnimationFrame(step); else if (end) end();
  })(t0);
}
function expand(persistent = false) {
  persistentFull ||= persistent;
  if (persistent) { clearTimeout(idle); lockScroll(); }
  if (mode === 'full') return;
  stage.setAttribute('role', 'dialog'); stage.setAttribute('aria-modal', 'true');
  const r = box.getBoundingClientRect();
  if (mode === 'idle') {
    stage.classList.add('fixed'); setShape({ x: r.left, y: r.top, w: r.width, h: r.height, k: 1 });
    cx = r.left + r.width / 2; cy = r.top + r.height / 2;
  }
  mode = 'full'; tcx = innerWidth / 2; tcy = innerHeight / 2;
  render();
  morph({ x: 0, y: 0, w: innerWidth, h: innerHeight, k: 0 }, 450, () => { stage.style.clipPath = 'none'; });
}
function collapse() {
  if (last || mode !== 'full') return;
  mode = 'closing'; unlockScroll();
  stage.setAttribute('role', 'group'); stage.removeAttribute('aria-modal');
  const r = box.getBoundingClientRect();
  if (stage.style.clipPath === 'none') setShape({ x: 0, y: 0, w: innerWidth, h: innerHeight, k: 0 });
  tcx = r.left + r.width / 2; tcy = r.top + r.height / 2;
  morph({ x: r.left, y: r.top, w: r.width, h: r.height, k: 1 }, 450, () => { mode = 'idle'; stage.classList.remove('fixed'); idleShape(); render(); });
}
addEventListener('resize', () => { if (mode === 'idle') idleShape(); else if (mode === 'full') { ++anim; setShape({ x: 0, y: 0, w: innerWidth, h: innerHeight, k: 0 }); stage.style.clipPath = 'none'; render(); } });
function arm() { clearTimeout(idle); idle = setTimeout(collapse, 2000); }
function look(dy, dp) { inputAt = performance.now(); if (!TOUCH) { expand(); if (!persistentFull) arm(); } yaw += dy; if (motion) { yawOff += dy; pitchOff += dp; document.getElementById('look-hint').style.display = 'none'; return; } if (!TOUCH || mode === 'full') pitch = Math.min(88, Math.max(-88, pitch + dp)); document.getElementById('look-hint').style.display = 'none'; }
stage.addEventListener('pointerdown', e => { inputAt = performance.now(); if (e.target.closest('button')) return; vx = vy = 0; sideways = 0; dragVelocity = 0; movedAt = performance.now(); downAt = performance.now(); moved = 0; last = [e.clientX, e.clientY]; stage.setPointerCapture(e.pointerId); playVideo(); });
stage.addEventListener('pointermove', e => { const box0 = box.getBoundingClientRect(); hoverX = (e.clientX - box0.left) / box0.width - .5; });
stage.addEventListener('pointerleave', () => { hoverX = 0; });
stage.addEventListener('pointermove', e => { if (!last) return; const moveNow = performance.now(), dx = (last[0] - e.clientX) * .12, dp = (e.clientY - last[1]) * .12; moved += Math.abs(e.clientX - last[0]) + Math.abs(e.clientY - last[1]); sideways += Math.abs(e.clientX - last[0]); if (TOUCH && mode === 'idle' && sideways >= 8) expand(true); vx = dx; vy = dp; if (moveNow > movedAt) dragVelocity = dragVelocity * .7 + Math.max(-240, Math.min(240, dx / ((moveNow - movedAt) / 1000))) * .3; movedAt = moveNow; look(dx, dp); last = [e.clientX, e.clientY]; });
const up = () => { if (last) { drift = performance.now() - movedAt < 80 ? dragVelocity : 0; if (Math.abs(drift) > 1) driftDirection = Math.sign(drift); inputAt = -Infinity; } dragVelocity = 0; last = null; if (!TOUCH && !persistentFull) { clearTimeout(idle); collapse(); } };
stage.addEventListener('pointerup', () => { if (TOUCH && mode === 'idle' && moved < 8 && performance.now() - downAt < 350) { expand(true); } up(); });
stage.addEventListener('pointercancel', () => { vx = vy = 0; dragVelocity = 0; up(); });
function onTilt(e) {
  if (e.alpha == null || e.beta == null || e.gamma == null) return;
  const r = Math.PI / 180, A = e.alpha * r, B = e.beta * r, G = e.gamma * r;
  const sA = Math.sin(A), cA = Math.cos(A), sB = Math.sin(B), cB = Math.cos(B), sG = Math.sin(G), cG = Math.cos(G);
  const fx = -(cG * sA * sB + cA * sG), fy = -(sA * sG - cA * cG * sB), fz = -(cB * cG);
  const h = [Math.atan2(fx, fy) / r, Math.asin(Math.max(-1, Math.min(1, fz))) / r];
  if (!head) { yawOff = yaw - h[0]; pitchOff = 0; }
  head = h;
}
v.addEventListener('pause', () => { if (!document.hidden && onScreen()) playVideo(); });
document.addEventListener('visibilitychange', () => { if (!document.hidden && near) playVideo(); });
document.getElementById('look-motion').onclick = async function () {
  if (motion) { motion = false; head = null; removeEventListener('deviceorientation', onTilt); this.setAttribute('aria-pressed', 'false'); this.textContent = 'LOOK WITH MOTION'; return; }
  if (window.DeviceOrientationEvent && DeviceOrientationEvent.requestPermission) { try { if (await DeviceOrientationEvent.requestPermission() !== 'granted') { this.textContent = 'MOTION NOT ALLOWED'; return; } } catch (err) { this.textContent = 'MOTION UNAVAILABLE'; return; } }
  else if (!window.DeviceOrientationEvent) { this.textContent = 'MOTION UNAVAILABLE'; return; }
  playVideo(); if (mode !== 'full') expand(true); motion = true; head = null; vx = vy = 0; addEventListener('deviceorientation', onTilt); this.setAttribute('aria-pressed', 'true'); this.textContent = 'MOVE YOUR PHONE TO LOOK'; document.getElementById('look-hint').style.display = 'none';
};
if (TOUCH) { stage.classList.add('touch'); document.querySelector('#look-hint span').textContent = 'DRAG, OR TAP TO ENTER'; }
document.getElementById('look-close').onclick = () => { exitFull(); };
if (v.requestVideoFrameCallback) { const onFrame = () => { fresh = true; v.requestVideoFrameCallback(onFrame); }; v.requestVideoFrameCallback(onFrame); }
stage.addEventListener('keydown', e => { inputAt = performance.now();
  if (e.key === 'Escape') { exitFull(); return; }
  if (e.key === 'Tab' && mode === 'full') {
    const focusable = [stage, ...stage.querySelectorAll('button')].filter(el => getComputedStyle(el).display !== 'none');
    const index = focusable.indexOf(document.activeElement);
    const next = (index + (e.shiftKey ? -1 : 1) + focusable.length) % focusable.length;
    e.preventDefault(); focusable[next].focus(); return;
  }
  if (e.target.closest('button')) return;
  if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); expand(true); return; } const d = { ArrowLeft: [-5, 0], ArrowRight: [5, 0], ArrowUp: [0, 5], ArrowDown: [0, -5] }[e.key]; if (d) { e.preventDefault(); look(d[0], d[1]); } });

idleShape();
new ResizeObserver(() => { if (mode === 'idle') idleShape(); }).observe(box);
const observer = new IntersectionObserver(entries => {
  near = entries[0].isIntersecting;
  if (near) { loadScene(); startFrames(); }
}, { rootMargin: '100px' });
observer.observe(box);
// Assets load just ahead of the scroll; the video and idle motion wait until the scene is really on screen.
const sightObserver = new IntersectionObserver(entries => {
  visible = entries[0].isIntersecting;
  if (visible) playVideo(); else if (mode === 'idle') v.pause();
}, { threshold: .2 });
sightObserver.observe(box);

})();
