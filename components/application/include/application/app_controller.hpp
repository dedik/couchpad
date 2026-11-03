#pragma once

// Kontroler aplikacji: zamiar z dotyku zamienia na polecenie i od razu oznacza
// sterowanie jako oczekujące; zdarzenia ze źródła zamienia na nowy stan interfejsu.
//
// Dotknięcie nigdy nie czeka na sieć — `dispatch()` wraca natychmiast.

#include "application/snapshot_source.hpp"
#include "domain/app_state.hpp"
#include "domain/intent.hpp"

namespace application {

class AppController {
public:
    using ChangeHandler = void (*)(void* context);

    explicit AppController(SnapshotSource& source) : source_(&source) {}

    // Podmiana źródła czyści stan: rewizje dwóch źródeł nie są porównywalne,
    // a zapamiętany snapshot nie opisuje już niczego prawdziwego.
    void set_source(SnapshotSource& source);

    void start();

    // Wołane cyklicznie z zadania UI. Odbiera zdarzenia i pilnuje limitów czasu.
    void pump();

    void dispatch(const domain::UiIntent& intent);

    const domain::AppState& state() const { return state_; }
    // Lista kanałów bieżącego źródła (ADR 0008); nullptr, gdy jej nie zna.
    const domain::ChannelList* channels() const { return source_->channels(); }
    const domain::Weather* weather() const { return source_->weather(); }

    void set_on_change(ChangeHandler handler, void* context) {
        on_change_ = handler;
        on_change_context_ = context;
    }

private:
    void notify();

    SnapshotSource* source_;
    domain::AppState state_;
    // Odebrane zdarzenie. Pole, a nie zmienna lokalna w `pump()`: niesie cały
    // snapshot (z klimatyzacją ok. 3,6 kB), a `pump()` biegnie na stosie zadania LVGL.
    SourceEvent event_;
    // Kiedy weszliśmy w fazę łączenia. Snapshot, który nie przychodzi, musi
    // w końcu zostać nazwany błędem, a nie wiecznym „łączeniem".
    int64_t connecting_since_ms_ = 0;

    ChangeHandler on_change_ = nullptr;
    void* on_change_context_ = nullptr;
};

}  // namespace application
