// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   any.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_ANY_H
#define UHDM_ANY_H

#include <uhdm/RTTI.h>
#include <uhdm/SymbolFactory.h>
#include <uhdm/containers.h>
#include <uhdm/uhdm_types.h>
#include <uhdm/uhdm_vpi_user.h>

#include <algorithm>
#include <map>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace uhdm {
class Any;
class Comment;
class Serializer;
class UhdmComparer;

static inline constexpr std::string_view kEmpty("");

class ClientData {
 public:
  virtual ~ClientData() = default;
};

class Any : public RTTI {
  UHDM_IMPLEMENT_RTTI(Any, RTTI)
  friend Serializer;

 public:
  static constexpr UhdmType kUhdmType = UhdmType::Any;

  explicit Any(Serializer *serializer) : m_serializer(serializer) {}
  Any(Serializer *serializer, const Any &rhs);
  virtual ~Any() = default;
  Any(const Any &rhs) = delete;
  Any &operator=(const Any &rhs) = delete;

  Serializer *getSerializer() const { return m_serializer; }

  uint32_t getUhdmId() const { return m_uhdmId; }
  bool setUhdmId(uint32_t data) {
    m_uhdmId = data;
    return true;
  }

  Any *getParent() { return m_parent; }
  const Any *getParent() const { return m_parent; }
  template <typename T>
  T *getParent() {
    return (m_parent == nullptr) ? nullptr : m_parent->template Cast<T>();
  }
  template <typename T>
  const T *getParent() const {
    return (m_parent == nullptr) ? nullptr : m_parent->template Cast<T>();
  }
  template <typename T, typename = typename std::enable_if_t<!std::is_base_of_v<Any, std::remove_pointer_t<T>> &&
                                                             !std::is_same<T, std::nullptr_t>::value>>
  bool setParent(T, bool = false) = delete;
  bool setParent(Any *data, bool force = false);

  std::string_view getFile() const;
  bool setFile(std::string_view data);

  std::string_view getPpFile() const;
  bool setPpFile(std::string_view data);

  uint32_t getStartLine() const { return m_startLine; }
  bool setStartLine(uint32_t data) {
    m_startLine = data;
    return true;
  }

  uint16_t getStartColumn() const { return m_startColumn; }
  bool setStartColumn(uint16_t data) {
    m_startColumn = data;
    return true;
  }

  bool getStartLocation(uint32_t &line, uint16_t &column) const {
    line = m_startLine;
    column = m_startColumn;
    return (m_startLine != 0) && (m_startColumn != 0);
  }
  bool setStartLocation(uint32_t line, uint16_t column) {
    m_startLine = line;
    m_startColumn = column;
    return true;
  }

  uint32_t getEndLine() const { return m_endLine; }
  bool setEndLine(uint32_t data) {
    m_endLine = data;
    return true;
  }

  uint16_t getEndColumn() const { return m_endColumn; }
  bool setEndColumn(uint16_t data) {
    m_endColumn = data;
    return true;
  }

  bool getEndLocation(uint32_t &line, uint16_t &column) const {
    line = m_endLine;
    column = m_endColumn;
    return (m_endLine != 0) && (m_endColumn != 0);
  }
  bool setEndLocation(uint32_t line, uint16_t column) {
    m_endLine = line;
    m_endColumn = column;
    return true;
  }

  uint32_t getPpStartLine() const { return m_ppStartLine; }
  bool setPpStartLine(uint32_t data) {
    m_ppStartLine = data;
    return true;
  }

  uint16_t getPpStartColumn() const { return m_ppStartColumn; }
  bool setPpStartColumn(uint16_t data) {
    m_ppStartColumn = data;
    return true;
  }

  bool getPpStartLocation(uint32_t &line, uint16_t &column) const {
    line = m_ppStartLine;
    column = m_ppStartColumn;
    return (m_ppStartLine != 0) && (m_ppStartColumn != 0);
  }
  bool setPpStartLocation(uint32_t line, uint16_t column) {
    m_ppStartLine = line;
    m_ppStartColumn = column;
    return true;
  }

  uint32_t getPpEndLine() const { return m_ppEndLine; }
  bool setPpEndLine(uint32_t data) {
    m_ppEndLine = data;
    return true;
  }

  uint16_t getPpEndColumn() const { return m_ppEndColumn; }
  bool setPpEndColumn(uint16_t data) {
    m_ppEndColumn = data;
    return true;
  }

  bool getPpEndLocation(uint32_t &line, uint16_t &column) const {
    line = m_ppEndLine;
    column = m_ppEndColumn;
    return (m_ppEndLine != 0) && (m_ppEndColumn != 0);
  }
  bool setPpEndLocation(uint32_t line, uint16_t column) {
    m_ppEndLine = line;
    m_ppEndColumn = column;
    return true;
  }

  virtual std::string_view getName() const { return kEmpty; }
  virtual std::string_view getDefName() const { return kEmpty; }

  virtual uint32_t getVpiType() const = 0;
  virtual UhdmType getUhdmType() const = 0;

  ClientData *getClientData() { return m_clientData; }
  const ClientData *getClientData() const { return m_clientData; }
  void setClientData(ClientData *data) { m_clientData = data; }

  virtual const Any *getByVpiName(std::string_view name) const { return nullptr; }

  using get_by_vpi_type_return_t = std::tuple<UhdmType, const Any *, const std::vector<const Any *> *>;
  virtual get_by_vpi_type_return_t getByVpiType(int32_t type) const;

  using vpi_property_value_t = std::variant<int64_t, const char *>;
  virtual vpi_property_value_t getVpiPropertyValue(int32_t property) const;

  virtual int32_t compare(const Any *other, UhdmComparer *comparer) const;

  virtual bool isFiltered(const Any *data, int32_t relation) const { return true; }

  template <typename C,
            typename = typename std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>>>
  bool isFiltered(const C *collection, int32_t relation, AnySet &violations) const {
    if (collection == nullptr) return true;
    bool filtered = true;
    for (const typename C::value_type &any : *collection) {
      if (!isFiltered(any, relation)) {
        violations.emplace(any);
        filtered = false;
      }
    }
    return filtered;
  }

 protected:
  std::string computeFullName() const;

  virtual void swap(const Any *what, Any *with);
  virtual void swap(const AnyMap<Any *> &replacements);

  template <typename T>
  static void swapT(std::vector<T *> &collection, const Any *what, Any *with) {
    auto it = std::find(collection.begin(), collection.end(), what);
    if (it != collection.end()) {
      if (with == nullptr) {
        collection.erase(it);
      } else if (T *const withT = with->template Cast<T>()) {
        *it = withT;
      } else {
        collection.erase(it);
      }
    }
  }

  template <typename T>
  static void swapT(std::vector<T *> &collection, const AnyMap<Any *> &replacements) {
    if (!std::any_of(collection.cbegin(), collection.cend(),
                     [&replacements](const Any *const any) { return replacements.find(any) != replacements.cend(); })) {
      return;
    }

    AnySet unique;
    const std::vector<T *> ordered = std::move(collection);
    for (auto whatT : ordered) {
      if (auto it = replacements.find(whatT); it != replacements.cend()) {
        if (it->second != nullptr) {
          if (T *const withT = it->second->template Cast<T>()) {
            if (unique.emplace(it->second).second) {
              collection.emplace_back(withT);
            }
          } else {
            if (unique.emplace(whatT).second) collection.emplace_back(whatT);
          }
        }
      } else {
        if (unique.emplace(whatT).second) collection.emplace_back(whatT);
      }
    }
  }

  virtual void onChildAdded(Any *child) {}
  virtual void onChildRemoved(Any *child) {}

 protected:
  Serializer *const m_serializer = nullptr;
  ClientData *m_clientData = nullptr;
  CommentCollection *m_comments = nullptr;

  uint32_t m_uhdmId = 0;
  Any *m_parent = nullptr;

  SymbolId m_fileId = BadSymbolId;
  SymbolId m_ppFileId = BadSymbolId;

  uint32_t m_startLine = 0;
  uint32_t m_endLine = 0;
  uint16_t m_startColumn = 0;
  uint16_t m_endColumn = 0;

  uint32_t m_ppStartLine = 0;
  uint32_t m_ppEndLine = 0;
  uint16_t m_ppStartColumn = 0;
  uint16_t m_ppEndColumn = 0;
};

inline bool AnyLessComparer::operator()(const uhdm::Any *lhs, const uhdm::Any *rhs) const {
  return lhs->getUhdmId() < rhs->getUhdmId();
}

}  // namespace uhdm

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(any_cast, uhdm::Any)

#endif  // UHDM_ANY_H
