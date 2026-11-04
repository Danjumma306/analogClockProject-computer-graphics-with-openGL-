'use strict';
(() => {
  const svgNS = 'http://www.w3.org/2000/svg';
  const ticks = document.getElementById('ticks');
  for (let i = 0; i < 60; i++) {
    const major = i % 5 === 0;
    const tick = document.createElementNS(svgNS, 'line');
    Object.entries({ x1: 300, x2: 300, y1: 70, y2: major ? 90 : 80, stroke: '#0b5d3b', 'stroke-width': major ? 3 : 1, transform: `rotate(${i * 6} 300 300)` }).forEach(([key, value]) => tick.setAttribute(key, value));
    ticks.appendChild(tick);
  }
  for (let i = 1; i <= 12; i++) {
    const angle = i * Math.PI / 6;
    const text = document.createElementNS(svgNS, 'text');
    text.setAttribute('x', 300 + 190 * Math.sin(angle));
    text.setAttribute('y', 300 - 190 * Math.cos(angle));
    text.setAttribute('class', 'clock-number');
    text.textContent = i;
    document.getElementById('numerals').appendChild(text);
  }
  const hourHand = document.getElementById('hour-hand');
  const minuteHand = document.getElementById('minute-hand');
  const secondHand = document.getElementById('second-hand');
  const time = document.getElementById('digital-time');
  const date = document.getElementById('date');
  const analogClock = document.querySelector('.clock');
  const dateFormat = new Intl.DateTimeFormat('en-GB', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' });
  const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  document.getElementById('zone').textContent = `LOCAL TIME${timezone ? ' · ' + timezone.replaceAll('_', ' ').split('/').pop().toUpperCase() : ''}`;
  let lastSecond = '';
  function updateClock() {
    const now = new Date();
    const signature = now.toString();
    if (signature !== lastSecond) {
      const seconds = now.getSeconds();
      const minutes = now.getMinutes() + seconds / 60;
      const hours = now.getHours() % 12 + minutes / 60;
      hourHand.setAttribute('transform', `rotate(${hours * 30} 300 300)`);
      minuteHand.setAttribute('transform', `rotate(${minutes * 6} 300 300)`);
      secondHand.setAttribute('transform', `rotate(${seconds * 6} 300 300)`);
      const digital = [now.getHours(), now.getMinutes(), seconds].map(value => String(value).padStart(2, '0')).join(':');
      time.textContent = digital;
      time.dateTime = digital;
      date.textContent = dateFormat.format(now).toUpperCase();
      analogClock.setAttribute('aria-label', `Analog clock, local time ${digital}`);
      lastSecond = signature;
    }
    setTimeout(updateClock, 1000 - Date.now() % 1000 + 10);
  }
  updateClock();
  document.addEventListener('visibilitychange', () => { if (!document.hidden) lastSecond = ''; });
  const status = document.getElementById('status');
  const shareButton = document.getElementById('share-button');
  if (location.protocol === 'https:' || location.protocol === 'http:') shareButton.hidden = false;
  shareButton.addEventListener('click', async () => {
    try {
      const url = location.href.split('#')[0];
      if (navigator.share) await navigator.share({ title: document.title, text: 'View the Palm University College clock.', url });
      else if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(url); status.textContent = 'Clock link copied.'; }
      else status.textContent = 'Copy the address from your browser to share this clock.';
    } catch (error) { if (error.name !== 'AbortError') status.textContent = 'Copy the address from your browser to share this clock.'; }
  });
  const fullscreenButton = document.getElementById('fullscreen-button');
  fullscreenButton.hidden = !document.fullscreenEnabled;
  fullscreenButton.addEventListener('click', async () => {
    try {
      if (document.fullscreenElement) await document.exitFullscreen();
      else await document.documentElement.requestFullscreen();
    } catch { status.textContent = 'Full screen is unavailable in this browser.'; }
  });
  document.addEventListener('fullscreenchange', () => { fullscreenButton.querySelector('span').textContent = document.fullscreenElement ? 'Exit full screen' : 'Full screen'; });
})();
