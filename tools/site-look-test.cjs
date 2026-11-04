const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../docs/site/look.js'), 'utf8');

function harness({reduced = true, saveData = false, webgl = false, hevc = true, av1 = null, caps = null} = {}) {
  const images = [], timers = [], raf = [];
  const gl = new Proxy({}, {get: (_, k) => k === 'getShaderParameter' || k === 'getProgramParameter' ? () => true : () => ({})});
  const elements = {};
  for (const id of ['look-stage', 'look-box', 'look-gl', 'look-video', 'look-poster', 'look-frame', 'look-frame-path', 'look-heading', 'look-ticks', 'look-hint', 'look-motion', 'look-close', 'look-glare', 'look-glare-rim', 'look-glare-edge']) {
    elements[id] = {
      style: {setProperty() {}}, handlers: {}, attributes: {}, classes: new Set(), textContent: '',
      dataset: {scenes: JSON.stringify([{folder:'lifeboat',label:'Lifeboats over Halo',...(av1?{av1}:{})}]), assetBase:'https://media.visr.gg/look/'},
      setAttribute(k,v) { this.attributes[k] = v; },
      hasAttribute(k) { return Object.hasOwn(this.attributes,k) || k === 'src' && Boolean(this.src); },
      removeAttribute(k) { delete this.attributes[k]; if (k === 'src') delete this.src; },
      addEventListener(k, fn) { (this.handlers[k] ||= []).push(fn); },
      getBoundingClientRect() { return {left:30,top:500,width:800,height:400}; },
      getContext() { return webgl ? gl : null; },
      canPlayType() { return hevc ? 'probably' : ''; }, load() {}, pause() {},
      play() { this.plays = (this.plays || 0) + 1; return Promise.resolve(); },
      focus() {doc.activeElement=this;}, setPointerCapture() {},
      querySelectorAll() {return [elements['look-motion'],elements['look-close']];},
    };
    const el = elements[id];
    el.classList = {add(k) {el.classes.add(k);},remove(k) {el.classes.delete(k);}};
  }
  const doc = {hidden:false, documentElement:{style:{overflow:''}}, getElementById:k=>elements[k], querySelector:()=>({textContent:''}), addEventListener() {}};
  const intersections = [];
  const context = {document:doc, navigator:{connection:{saveData,addEventListener(){}},...(caps?{mediaCapabilities:{decodingInfo:async()=>caps}}:{})}, matchMedia:q=>({matches:q.includes('coarse') ? false : reduced,addEventListener(){}}),
    window:{}, innerWidth:1200,innerHeight:800,devicePixelRatio:1,performance:{now:()=>0},getComputedStyle:el=>({display:el===elements['look-motion']?'none':'flex'}),
    addEventListener(){},removeEventListener(){},requestAnimationFrame:fn=>raf.push(fn),
    setTimeout:fn=>(timers.push(fn),timers.length),clearTimeout:id=>{if(id)timers[id-1]=null;},
    ResizeObserver:class {observe(){}},IntersectionObserver:class {constructor(fn){intersections.push(fn);}observe(){}},
    Image:class {set src(v){assert.equal(this.crossOrigin,'anonymous');images.push(v);}}, Uint8Array,Math,String,console,
  };
  vm.runInNewContext(source, context);
  function key(key, button = false) {for (const fn of elements['look-stage'].handlers.keydown) fn({key,target:{closest:()=>button},preventDefault(){}});}
  return {elements,doc,key,images,timers,raf,near:()=>intersections[0]([{isIntersecting:true}]),visible:(isIntersecting=true)=>intersections[1]([{isIntersecting}])};
}
function check(name, fn) {try {fn();console.log('PASS',name);}catch(e){console.error('FAIL',name,e.message);process.exitCode=1;}}
async function checkAsync(name, fn) {try {await fn();console.log('PASS',name);}catch(e){console.error('FAIL',name,e.message);process.exitCode=1;}}
check('No heavy scene assets before intersection',()=>{const h=harness({webgl:true,reduced:false});assert.equal(h.images.length,0);assert.equal(h.elements['look-video'].src,undefined);});
check('Reduced motion loads still and mask without video',()=>{const h=harness({webgl:true});h.near();assert.equal(h.images.length,2);assert.equal(h.elements['look-video'].src,undefined);});
check('Save-Data loads still and mask without video',()=>{const h=harness({webgl:true,reduced:false,saveData:true});h.near();assert.equal(h.images.length,2);assert.equal(h.elements['look-video'].src,undefined);});
check('No WebGL keeps poster and avoids heavy assets',()=>{const h=harness();h.near();assert.equal(h.images.length,0);assert.equal(h.elements['look-video'].src,undefined);});
check('Repeated Enter preserves initial overflow for Close',()=>{const h=harness();h.key('Enter');h.key('Enter');h.elements['look-close'].onclick();assert.equal(h.doc.documentElement.style.overflow,'');});
check('Explicit full entry stays open after keyboard look',()=>{const h=harness();h.key('Enter');h.key('ArrowRight');assert.equal(h.timers.length,0);assert.equal(h.elements['look-stage'].classes.has('fixed'),true);h.elements['look-close'].onclick();assert.equal(h.doc.documentElement.style.overflow,'');});
check('Escape closes while a button has focus',()=>{const h=harness();h.key('Enter');h.key('Escape',true);assert.equal(h.elements['look-stage'].classes.has('fixed'),false);});
check('Tab stays within the expanded panorama',()=>{const h=harness();h.key('Enter');h.doc.activeElement=h.elements['look-close'];h.key('Tab',true);assert.equal(h.doc.activeElement,h.elements['look-stage']);});
check('A failed HEVC load retries H.264 once',()=>{const h=harness({webgl:true,reduced:false});h.near();assert.match(h.elements['look-video'].src,/shape\.mp4$/);h.elements['look-video'].handlers.error[0]();assert.match(h.elements['look-video'].src,/shape-h264\.mp4$/);h.elements['look-video'].handlers.error[0]();assert.match(h.elements['look-video'].src,/shape-h264\.mp4$/);});

check('Explicit entry cancels a pending keyboard collapse',()=>{
  const h=harness();h.key('ArrowRight');h.key('Enter');
  for(const fn of h.timers) if(fn) fn();
  assert.equal(h.elements['look-stage'].classes.has('fixed'),true);
  h.elements['look-close'].onclick();assert.equal(h.doc.documentElement.style.overflow,'');
});

check('The video loads near the viewport but plays only once on screen',()=>{
  const h=harness({webgl:true,reduced:false});h.near();
  const v=h.elements['look-video'];
  assert.match(v.src,/shape\.mp4$/);assert.equal(v.plays||0,0);
  h.visible();assert.equal(v.plays,1);
});

check('Unsupported HEVC selects H.264 without trying HEVC',()=>{
  const h=harness({webgl:true,reduced:false,hevc:false});h.near();
  assert.match(h.elements['look-video'].src,/shape-h264\.mp4$/);
});

check('Scene textures and videos use the media base with anonymous CORS',()=>{
  const h=harness({webgl:true,reduced:false});h.near();
  assert.deepEqual(h.images,['https://media.visr.gg/look/lifeboat/still8k.jpg','https://media.visr.gg/look/lifeboat/mask.png']);
  const v=h.elements['look-video'];
  assert.equal(v.crossOrigin,'anonymous');
  assert.equal(v.src,'https://media.visr.gg/look/lifeboat/shape.mp4');
  v.handlers.error[0]();
  assert.equal(v.src,'https://media.visr.gg/look/lifeboat/shape-h264.mp4');
  assert.equal(h.elements['look-poster'].src,'look/lifeboat/poster.jpg');
});

checkAsync('Hardware AV1 is tried first, then HEVC',async()=>{
  const h=harness({webgl:true,reduced:false,av1:'av01.0.13M.08',caps:{supported:true,smooth:true,powerEfficient:true}});h.near();
  await new Promise(r=>setImmediate(r));
  const v=h.elements['look-video'];
  assert.equal(v.src,'https://media.visr.gg/look/lifeboat/shape-av1.mp4');
  v.handlers.error[0]();
  assert.equal(v.src,'https://media.visr.gg/look/lifeboat/shape.mp4');
});

checkAsync('AV1 without an efficient decoder is skipped',async()=>{
  const h=harness({webgl:true,reduced:false,av1:'av01.0.13M.08',caps:{supported:true,smooth:true,powerEfficient:false}});h.near();
  await new Promise(r=>setImmediate(r));
  assert.equal(h.elements['look-video'].src,'https://media.visr.gg/look/lifeboat/shape.mp4');
});
