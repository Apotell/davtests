/*
 Copyright 2020 Apotell

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

// Tests for tests/LibraryIntercon (tags: LibraryIntercon), compiled with
// "-map lib.map -cfg cfgReal ... -fileunit". This is the IEEE 1800-2023
// 6.6.8 "Generic interconnect" example (slightly modified).
//
//   lib.map:
//     library realLib *.svr;
//     library logicLib *.sv;
//     library work nets.pkg;
//     config cfgReal;  design logicLib.top; default liblist realLib logicLib;  endconfig
//     config cfgLogic; design logicLib.top; default liblist logicLib realLib; endconfig
//
//   top.sv:
//     module top();
//       interconnect [0:3] [0:1] aBus;
//       logic [0:3] dBus;
//       driver driverArray[0:3](aBus);
//       cmp cmpArray[0:3](aBus,rst,dBus);
//     endmodule : top
//
//   cmp.sv / driver.sv    : logic-valued versions of 'cmp' / 'driver'
//   cmp.svr / driver.svr  : real-valued versions, both with a module-header
//                           'import NetsPkg::*;' and ports of type realNet
//   nets.pkg              : package NetsPkg; nettype real realNet; endpackage
//
// What is checked (IEEE 1800-2023):
//   - 33.3.1: three libraries are declared in the map file: realLib (file
//     spec '*.svr'), logicLib ('*.sv') and work ('nets.pkg')
//   - 33.2.1/33.3.1: each source file is compiled into the library whose
//     file_path_spec it matches: cmp.svr/driver.svr -> realLib,
//     cmp.sv/driver.sv/top.sv -> logicLib, nets.pkg (NetsPkg) -> work; and
//     each library is the collection of the cells mapped into it
//   - 33.2.1: a cell name only has to be unique within its library, so the
//     two 'cmp' and the two 'driver' modules (in realLib and logicLib) are
//     not multiply-defined design units
//   - 26.4: the module-header 'import NetsPkg::*;' of cmp.svr/driver.svr makes
//     'realNet' visible in the port list, so it shall bind; built-in type
//     keyword 'logic' is never an identifier to bind
//   - 33.4.1: both configs exist; each has exactly one design statement
//     (33.4.1.1) naming logicLib.top, and one rule made of a 'default'
//     selector (33.4.1.2) and a 'liblist' expansion clause (33.4.1.5) with
//     the libraries in source order
//   - 6.6.8: 'aBus' in top is an interconnect net (vpiInterconnect)
//   - 33.4.1.5 with -cfg cfgReal: 'default liblist realLib logicLib' makes
//     the instances of 'driver' and 'cmp' under logicLib.top bind to the
//     realLib cells (driver.svr / cmp.svr), searched first
//
// What is NOT checked and why:
//   - the net types that the interconnect bits resolve to (23.3.3.7):
//     requires full net-type resolution, outside the scope of this
//     library/config test.
//   - the legality of the packed dimensions on 'interconnect' and on the
//     realNet ports in this modified example: orthogonal to the
//     library/config mapping under test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/clause.h>
#include <hldb/config_decl.h>
#include <hldb/config_rule.h>
#include <hldb/design.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
#include <hldb/library.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <filesystem>
#include <string>

namespace hlc {

class LibraryInterconTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LibraryIntercon.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string fileName(const hldb::Any *any) {
    return std::filesystem::path(std::string(any->getFile())).filename().string();
  }

  static const hldb::Library *findLibrary(std::string_view name) {
    if (m_design->getLibraries() == nullptr) return nullptr;
    for (const hldb::Library *const lib : *m_design->getLibraries()) {
      if (lib->getName() == name) return lib;
    }
    return nullptr;
  }

  // Module definition by def name and source file name.
  static const hldb::Module *findModule(std::string_view defName, std::string_view file) {
    if (m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName && fileName(m) == file) return m;
    }
    return nullptr;
  }

  static const hldb::ConfigDecl *findConfig(std::string_view name) {
    if (m_design->getConfigs() == nullptr) return nullptr;
    for (const hldb::ConfigDecl *const c : *m_design->getConfigs()) {
      if (c->getName() == name) return c;
    }
    return nullptr;
  }

  static void checkLibraryFileSpec(std::string_view libName, std::string_view spec) {
    const hldb::Library *const lib = findLibrary(libName);
    ASSERT_NE(lib, nullptr) << "library '" << libName << "' not found";
    ASSERT_NE(lib->getSourceFiles(), nullptr) << "33.3.1: '" << libName << "' declares file_path_spec '" << spec << "'";
    ASSERT_EQ(lib->getSourceFiles()->size(), 1u);
    EXPECT_EQ(lib->getSourceFiles()->at(0)->getName(), spec);
  }

  static void checkModuleLibrary(std::string_view defName, std::string_view file, std::string_view libName) {
    const hldb::Module *const m = findModule(defName, file);
    ASSERT_NE(m, nullptr) << "module '" << defName << "' from '" << file << "' not found";
    ASSERT_NE(m->getLibrary(), nullptr) << defName << " (" << file << ")";
    EXPECT_EQ(m->getLibrary()->getName(), libName) << defName << " (" << file << ")";
    EXPECT_EQ(m->getLibrary()->getActual(), findLibrary(libName)) << defName << " (" << file << ")";
  }

  static bool libraryHasCell(std::string_view libName, const hldb::Any *cell) {
    const hldb::Library *const lib = findLibrary(libName);
    if (lib == nullptr || lib->getCells() == nullptr || cell == nullptr) return false;
    for (const hldb::Any *const c : *lib->getCells()) {
      if (c == cell) return true;
    }
    return false;
  }

  static void checkConfig(std::string_view name, std::string_view lib0, std::string_view lib1) {
    const hldb::ConfigDecl *const cfg = findConfig(name);
    ASSERT_NE(cfg, nullptr) << "config '" << name << "' not found";

    // 33.4.1.1: design logicLib.top;
    ASSERT_NE(cfg->getTopModules(), nullptr);
    ASSERT_EQ(cfg->getTopModules()->size(), 1u) << "one top cell in the design statement";
    const hldb::RefObj *const design = cfg->getTopModules()->at(0);
    ASSERT_NE(design->getPathElems(), nullptr);
    ASSERT_EQ(design->getPathElems()->size(), 2u) << "library_identifier . cell_identifier";
    const hldb::RefObj *const lib = any_cast<hldb::RefObj>(design->getPathElems()->at(0));
    const hldb::RefObj *const cell = any_cast<hldb::RefObj>(design->getPathElems()->at(1));
    ASSERT_NE(lib, nullptr);
    ASSERT_NE(cell, nullptr);
    EXPECT_EQ(lib->getName(), "logicLib");
    EXPECT_EQ(lib->getActual(), findLibrary("logicLib"));
    EXPECT_EQ(cell->getName(), "top");
    EXPECT_EQ(design->getActual(), findModule("top", "top.sv"));

    // default liblist <lib0> <lib1>;
    ASSERT_NE(cfg->getConfigRules(), nullptr);
    ASSERT_EQ(cfg->getConfigRules()->size(), 1u);
    const hldb::ConfigRule *const rule = cfg->getConfigRules()->at(0);
    ASSERT_NE(rule->getTargetClause(), nullptr) << "liblist expansion clause missing";
    const hldb::Clause *const target = rule->getTargetClause();
    EXPECT_EQ(target->getClauseType(), vpiLiblistClause);
    ASSERT_NE(target->getLibraries(), nullptr);
    ASSERT_EQ(target->getLibraries()->size(), 2u);
    EXPECT_EQ(target->getLibraries()->at(0)->getName(), lib0) << "33.4.1.5: liblist order is significant";
    EXPECT_EQ(target->getLibraries()->at(1)->getName(), lib1) << "33.4.1.5: liblist order is significant";
    EXPECT_EQ(target->getLibraries()->at(0)->getActual(), findLibrary(lib0));
    EXPECT_EQ(target->getLibraries()->at(1)->getActual(), findLibrary(lib1));

    ASSERT_NE(rule->getSelector(), nullptr) << "33.4.1.2: the rule's 'default' selection clause is missing";
    EXPECT_EQ(rule->getSelector()->getClauseType(), vpiDefaultClause);
  }

  // Module definition that an instance (array) in 'top' is bound to.
  static const hldb::Module *getBoundModule(std::string_view instName) {
    const hldb::Module *const top = findModule("top", "top.sv");
    if (top == nullptr || top->getRefInstances() == nullptr) return nullptr;
    for (const hldb::RefInstance *const ri : *top->getRefInstances()) {
      if (ri->getName() != instName || ri->getTypespec() == nullptr) continue;
      const hldb::Typespec *ts = ri->getTypespec()->getActual();
      if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts)) {
        ts = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
      }
      const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(ts);
      return (mt == nullptr) ? nullptr : mt->getModule();
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Library declarations -- 33.3.1
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, ThreeLibrariesDeclared) {
  ASSERT_NE(m_design->getLibraries(), nullptr);
  EXPECT_EQ(m_design->getLibraries()->size(), 3u);
  EXPECT_NE(findLibrary("realLib"), nullptr);
  EXPECT_NE(findLibrary("logicLib"), nullptr);
  EXPECT_NE(findLibrary("work"), nullptr);
}

TEST_F(LibraryInterconTest, RealLibFileSpec) { checkLibraryFileSpec("realLib", "*.svr"); }

TEST_F(LibraryInterconTest, LogicLibFileSpec) { checkLibraryFileSpec("logicLib", "*.sv"); }

TEST_F(LibraryInterconTest, WorkFileSpec) { checkLibraryFileSpec("work", "nets.pkg"); }

// ---------------------------------------------------------------------------
// File -> library mapping -- 33.2.1, 33.3.1
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, FiveModuleDefinitionsExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 5u);
  EXPECT_NE(findModule("cmp", "cmp.sv"), nullptr);
  EXPECT_NE(findModule("cmp", "cmp.svr"), nullptr);
  EXPECT_NE(findModule("driver", "driver.sv"), nullptr);
  EXPECT_NE(findModule("driver", "driver.svr"), nullptr);
  EXPECT_NE(findModule("top", "top.sv"), nullptr);
}

TEST_F(LibraryInterconTest, SvrModulesAreInRealLib) {
  checkModuleLibrary("cmp", "cmp.svr", "realLib");
  checkModuleLibrary("driver", "driver.svr", "realLib");
}

TEST_F(LibraryInterconTest, SvModulesAreInLogicLib) {
  checkModuleLibrary("cmp", "cmp.sv", "logicLib");
  checkModuleLibrary("driver", "driver.sv", "logicLib");
  checkModuleLibrary("top", "top.sv", "logicLib");
}

TEST_F(LibraryInterconTest, NetsPkgIsInWork) {
  const hldb::Package *const pkg = hldb::findByName<hldb::Package>("NetsPkg", m_design->getAllPackages());
  ASSERT_NE(pkg, nullptr) << "package 'NetsPkg' not found";
  ASSERT_NE(pkg->getLibrary(), nullptr);
  EXPECT_EQ(pkg->getLibrary()->getName(), "work");
  EXPECT_EQ(pkg->getLibrary()->getActual(), findLibrary("work"));
}

TEST_F(LibraryInterconTest, LibrariesHoldTheirCells) {
  EXPECT_TRUE(libraryHasCell("realLib", findModule("cmp", "cmp.svr"))) << "33.2.1: a library is a collection of cells";
  EXPECT_TRUE(libraryHasCell("realLib", findModule("driver", "driver.svr")));
  EXPECT_TRUE(libraryHasCell("logicLib", findModule("cmp", "cmp.sv")));
  EXPECT_TRUE(libraryHasCell("logicLib", findModule("driver", "driver.sv")));
  EXPECT_TRUE(libraryHasCell("logicLib", findModule("top", "top.sv")));
  EXPECT_TRUE(libraryHasCell("work", hldb::findByName<hldb::Package>("NetsPkg", m_design->getAllPackages())));
}

TEST_F(LibraryInterconTest, SameCellNameInDifferentLibrariesIsLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_DESIGN_UNIT), nullptr)
      << "33.2.1: 'cmp' and 'driver' each appear once in realLib and once in logicLib; cell names only need to be "
         "unique within a library";
}

// ---------------------------------------------------------------------------
// Module-header package import -- 26.4
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, HeaderImportedRealNetBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "realNet"), nullptr)
      << "26.4: 'import NetsPkg::*;' in the module header makes 'realNet' visible in the port list";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "logic"), nullptr)
      << "'logic' is a built-in data type keyword, not an identifier to bind";
}

// ---------------------------------------------------------------------------
// Configurations -- 33.4.1
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, TwoConfigsDeclared) {
  ASSERT_NE(m_design->getConfigs(), nullptr);
  EXPECT_EQ(m_design->getConfigs()->size(), 2u);
}

TEST_F(LibraryInterconTest, CfgRealShape) { checkConfig("cfgReal", "realLib", "logicLib"); }

TEST_F(LibraryInterconTest, CfgLogicShape) { checkConfig("cfgLogic", "logicLib", "realLib"); }

// ---------------------------------------------------------------------------
// top: interconnect net -- 6.6.8
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, ABusIsInterconnectNet) {
  const hldb::Module *const top = findModule("top", "top.sv");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const aBus = hldb::findByName<hldb::Net>("aBus", top->getNets());
  ASSERT_NE(aBus, nullptr) << "'interconnect ... aBus' not found";
  EXPECT_EQ(aBus->getNetType(), vpiInterconnect);
}

// ---------------------------------------------------------------------------
// Binding under -cfg cfgReal -- 33.4.1.5
// ---------------------------------------------------------------------------

TEST_F(LibraryInterconTest, DriverArrayBindsToRealLibDriver) {
  const hldb::Module *const bound = getBoundModule("driverArray");
  ASSERT_NE(bound, nullptr) << "'driver driverArray[0:3](aBus)' is not bound to any module definition";
  EXPECT_EQ(bound, findModule("driver", "driver.svr"))
      << "33.4.1.5: cfgReal's 'default liblist realLib logicLib' searches realLib first -> driver.svr; bound to "
      << fileName(bound);
}

TEST_F(LibraryInterconTest, CmpArrayBindsToRealLibCmp) {
  const hldb::Module *const bound = getBoundModule("cmpArray");
  ASSERT_NE(bound, nullptr) << "'cmp cmpArray[0:3](aBus,rst,dBus)' is not bound to any module definition";
  EXPECT_EQ(bound, findModule("cmp", "cmp.svr"))
      << "33.4.1.5: cfgReal's 'default liblist realLib logicLib' searches realLib first -> cmp.svr; bound to "
      << fileName(bound);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
