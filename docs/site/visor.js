/* The hero headset's front as a convex dome: the screen, dark glass and glare, lit by its curve. */
(() => {
const figure = document.querySelector('.hero-media');
const media = figure && figure.querySelector('.visor-face > img, .visor-face > video');
const canvas = document.createElement('canvas');
const gl = media && canvas.getContext('webgl', { alpha: true, premultipliedAlpha: true, antialias: true });
if (!gl) return;
const reduced = matchMedia('(prefers-reduced-motion: reduce)');
// A hero video's data-loading is "sheet cols frames fps": the game's loading screen, a sprite sheet preloaded in the head, plays on the screen until the clip can play through.
const spec = media.tagName === 'VIDEO' && (media.dataset.loading || '').split(' ')[0] && media.dataset.loading.split(' ');
const sheet = spec && new Image(), tile = document.createElement('canvas');
let source = media, phase = 'play', t0 = 0, ready = false;
const path = new Path2D(figure.querySelector('.visor-rim path').getAttribute('d'));
const SCREEN = [.925, .87], MARGIN = [.0375, .065], MASK_W = 2048, MASK_H = 1024;

function maskCanvas(draw) {
  const c = document.createElement('canvas'); c.width = MASK_W; c.height = MASK_H;
  const x = c.getContext('2d'); x.scale(MASK_W / 200, MASK_H / 100); draw(x); return c;
}
function atScreen(x) { x.translate(200 * MARGIN[0], 100 * MARGIN[1]); x.scale(SCREEN[0], SCREEN[1]); }
// Red is the glass, green the screen.
const shapes = maskCanvas(x => {
  x.fillStyle = '#000'; x.fillRect(0, 0, 200, 100); x.globalCompositeOperation = 'lighter';
  x.fillStyle = '#f00'; x.fill(path); atScreen(x);
  x.fillStyle = '#0f0'; x.fill(path);
});
const shade = maskCanvas(x => {
  x.fillStyle = '#fff'; x.fillRect(0, 0, 200, 100); atScreen(x); x.clip(path);
  x.shadowColor = '#000'; x.shadowBlur = 90; x.strokeStyle = '#000'; x.lineWidth = 3; x.stroke(path);
});

// Red is the wide glow of the glass lip, green the thin metal edge behind it.
const lip = maskCanvas(x => {
  x.fillStyle = '#000'; x.fillRect(0, 0, 200, 100); x.clip(path); x.globalCompositeOperation = 'lighter';
  x.save(); x.shadowColor = '#f00'; x.shadowBlur = 70; x.strokeStyle = '#f00'; x.lineWidth = 3; x.stroke(path); x.restore();
  x.strokeStyle = '#0f0'; x.lineWidth = 1; x.stroke(path);
});

const compile = (type, source) => { const s = gl.createShader(type); gl.shaderSource(s, source); gl.compileShader(s); return s; };
const program = gl.createProgram();
gl.attachShader(program, compile(gl.VERTEX_SHADER, `
  attribute vec2 p; uniform float yaw, pitch; varying vec2 uv; varying vec3 n;
  const float PERSPECTIVE = 440., BULGE = 30.;
  void main() {
    // Curved side to side, and nearly even top to bottom, rolling off only at the very edges.
    float across = max(0., 1. - .85 * p.x * p.x / 10000.), v = p.y / 50., down = 1. - .4 * v * v * v * v;
    float z = BULGE * across * down;
    vec3 normal = normalize(vec3(BULGE * 1.7 * p.x / 10000. * down, BULGE * across * 1.6 * v * v * v / 50., 1.));
    float cy = cos(yaw), sy = sin(yaw), cp = cos(pitch), sp = sin(pitch);
    vec3 a = vec3(p.x * cy + z * sy, p.y, -p.x * sy + z * cy);
    a = vec3(a.x, a.y * cp - a.z * sp, a.y * sp + a.z * cp);
    n = vec3(normal.x * cy + normal.z * sy, normal.y, -normal.x * sy + normal.z * cy);
    n = vec3(n.x, n.y * cp - n.z * sp, n.y * sp + n.z * cp);
    uv = vec2(p.x / 200. + .5, p.y / 100. + .5);
    gl_Position = vec4(a.x / 150., -a.y / 75., 0., (PERSPECTIVE - a.z) / PERSPECTIVE);
  }`));
gl.attachShader(program, compile(gl.FRAGMENT_SHADER, `
  precision mediump float;
  uniform sampler2D screen, shapes, shade, lip; uniform float glare, sheen; varying vec2 uv; varying vec3 n;
  const vec2 M = vec2(${MARGIN.join(', ')}), S = vec2(${SCREEN.join(', ')});
  const float ZOOM = 1.15, LIFT = .09;
  void main() {
    vec3 m = texture2D(shapes, uv).rgb, N = normalize(n);
    vec2 e = texture2D(lip, uv).rg;
    // Black glass that catches the room along its curve: pale along the bottom and nose, dark at the top.
    float low = smoothstep(.5, 1., uv.y), side = smoothstep(.34, .5, abs(uv.x - .5));
    float catchLight = pow(e.r, 1.3) * (.12 + .62 * low + .16 * side * (1. - low));
    vec3 glass = vec3(.006, .008, .013) + vec3(.62, .66, .74) * catchLight;
    float edge = texture2D(shade, uv).r;
    // The picture's lower edge melts a little into the black glass.
    // The 2:1 picture covers the slightly wider screen.
    vec2 at = (uv - M) / S; at.y = .5 + (at.y - .5) * S.y / S.x;
    // Zoomed in and lifted so the reticle sits in the middle of the glass above the nose; the lost bottom is HUD.
    at = .5 + (at - .5) / ZOOM; at.y += LIFT;
    vec3 picture = texture2D(screen, at).rgb * edge * mix(1., smoothstep(.05, .7, edge), low);
    vec3 c = mix(glass, picture, m.g);
    // A faint reflection of the room over the whole front, stronger where the dome turns away.
    c += vec3(.5, .55, .68) * .1 * smoothstep(.1, .75, length(N.xy));
    // A bright hairline along the top of the dome, and a dark outer edge with metal only along the bottom.
    float hairline = smoothstep(.72, .98, e.r) * smoothstep(.3, .05, uv.y) * smoothstep(.48, .2, abs(uv.x - .46));
    c = mix(c, vec3(.92, .95, 1.), hairline * .85);
    c = mix(c, mix(vec3(.01), vec3(.72, .74, .78), low), e.g);
    // The long reflection across the top: a sharp lower edge following the curve, soft above, fading toward its ends.
    float t = (uv.x - .5 - (glare + .16) * .35) / .36;
    float bottom = .135 + sheen * .35 + .1 * t * t, top = .05 + sheen * .35 + .1 * t * t;
    float band = smoothstep(bottom + .004, bottom - .004, uv.y) * smoothstep(top - .02, top + .05, uv.y) * smoothstep(1., .7, abs(t));
    c = mix(c, vec3(.76, .82, .95), band * (.16 + .34 * (1. - t * t)));
    gl_FragColor = vec4(c * m.r, m.r);
  }`));
gl.linkProgram(program); gl.useProgram(program);
const u = name => gl.getUniformLocation(program, name);

// A grid over the visor's 200x100 canvas.
const COLS = 80, ROWS = 40, vertices = [];
for (let r = 0; r < ROWS; r++) for (let q = 0; q < COLS; q++) for (const [dq, dr] of [[0, 0], [1, 0], [0, 1], [0, 1], [1, 0], [1, 1]])
  vertices.push((q + dq) / COLS * 200 - 100, (r + dr) / ROWS * 100 - 50);
gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
gl.enableVertexAttribArray(gl.getAttribLocation(program, 'p'));
gl.vertexAttribPointer(gl.getAttribLocation(program, 'p'), 2, gl.FLOAT, false, 0, 0);

function texture(unit, name, source) {
  gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source); gl.uniform1i(u(name), unit);
}
gl.uniform1f(u('yaw'), -32 * Math.PI / 180); gl.uniform1f(u('pitch'), -10 * Math.PI / 180);

function frameAt(n) {
  const cols = +spec[1], w = sheet.naturalWidth / cols, h = sheet.naturalHeight / Math.ceil(+spec[2] / cols);
  tile.width = w; tile.height = h; tile.getContext('2d').drawImage(sheet, n % cols * w, Math.floor(n / cols) * h, w, h, 0, 0, w, h);
}
const buffered = () => media.duration > 0 && media.buffered.length > 0 && media.buffered.end(media.buffered.length - 1) >= media.duration - .25;
// Loop the loading screen to its last frame, start the clip from 0 behind it, and cut over once the clip is playing.
function tick(now) {
  if (phase === 'play' || phase === 'still') return;
  if (!t0) t0 = now;
  const n = Math.floor((now - t0) * +spec[3] / 1000);
  if (phase === 'load') {
    if (n >= +spec[2] && (ready || buffered())) { media.currentTime = 0; media.play().catch(() => {}); phase = 'start'; t0 = now; }
    else frameAt(n % +spec[2]);
  } else if ((!media.seeking && media.readyState >= 3 && media.currentTime > 0) || now - t0 > 3000) { phase = 'play'; source = media; }
}

function draw() {
  tick(performance.now());
  const box = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const w = Math.round(canvas.offsetWidth * dpr), h = Math.round(canvas.offsetHeight * dpr);
  if (w && (canvas.width !== w || canvas.height !== h)) { canvas.width = w; canvas.height = h; }
  gl.viewport(0, 0, canvas.width, canvas.height);
  const style = getComputedStyle(figure.querySelector('.visor-glass'));
  // --glare runs 40% to 46% and --sheen -8% to -13% with the headset's float.
  gl.uniform1f(u('glare'), -.16 + (parseFloat(style.getPropertyValue('--glare')) - 40) * .012);
  gl.uniform1f(u('sheen'), (parseFloat(style.getPropertyValue('--sheen')) + 8) * .01);
  if (source !== media || (media.tagName === 'VIDEO' && media.readyState >= 2)) { gl.activeTexture(gl.TEXTURE0); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source); }
  gl.clearColor(0, 0, 0, 0); gl.clear(gl.COLOR_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES, 0, vertices.length / 2);
  return box;
}
let onScreen = true;
function frame() { if (!onScreen || document.hidden) { looping = false; return; } draw(); requestAnimationFrame(frame); }
let looping = false;
function start() { if (reduced.matches) { draw(); return; } if (!looping) { looping = true; requestAnimationFrame(frame); } }

function begin() {
  texture(0, 'screen', source); texture(1, 'shapes', shapes); texture(2, 'shade', shade); texture(3, 'lip', lip);
  canvas.className = 'visor-dome'; canvas.setAttribute('aria-hidden', 'true');
  figure.appendChild(canvas); figure.classList.add('is-convex');
  new IntersectionObserver(entries => { onScreen = entries[0].isIntersecting; if (onScreen) start(); }).observe(figure);
  document.addEventListener('visibilitychange', () => { if (!document.hidden && onScreen) start(); });
  addEventListener('resize', start);
  start();
}
function loadingFirst() {
  source = tile; phase = 'load'; media.addEventListener('canplaythrough', () => { ready = true; }, { once: true });
  if (reduced.matches) {
    // One loading frame, then the poster.
    phase = 'still'; frameAt(Math.floor(+spec[2] * .4));
    const poster = new Image(); poster.onload = () => { source = poster; start(); }; poster.src = media.poster;
    media.addEventListener('loadeddata', () => { if (poster.src) poster.src = media.poster; }, { once: true });
  } else frameAt(0);
  begin();
}
if (sheet && media.readyState < 4) { sheet.onload = loadingFirst; sheet.onerror = () => media.addEventListener('loadeddata', begin, { once: true }); sheet.src = spec[0]; }
else if (media.tagName === 'VIDEO') { if (media.readyState >= 2) begin(); else media.addEventListener('loadeddata', begin, { once: true }); }
else if (media.complete && media.naturalWidth) begin();
else media.addEventListener('load', begin, { once: true });
})();
