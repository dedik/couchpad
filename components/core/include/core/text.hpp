#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace core {

// Krótki napis o stałej pojemności. Wartości ze snapshotu są z natury krótkie
// (nazwy źródeł, trybów, etykiety), a brak alokacji oznacza brak niespodzianek
// w zadaniu UI. Projekt kompiluje się bez wyjątków, więc std::string odpada.
template <std::size_t N>
class Text {
public:
    Text() { data_[0] = '\0'; }
    Text(const char* value) { assign(value); }  // NOLINT(google-explicit-constructor)

    void assign(const char* value) {
        if (value == nullptr) {
            clear();
            return;
        }
        const std::size_t length = std::min(std::strlen(value), N - 1);
        std::memcpy(data_, value, length);
        data_[length] = '\0';
    }

    void clear() { data_[0] = '\0'; }
    bool empty() const { return data_[0] == '\0'; }
    const char* c_str() const { return data_; }

    bool operator==(const Text& other) const { return std::strcmp(data_, other.data_) == 0; }
    bool operator!=(const Text& other) const { return !(*this == other); }
    bool operator==(const char* other) const {
        return other != nullptr && std::strcmp(data_, other) == 0;
    }

private:
    char data_[N];
};

}  // namespace core
