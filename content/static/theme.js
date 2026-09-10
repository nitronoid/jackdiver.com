(function () {
  'use strict';

  var LABEL = {
    light: 'Light',
    dark: 'Dark'
  };

  var DESCRIPTION = {
    light: 'Theme: light. Activate for dark.',
    dark: 'Theme: dark. Activate for light.'
  };

  function read() {
    try {
      var saved = localStorage.getItem('theme');
      return saved === 'light' || saved === 'dark' ? saved : null;
    } catch (e) {
      return null;
    }
  }

  function write(theme) {
    try {
      localStorage.setItem('theme', theme);
    } catch (e) {
    }
  }

  function apply(theme) {
    document.documentElement.style.colorScheme = theme;
  }

  function main() {
    var button = document.querySelector('[data-theme-toggle]');
    if (button === null) return;

    var label = button.querySelector('.theme-toggle-label');
    var dark = window.matchMedia
      ? window.matchMedia('(prefers-color-scheme: dark)')
      : null;

    // Already applied before first paint by the inline script in each page's head
    // (src/blog/frame.cxx); this file only tracks it for the toggle.
    var chosen = read();

    function current() {
      if (chosen !== null) return chosen;
      return dark !== null && dark.matches ? 'dark' : 'light';
    }

    function render() {
      var theme = current();
      if (label !== null) label.textContent = LABEL[theme];
      button.setAttribute('aria-label', DESCRIPTION[theme]);
      button.dataset.theme = theme;
    }

    button.addEventListener('click', function () {
      chosen = current() === 'dark' ? 'light' : 'dark';
      write(chosen);
      apply(chosen);
      render();
    });

    if (dark !== null) {
      var onchange = function () {
        if (chosen === null) render();
      };
      if (dark.addEventListener) {
        dark.addEventListener('change', onchange);
      } else if (dark.addListener) {
        dark.addListener(onchange);
      }
    }

    render();

    button.hidden = false;
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', main);
  } else {
    main();
  }
})();
