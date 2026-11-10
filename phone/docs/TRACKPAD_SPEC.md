# Couchpad Pilot — Apple TV Glass Trackpad (M3 Expressive) Specification

> **Stan i status**: Doceniony i zatwierdzony komponent UI (`.m3-glass-trackpad`).  
> **Projekt**: Pilot Couchpad Android (Material You M3 Expressive, Akcent Papaja `#FF7300`).
>
> **Wdrożenie (2026-11-10)**: `native/.../ui/appletv/Trackpad.kt` i makiety `docs/mockups/remote-*.html`. Krążek na środku jest pusty, bez ikony pilota, i gładzik nie ma podpisu — oba elementy z kodu niżej świadomie pominięto. W stylu OLED płyta jest czarna z samym obrysem, bez połysku.

---

## 1. Koncepcja i Filozofia Projektowa

Gładzik dotykowy Apple TV w Couchpadzie został zaprojektowany tak, aby łączyć:
1. **Fizyczną haptykę pilota Apple TV Remote (Siri Remote)**: matowa, aksamitna powierzchnia ze szkła z subtelną krawędzią, centralnym wklęsłym punktem odniesienia (touch target) oraz delikatnymi znacznikami kierunkowymi (D-pad arrows).
2. **Język Google Material You M3 Expressive**: zaokrąglone narożniki (`border-radius: 28px`), dynamiczne tło z mikro-gradientem powierzchni (`surface-container-high`), subtelny pierścień poświaty papaja (`#FF7300`) i haptyczne stany aktywne.
3. **Ergonomię kciuka**: centralny wskaźnik z ikoną pilota/ekranu daje orientację bez konieczności patrzenia na ekran telefonu.

---

## 2. Anatomia Komponentu

```
┌────────────────────────────────────────────────────────┐
│  .m3-glass-trackpad                                    │
│                                                        │
│                           ▲  (.trackpad-arrow.up)      │
│                                                        │
│                                                        │
│   ◄                       ◎                       ►    │
│  (.arrow.left)    .trackpad-center-badge     (.arrow.rt)│
│                     (ikona + subtelna                  │
│                      poświata papaja)                  │
│                                                        │
│                           ▼  (.trackpad-arrow.down)    │
│                                                        │
│              "Gładzik aktywny — przesuń lub kliknij"   │
└────────────────────────────────────────────────────────┘
```

---

## 3. Kod HTML i CSS Komponentu

### Struktura HTML:
```html
<div class="m3-glass-trackpad">
  <div class="trackpad-arrow top"><i class="pixel-icon px_up"></i></div>
  <div class="trackpad-arrow bottom"><i class="pixel-icon px_down"></i></div>
  <div class="trackpad-arrow left"><i class="pixel-icon px_left"></i></div>
  <div class="trackpad-arrow right"><i class="pixel-icon px_right"></i></div>
  
  <div class="trackpad-center-badge">
    <div class="center-badge-glow"></div>
    <i class="pixel-icon px_remote"></i>
  </div>
  
  <div class="trackpad-caption">
    Gładzik dotykowy — przesuń lub kliknij OK
  </div>
</div>
```

### Styl CSS (Czysty OLED & Jasny Material You):
```css
/* Kontener gładzika */
.m3-glass-trackpad {
  position: relative;
  flex: 1;
  min-height: 220px;
  border-radius: 28px;
  background: radial-gradient(circle at 50% 50%, rgba(255, 115, 0, 0.04) 0%, rgba(255, 255, 255, 0.02) 60%, transparent 100%),
              linear-gradient(180deg, rgba(255, 255, 255, 0.05) 0%, rgba(255, 255, 255, 0.01) 100%),
              #121316;
  border: 1px solid rgba(255, 255, 255, 0.08);
  box-shadow: inset 0 1px 1px rgba(255, 255, 255, 0.12),
              inset 0 -2px 6px rgba(0, 0, 0, 0.4),
              0 8px 24px rgba(0, 0, 0, 0.35);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  user-select: none;
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.2, 0, 0, 1);
  overflow: hidden;
}

/* Wersja jasna (Light Dynamic) */
[data-theme="light"] .m3-glass-trackpad {
  background: radial-gradient(circle at 50% 50%, rgba(255, 115, 0, 0.06) 0%, rgba(0, 0, 0, 0.01) 70%, transparent 100%),
              linear-gradient(180deg, #F0ECE4 0%, #E7E2DA 100%);
  border: 1px solid rgba(0, 0, 0, 0.08);
  box-shadow: inset 0 1px 1px rgba(255, 255, 255, 0.8),
              inset 0 -2px 4px rgba(0, 0, 0, 0.05),
              0 6px 18px rgba(0, 0, 0, 0.06);
}

/* Wskaźniki kierunkowe (strzałki subtelne, nieinwazyjne) */
.trackpad-arrow {
  position: absolute;
  color: rgba(255, 255, 255, 0.25);
  font-size: 11px;
  pointer-events: none;
  transition: color 0.15s ease;
}
[data-theme="light"] .trackpad-arrow {
  color: rgba(0, 0, 0, 0.25);
}
.trackpad-arrow.top    { top: 14px; left: 50%; transform: translateX(-50%); }
.trackpad-arrow.bottom { bottom: 32px; left: 50%; transform: translateX(-50%); }
.trackpad-arrow.left   { left: 16px; top: 50%; transform: translateY(-50%); }
.trackpad-arrow.right  { right: 16px; top: 50%; transform: translateY(-50%); }

/* Centralny wklęsły touch-badge */
.trackpad-center-badge {
  position: relative;
  width: 46px;
  height: 46px;
  border-radius: 50%;
  background: radial-gradient(circle at 35% 35%, #222328 0%, #17181c 100%);
  border: 1px solid rgba(255, 115, 0, 0.45);
  box-shadow: 0 0 16px rgba(255, 115, 0, 0.22),
              inset 0 1px 2px rgba(255, 255, 255, 0.15),
              inset 0 -1px 2px rgba(0, 0, 0, 0.6);
  display: flex;
  align-items: center;
  justify-content: center;
  color: #FF8800;
  transition: transform 0.12s cubic-bezier(0.2, 0, 0, 1), box-shadow 0.15s ease;
}

[data-theme="light"] .trackpad-center-badge {
  background: radial-gradient(circle at 35% 35%, #FFFFFF 0%, #EDE8E0 100%);
  border: 1.5px solid #FF7300;
  box-shadow: 0 0 12px rgba(255, 115, 0, 0.25),
              inset 0 1px 2px rgba(255, 255, 255, 0.9),
              inset 0 -1px 2px rgba(0, 0, 0, 0.08);
  color: #C95000;
}

/* Stan naciśnięcia / gestu */
.m3-glass-trackpad:active .trackpad-center-badge {
  transform: scale(0.92);
  border-color: #FF8800;
  box-shadow: 0 0 24px rgba(255, 115, 0, 0.45);
}

/* Etykieta podpowiedzi na dole gładzika */
.trackpad-caption {
  position: absolute;
  bottom: 12px;
  font-size: 10px;
  font-weight: 500;
  letter-spacing: 0.3px;
  color: rgba(255, 255, 255, 0.4);
  text-align: center;
  pointer-events: none;
}
[data-theme="light"] .trackpad-caption {
  color: rgba(0, 0, 0, 0.45);
}
```

---

## 4. Dlaczego ten komponent działa i jak rozszerzyć tę zasadę na cały pilot

1. **Głębia haptyczna**: Użycie mikroskopijnych gradientów, wewnętrznego podświetlenia krawędzi (`inset 0 1px 1px rgba(255,255,255,0.12)`) i miękkiego cienia tworzy wrażenie fizycznego, drogiego przedmiotu trzymanego w dłoni, a nie płaskiej strony internetowej.
2. **Subtelny akcent Papaja**: Papaja (`#FF7300`) nie zalewa całego elementu — jest użytkowana jak precyzyjny laser na obwódce centralnego wskaźnika i w delikatnej poświacie.
3. **Zbalansowana skala**: Centralny krążek 46px jest idealny pod opuszek kciuka.
4. **Standard przeniesiony na inne panele**:
   - Przyciski steppera (`−` / `+`) powinny mieć styl fizycznych kapsuł/dysków 32-34px z subtelnym beveled border, a nie wielkich jaskrawych pomarańczowych plam.
   - Płytki urządzeń i pomieszczeń powinny mieć tę samą aksamitną powierzchnię `surface-container` z delikatnym wewnętrznym światłem.
