/* Dom: pomieszczenia — wspólna logika dla mockup-pulpit (pion) i mockup-landscape (poziom).
 * Materiał projektowy, nie kod firmware.
 *
 * Poziomy:  Dom (siatka pokoi) → pokój (lista encji) → encja światła (grupa) → żarówka / efekt
 *
 *   PilotRooms.grid(el, { land, open: id => …, toast })      siatka pokoi na zakładce Dom
 *   PilotRooms.detail(el, { land, nav: ({title, sub}) => …, openKlima: () => … })
 *                                                           podstrona pokoju z własnym stosem
 *   PilotRooms.back()  → true, gdy cofnięto poziom wewnątrz; false = zamknij podstronę
 *
 * Stan jest wspólny dla wszystkich widoków. Dotknięcie pokazuje stan oczekujący, a zmianę
 * „potwierdza” HA po ~0,7 s — jak w firmware (stan przychodzi z MQTT, nie z dotyku).
 */
(() => {
  const BASE = document.currentScript.src.replace(/rooms\.js.*$/, '') + 'assets/';
  const A = n => `${BASE}${n}.png`;
  const CTK = [2700, 3000, 3500, 4000, 4500, 5500, 6500];          // light.lampa: 2700–6500 K
  const CT = ['#ffbf75', '#ffd291', '#ffe2b5', '#fff0da', '#f6f6f6', '#e3ecff', '#cddcff'];
  const EFFECTS = [  // effect_list z light.lampa (WiZ), etykiety po polsku
    ['Cozy', 'Przytulnie', 'sofa'], ['Relax', 'Relaks', 'leaf'], ['TV time', 'Seans TV', 'tv'],
    ['Candlelight', 'Świeca', 'flame'], ['Warm white', 'Ciepła biel', 'sun-dim'], ['Golden white', 'Złota biel', 'sun-medium'],
    ['Daylight', 'Dzienne', 'sun'], ['Cool white', 'Zimna biel', 'snowflake'], ['Focus', 'Skupienie', 'target'],
    ['Night light', 'Nocne', 'moon'], ['Bedtime', 'Do snu', 'bed'], ['Wake-up', 'Pobudka', 'sunrise'],
    ['Pulse', 'Puls', 'activity'], ['Steampunk', 'Steampunk', 'cog'], ['Diwali', 'Diwali', 'sparkles'], ['Alarm', 'Alarm', 'siren'],
  ];
  const effLabel = e => (EFFECTS.find(x => x[0] === e) || [e, e])[1];

  // Grupa light.lampa = trzy żarówki WiZ (atrybut entity_id w HA).
  const lampa = {
    id: 'lampa', entity: 'light.lampa', label: 'Lampa', effect: null,
    bulbs: [
      { id: 'lewe', entity: 'light.wiz_tunable_white_49d4f0', label: 'Lewa', on: true, br: 70, ct: 2 },
      { id: 'srodek', entity: 'light.living_room_living_room_center', label: 'Środkowa', on: true, br: 60, ct: 2 },
      { id: 'prawe', entity: 'light.living_room_living_room_right', label: 'Prawa', on: false, br: 40, ct: 2 },
    ],
  };
  const sw = (entity) => ({ entity, label: 'Światło', kind: 'sw', on: false });
  const rooms = [
    // temp / hum: czujnik pokoju (stopnie, %); kuchnia = salon, przedpokój = biuro (wspólna przestrzeń)
    { id: 'salon', name: 'Salon', temp: 22.8, hum: 41, light: lampa, entities: ['lampa', 'klima'] },
    { id: 'kuchnia', name: 'Kuchnia', temp: 22.8, hum: 41, light: sw('switch.kuchnia_wlacznik_swiatla_l1') },
    { id: 'przedpokoj', name: 'Przedpokój', temp: 21.9, hum: 33, light: { ...sw('switch.nspanel_korytarz_relay_1'), on: true } },
    { id: 'biuro', name: 'Biuro', temp: 21.9, hum: 33, light: sw('switch.nspanel_biuro_relay_1') },
    { id: 'lazienka', name: 'Łazienka', temp: 23.5, hum: 67, light: null },
    { id: 'sypialnia', name: 'Sypialnia', temp: 20.1, hum: 45, light: sw('switch.nspanel_sypialnia_relay_1') },
  ];

  const deg = v => v.toFixed(1).replace('.', ',') + '°';
  // Ocena odczytu zamiast podpisu: ikona mówi, co mierzymy, słowo — czy jest dobrze.
  // Temperatura: 19–24,5 °C komfort. Wilgotność: 40–60 % komfort, poniżej 35 % sucho.
  const tempWord = t => t < 19 ? ['chłodno', 'cold'] : t > 24.5 ? ['ciepło', 'warm'] : ['komfortowo', 'ok'];
  const humWord = h => h < 35 ? ['sucho', 'warm'] : h > 60 ? ['wilgotno', 'cold'] : h < 40 ? ['lekko sucho', 'ok'] : ['komfortowo', 'ok'];

  // Karta klimatu pokoju: dwa odczyty z czujnika, ikona pixel art + duża liczba + ocena.
  // Pion: pod paskiem pokoi, nad urządzeniami. Poziom: pierwsza kolumna w rzędzie kart.
  function climaHtml(land) {
    const r = room();
    if (r.temp === undefined && r.hum === undefined) return '';
    const cell = (icon, val, unit, label, [word, cls]) => `<div class="rcv" aria-label="${label} ${val} ${unit}, ${word}"><img src="${A(icon)}" alt="">
        <div><b>${val}<small>${unit}</small></b><span class="rcl ${cls}">${word}</span></div></div>`;
    const t = r.temp !== undefined ? cell('sensor_temp', deg(r.temp).slice(0, -1), '°C', 'Temperatura', tempWord(r.temp)) : '';
    const h = r.hum !== undefined ? cell('sensor_humidity', r.hum, '%', 'Wilgotność', humWord(r.hum)) : '';
    return `<div class="rclim ${land ? 'is-land' : ''}">${t}${t && h ? '<i class="rsep"></i>' : ''}${h}</div>`;
  }

  /* ---------- model światła: grupa albo pojedyncza żarówka ---------- */
  const isGroup = l => !!(l && l.bulbs);
  const lOn = l => isGroup(l) ? l.bulbs.some(b => b.on) : !!(l && l.on);
  const lBr = l => {
    if (!isGroup(l)) return l.br;
    const on = l.bulbs.filter(b => b.on);
    return on.length ? Math.round(on.reduce((s, b) => s + b.br, 0) / on.length) : 0;
  };
  const lCt = l => isGroup(l) ? (l.bulbs.find(b => b.on) || l.bulbs[0]).ct : l.ct;
  const dimmable = l => isGroup(l) || l.br !== undefined;
  const lampState = l => !l ? 'none' : l.na ? 'na' : !lOn(l) ? 'off' : (dimmable(l) && lBr(l) < 50 ? 'dim' : 'on');
  const pendingOf = l => isGroup(l) ? (l.pending || l.bulbs.some(b => b.pending)) : l.pending;

  const views = [];
  let roomId = 'salon';
  let stack = [];  // [{t:'light'}, {t:'bulb', id}]

  function confirm(target, apply) {  // stan oczekujący → potwierdzenie z HA
    if (target.pending) return;
    target.pending = true; renderAll();
    setTimeout(() => { apply(); target.pending = false; renderAll(); }, 700);
  }
  function togglePower(l) {
    confirm(l, () => {
      if (isGroup(l)) { const on = !lOn(l); l.bulbs.forEach(b => { b.on = on; }); if (!on) l.effect = null; }
      else l.on = !l.on;
    });
  }

  /* ---------- siatka pokoi ---------- */
  const status = l => {
    if (!l) return '<i class="rdot off"></i>brak światła';
    if (l.na) return '<i class="rdot off"></i>niedostępne';
    if (!lOn(l)) return `<i class="rdot off"></i>${l.label} · wył.`;
    if (isGroup(l) && l.effect) return `<i class="rdot"></i>${l.label} · ${effLabel(l.effect)}`;
    return `<i class="rdot"></i>${l.label} · ${dimmable(l) ? lBr(l) + '%' : 'wł.'}`;
  };
  const chip = r => {
    const s = lampState(r.light);
    if (s === 'none') return '<span class="rlamp none" aria-hidden="true"></span>';
    const bar = dimmable(r.light) && lOn(r.light) ? `<span class="rbar"><i style="width:${lBr(r.light)}%"></i></span>` : '';
    return `<span class="rlamp ${s} ${pendingOf(r.light) ? 'pending' : ''}" data-power="${r.id}" role="button" aria-label="${r.name}: przełącz światło"><img src="${A('lamp_' + s)}" alt="">${bar}</span>`;
  };
  const gridHtml = () => rooms.map(r => `<div class="room ${lOn(r.light) ? 'is-lit' : ''} ${r.temp !== undefined ? 'has-temp' : ''}" data-room="${r.id}" role="button">
      <img class="room-icon" src="${A('room_' + r.id)}" alt="">${chip(r)}
      <span class="room-name">${r.name}</span><span class="room-st">${status(r.light)}</span>${r.temp !== undefined ? `<span class="room-temp">${deg(r.temp)}</span>` : ''}
    </div>`).join('');

  /* ---------- poziomy podstrony ---------- */
  const room = () => rooms.find(r => r.id === roomId);
  const top = () => stack[stack.length - 1];
  const target = () => { const t = top(); return t && t.t === 'bulb' ? lampa.bulbs.find(b => b.id === t.id) : lampa; };

  // Zasilanie w nagłówku: tylko na poziomach światła (grupa, żarówka, efekty).
  function headerPower() {
    const t = top();
    if (!t) return null;
    const l = t.t === 'bulb' ? target() : lampa;
    return { on: lOn(l), pending: !!pendingOf(l) };
  }
  function title() {
    const t = top();
    if (!t) return { title: room().name, sub: 'POMIESZCZENIE', power: null };
    const power = headerPower();
    if (t.t === 'light') return { title: lampa.label, sub: `${room().name.toUpperCase()} · ${lampa.bulbs.length} ŻARÓWKI`, power };
    return { title: `Żarówka ${target().label.toLowerCase()}`, sub: `${lampa.label.toUpperCase()} · ${room().name.toUpperCase()}`, power };
  }
  // Przycisk zasilania w nagłówku makiety woła to — przełącza światło bieżącego poziomu.
  function power() {
    const t = top();
    if (!t) return;
    togglePower(t.t === 'bulb' ? target() : lampa);
  }

  const pwr = (on, pending, key) => `<span class="rpwr ${on ? 'on' : ''} ${pending ? 'pending' : ''}" data-power="${key}" role="button" aria-label="Zasilanie"><svg viewBox="0 0 24 24" fill="none" stroke-width="2.4" stroke-linecap="round"><path d="M12 3v8"/><path d="M6.3 6.8a8 8 0 1 0 11.4 0"/></svg></span>`;
  const chev = '<svg class="rchev" viewBox="0 0 24 24" fill="none" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"><path d="m9 6 6 6-6 6"/></svg>';

  // Poziom 1: pokój = lista encji
  function genericRoomHtml(land) {
    const r = room(), l = r.light;
    const cards = [];
    if (l) {
      const s = lampState(l);
      cards.push(`<div class="rent">
        <div class="rwell sm ${s}"><img src="${A('lamp_' + s)}" alt=""></div>
        <div class="rlc"><b>Światło</b><span>${lOn(l) ? 'włączone' : 'wyłączone'}</span><em>${l.entity}</em></div>
        ${pwr(lOn(l), pendingOf(l), r.id)}
      </div>`);
    }
    cards.push(`<div class="rent rempty"><div class="rlc"><b>${l ? 'Inne encje' : 'Brak encji'}</b><span>do wybrania w konfiguracji pilota</span></div></div>`);
    return roomBody(land, cards.join(''));
  }

  function roomHtml(land) {
    if (room().light !== lampa) return genericRoomHtml(land);
    const s = lampState(lampa), on = lOn(lampa);
    const lampSub = !on ? 'wyłączona' : lampa.effect ? `efekt: ${effLabel(lampa.effect)}` : `${lBr(lampa)}% · ${CTK[lCt(lampa)]} K`;
    const lit = lampa.bulbs.filter(b => b.on).length;
    const lampCard = `<div class="rent" data-go="light" role="button">
        <div class="rwell sm ${s}"><img src="${A('lamp_' + s)}" alt=""></div>
        <div class="rlc"><b>Lampa</b><span>${lampSub}</span><em>${lit} z ${lampa.bulbs.length} żarówek</em></div>
        ${pwr(on, pendingOf(lampa), 'lampa')}${chev}
        ${on ? `<span class="rbar wide"><i style="width:${lBr(lampa)}%"></i></span>` : ''}
      </div>`;
    const klCard = `<div class="rent" data-go="klima" role="button">
        <div class="rwell sm kl"><img src="${A('dev_aircon')}" alt=""></div>
        <div class="rlc"><b>Klimatyzacja</b><span>Chłodzenie · 22,0°</span><em>w pokoju 24,0°</em></div>
        ${chev}
      </div>`;
    return roomBody(land, lampCard + klCard);
  }

  // Pion: karta klimatu nad listą urządzeń. Poziom: karta klimatu jako pierwsza kolumna rzędu.
  function roomBody(land, cards) {
    if (land) return `<p class="rsec">W pokoju · urządzenia</p><div class="rents is-land">${climaHtml(true)}${cards}</div>`;
    const c = climaHtml(false);
    return `${c ? `<p class="rsec">W pokoju</p>${c}` : ''}<p class="rsec">Urządzenia</p><div class="rents">${cards}</div>`;
  }

  function segs(n, f, cls, off) {
    return `<div class="rseg ${cls} ${off ? 'off' : ''}">${Array.from({ length: n }, (_, i) =>
      `<i data-i="${i}" class="${f(i) ? 'f' : ''}" ${cls === 'ct' ? `style="background:${CT[i]}"` : ''}></i>`).join('')}</div>`;
  }

  // Poziom 2: encja światła (grupa) albo poziom 3: pojedyncza żarówka
  // Poziom 2 i 3: światło zbudowane jak podstrona klimy — jedna duża tarcza jasności
  // (łuk 270°, −/+ w przerwie) i obok jednolite kafle: barwa, efekt, żarówki.
  const ARC = 650.3, CIRC = 867.1;   // r = 138: 270° z obwodu
  let dialDraft = null;               // wartość pod palcem w trakcie gestu
  function lightHtml(land) {
    const l = target(), group = isGroup(l), on = lOn(l), br = dialDraft ?? lBr(l), ct = lCt(l);
    const effectOn = group && l.effect && on;
    const cap = !on ? 'WYŁĄCZONA' : effectOn ? 'EFEKT' : 'JASNOŚĆ';
    const note = !on ? 'dotknij +, żeby włączyć' : effectOn ? effLabel(l.effect) : `${CTK[ct]} K · ${ctName(ct)}`;
    const dash = on ? ARC * br / 100 : 0;
    const dial = `<div class="ldial ${on ? '' : 'off'} ${pendingOf(l) ? 'pending' : ''}">
        <svg viewBox="0 0 300 300" class="ldial-ring">
          <circle cx="150" cy="150" r="138" fill="none" stroke="var(--surface-3)" stroke-width="20" stroke-linecap="round" stroke-dasharray="${ARC} ${CIRC}" transform="rotate(135 150 150)"/>
          <circle cx="150" cy="150" r="138" fill="none" stroke="${effectOn ? 'url(#lgrad)' : 'var(--accent)'}" stroke-width="20" stroke-linecap="round" stroke-dasharray="${dash} ${CIRC}" transform="rotate(135 150 150)"/>
          <defs><linearGradient id="lgrad" x1="0" x2="1"><stop offset="0" stop-color="#f2a93b"/><stop offset="1" stop-color="#ff7e6b"/></linearGradient></defs>
        </svg>
        <div class="ldial-in"><span class="ldial-cap">${cap}</span><b>${on ? br : '—'}<small>${on ? '%' : ''}</small></b><span class="ldial-note">${note}</span></div>
        <span class="ldial-scale lo">1%</span><span class="ldial-scale hi">100%</span>
        <button class="ldial-step minus" data-bstep="-1" aria-label="Ciemniej">−</button>
        <button class="ldial-step plus" data-bstep="1" aria-label="Jaśniej">+</button>
      </div>`;
    const swatches = `<p class="rsec">Barwa</p><div class="lswatch ${on && !effectOn ? '' : 'off'}">${CTK.map((k, i) =>
      `<button data-ct="${i}" class="${on && !effectOn && i === ct ? 'on' : ''}" style="background:${CT[i]}" aria-label="${k} K"></button>`).join('')}</div>
      <div class="lswatch-scale"><span>ciepła</span><span>zimna</span></div>`;
    if (!group) {
      const side = `${swatches}<p class="rsec">W grupie</p><div class="ltile static"><span class="ltile-cap">LAMPA</span><b>zmiana tej żarówki przerywa efekt grupy</b></div>`;
      return `<div class="llayout ${land ? 'is-land' : ''}">${dial}<div class="lside">${side}</div></div>`;
    }
    const effIcon = (EFFECTS.find(e => e[0] === l.effect) || [0, 0, 'circle-off'])[2];
    const eff = `<p class="rsec">Efekt · cała lampa</p><button class="ltile wide ${effectOn ? 'on' : ''}" data-go="effect">
        <i data-lucide="${effIcon}"></i><span class="ltile-txt"><span class="ltile-cap">EFEKT</span><b>${l.effect ? effLabel(l.effect) : 'Brak'}</b></span>${chev}</button>`;
    const bulbs = `<p class="rsec">Żarówki</p><div class="lbulbs">${l.bulbs.map(b => `<button class="ltile ${b.on ? 'on' : ''} ${b.pending ? 'pending' : ''}" data-go="bulb:${b.id}">
        <img src="${A('bulb_' + (b.on ? 'on' : 'off'))}" alt=""><span class="ltile-cap">${b.label.toUpperCase()}</span><b>${b.on ? b.br + '%' : 'wył.'}</b></button>`).join('')}</div>`;
    return `<div class="llayout ${land ? 'is-land' : ''}">${dial}<div class="lside">${swatches}${eff}${bulbs}</div></div>`;
  }
  const CT_NAMES = ['bardzo ciepła', 'ciepła', 'ciepła biel', 'neutralna', 'biała', 'chłodna', 'zimna'];
  const ctName = i => CT_NAMES[i] || '';

  // Wybór efektu: lista jak wybór źródła czy trybu klimy — w poziomie panel przy prawej
  // krawędzi nad przyciemnionym ekranem, w pionie pełny ekran. Wybór zamyka listę.
  function openEffects(el, land) {
    const host = el.closest('.rpage') || el.closest('.page') || el.parentElement;
    let sheet = host.querySelector(':scope > .rsheet');
    if (!sheet) {
      sheet = document.createElement('div');
      sheet.className = 'rsheet';
      sheet.innerHTML = '<div class="rsheet-panel"><div class="rsheet-head"><button class="rsheet-back" aria-label="Cofnij">‹</button><b>Efekt</b></div><div class="rsheet-list"></div></div>';
      host.appendChild(sheet);
      sheet.addEventListener('click', e => {
        if (e.target === sheet || e.target.closest('.rsheet-back')) { sheet.hidden = true; return; }
        const row = e.target.closest('[data-eff]');
        if (!row) return;
        const k = row.dataset.eff || null;
        sheet.hidden = true;   // lista znika od razu; oczekiwanie widać na kaflu efektu
        confirm(lampa, () => { lampa.effect = k; if (k) lampa.bulbs.forEach(b => { b.on = true; }); });
      });
    }
    sheet.classList.toggle('is-land', land);
    const items = [[null, 'Brak', 'circle-off'], ...EFFECTS];
    sheet.querySelector('.rsheet-list').innerHTML = items.map(([k, lab, ic]) =>
      `<button class="rsheet-row ${lampa.effect === k ? 'on' : ''}" data-eff="${k ?? ''}"><i data-lucide="${ic}"></i><b>${lab}</b><span>${lampa.effect === k ? 'aktywny' : ''}</span></button>`).join('');
    sheet.hidden = false;
    sheet.querySelector('.rsheet-list').scrollTop = 0;
    if (window.lucide) window.lucide.createIcons();
  }

  function detailHtml(land) {
    const t = top();
    if (!t) return roomHtml(land);
    return lightHtml(land);
  }

  const railHtml = () => rooms.map(r => `<button class="rrail-item" type="button" data-pick-room="${r.id}" aria-selected="${r.id === roomId}" aria-label="${r.name}"><img src="${A('room_' + r.id)}" alt=""></button>`).join('');

  function renderAll() {
    views.forEach(v => {
      if (v.kind === 'grid') { v.el.innerHTML = gridHtml(); return; }
      if (v.kind === 'rail') { v.el.innerHTML = railHtml(); return; }
      v.el.innerHTML = detailHtml(v.land);
      v.el.classList.toggle('is-split', false);
      if (v.opts.nav) v.opts.nav(title());
    });
    if (window.lucide) window.lucide.createIcons();
  }

  function onPower(key) {
    if (key === 'lampa' || rooms.some(r => r.id === key && r.light === lampa)) return togglePower(lampa);
    if (key.startsWith('bulb:')) return togglePower(lampa.bulbs.find(b => b.id === key.slice(5)));
    const r = rooms.find(x => x.id === key);
    if (r && r.light) togglePower(r.light);
  }

  function grid(el, opts = {}) {
    el.classList.add('rooms');
    el.classList.toggle('is-land', !!opts.land);
    views.push({ kind: 'grid', el, land: !!opts.land, opts });
    el.addEventListener('click', e => {
      const p = e.target.closest('[data-power]');
      if (p) { e.stopPropagation(); onPower(p.dataset.power); return; }
      const r = e.target.closest('[data-room]');
      if (!r) return;
      const rm = rooms.find(x => x.id === r.dataset.room);
      if (opts.open) { roomId = rm.id; stack = []; renderAll(); opts.open(rm.id); }
    });
    renderAll();
  }

  function detail(el, opts = {}) {
    el.classList.add('rdetail');
    el.classList.toggle('is-land', !!opts.land);
    views.push({ kind: 'detail', el, land: !!opts.land, opts });
    dialGesture(el);
    el.addEventListener('click', e => {
      const p = e.target.closest('[data-power]');
      if (p) { e.stopPropagation(); onPower(p.dataset.power); return; }
      const step = e.target.closest('[data-bstep]');
      if (step) {
        const l = target(), list = isGroup(l) ? l.bulbs : [l], dir = +step.dataset.bstep;
        const now = lOn(l) ? lBr(l) : 0;
        const next = Math.max(10, Math.min(100, Math.round(now / 10) * 10 + dir * 10));
        if (!lOn(l) && dir < 0) return;
        confirm(l, () => { list.forEach(b => { if (b.on || !lOn(l)) { b.on = true; b.br = next; } }); });
        return;
      }
      const sw = e.target.closest('[data-ct]');
      if (sw) { const l = target(); if (!lOn(l)) return; (isGroup(l) ? l.bulbs.filter(b => b.on) : [l]).forEach(b => { b.ct = +sw.dataset.ct; }); if (!isGroup(l)) lampa.effect = null; else l.effect = null; renderAll(); return; }
      const seg = e.target.closest('.rseg i');
      if (seg) {
        const l = target(), i = +seg.dataset.i, list = isGroup(l) ? l.bulbs.filter(b => b.on) : [l];
        if (seg.parentElement.classList.contains('br')) list.forEach(b => { b.br = (i + 1) * 10; });
        else { list.forEach(b => { b.ct = i; }); }
        if (!isGroup(l)) lampa.effect = null;
        renderAll(); return;
      }
      const go = e.target.closest('[data-go]');
      if (!go) return;
      const g = go.dataset.go;
      if (g === 'klima') { if (opts.openKlima) opts.openKlima(); return; }
      if (g === 'effect') { openEffects(el, !!opts.land); return; }
      if (g === 'light') stack.push({ t: g });
      else if (g.startsWith('bulb:')) stack.push({ t: 'bulb', id: g.slice(5) });
      renderAll();
    });
    renderAll();
  }

  // Pasek pomieszczeń na podstronie: przełącza pokój bez cofania do siatki.
  function rail(el, opts = {}) {
    el.classList.add('rrail');
    el.classList.toggle('is-land', !!opts.land);
    views.push({ kind: 'rail', el, land: !!opts.land, opts });
    el.addEventListener('click', e => {
      const b = e.target.closest('[data-pick-room]');
      if (!b || b.dataset.pickRoom === roomId && !stack.length) return;
      roomId = b.dataset.pickRoom; stack = []; renderAll();
    });
    renderAll();
  }

  function dialGesture(el) {
    let last = null;
    const angle = e => { const r = el.querySelector('.ldial-ring').getBoundingClientRect(); return Math.atan2(e.clientY - (r.top + r.height / 2), e.clientX - (r.left + r.width / 2)) * 180 / Math.PI; };
    el.addEventListener('pointerdown', e => { if (!e.target.closest('.ldial-ring')) return; const l = target(); if (!lOn(l)) return; last = angle(e); dialDraft = lBr(l); });
    el.addEventListener('pointermove', e => {
      if (last === null) return;
      const a = angle(e); let d = a - last; if (d > 180) d -= 360; if (d < -180) d += 360; last = a;
      dialDraft = Math.max(1, Math.min(100, dialDraft + d * 100 / 270)); dialDraft = Math.round(dialDraft); renderAll();
    });
    const end = () => { if (last === null) return; last = null; const v = dialDraft, l = target(); dialDraft = null;
      confirm(l, () => { (isGroup(l) ? l.bulbs.filter(b => b.on) : [l]).forEach(b => { b.br = v; }); }); };
    el.addEventListener('pointerup', end); el.addEventListener('pointerleave', end);
  }

  function back() {
    if (!stack.length) return false;
    stack.pop(); renderAll(); return true;
  }
  const name = () => room().name;
  window.PilotRooms = { grid, detail, rail, back, name, title, power };
})();
