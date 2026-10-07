(function () {
  var MOBILE_CLASS = "gemtd-mobile-layout";
  var DESKTOP_CLASS = "gemtd-desktop-layout";
  var AWAITING_PLAY_CLASS = "gemtd-mobile-awaiting-play";
  var PLAYING_CLASS = "gemtd-mobile-playing";
  var GAME_ASPECT_RATIO = 705 / 540;
  var BOSS_SPELL_BAR_HEIGHT = 44;
  var ASSET_VERSION = "site-20260923-display-telemetry";
  var DEFAULT_GAME_SCRIPT_SRC = ["season1", "season2", "season3", "battle"].indexOf(queryPlayMode()) === -1
    ? "/classic/game-1.0.5-legendary-preview-1.js"
    : "/game.js?v=" + ASSET_VERSION;
  // A run still in progress on an earlier Season 2 version keeps playing on
  // that version's archived client: the game reloads the page with
  // `?legacy=<version>` when such a run is resumed, and the page loads
  // `/legacy/game-<version>.js` instead of the current bundle.
  var LEGACY_CLIENT_VERSION_PATTERN = /^season2-1\.0\.[0-9]{1,3}$/;
  var legacyClientVersion = queryLegacyClientVersion();
  var GAME_SCRIPT_SRC = legacyClientVersion
    ? "/legacy/game-" + legacyClientVersion + ".js"
    : DEFAULT_GAME_SCRIPT_SRC;

  function queryPlayMode() {
    try {
      return new URLSearchParams(window.location.search).get("mode") || "classic";
    } catch (error) {
      return "classic";
    }
  }

  function queryLegacyClientVersion() {
    try {
      var value = new URLSearchParams(window.location.search).get("legacy");

      if (value && LEGACY_CLIENT_VERSION_PATTERN.test(value)) {
        return value;
      }
    } catch (error) {
      return null;
    }

    return null;
  }

  function currentSeasonTwoStartUrl() {
    var url = new URL(window.location.href);
    url.pathname = "/";
    url.hash = "";
    url.searchParams.set("mode", "season2");
    url.searchParams.set("fresh", "season2");
    url.searchParams.delete("legacy");
    if (selectedLayout() === "mobile") {
      url.searchParams.set("layout", "mobile");
      url.searchParams.set("mobileStart", "1");
    }
    return url.toString();
  }

  function syncLegacyClientNotice() {
    var notice = document.querySelector("[data-gemtd-legacy-client-notice]");
    if (!notice) {
      return;
    }
    notice.hidden = !legacyClientVersion;
    if (!legacyClientVersion) {
      return;
    }
    var version = notice.querySelector("[data-gemtd-legacy-client-version]");
    var startCurrent = notice.querySelector("[data-gemtd-start-current-season]");
    if (version) {
      version.textContent = legacyClientVersion.replace("season2-", "Season 2 ");
    }
    if (startCurrent) {
      startCurrent.href = currentSeasonTwoStartUrl();
    }
  }

  function bindLegacyClientNotice() {
    var notice = document.querySelector("[data-gemtd-legacy-client-notice]");
    var continueLegacy = notice && notice.querySelector("[data-gemtd-continue-legacy-run]");
    if (!continueLegacy || continueLegacy.getAttribute("data-gemtd-bound") === "true") {
      return;
    }
    continueLegacy.setAttribute("data-gemtd-bound", "true");
    continueLegacy.addEventListener("click", function () {
      notice.hidden = true;
    });
  }
  var gameScriptLoading = false;
  var gameScriptLoaded = false;
  var mobilePlayStarted = false;
  var mobileAuthLaunch = false;
  var iosFullscreenPromptDismissed = false;
  var lastMobilePanelTrigger = null;
  var runtimeLayoutOverride = null;
  var resizeTimer = null;

  function queryLayoutOverride() {
    try {
      var value = new URLSearchParams(window.location.search).get("layout");

      if (value === "mobile" || value === "desktop" || value === "auto") {
        return value;
      }
    } catch (error) {
      return null;
    }

    return null;
  }

  function queryEmbeddedPanel() {
    try {
      return new URLSearchParams(window.location.search).get("embed") === "game-panel";
    } catch (error) {
      return false;
    }
  }

  function matches(query) {
    return Boolean(window.matchMedia && window.matchMedia(query).matches);
  }

  function looksLikeIosSafari() {
    var userAgent = window.navigator.userAgent || "";
    var platform = window.navigator.platform || "";
    var iosDevice =
      /iP(hone|od|ad)/i.test(userAgent) ||
      (platform === "MacIntel" && window.navigator.maxTouchPoints > 1);
    var webkit = /WebKit/i.test(userAgent);
    var alternateIosBrowser = /CriOS|FxiOS|EdgiOS|OPiOS/i.test(userAgent);

    return Boolean(iosDevice && webkit && !alternateIosBrowser);
  }

  function standaloneDisplay() {
    return Boolean(
      window.navigator.standalone ||
        matches("(display-mode: fullscreen)") ||
        matches("(display-mode: standalone)") ||
        matches("(display-mode: minimal-ui)"),
    );
  }

  function fullscreenElement() {
    return (
      document.fullscreenElement ||
      document.webkitFullscreenElement ||
      document.msFullscreenElement ||
      null
    );
  }

  function setIosFullscreenPromptVisible(visible) {
    var prompt = document.querySelector("[data-gemtd-ios-fullscreen-prompt]");

    if (!prompt) {
      return;
    }

    if (visible && iosFullscreenPromptDismissed) {
      return;
    }

    prompt.hidden = !visible;
  }

  function syncIosFullscreenPrompt(playing) {
    var shouldShow = Boolean(
      playing && looksLikeIosSafari() && !standaloneDisplay() && !fullscreenElement() && !mobilePanelOpen(),
    );

    setIosFullscreenPromptVisible(shouldShow);
  }

  function looksLikePhoneBrowser() {
    var userAgent = window.navigator.userAgent || "";
    var mobileUserAgent = /Android.+Mobile|iPhone|iPod|IEMobile|Opera Mini|Mobile Safari/i.test(userAgent);
    var hasTouch = "ontouchstart" in window || window.navigator.maxTouchPoints > 0;
    var coarsePointer = matches("(pointer: coarse)");
    var noHover = matches("(hover: none)");
    var narrowViewport = matches("(max-width: 820px)");
    var phoneLandscapeViewport =
      matches("(orientation: landscape)") && matches("(max-width: 960px)") && matches("(max-height: 540px)");
    var smallScreen = Math.min(window.screen.width || 0, window.screen.height || 0) <= 540;

    return Boolean(
      hasTouch &&
        (mobileUserAgent || smallScreen) &&
        (coarsePointer || noHover || mobileUserAgent) &&
        (narrowViewport || phoneLandscapeViewport || (mobileUserAgent && smallScreen)),
    );
  }

  function selectedLayout() {
    if (runtimeLayoutOverride) {
      return runtimeLayoutOverride;
    }

    var queryOverride = queryLayoutOverride();

    if (queryOverride && queryOverride !== "auto") {
      return queryOverride;
    }

    return looksLikePhoneBrowser() ? "mobile" : "desktop";
  }

  function setClass(target, mobile) {
    if (!target) {
      return;
    }

    target.classList.toggle(MOBILE_CLASS, mobile);
    target.classList.toggle(DESKTOP_CLASS, !mobile);
  }

  function setPlayStateClass(target, awaitingPlay, playing) {
    if (!target) {
      return;
    }

    target.classList.toggle(AWAITING_PLAY_CLASS, awaitingPlay);
    target.classList.toggle(PLAYING_CLASS, playing);
  }

  function hasGameContainer() {
    return Boolean(document.getElementById("gemtd"));
  }

  function loadGameScript() {
    if (gameScriptLoaded || gameScriptLoading || !hasGameContainer()) {
      return;
    }

    gameScriptLoading = true;

    var script = document.createElement("script");
    script.src = GAME_SCRIPT_SRC;
    script.async = false;
    script.crossOrigin = "anonymous";
    script.onload = function () {
      gameScriptLoaded = true;
      gameScriptLoading = false;
    };
    script.onerror = function () {
      gameScriptLoading = false;
      // An archived client that is no longer on the site falls back to the
      // current bundle rather than leaving the page empty.
      if (GAME_SCRIPT_SRC !== DEFAULT_GAME_SCRIPT_SRC) {
        GAME_SCRIPT_SRC = DEFAULT_GAME_SCRIPT_SRC;
        loadGameScript();
      }
    };

    document.head.appendChild(script);
  }

  function viewportSize() {
    var viewport = window.visualViewport;

    return {
      width: viewport && viewport.width ? viewport.width : window.innerWidth,
      height: viewport && viewport.height ? viewport.height : window.innerHeight,
    };
  }

  function seasonTwoSpellsEnabled() {
    try {
      var mode = new URLSearchParams(window.location.search).get("mode");
      return mode === "season2" || mode === "season3";
    } catch (error) {
      return false;
    }
  }

  function dedicatedMobileGameEnabled() {
    try {
      var mode = new URLSearchParams(window.location.search).get("mode");
      return mode === "season2" || mode === "season3" || mode === "battle";
    } catch (error) {
      return false;
    }
  }

  function slatesEnabled() {
    try {
      var mode = new URLSearchParams(window.location.search).get("mode");
      return mode === "season2" || mode === "season3" || mode === "battle";
    } catch (error) {
      return false;
    }
  }

  function syncSeasonTwoSlateRecipes() {
    var slateRecipes = document.querySelector("[data-gemtd-season2-slate-recipes]");
    var seasonTwo = slatesEnabled();

    if (slateRecipes) {
      slateRecipes.hidden = !seasonTwo;
    }
  }

  function syncSeasonTwoPollLink() {
    var pollLink = document.querySelector("[data-gemtd-season2-poll-link]");

    if (pollLink) {
      pollLink.hidden = !seasonTwoSpellsEnabled();
    }
  }

  function syncSeasonTwoLaunchNotice() {
    var notices = document.querySelectorAll("[data-gemtd-season2-launch-notice]");
    var seasonTwo = queryPlayMode() === "season2" ||
      (document.body && document.body.id === "tab6");

    for (var i = 0; i < notices.length; i++) {
      notices[i].hidden = !seasonTwo;
    }
  }

  function setGameSizeVars(mobile) {
    var root = document.documentElement;

    if (!mobile) {
      root.style.removeProperty("--gemtd-mobile-game-width");
      root.style.removeProperty("--gemtd-mobile-game-height");
      root.style.removeProperty("--gemtd-mobile-spell-bar-height");
      root.style.removeProperty("--gemtd-mobile-map-size");
      return;
    }

    var size = viewportSize();
    var availableWidth = Math.max(1, Math.floor(size.width));
    var availableHeight = Math.max(1, Math.floor(size.height));
    var widthFromHeight = Math.floor(availableHeight * GAME_ASPECT_RATIO);
    var gameWidth = Math.min(availableWidth, widthFromHeight);
    var gameHeight = Math.floor(gameWidth / GAME_ASPECT_RATIO);
    var landscape = availableWidth > availableHeight;
    var panelBudget = landscape
      ? Math.min(320, Math.max(258, Math.floor(availableWidth * 0.36)))
      : 0;
    var verticalChromeBudget = landscape ? 0 : 270;
    var mapSize = Math.max(
      220,
      Math.min(
        availableWidth - panelBudget,
        availableHeight - verticalChromeBudget,
      ),
    );

    root.style.setProperty("--gemtd-mobile-game-width", gameWidth + "px");
    root.style.setProperty("--gemtd-mobile-game-height", gameHeight + "px");
    root.style.setProperty("--gemtd-mobile-spell-bar-height", BOSS_SPELL_BAR_HEIGHT + "px");
    root.style.setProperty("--gemtd-mobile-map-size", Math.floor(mapSize) + "px");
  }

  function syncGameLoading(mobile) {
    if (!hasGameContainer()) {
      return;
    }

    if (!mobile || mobilePlayStarted) {
      loadGameScript();
    }
  }

  function requestFullscreen(target) {
    if (!target) {
      return Promise.resolve(false);
    }

    var request =
      target.requestFullscreen ||
      target.webkitRequestFullscreen ||
      target.webkitEnterFullscreen ||
      target.msRequestFullscreen;

    if (!request) {
      return Promise.resolve(false);
    }

    try {
      var result = request.call(target, { navigationUI: "hide" });
      return result && result.then
        ? result.then(
            function () {
              return true;
            },
            function () {
              return false;
            },
          )
        : Promise.resolve(true);
    } catch (error) {
      return Promise.resolve(false);
    }
  }

  function enterMobilePlayMode() {
    mobilePlayStarted = true;
    setPlayStateClass(document.documentElement, false, true);
    setPlayStateClass(document.body, false, true);
    syncDedicatedSeasonTwoShell(true, true);
    setGameSizeVars(true);

    // Keep the score/comment panel inside the fullscreen element so it remains
    // available while a game is in progress.
    var target =
      document.getElementById("content") || document.querySelector(".game-shell") || document.getElementById("gemtd");

    var afterFullscreen = function () {
      setGameSizeVars(true);
      loadGameScript();
      syncIosFullscreenPrompt(true);
      scheduleApplyLayout();
    };

    window.scrollTo(0, 0);
    requestFullscreen(target).then(afterFullscreen, afterFullscreen);
  }

  function requestedPlayMode() {
    try {
      var mode = new URLSearchParams(window.location.search).get("mode");
      if (mode === "season1" || mode === "season2" || mode === "season3" || mode === "battle") {
        return mode;
      }
    } catch (error) {
      return "classic";
    }

    return "classic";
  }

  function mobileModeUrl(mode) {
    var url = new URL(window.location.href);
    url.pathname = "/";
    url.hash = "";
    if (mode === "classic") {
      url.searchParams.delete("mode");
    } else {
      url.searchParams.set("mode", mode);
    }
    url.searchParams.set("layout", "mobile");
    url.searchParams.set("mobileStart", "1");
    return url.toString();
  }

  function startMobileMode(mode) {
    if (!legacyClientVersion) {
      GAME_SCRIPT_SRC = DEFAULT_GAME_SCRIPT_SRC;
    }
    if (requestedPlayMode() === mode) {
      enterMobilePlayMode();
      return;
    }

    var destination = mobileModeUrl(mode);

    // On the initial phone hub the game bundle is deliberately not loaded yet,
    // so the mode can be changed in-place while preserving the tap that permits
    // fullscreen. If a game bundle already exists, reload into a clean ruleset.
    if (!gameScriptLoaded && !gameScriptLoading) {
      var url = new URL(destination);
      url.searchParams.delete("mobileStart");
      window.history.replaceState(window.history.state, "", url.toString());
      applyLayout();
      enterMobilePlayMode();
      return;
    }

    window.location.assign(destination);
  }

  function bindMobileModeButtons() {
    var modeButtons = document.querySelectorAll("[data-gemtd-mobile-play-mode]");

    for (var i = 0; i < modeButtons.length; i++) {
      var modeButton = modeButtons[i];

      if (modeButton.getAttribute("data-gemtd-bound") === "true") {
        continue;
      }

      modeButton.setAttribute("data-gemtd-bound", "true");
      modeButton.addEventListener("click", function (event) {
        event.preventDefault();
        var mode = event.currentTarget.getAttribute("data-gemtd-mobile-play-mode");
        if (mode === "battle") {
          window.location.assign("/battle/?layout=mobile");
        } else if (mode === "classic" || mode === "season1" || mode === "season2" || mode === "season3") {
          startMobileMode(mode);
        }
      });
    }
  }

  function consumeMobileStartRequest() {
    if (selectedLayout() !== "mobile") {
      return;
    }

    try {
      var url = new URL(window.location.href);
      if (url.searchParams.get("mobileStart") !== "1") {
        return;
      }
      url.searchParams.delete("mobileStart");
      window.history.replaceState(window.history.state, "", url.toString());
      enterMobilePlayMode();
    } catch (error) {
      return;
    }
  }

  function openMobileAuthWhenReady(attempt) {
    if (typeof window.gemtdShowAuth === "function") {
      window.gemtdShowAuth("login");
      return;
    }

    if (attempt < 160) {
      window.setTimeout(function () {
        openMobileAuthWhenReady(attempt + 1);
      }, 50);
    }
  }

  function bindMobileAuthButton() {
    var authButton = document.querySelector("[data-gemtd-mobile-auth]");

    if (!authButton || authButton.getAttribute("data-gemtd-bound") === "true") {
      return;
    }

    authButton.setAttribute("data-gemtd-bound", "true");
    authButton.addEventListener("click", function (event) {
      event.preventDefault();
      var alreadyLoggedIn = authButton.getAttribute("data-gemtd-logged-in") === "true";
      mobileAuthLaunch = !alreadyLoggedIn;
      enterMobilePlayMode();
      if (!alreadyLoggedIn) {
        openMobileAuthWhenReady(0);
      }
    });
  }

  function bindIosFullscreenPrompt() {
    var dismissButton = document.querySelector("[data-gemtd-ios-fullscreen-dismiss]");

    if (!dismissButton || dismissButton.getAttribute("data-gemtd-bound") === "true") {
      return;
    }

    dismissButton.setAttribute("data-gemtd-bound", "true");
    dismissButton.addEventListener("click", function (event) {
      event.preventDefault();
      iosFullscreenPromptDismissed = true;
      setIosFullscreenPromptVisible(false);
    });
  }

  function mobilePanel() {
    return document.querySelector("[data-gemtd-mobile-panel]");
  }

  function mobilePanelOpen() {
    var panel = mobilePanel();

    return Boolean(panel && !panel.hidden);
  }

  function closeMobilePanel(restoreFocus) {
    var panel = mobilePanel();

    if (!panel) {
      return;
    }

    panel.hidden = true;
    panel.setAttribute("aria-hidden", "true");
    syncIosFullscreenPrompt(document.documentElement.classList.contains(PLAYING_CLASS));

    if (restoreFocus && lastMobilePanelTrigger && typeof lastMobilePanelTrigger.focus === "function") {
      lastMobilePanelTrigger.focus({ preventScroll: true });
    }
  }

  function openMobilePanel(trigger) {
    var panel = mobilePanel();
    var frame = document.querySelector("[data-gemtd-mobile-panel-frame]");
    var title = document.getElementById("gemtd-mobile-panel-title");
    var closeButton = document.querySelector("[data-gemtd-mobile-panel-close]");

    if (!panel || !frame || !trigger) {
      return;
    }

    var panelTitle = trigger.getAttribute("data-gemtd-mobile-panel-title") || trigger.textContent || "Panel";
    var panelSrc = trigger.getAttribute("data-gemtd-mobile-panel-src") || trigger.getAttribute("href");

    if (!panelSrc) {
      return;
    }

    lastMobilePanelTrigger = trigger;

    if (title) {
      title.textContent = panelTitle;
    }

    frame.setAttribute("title", panelTitle);

    if (frame.getAttribute("src") !== panelSrc) {
      frame.setAttribute("src", panelSrc);
    }

    panel.hidden = false;
    panel.setAttribute("aria-hidden", "false");
    setIosFullscreenPromptVisible(false);

    if (closeButton && typeof closeButton.focus === "function") {
      closeButton.focus({ preventScroll: true });
    }
  }

  function bindMobilePanels() {
    var triggers = document.querySelectorAll("[data-gemtd-mobile-panel-open]");
    var closeButton = document.querySelector("[data-gemtd-mobile-panel-close]");

    for (var i = 0; i < triggers.length; i++) {
      var trigger = triggers[i];

      if (trigger.getAttribute("data-gemtd-bound") === "true") {
        continue;
      }

      trigger.setAttribute("data-gemtd-bound", "true");
      trigger.addEventListener("click", function (event) {
        event.preventDefault();
        openMobilePanel(event.currentTarget);
      });
    }

    if (closeButton && closeButton.getAttribute("data-gemtd-bound") !== "true") {
      closeButton.setAttribute("data-gemtd-bound", "true");
      closeButton.addEventListener("click", function (event) {
        event.preventDefault();
        closeMobilePanel(true);
      });
    }

    if (document.documentElement.getAttribute("data-gemtd-panel-escape-bound") !== "true") {
      document.documentElement.setAttribute("data-gemtd-panel-escape-bound", "true");
      document.addEventListener("keydown", function (event) {
        if (event.key === "Escape" && mobilePanelOpen()) {
          closeMobilePanel(true);
        }
      });
    }
  }

  function syncDedicatedSeasonTwoShell(mobile, playing) {
    var active = Boolean(mobile && playing && dedicatedMobileGameEnabled());
    var battle = active && requestedPlayMode() === "battle";
    document.documentElement.classList.toggle("gemtd-mobile-season2-active", active);
    document.documentElement.classList.toggle("gemtd-mobile-battle-active", battle);
    if (!active) {
      document.documentElement.classList.remove("gemtd-mobile-season2-terminal");
    }
  }

  function applyLayout() {
    var mobile = selectedLayout() === "mobile";
    var awaitingPlay = mobile && hasGameContainer() && !mobilePlayStarted;
    var playing = mobile && hasGameContainer() && mobilePlayStarted;

    setClass(document.documentElement, mobile);
    setClass(document.body, mobile);
    if (document.body) {
      document.body.classList.toggle("gemtd-embedded-page", queryEmbeddedPanel());
    }
    setPlayStateClass(document.documentElement, awaitingPlay, playing);
    setPlayStateClass(document.body, awaitingPlay, playing);
    syncDedicatedSeasonTwoShell(mobile, playing);
    var gameShell = document.querySelector(".game-shell");
    if (gameShell) {
      gameShell.classList.toggle("gemtd-season-two-spells", seasonTwoSpellsEnabled());
    }
    syncSeasonTwoLaunchNotice();
    syncLegacyClientNotice();
    syncSeasonTwoSlateRecipes();
    syncSeasonTwoPollLink();
    setGameSizeVars(mobile);
    syncGameLoading(mobile);
    // A launch-screen panel should survive resizes and orientation changes.
    // Only dismiss it when this page actually leaves the mobile layout.
    if (!mobile) {
      closeMobilePanel(false);
    }
    syncIosFullscreenPrompt(playing);

    window.dispatchEvent(
      new CustomEvent("gemtd:layoutchange", {
        detail: {
          mobile: mobile,
          layout: mobile ? "mobile" : "desktop",
        },
      }),
    );
  }

  function scheduleApplyLayout() {
    window.clearTimeout(resizeTimer);
    resizeTimer = window.setTimeout(applyLayout, 120);
  }

  window.GemTDLayout = {
    isMobileLayout: function () {
      return document.documentElement.classList.contains(MOBILE_CLASS);
    },
    setLayout: function (layout) {
      runtimeLayoutOverride = layout === "mobile" || layout === "desktop" ? layout : null;
      applyLayout();
    },
    clearLayout: function () {
      runtimeLayoutOverride = null;
      applyLayout();
    },
    finishMobileAuth: function () {
      if (!mobileAuthLaunch) {
        return false;
      }
      mobileAuthLaunch = false;
      window.location.reload();
      return true;
    },
  };

  applyLayout();

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", function () {
      bindMobileModeButtons();
      bindMobileAuthButton();
      bindIosFullscreenPrompt();
      bindMobilePanels();
      bindLegacyClientNotice();
      applyLayout();
      consumeMobileStartRequest();
    });
  } else {
    bindMobileModeButtons();
    bindMobileAuthButton();
    bindIosFullscreenPrompt();
    bindMobilePanels();
    bindLegacyClientNotice();
    applyLayout();
    consumeMobileStartRequest();
  }

  window.addEventListener("resize", scheduleApplyLayout, { passive: true });
  window.addEventListener("orientationchange", scheduleApplyLayout, { passive: true });
  document.addEventListener("fullscreenchange", scheduleApplyLayout);
  document.addEventListener("webkitfullscreenchange", scheduleApplyLayout);

  if (window.visualViewport) {
    window.visualViewport.addEventListener("resize", scheduleApplyLayout, { passive: true });
    window.visualViewport.addEventListener("scroll", scheduleApplyLayout, { passive: true });
  }
})();

