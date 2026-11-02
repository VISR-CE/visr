// Home page behavior: the platform tabs, and the looping clips in the media slots. Without script the page still
// reads in full: every platform panel is shown and each slot shows its poster image.
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");

  // ---- Platform tabs -------------------------------------------------------------------------------------------
  function setupTabs(root) {
    var panels = Array.prototype.slice.call(root.querySelectorAll(":scope > section"));
    if (panels.length < 2) return;
    var list = document.createElement("div");
    list.className = "tablist";
    list.setAttribute("role", "tablist");
    list.setAttribute("aria-label", "Platforms");
    var tabs = panels.map(function (panel, index) {
      var heading = panel.querySelector("h2");
      var tab = document.createElement("button");
      tab.type = "button";
      tab.className = "tab";
      tab.setAttribute("role", "tab");
      tab.id = "tab-" + panel.getAttribute("aria-labelledby");
      tab.setAttribute("aria-controls", "panel-" + panel.getAttribute("aria-labelledby"));
      tab.textContent = heading.textContent.trim();
      panel.id = "panel-" + panel.getAttribute("aria-labelledby");
      panel.setAttribute("role", "tabpanel");
      panel.setAttribute("aria-labelledby", tab.id);
      tab.addEventListener("click", function () { select(index, false); });
      list.appendChild(tab);
      return tab;
    });
    // The headings name the panels for the tab list now; the tabs carry the visible label.
    panels.forEach(function (panel) { panel.querySelector("h2").classList.add("sr-only"); });
    root.querySelector(".platforms-title").after(list);
    root.classList.add("is-tabbed");

    // On a phone, iPhone and iPad share one tab, with both pictures stacked in its panel.
    var narrow = window.matchMedia("(max-width: 899px)");
    var findPanel = function (name) { return panels.findIndex(function (panel) { return panel.id === "panel-" + name; }); };
    var iphone = findPanel("iphone"), ipad = findPanel("ipad"), selected = 0;
    var combined = function () { return narrow.matches && iphone >= 0 && ipad >= 0; };
    function arrangeDevices() {
      if (iphone < 0 || ipad < 0) return;
      var figure = root.querySelector(".device-ipad");
      var home = combined() ? panels[iphone] : panels[ipad];
      home.querySelector(".platform-media").appendChild(figure);
      tabs[ipad].hidden = combined();
      tabs[iphone].textContent = combined() ? "iPhone and iPad" : "iPhone";
    }

    function select(index, focus) {
      if (combined() && index === ipad) index = iphone;
      selected = index;
      panels.forEach(function (panel, i) {
        panel.hidden = i !== index;
        tabs[i].setAttribute("aria-selected", String(i === index));
        tabs[i].tabIndex = i === index ? 0 : -1;
      });
      if (focus) tabs[index].focus();
      playVisibleClips();
    }

    list.addEventListener("keydown", function (event) {
      var current = tabs.indexOf(document.activeElement);
      if (current < 0) return;
      var next = { ArrowRight: current + 1, ArrowLeft: current - 1, Home: 0, End: tabs.length - 1 }[event.key];
      if (next === undefined) return;
      event.preventDefault();
      next = (next + tabs.length) % tabs.length;
      if (combined() && next === ipad) next = (next + (event.key === "ArrowLeft" ? -1 : 1) + tabs.length) % tabs.length;
      select(next, true);
    });

    // A link to a platform (#apple-tv, #mac, ...) opens that tab.
    function fromHash() {
      var wanted = decodeURIComponent(location.hash.slice(1));
      var index = panels.findIndex(function (panel) { return panel.getAttribute("aria-labelledby") === "tab-" + wanted; });
      if (index < 0) return false;
      select(index, false);
      root.scrollIntoView({ block: "start", behavior: reduceMotion.matches ? "auto" : "smooth" });
      return true;
    }
    window.addEventListener("hashchange", fromHash);
    arrangeDevices();
    narrow.addEventListener("change", function () { arrangeDevices(); select(selected, false); });
    if (!fromHash()) select(0, false);
  }

  // ---- Looping clips -------------------------------------------------------------------------------------------
  var clips = [];

  function setupClips() {
    clips = Array.prototype.slice.call(document.querySelectorAll("video.media-video"));
    if (!clips.length) return;
    clips.forEach(function (clip) {
      var holder = clip.parentElement;
      var button = document.createElement("button");
      button.type = "button";
      button.className = "clip-toggle";
      button.setAttribute("aria-label", "Pause video");
      button.addEventListener("click", function () {
        clip.dataset.userPaused = clip.paused ? "" : "1";
        if (clip.paused) clip.play().catch(function () {}); else clip.pause();
      });
      clip.addEventListener("play", function () { button.setAttribute("aria-label", "Pause video"); button.dataset.state = "playing"; });
      clip.addEventListener("pause", function () { button.setAttribute("aria-label", "Play video"); button.dataset.state = "paused"; });
      button.dataset.state = "paused";
      holder.appendChild(button);
    });
    if ("IntersectionObserver" in window) {
      var observer = new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          entry.target.dataset.onScreen = entry.isIntersecting ? "1" : "";
        });
        playVisibleClips();
      }, { threshold: 0.35 });
      clips.forEach(function (clip) { observer.observe(clip); });
    }
    reduceMotion.addEventListener("change", playVisibleClips);
    document.addEventListener("visibilitychange", playVisibleClips);
  }

  // Play a clip only while it is on screen, its tab is showing, the page is visible, motion is allowed and the
  // visitor has not paused it. Everything else holds still on the poster.
  function playVisibleClips() {
    clips.forEach(function (clip) {
      if (!clip.dataset.loaded && clip.dataset.onScreen) {
        clip.preload = "auto";
        clip.dataset.loaded = "1";
      }
      var shown = clip.dataset.onScreen && !clip.closest("[hidden]") && !document.hidden;
      var wanted = shown && !reduceMotion.matches && !clip.dataset.userPaused;
      if (wanted && clip.paused) clip.play().catch(function () {});
      else if (!wanted && !clip.paused) clip.pause();
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    var tabs = document.querySelector("[data-tabs]");
    if (tabs) setupTabs(tabs);
    setupClips();
  });
})();

// The hero's floor grid starts just below its last paragraph, wherever the layout puts that.
(() => {
  const head = document.querySelector('.head');
  const floor = head && head.querySelector('.hero-floor');
  const ledes = head ? head.querySelectorAll('.lede') : [];
  if (!floor || !ledes.length) return;
  const place = () => {
    const box = head.getBoundingClientRect();
    const top = ledes[ledes.length - 1].getBoundingClientRect().bottom - box.top + 16;
    floor.style.top = `${top}px`;
    floor.style.height = `${Math.max(0, box.height - top)}px`;
  };
  place();
  addEventListener('resize', place);
  addEventListener('load', place);
  document.fonts?.ready.then(place);
})();
