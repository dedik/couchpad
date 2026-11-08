package app.couchpad.pilot;

import org.libsdl.app.SDLActivity;

/** Pełnoekranowy pilot: SDL2 + biblioteka `main` z interfejsem LVGL firmware'u. */
public class PilotActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"c++_shared", "SDL2", "main"};
    }
}
