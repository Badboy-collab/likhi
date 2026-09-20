#ifndef BANGLA_INSCRIPT_LAYOUT_H
#define BANGLA_INSCRIPT_LAYOUT_H

#include <string>
#include <unordered_map>

namespace bangla {

class InscriptLayout {
public:
    InscriptLayout();
    ~InscriptLayout() = default;

    /**
     * Map an ASCII character to Bengali Unicode under National INSCRIPT standard.
     * @param key ASCII character (e.g. 'k' -> "ক", 'K' -> "খ")
     * @param is_shift Whether shift modifier is active
     * @return UTF-8 Bengali character string, or original character if no mapping exists.
     */
    std::string GetChar(char key, bool is_shift = false) const;

    /**
     * Map a wchar_t character to UTF-16 wchar_t.
     */
    wchar_t GetCharW(wchar_t key, bool is_shift = false) const;

    bool HasMapping(char key, bool is_shift = false) const;

private:
    std::unordered_map<char, std::string> normal_map_u8_;
    std::unordered_map<char, std::string> shift_map_u8_;
    std::unordered_map<wchar_t, wchar_t> normal_map_w_;
    std::unordered_map<wchar_t, wchar_t> shift_map_w_;
};

} // namespace bangla

#endif // BANGLA_INSCRIPT_LAYOUT_H
