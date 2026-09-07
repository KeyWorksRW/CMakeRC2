#pragma once

#ifndef CMRC_CMRC_HPP_INCLUDED
#define CMRC_CMRC_HPP_INCLUDED

// CR: [09-06-2026]

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#if defined(_MSVC_LANG)
#define CMRC_CPLUSPLUS _MSVC_LANG
#else
#define CMRC_CPLUSPLUS __cplusplus
#endif

#if CMRC_CPLUSPLUS >= 201703L && defined(__has_include)
#if __has_include(<string_view>)
#include <string_view>
#define CMRC_HAS_STRING_VIEW 1
#endif
#endif
#ifndef CMRC_HAS_STRING_VIEW
#define CMRC_HAS_STRING_VIEW 0
#endif

#if !(defined(__EXCEPTIONS) || defined(__cpp_exceptions) ||                    \
      defined(_CPPUNWIND) || defined(CMRC_NO_EXCEPTIONS))
#define CMRC_NO_EXCEPTIONS 1
#endif

// When CMRC_CMRC_HPP_BASE64 is defined by the generated resource TU
// (CMRC_BASE64 build option), a base64 decoder is compiled into cmrc::detail so
// generated resource files can be stored as compact base64 strings instead of
// ~6x-expanded '\xNN' character literals. The decoder is intentionally excluded
// from every other TU (including lib.cpp, which only uses raw begin/end
// pointers) so base64 support costs nothing when CMRC_BASE64 is OFF.
#if defined(CMRC_CMRC_HPP_BASE64)
#include <cstddef>
#endif

namespace cmrc {
namespace detail {
struct dummy;
}
} // namespace cmrc

#define CMRC_DECLARE(libid)                                                    \
  namespace cmrc {                                                             \
  namespace detail {                                                           \
  struct dummy;                                                                \
  static_assert(std::is_same<dummy, ::cmrc::detail::dummy>::value,             \
                "CMRC_DECLARE() must only appear at the global namespace");    \
  }                                                                            \
  }                                                                            \
  namespace cmrc {                                                             \
  namespace libid {                                                            \
  cmrc::embedded_filesystem get_filesystem();                                  \
  }                                                                            \
  }                                                                            \
  static_assert(true, "")

namespace cmrc {

class file {
  const char *_begin = nullptr;
  const char *_end = nullptr;

public:
  using iterator = const char *;
  using const_iterator = iterator;
  iterator begin() const noexcept { return _begin; }
  iterator cbegin() const noexcept { return _begin; }
  iterator end() const noexcept { return _end; }
  iterator cend() const noexcept { return _end; }
  std::size_t size() const {
    return static_cast<std::size_t>(std::distance(begin(), end()));
  }

#if CMRC_HAS_STRING_VIEW
  std::string_view view() const noexcept {
    return std::string_view(_begin, size());
  }
#endif

  file() = default;
  file(iterator beg, iterator end) noexcept : _begin(beg), _end(end) {}
};

class directory_entry;

namespace detail {

#if defined(CMRC_CMRC_HPP_BASE64)
// Decodes a resource stored as separate base64 chunk literals. MSVC's
// per-literal (~16 K) and post-concatenation (64 K on pre-2022 versions) caps
// make a single giant string literal illegal on older compilers, so the
// generated resource TU splits the base64 into several static arrays and this
// decoder stitches them back together. It returns the decoded bytes by value;
// the generated TU stores that std::string in a namespace-scope static so the
// buffer outlives the whole process (the same lifetime the resource library
// assumes), then points its const char* const begin/end symbols into it.
inline std::string b64_decode(const char *const *chunks,
                              const std::size_t *lens, std::size_t count) {
  std::size_t total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total += lens[i];
  }
  std::string encoded;
  encoded.reserve(total);
  for (std::size_t i = 0; i < count; ++i) {
    encoded.append(chunks[i], lens[i]);
  }

  const char tbl[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                        'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                        'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
                        'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                        's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2',
                        '3', '4', '5', '6', '7', '8', '9', '+', '/'};
  signed char rev[256];
  for (int i = 0; i < 256; ++i) {
    rev[i] = static_cast<signed char>(-1);
  }
  for (int i = 0; i < 64; ++i) {
    rev[static_cast<unsigned char>(tbl[i])] = static_cast<signed char>(i);
  }

  std::size_t len = encoded.size();
  // Ignore trailing '=' padding; valid base64 lengths after stripping are
  // 0, 2, 3 mod 4 (only the padding char sequence makes 1 mod 4 possible).
  while (len > 0 && encoded[len - 1] == '=') {
    --len;
  }
  std::size_t out_len = (len / 4) * 3;
  if (len % 4 == 2) {
    out_len += 1;
  } else if (len % 4 == 3) {
    out_len += 2;
  }
  std::string out;
  out.reserve(out_len);
  std::size_t i = 0;
  while (i + 4 <= len) {
    const int a = rev[static_cast<unsigned char>(encoded[i])];
    const int b = rev[static_cast<unsigned char>(encoded[i + 1])];
    const int c = rev[static_cast<unsigned char>(encoded[i + 2])];
    const int d = rev[static_cast<unsigned char>(encoded[i + 3])];
    out.push_back(static_cast<char>((a << 2) | (b >> 4)));
    if (c != -1) {
      out.push_back(static_cast<char>(((b & 0x0f) << 4) | (c >> 2)));
    }
    if (d != -1) {
      out.push_back(static_cast<char>(((c & 0x03) << 6) | d));
    }
    i += 4;
  }
  // Trailing partial group left after padding was stripped:
  // 2 chars -> 1 byte, 3 chars -> 2 bytes.
  if (len - i == 2) {
    const int a = rev[static_cast<unsigned char>(encoded[i])];
    const int b = rev[static_cast<unsigned char>(encoded[i + 1])];
    out.push_back(static_cast<char>((a << 2) | (b >> 4)));
  } else if (len - i == 3) {
    const int a = rev[static_cast<unsigned char>(encoded[i])];
    const int b = rev[static_cast<unsigned char>(encoded[i + 1])];
    const int c = rev[static_cast<unsigned char>(encoded[i + 2])];
    out.push_back(static_cast<char>((a << 2) | (b >> 4)));
    out.push_back(static_cast<char>(((b & 0x0f) << 4) | (c >> 2)));
  }
  return out;
}
#endif // CMRC_CMRC_HPP_BASE64

class directory;
class file_data;

#if CMRC_HAS_STRING_VIEW
using path_param = std::string_view;
#else
using path_param = const std::string &;
#endif

class file_or_directory {
  union _data_t {
    class file_data *file_data;
    class directory *directory;
  } _data;
  bool _is_file = true;

public:
  explicit file_or_directory(file_data &f) { _data.file_data = &f; }
  explicit file_or_directory(directory &d) {
    _data.directory = &d;
    _is_file = false;
  }
  bool is_file() const noexcept { return _is_file; }
  bool is_directory() const noexcept { return !is_file(); }
  const directory &as_directory() const noexcept {
    assert(!is_file());
    return *_data.directory;
  }
  const file_data &as_file() const noexcept {
    assert(is_file());
    return *_data.file_data;
  }
};

class file_data {
public:
  const char *begin_ptr;
  const char *end_ptr;
  file_data(const file_data &) = delete;
  file_data(const char *b, const char *e) : begin_ptr(b), end_ptr(e) {}
};

struct created_subdirectory {
  class directory &directory;
  class file_or_directory &index_entry;
};

class directory {
  std::list<file_data> _files;
  std::list<directory> _dirs;
#if CMRC_CPLUSPLUS >= 201402L
  std::map<std::string, file_or_directory, std::less<>> _index;
#else
  std::map<std::string, file_or_directory> _index;
#endif

#if CMRC_CPLUSPLUS >= 201402L
  using base_iterator =
      std::map<std::string, file_or_directory, std::less<>>::const_iterator;
#else
  using base_iterator =
      std::map<std::string, file_or_directory>::const_iterator;
#endif

public:
  directory() = default;
  directory(const directory &) = delete;

  created_subdirectory add_subdir(std::string name) & {
    assert(_index.find(name) == _index.end());
    _dirs.emplace_back();
    auto &back = _dirs.back();
    auto &fod = _index.emplace(name, file_or_directory{back}).first->second;
    return created_subdirectory{back, fod};
  }

  file_or_directory *add_file(std::string name, const char *begin,
                              const char *end) & {
#if CMRC_CPLUSPLUS >= 201402L
    std::map<std::string, file_or_directory, std::less<>>::iterator existing =
        _index.find(name);
#else
    std::map<std::string, file_or_directory>::iterator existing =
        _index.find(name);
#endif
    assert(existing == _index.end());
    if (existing != _index.end()) {
      return &existing->second;
    }
    _files.emplace_back(begin, end);
    return &_index.emplace(name, file_or_directory{_files.back()})
                .first->second;
  }

  class iterator {
    base_iterator _base_iter;
    base_iterator _end_iter;
    mutable std::shared_ptr<directory_entry> _cached_entry;

  public:
    using value_type = directory_entry;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *;
    using reference = const value_type &;
    using iterator_category = std::input_iterator_tag;

    iterator() = default;
    explicit iterator(base_iterator iter, base_iterator end)
        : _base_iter(iter), _end_iter(end) {}

    iterator begin() const noexcept { return *this; }

    iterator end() const noexcept { return iterator(_end_iter, _end_iter); }

    inline value_type operator*() const noexcept;

    const value_type *operator->() const noexcept {
      return _cached_entry.get();
    }

    bool operator==(const iterator &rhs) const noexcept {
      return _base_iter == rhs._base_iter;
    }

    bool operator!=(const iterator &rhs) const noexcept {
      return !(*this == rhs);
    }

    iterator &operator++() noexcept {
      ++_base_iter;
      return *this;
    }

    iterator operator++(int) noexcept {
      auto cp = *this;
      ++_base_iter;
      return cp;
    }
  };

  using const_iterator = iterator;

  iterator begin() const noexcept {
    return iterator(_index.begin(), _index.end());
  }

  iterator end() const noexcept { return iterator(_index.end(), _index.end()); }
};

inline std::string normalize_path(detail::path_param path) {
  // Work on a copy: a string_view parameter cannot be modified in place, and
  // the const-reference fallback must not mutate the caller's string either.
  std::string p(path);
  // Translate backslashes to forward slashes.
  for (std::string::size_type idx = p.find('\\'); idx != std::string::npos;
       idx = p.find('\\', idx + 1)) {
    p[idx] = '/';
  }
  while (p.find("/") == 0) {
    p.erase(p.begin());
  }
  while (!p.empty() && (p.rfind("/") == p.size() - 1)) {
    p.pop_back();
  }
  auto off = p.npos;
  while ((off = p.find("//")) != p.npos) {
    p.erase(p.begin() + static_cast<std::string::difference_type>(off));
  }
  // Collapse "." and ".." path components.
  std::vector<std::string> segments;
  std::string::size_type pos = 0;
  while (pos <= p.size()) {
    std::string::size_type sep = p.find('/', pos);
    std::string segment =
        p.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
    if (segment.empty() || segment == ".") {
      // Skip empty (duplicate slash) and "." components.
    } else if (segment == "..") {
      if (!segments.empty()) {
        segments.pop_back();
      }
    } else {
      segments.push_back(segment);
    }
    if (sep == std::string::npos) {
      break;
    }
    pos = sep + 1;
  }
  std::string result;
  bool first_segment = true;
  for (const std::string &segment : segments) {
    if (!first_segment) {
      result.push_back('/');
    }
    result += segment;
    first_segment = false;
  }
  return result;
}

#if CMRC_CPLUSPLUS >= 201402L
using index_type =
    std::map<std::string, const cmrc::detail::file_or_directory *, std::less<>>;
#else
using index_type =
    std::map<std::string, const cmrc::detail::file_or_directory *>;
#endif

} // namespace detail

class directory_entry {
  std::string _fname;
  const detail::file_or_directory *_item;

public:
  directory_entry() = delete;
  explicit directory_entry(std::string filename,
                           const detail::file_or_directory &item)
      : _fname(filename), _item(&item) {}

  const std::string &filename() const & { return _fname; }
  std::string filename() const && { return std::move(_fname); }

  bool is_file() const { return _item->is_file(); }

  bool is_directory() const { return _item->is_directory(); }
};

directory_entry detail::directory::iterator::operator*() const noexcept {
  assert(begin() != end());
  _cached_entry.reset(
      new directory_entry(_base_iter->first, _base_iter->second));
  return *_cached_entry;
}

using directory_iterator = detail::directory::iterator;

class embedded_filesystem {
  // Never-null:
  const cmrc::detail::index_type *_index;
  const detail::file_or_directory *_get(detail::path_param path) const {
    const std::string normalized = detail::normalize_path(path);
    auto found = _index->find(normalized);
    if (found == _index->end()) {
      return nullptr;
    } else {
      return found->second;
    }
  }

public:
  explicit embedded_filesystem(const detail::index_type &index)
      : _index(&index) {}

  file open(detail::path_param path) const {
    auto entry_ptr = _get(path);
    if (!entry_ptr) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error no such file or directory: %s\n",
              std::string(path).c_str());
      abort();
#else
      throw std::system_error(
          make_error_code(std::errc::no_such_file_or_directory),
          std::string(path));
#endif
    }
    if (!entry_ptr->is_file()) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error is a directory: %s\n", std::string(path).c_str());
      abort();
#else
      throw std::system_error(make_error_code(std::errc::is_a_directory),
                              std::string(path));
#endif
    }
    auto &dat = entry_ptr->as_file();
    return file{dat.begin_ptr, dat.end_ptr};
  }

  bool is_file(detail::path_param path) const noexcept {
    auto entry_ptr = _get(path);
    return entry_ptr && entry_ptr->is_file();
  }

  bool is_directory(detail::path_param path) const noexcept {
    auto entry_ptr = _get(path);
    return entry_ptr && entry_ptr->is_directory();
  }

  bool exists(detail::path_param path) const noexcept { return !!_get(path); }

  directory_iterator iterate_directory(detail::path_param path) const {
    auto entry_ptr = _get(path);
    if (!entry_ptr) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error no such file or directory: %s\n",
              std::string(path).c_str());
      abort();
#else
      throw std::system_error(
          make_error_code(std::errc::no_such_file_or_directory),
          std::string(path));
#endif
    }
    if (!entry_ptr->is_directory()) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error not a directory: %s\n", std::string(path).c_str());
      abort();
#else
      throw std::system_error(make_error_code(std::errc::not_a_directory),
                              std::string(path));
#endif
    }
    return entry_ptr->as_directory().begin();
  }
};

} // namespace cmrc

#endif // CMRC_CMRC_HPP_INCLUDED
