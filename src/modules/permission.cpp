#include "modules/permission.hpp"

namespace noteman::modules::permission {

int bit(Class cls, Access access) {
    const bool read = access == Access::Read;
    switch (cls) {
    case Class::Owner: return read ? kOwnerRead : kOwnerWrite;
    case Class::Group: return read ? kGroupRead : kGroupWrite;
    case Class::Other: return read ? kOtherRead : kOtherWrite;
    }
    return 0;
}

Class resolve_class(bool is_owner, bool in_group) {
    if (is_owner) return Class::Owner;
    if (in_group) return Class::Group;
    return Class::Other;
}

bool allows(int mode, Class cls, Access access) {
    return is_valid(mode) && (mode & bit(cls, access)) != 0;
}

std::string to_string(int mode) {
    std::string out(6, '-');
    if (mode & kOwnerRead) out[0] = 'r';
    if (mode & kOwnerWrite) out[1] = 'w';
    if (mode & kGroupRead) out[2] = 'r';
    if (mode & kGroupWrite) out[3] = 'w';
    if (mode & kOtherRead) out[4] = 'r';
    if (mode & kOtherWrite) out[5] = 'w';
    return out;
}

std::optional<int> parse(std::string_view text) {
    if (text.size() != 6) return std::nullopt;
    int mode = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        const char expected = (i % 2 == 0) ? 'r' : 'w';
        if (text[i] == expected) {
            mode |= 1 << (5 - i);
        } else if (text[i] != '-') {
            return std::nullopt;
        }
    }
    return mode;
}

} // namespace noteman::modules::permission
