#pragma once

// Uzupełnienie board.hpp na telefonie: pętla zdarzeń SDL i LVGL w wątku głównym.

namespace board {

// Obsługuje zdarzenia SDL i timery LVGL do zamknięcia aplikacji.
void run_loop();
void request_quit();

}  // namespace board
