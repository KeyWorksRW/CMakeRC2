// CR: [09-06-2026]

#pragma once

#ifndef CMRC_CMRC_HPP_INCLUDED
#define CMRC_CMRC_HPP_INCLUDED

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

#if !(defined(__EXCEPTIONS) || defined(__cpp_exceptions) ||                    \
      defined(_CPPUNWIND) || defined(CMRC_NO_EXCEPTIONS))
#define CMRC_NO_EXCEPTIONS 1
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

  file() = default;
  file(iterator beg, iterator end) noexcept : _begin(beg), _end(end) {}
};

class directory_entry;

namespace detail {

class directory;
class file_data;

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
  std::map<std::string, file_or_directory> _index;

  using base_iterator =
      std::map<std::string, file_or_directory>::const_iterator;

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
    std::map<std::string, file_or_directory>::iterator existing =
        _index.find(name);
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

inline std::string normalize_path(std::string path) {
  // Translate backslashes to forward slashes.
  for (std::string::size_type idx = path.find('\\'); idx != std::string::npos;
       idx = path.find('\\', idx + 1)) {
    path[idx] = '/';
  }
  while (path.find("/") == 0) {
    path.erase(path.begin());
  }
  while (!path.empty() && (path.rfind("/") == path.size() - 1)) {
    path.pop_back();
  }
  auto off = path.npos;
  while ((off = path.find("//")) != path.npos) {
    path.erase(path.begin() + static_cast<std::string::difference_type>(off));
  }
  // Collapse "." and ".." path components.
  std::vector<std::string> segments;
  std::string::size_type pos = 0;
  while (pos <= path.size()) {
    std::string::size_type sep = path.find('/', pos);
    std::string segment = path.substr(
        pos, sep == std::string::npos ? std::string::npos : sep - pos);
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

using index_type =
    std::map<std::string, const cmrc::detail::file_or_directory *>;

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
  const detail::file_or_directory *_get(std::string path) const {
    path = detail::normalize_path(path);
    auto found = _index->find(path);
    if (found == _index->end()) {
      return nullptr;
    } else {
      return found->second;
    }
  }

public:
  explicit embedded_filesystem(const detail::index_type &index)
      : _index(&index) {}

  file open(const std::string &path) const {
    auto entry_ptr = _get(path);
    if (!entry_ptr) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error no such file or directory: %s\n", path.c_str());
      abort();
#else
      throw std::system_error(
          make_error_code(std::errc::no_such_file_or_directory), path);
#endif
    }
    if (!entry_ptr->is_file()) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error is a directory: %s\n", path.c_str());
      abort();
#else
      throw std::system_error(make_error_code(std::errc::is_a_directory), path);
#endif
    }
    auto &dat = entry_ptr->as_file();
    return file{dat.begin_ptr, dat.end_ptr};
  }

  bool is_file(const std::string &path) const noexcept {
    auto entry_ptr = _get(path);
    return entry_ptr && entry_ptr->is_file();
  }

  bool is_directory(const std::string &path) const noexcept {
    auto entry_ptr = _get(path);
    return entry_ptr && entry_ptr->is_directory();
  }

  bool exists(const std::string &path) const noexcept { return !!_get(path); }

  directory_iterator iterate_directory(const std::string &path) const {
    auto entry_ptr = _get(path);
    if (!entry_ptr) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error no such file or directory: %s\n", path.c_str());
      abort();
#else
      throw std::system_error(
          make_error_code(std::errc::no_such_file_or_directory), path);
#endif
    }
    if (!entry_ptr->is_directory()) {
#ifdef CMRC_NO_EXCEPTIONS
      fprintf(stderr, "Error not a directory: %s\n", path.c_str());
      abort();
#else
      throw std::system_error(make_error_code(std::errc::not_a_directory),
                              path);
#endif
    }
    return entry_ptr->as_directory().begin();
  }
};

} // namespace cmrc

#endif // CMRC_CMRC_HPP_INCLUDED
