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
 * File:   Serializer.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_SERIALIZER_H
#define UHDM_SERIALIZER_H

#include <uhdm/EventListener.h>
#include <uhdm/SymbolFactory.h>
#include <uhdm/any.h>
#include <uhdm/containers.h>
#include <uhdm/vpi_uhdm.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#define UHDM_MAX_BIT_WIDTH (1024 * 1024)

namespace uhdm {
class ScopedScope;
class Serializer;

class Factory final {
  friend class Serializer;

 public:
  using objects_t = std::vector<Any *>;
  using collections_t = std::vector<objects_t *>;

 public:
  template <typename T, typename... Args>
  T *make(Args &&...args) {
    T *const any = new T(std::forward<Args>(args)...);
    m_objects.emplace_back(any);
    return any;
  }

  template <typename T>
  std::vector<T *> *makeCollection() {
    std::vector<T *> *const collection = new std::vector<T *>;
    m_collections.emplace_back((objects_t *)collection);
    return collection;
  }

  bool erase(const Any *any) {
    objects_t::iterator it = std::find(m_objects.begin(), m_objects.end(), any);
    if (it != m_objects.end()) {
      delete any;
      m_objects.erase(it);
      return true;
    }
    return false;
  }

  template <typename T>
  bool erase(const std::vector<T *> *collection) {
    collections_t::iterator it =
        std::find(m_collections.begin(), m_collections.end(), static_cast<const std::vector<Any *> *>(collection));
    if (it != m_collections.end()) {
      delete collection;
      m_collections.erase(it);
      return true;
    }
    return false;
  }

  uint32_t eraseIfNotIn(const AnySet &container) {
    objects_t keepers;
    for (objects_t::reference any : m_objects) {
      if (container.find(any) == container.cend()) {
        delete any;
      } else {
        keepers.emplace_back(any);
      }
    }
    const uint32_t count = m_objects.size() - keepers.size();
    keepers.swap(m_objects);
    return count;
  }

  void mapToIndex(AnyMap<uint32_t> &table, uint32_t index = 1) const {
    for (objects_t::const_reference any : m_objects) {
      table.emplace(any, index++);
    }
  }

  void purge() {
    for (objects_t::reference any : m_objects) {
      delete any;
    }
    for (collections_t::reference collection : m_collections) {
      delete collection;
    }

    m_objects.clear();
    m_collections.clear();
  }

  const objects_t &getObjects() { return m_objects; }
  const objects_t &getObjects() const { return m_objects; }

  const collections_t &getCollections() { return m_collections; }
  const collections_t &getCollections() const { return m_collections; }

 private:
  objects_t m_objects;
  collections_t m_collections;
};

class Serializer final {
 public:
  using IdMap = AnyMap<uint32_t>;
  using factories_t = std::map<UhdmType, Factory *>;

  static constexpr uint32_t kBadIndex = static_cast<uint32_t>(-1);
  static const uint32_t kVersion;

  Serializer();
  ~Serializer();

  bool save(std::ostream &strm);
  bool save(const std::string &filepath);
  bool save(const std::filesystem::path &filepath);
  void purge();

  void setGCEnabled(bool enabled) { m_enableGC = enabled; }
  void collectGarbage();

  void setEventListener(EventListener *listener) { m_eventListener = listener; }
  EventListener* getEventListener() const { return m_eventListener; }

  IdMap getAllObjects() const;
  const factories_t &getFactories() const { return m_factories; }

  template <typename T>
  const Factory *getFactory() const {
    return m_factories.at(T::kUhdmType);
  }

  template <typename T>
  const Factory::objects_t &getObjects() const {
    return getFactory<T>()->getObjects();
  }

  std::vector<vpiHandle> restore(std::istream &strm);
  std::vector<vpiHandle> restore(const std::string &filepath);
  std::vector<vpiHandle> restore(const std::filesystem::path &filepath);
  std::map<std::string_view, uint32_t, std::less<>> getObjectStats() const;
  std::map<std::string_view, std::set<uint32_t>, std::less<>> getObjectIdSets() const;
  void printStats(std::ostream &strm, std::string_view infoText) const;

  void swap(const Any *what, Any *with);
  void swap(const AnyMap<Any *> &replacements);

 private:
  template <typename T>
  T *make(Factory *const factory) {
    T *const obj = factory->template make<T>(this);
    obj->setUhdmId(++m_objId);
    return obj;
  }

  template <typename T>
  void make(Factory *const factory, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) make<T>(factory);
  }

  template <typename T>
  std::vector<T *> *makeCollection(Factory *const factory) {
    return factory->template makeCollection<T>();
  }

 public:
  template <typename T>
  T *make() {
    return make<T>(m_factories[T::kUhdmType]);
  }

  template <typename T>
  void make(uint32_t count) {
    make<T>(m_factories[T::kUhdmType], count);
  }

  template <typename T>
  std::vector<T *> *makeCollection() {
    return makeCollection<T>(m_factories[T::kUhdmType]);
  }

  template <typename T>
  T *clone(const T *source) {
    Factory *const factory = m_factories[T::kUhdmType];
    T *const target = factory->template make<T>(this, *source);
    target->setUhdmId(++m_objId);
    return target;
  }

  uint32_t getLastObjectId() const { return m_objId; }
  SymbolId makeSymbol(std::string_view symbol);
  std::string_view getSymbol(SymbolId id) const;
  SymbolId getSymbolId(std::string_view symbol) const;

  SymbolCollection *makeSymbolCollection();

  vpiHandle makeUhdmHandle(UhdmType type, const void *object, uint32_t index = 0);
  bool erase(vpiHandle handle);

  bool erase(const Any *p);
  template <typename T>
  bool erase(const std::vector<T *> *collection) {
    return m_factories[T::kUhdmType]->template erase<T>(collection);
  }

  struct SaveAdapter;
  friend struct SaveAdapter;

  struct RestoreAdapter;
  friend struct RestoreAdapter;

 private:
  template <typename T>
  T *getObject(uint32_t type, uint32_t index) const;

  uint64_t m_version = 0;
  uint32_t m_objId = 0;
  bool m_enableGC = true;
  EventListener *m_eventListener = StreamEventListener::getDefaultInstance();

  SymbolFactory m_symbolFactory;
  UhdmHandleFactory m_uhdmHandleFactory;

  factories_t m_factories;
};
}  // namespace uhdm

#endif  // UHDM_SERIALIZER_H
