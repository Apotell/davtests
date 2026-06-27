/*
 Copyright 2022 chipsalliance

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
 * File:   FileSystem.h
 * Author: hs
 *
 * Created on June 1, 2022, 3:00 AM
 */

#ifndef SURELOG_FILESYSTEM_H
#define SURELOG_FILESYSTEM_H
#pragma once

#include <Surelog/Common/PathId.h>
#include <Surelog/Common/SymbolId.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SURELOG {
class FileSystem;
class SymbolTable;

class IStoreProperties {
 public:
  virtual ~IStoreProperties() = default;
  IStoreProperties(const IStoreProperties &other) = delete;
  IStoreProperties &operator=(const IStoreProperties &rhs) = delete;

 protected:
  IStoreProperties() = default;
};

class IStore {
 public:
  [[nodiscard]] static std::string normalizeLogicalPath(std::string_view path);
  [[nodiscard]] static std::string normalizeRelativePath(std::string_view path);

 public:
  virtual ~IStore() = default;
  IStore(const IStore &other) = delete;
  IStore &operator=(const IStore &rhs) = delete;

  // Open/Close an input stream represented by the input path.
  [[nodiscard]] virtual std::istream &openInput(std::string_view path, std::ios_base::openmode mode) = 0;
  virtual bool close(std::istream &strm) = 0;

  // Open/Close an output stream represented by the input path.
  [[nodiscard]] virtual std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) = 0;
  virtual bool close(std::ostream &strm) = 0;

  // Rename directory/file represented by 'what' to 'to'
  [[nodiscard]] virtual bool rename(std::string_view what, std::string_view to) const { return false; }

  // Remove the file represented by 'filepath'. Note this cannot be used
  // for removing directories.
  [[nodiscard]] virtual bool remove(std::string_view filepath) const { return false; }

  // Make a new directory represented by the input 'dirpath'. The parent of the
  // input 'dir' should already exist.
  [[nodiscard]] virtual bool mkdir(std::string_view dirpath) const { return false; }

  // Remove the directory represented by the input 'dirpath'. The directory has
  // to be empty. For non-empty directories, use rmtree.
  [[nodiscard]] virtual bool rmdir(std::string_view dirpath) const { return false; }

  // Make a new directory and all its subsequent non-existent parents.
  [[nodiscard]] virtual bool mkdirs(std::string_view dirpath) const { return false; }

  // Remove the directoryy represented by input 'dirpath' and all
  // subdirectories & files under it.
  [[nodiscard]] virtual bool rmtree(std::string_view dirpath) const { return false; }

  // Returns true if the input 'path' represent a legal file or directory.
  [[nodiscard]] virtual bool exists(std::string_view path) const { return false; }

  // Returns true if the input 'path' represents a directory.
  [[nodiscard]] virtual bool isDirectory(std::string_view path) const { return false; }

  // Returns true if the input 'path' represents a file.
  [[nodiscard]] virtual bool isRegularFile(std::string_view path) const { return false; }

  // Returns the length of the file represented by the input 'filepath'.
  [[nodiscard]] virtual bool filesize(std::string_view filepath, std::streamsize *result) const { return false; }

  // Returns the 'last modified time' for the input 'filepath' expressed as
  // nanoseconds since the file clock epoch.
  [[nodiscard]] virtual bool modtime(std::string_view filepath, std::filesystem::file_time_type *result) const {
    return false;
  }

  // Returns the properties associated with the parent Store.
  [[nodiscard]] virtual IStoreProperties *getProperties() const { return nullptr; }

 protected:
  IStore() = default;

  static std::istringstream s_nullInputStream;
  static std::ostringstream s_nullOutputStream;

  friend class FileSystem;
};

template <typename FlushFn>
class BufferingOutputStream final : public std::ostream {
 public:
  explicit BufferingOutputStream(FlushFn flush) : std::ostream(nullptr), m_buffer(std::move(flush)) {
    rdbuf(&m_buffer);
  }

 private:
  class Buffer final : public std::stringbuf {
   public:
    explicit Buffer(FlushFn flush) : m_flush(std::move(flush)) {}
    ~Buffer() override { sync(); }

    int32_t sync() override {
      m_flush(str());
      return 0;
    }

   private:
    FlushFn m_flush;
  };

  Buffer m_buffer;
};

// Helper base that owns the streams it hands out by reference. Derive from
// this to get reference-stable stream management without re-implementing the
// bookkeeping in every backend.
class OwnedStreamStore : public IStore {
 public:
  ~OwnedStreamStore() override;

  bool close(std::istream &strm) override;
  bool close(std::ostream &strm) override;

 protected:
  std::istream &adopt(std::unique_ptr<std::istream> strm);
  std::ostream &adopt(std::unique_ptr<std::ostream> strm);

  template <class T>
  struct Comparer final {
    using is_transparent = std::true_type;
    struct Helper final {
      T *ptr = nullptr;
      Helper() : ptr(nullptr) {}
      Helper(Helper const &) = default;
      Helper(T *p) : ptr(p) {}  // NOLINT
      template <class U>
      Helper(std::shared_ptr<U> const &sp) : ptr(sp.get()) {}  // NOLINT
      template <class U, class... Ts>
      Helper(std::unique_ptr<U, Ts...> const &up) : ptr(up.get()) {}  // NOLINT
      bool operator<(const Helper &o) const { return std::less<T *>()(ptr, o.ptr); }
    };
    bool operator()(Helper const &&lhs, Helper const &&rhs) const { return lhs < rhs; }
  };

  using InputStreams = std::set<std::unique_ptr<std::istream>, Comparer<std::istream>>;
  using OutputStreams = std::set<std::unique_ptr<std::ostream>, Comparer<std::ostream>>;

  std::mutex m_inputStreamsMutex;
  std::mutex m_outputStreamsMutex;
  InputStreams m_inputStreams;
  OutputStreams m_outputStreams;
};

class InMemoryStore final : public OwnedStreamStore {
 public:
  InMemoryStore() = default;

  std::istream &openInput(std::string_view path, std::ios_base::openmode mode) override;
  std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) override;
  bool exists(std::string_view path) const override;

  void seedFile(std::string_view path, std::string_view contents);
  std::string readFile(std::string_view path) const;

 private:
  std::map<std::string, std::string, std::less<>> m_files;
  mutable std::mutex m_mutex;
};

class TarFileStore final : public OwnedStreamStore {
 public:
  explicit TarFileStore(std::string_view archivePath);

  std::istream &openInput(std::string_view path, std::ios_base::openmode mode) override;
  std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) override;
  bool exists(std::string_view path) const override;

 private:
  void ensureLoaded() const;

  std::string m_archivePath;
  mutable bool m_loaded = false;
  mutable std::map<std::string, std::string, std::less<>> m_files;
  mutable std::mutex m_mutex;
};

#ifdef SURELOG_WITH_ZLIB
class CompressedStore final : public OwnedStreamStore {
 public:
  explicit CompressedStore(IStore *backingStore);

  std::istream &openInput(std::string_view path, std::ios_base::openmode mode) override;
  std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) override;
  bool exists(std::string_view path) const override;

 private:
  IStore *const m_backingStore;
};
#endif  // SURELOG_WITH_ZLIB

#if 0
class EncryptedStore final : public OwnedStreamStore {
 public:
  EncryptedStore(IStore *backingStore, std::string_view key);

  std::istream& openInput(std::string_view path, std::ios_base::openmode mode) override;
  std::ostream& openOutput(std::string_view path, std::ios_base::openmode mode) override;
  bool exists(std::string_view path) const override;

 private:
  IStore *const m_backingStore;
  const std::string m_key;
};
#endif

class INetworkTransport {
 public:
  virtual ~INetworkTransport() = default;

  virtual std::string fetch(std::string_view uri) const = 0;
  virtual void store(std::string_view uri, std::string_view payload) = 0;
  virtual bool exists(std::string_view uri) const = 0;
};

class InMemoryNetworkTransport final : public INetworkTransport {
 public:
  std::string fetch(std::string_view uri) const override;
  void store(std::string_view uri, std::string_view payload) override;
  bool exists(std::string_view uri) const override;

  void seed(std::string_view uri, std::string_view payload);

 private:
  std::map<std::string, std::string, std::less<>> m_payloads;
  mutable std::mutex m_mutex;
};

class HttpNetworkTransport final : public INetworkTransport {
 public:
  std::string fetch(std::string_view uri) const override;
  void store(std::string_view uri, std::string_view payload) override;
  bool exists(std::string_view uri) const override;
};

class NetworkStore final : public OwnedStreamStore {
 public:
  NetworkStore(std::string_view baseUri, INetworkTransport *transport);

  std::istream &openInput(std::string_view path, std::ios_base::openmode mode) override;
  std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) override;
  bool exists(std::string_view path) const override;

 private:
  std::string makeUri(std::string_view path) const;

  const std::string m_baseUri;
  INetworkTransport *const m_transport;
};

/**
 * class LocalStore
 *
 * Native platform file system. Inherits FileSystem (the Surelog PathId-based
 * API) and OwnedStreamStore (stream ownership + IStore
 * backend contract) so that instances can be mounted directly into a VfsRuntime
 * without a wrapper, consistent with the other avfs backends.
 */
class LocalStore : public OwnedStreamStore {
 public:
  // Returns the executing binary's path by querying the OS
  [[nodiscard]] static std::filesystem::path getProgramPath();
  [[nodiscard]] static std::filesystem::path getProgramPath(std::string_view hint);

  // Normalizes the input path
  //   Standardizes the directory separator based on platform
  //   No trailing slash regardless of whether the path exists or not
  //   Shortens the path by removing any '.' and '..'
  [[nodiscard]] static std::filesystem::path normalize(const std::filesystem::path &p);
  [[nodiscard]] static bool is_subpath(const std::filesystem::path &parent, const std::filesystem::path &child);

 public:
  LocalStore() = default;

  [[nodiscard]] static bool readContent(std::string_view filepath, std::string &content);
  [[nodiscard]] static bool writeContent(std::string_view filepath, std::string_view content);

  // IStore string-path entry points — delegate to FileSystem which
  // dispatches through virtual openInput/openOutput (overridden below).
  [[nodiscard]] std::istream &openInput(std::string_view path, std::ios_base::openmode mode) override;
  [[nodiscard]] std::ostream &openOutput(std::string_view path, std::ios_base::openmode mode) override;

  // Rename directory/file represented by 'what' to 'to'
  [[nodiscard]] bool rename(std::string_view what, std::string_view to) const override;

  // Remove the file represented by 'filepath'. Note this cannot be used
  // for removing directories.
  [[nodiscard]] bool remove(std::string_view filepath) const override;

  // Make a new directory represented by the input 'dirpath'. The parent of the
  // input 'dir' should already exist.
  [[nodiscard]] bool mkdir(std::string_view dirpath) const override;

  // Remove the directory represented by the input 'dirpath'. The directory has
  // to be empty. For non-empty directories, use rmtree.
  [[nodiscard]] bool rmdir(std::string_view dirpath) const override;

  // Make a new directory and all its subsequent non-existent parents.
  [[nodiscard]] bool mkdirs(std::string_view dirpath) const override;

  // Remove the directoryy represented by input 'dir' and all
  // subdirectories & files under it.
  [[nodiscard]] bool rmtree(std::string_view dirpath) const override;

  // Returns true if the input 'path' represent a legal file or directory.
  [[nodiscard]] bool exists(std::string_view path) const override;

  // Returns true if the input 'path' represents a directory.
  [[nodiscard]] bool isDirectory(std::string_view path) const override;

  // Returns true if the input 'path' represents a file.
  [[nodiscard]] bool isRegularFile(std::string_view path) const override;

  // Returns the length of the file represented by the input 'filepath'.
  [[nodiscard]] bool filesize(std::string_view filepath, std::streamsize *result) const override;

  // Returns the 'last modified time' for the input 'filepath' expressed as
  // nanoseconds since the file clock epoch.
  [[nodiscard]] bool modtime(std::string_view filepath, std::filesystem::file_time_type *result) const override;
};

/**
 * class FileSystem
 *
 * Concrete interface between Surelog & file system access. All interactions
 * between the FileSystem and Surelog use PathId (in conjunction with
 * SymbolTable) for communication.
 *
 * FileSystem is the native, platform-specific implementation. Paths are
 * manipulated as plain strings throughout the public interface; std::filesystem
 * is an implementation detail confined to FileSystem.cpp.
 *
 * A sub-classed implementation can be used to implement support for UNC paths,
 * compressed tarballs of source and cache files, and a virtual file system
 * (i.e. no read/writes to disk). An example of such an implementation is in
 * LocalStore_test.cpp for reference.
 *
 * A few words on convention:
 *
 * Stream convention:
 *   Read/Write is used in context to text streams
 *   and Load/Save in context to binary streams.
 *
 * toPath vs. toPlatformAbsPath:
 *   toPath returns a printable representation of the PathId. This doesn't
 *     necessarily have to be a resolve-able path. What the string represent
 *     is up to the interpretation of the FileSystem implementation itself.
 *
 *   toPlatformAbsPath returns a string representation of PathId that needs to
 *     be resolve-able by the OS (it is used by external processes like CMake
 *     or a system call to a batch script).
 *
 */
class FileSystem {
 public:
  struct MountDescriptor final {
    std::string_view m_variableName;
    std::string_view m_mountPoint;
    MountDescriptor(std::string_view variableName, std::string_view mountPoint)
        : m_variableName(variableName), m_mountPoint(mountPoint) {}
  };

 public:
  FileSystem() = default;
  FileSystem(const FileSystem &other) = delete;
  FileSystem &operator=(const FileSystem &rhs) = delete;
  ~FileSystem();

  // Bind a variable to a filesystem at a specific mount point.
  [[nodiscard]] bool mount(std::string_view variableName, std::string_view mountPoint, IStore *fileSystem);
  [[nodiscard]] bool mount(std::string_view variableName, std::string_view mountPoint, IStore *fileSystem,
                           std::string &result);
  [[nodiscard]] bool isMounted(std::string_view variableName) const;
  [[nodiscard]] std::vector<MountDescriptor> getMountDescriptors() const;

  // Convert a native filesystem path to PathId
  [[nodiscard]] static PathId toPathId(std::string_view path, SymbolTable *symbolTable);

  // Returns the string/printable representation of the input id
  [[nodiscard]] static std::string_view toPath(PathId id);

  // Returns child, sibling, parent, leaf and type of filesystem path
  [[nodiscard]] static PathId getChild(PathId id, std::string_view name, SymbolTable *symbolTable);
  [[nodiscard]] static PathId getSibling(PathId id, std::string_view name, SymbolTable *symbolTable);
  [[nodiscard]] static PathId getParent(PathId id, SymbolTable *symbolTable);
  [[nodiscard]] static std::pair<SymbolId, std::string_view> getStem(PathId id, SymbolTable *symbolTable);
  [[nodiscard]] static std::pair<SymbolId, std::string_view> getLeaf(PathId id, SymbolTable *symbolTable);
  [[nodiscard]] static std::pair<SymbolId, std::string_view> getType(PathId id, SymbolTable *symbolTable);

  // Returns a copy of the input id registered with the input SymbolTable.
  [[nodiscard]] static PathId copy(PathId id, SymbolTable *toSymbolTable);

  // Open/Close an input stream represented by the input PathId.
  // openForRead defaults the ios_base::openmode to ios_base::text
  // openForLoad defaults the ios_base::openmode to ios_base::binary
  [[nodiscard]] std::istream &openInput(PathId fileId, std::ios_base::openmode mode) const;
  [[nodiscard]] std::istream &openForRead(PathId fileId) const;
  [[nodiscard]] std::istream &openForLoad(PathId fileId) const;
  bool close(std::istream &strm) const;

  // Open/Close an output stream represented by the input PathId.
  // openForWrite defaults the ios_base::openmode to ios_base::text
  // openForSave defaults the ios_base::openmode to ios_base::binary
  [[nodiscard]] std::ostream &openOutput(PathId fileId, std::ios_base::openmode mode) const;
  [[nodiscard]] std::ostream &openForWrite(PathId fileId) const;
  [[nodiscard]] std::ostream &openForSave(PathId fileId) const;
  bool close(std::ostream &strm) const;

  // Read/Write content i.e. blob of text as string from file represented by
  // input PathId. onlyIfModified defaults to true if using the overloaded
  // variation.
  [[nodiscard]] bool readContent(PathId fileId, std::string &content) const;
  [[nodiscard]] bool writeContent(PathId fileId, std::string_view content, bool onlyIfModified) const;
  [[nodiscard]] bool writeContent(PathId fileId, std::string_view content) const;

  // Read/Write lines of text from file represented by input PathId.
  // onlyIfModified defaults to true if using the overloaded variation.
  [[nodiscard]] bool readLines(PathId fileId, std::vector<std::string> &lines) const;
  [[nodiscard]] bool writeLines(PathId fileId, const std::vector<std::string> &lines, bool onlyIfModified) const;
  [[nodiscard]] bool writeLines(PathId fileId, const std::vector<std::string> &lines) const;

  // Read specific line of text from file represented by input PathId.
  // Note that the first line in the file is at index 1 (i.e. 1 based indexing)
  [[nodiscard]] bool readLine(PathId fileId, int32_t index, std::string &content) const;

  // Load/Save content i.e. blob from file represented by input PathId.
  // useTemp defaults to false if using the overloaded variations.
  [[nodiscard]] bool loadContent(PathId fileId, std::vector<char> &data) const;
  [[nodiscard]] bool saveContent(PathId fileId, const char *content, std::streamsize length, bool useTemp) const;
  [[nodiscard]] bool saveContent(PathId fileId, const char *content, std::streamsize length) const;
  [[nodiscard]] bool saveContent(PathId fileId, const std::vector<char> &data, bool useTemp) const;
  [[nodiscard]] bool saveContent(PathId fileId, const std::vector<char> &data) const;

  // Rename directory/file represented by 'whatId' to 'toId'
  [[nodiscard]] bool rename(PathId whatId, PathId toId) const;

  // Remove the file represented by 'fileId'. Note this cannot be used
  // for removing directories.
  [[nodiscard]] bool remove(PathId fileId) const;

  // Make a new directory represented by the input 'dirId'. The parent of the
  // input 'dirId' should already exist.
  [[nodiscard]] bool mkdir(PathId dirId) const;

  // Remove the directory represented by the input 'dirId'. The directory has
  // to be empty. For non-empty directories, use rmtree.
  [[nodiscard]] bool rmdir(PathId dirId) const;

  // Make a new directory and all its subsequent non-existent parents.
  [[nodiscard]] bool mkdirs(PathId dirId) const;

  // Remove the direcoty represented by input 'dirId' and all
  // subdirectories & files under it.
  [[nodiscard]] bool rmtree(PathId dirId) const;

  // Returns true if the input PathId is a pre-existing native filesystem path.
  [[nodiscard]] bool exists(PathId id) const;

  // Returns true if descendant exists under the directory
  // represented by the input PathId.
  [[nodiscard]] bool exists(PathId dirId, std::string_view descendant) const;

  // Returns true if the input id represents a directory.
  [[nodiscard]] bool isDirectory(PathId id) const;

  // Returns true if the input id represents a file.
  [[nodiscard]] bool isRegularFile(PathId id) const;

  // Returns the length of the file represented by the input id.
  [[nodiscard]] bool filesize(PathId fileId, std::streamsize *result) const;

  // Returns the 'last modified time' for the input fileId expressed as
  // nanoseconds since the file clock epoch, or 'defaultOnFail' if the file was
  // not found or the operation failed.
  [[nodiscard]] bool modtime(PathId fileId, std::filesystem::file_time_type *result) const;

  // Find the first directory in input 'directories' that contain
  // directory/file named 'name'.
  // If found, return the PathId representing that directory/file
  // and otherwise BadPathId
  [[nodiscard]] PathId locate(std::string_view name, const PathIdVector &directories, SymbolTable *symbolTable) const;

  // Returns a list of all files under the input 'dirId'.
  PathIdVector &collect(PathId dirId, SymbolTable *symbolTable, PathIdVector &container) const;
  // Returns a list of all files under the input 'dirId',
  // filtered by 'extension'.
  PathIdVector &collect(PathId dirId, std::string_view extension, SymbolTable *symbolTable,
                        PathIdVector &container) const;
  // Returns all files under the input 'dirId' that matches the input 'pattern'
  PathIdVector &matching(PathId dirId, std::string_view pattern, SymbolTable *symbolTable,
                         PathIdVector &container) const;
  PathIdVector &matching(PathId dirId, const std::regex &pattern, SymbolTable *symbolTable,
                         PathIdVector &container) const;

  // Print the list of active VFS mounts to the given stream (for diagnostics).
  void print(std::ostream &out) const;

 private:
  struct Mount final {
    std::string m_variableName;
    std::string m_mountPoint;
    IStore *m_store = nullptr;

    Mount(std::string_view variableName, std::string_view mountPoint, IStore *store)
        : m_variableName(variableName), m_mountPoint(mountPoint), m_store(store) {}
  };
  using mounts_t = std::vector<Mount>;

  [[nodiscard]] std::istream &openInput(std::string_view filepath, std::ios_base::openmode mode) const;
  [[nodiscard]] std::ostream &openOutput(std::string_view filepath, std::ios_base::openmode mode) const;
  [[nodiscard]] bool exists(std::string_view filepath) const;

  [[nodiscard]] static bool isMountMatch(std::string_view mountVariable, std::string_view logicalPath);
  [[nodiscard]] bool resolveMount(std::string_view logicalPath, const Mount *&mount, std::string &absolutePath) const;

  mounts_t m_mounts;
};
}  // namespace SURELOG

#endif  // SURELOG_FILESYSTEM_H
