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

// Tests for tests/CompilationUnitScope -- how file boundaries and file order
// decide what a compilation unit sees (IEEE 1800-2023 Sec 3.12.1).
//
//   x.h           parameter  a = 20;
//   b.h           parameter  x = a;
//   ts_set.sv     `timescale 1us/1ns
//   ts_use.sv     module m_ts; endmodule
//   pkg.sv        package P; parameter int PV = 7; endpackage
//   unit_decl.sv  import P::*;  class C; endclass  checker chk; endchecker
//   unit_use.sv   module m_use; C h; localparam int q = PV; chk u_chk(); endmodule
//   cu_tu.sv      timeunit 100ps / 10ps;
//   cu_use.sv     module m_cu; endmodule
//
// The files are compiled once per test suite below, each from its own option
// file (.f, not .hlc, so regression.py does not pick them up as separate
// regression runs):
//
//   suite                 option file             order                     mode
//   SingleUnitTest        SingleUnit.f            x.h b.h ts_set ts_use     one unit
//                                                 pkg unit_decl unit_use
//   SingleUnitReversed..  SingleUnitReversed.f    b.h x.h ts_use ts_set     one unit
//   FileUnitTest          FileUnit.f              same as SingleUnit.f      -fileunit
//   FileUnitReversed..    FileUnitReversed.f      b.h x.h ts_use ts_set     -fileunit
//   CuTimeunitTest        CuTimeunit.f            cu_tu cu_use              one unit
//   CuTimeunitFileUnit..  CuTimeunitFileUnit.f    cu_tu cu_use              -fileunit
//
// The compilation-unit scope "can contain any item that can be defined within a
// package (see 26.2)" -- so the class C, the checker chk and the package import
// in unit_decl.sv are compilation-unit items, visible to unit_use.sv only when
// both files are in one unit.
//
// Sec 3.14.2.3 (c): without a `timescale, a design element takes "the time units
// of the compilation unit" -- the compilation-unit timeunit in cu_tu.sv applies
// to m_cu in cu_use.sv only when both files are in one unit.
//
// Sec 3.12.1 requires tools to support both:
//   a) "All files on a given compilation command line make a single compilation
//      unit (in which case the declarations within those files are accessible
//      following normal visibility rules throughout the entire set of files)."
//   b) "Each file is a separate compilation unit (in which case the declarations
//      in each compilation-unit scope are accessible only within its
//      corresponding file)."
// HLC's default is (a); -fileunit selects (b).
//
// Visibility inside one unit is order dependent: "Other than for task and
// function names (see 23.8.1), references shall only be made to names already
// defined in the compilation unit." With several files in one unit, "already
// defined" follows the order the files are compiled in.
//
// Directives follow the same unit boundaries: "Within a separately compiled
// unit, compiler directives once seen by a tool apply to all subsequent source
// text. However, compiler directives from one separately compiled unit shall
// not affect other compilation units." So the `timescale in ts_set.sv applies
// to m_ts in ts_use.sv only when both are in one unit and ts_set.sv comes
// first (Sec 3.14.2.3 rule (b), Sec 22.7).
//
// The PLI "shall provide an iterator to traverse all the compilation units":
// Design::getUnits(), each Unit listing its files in Unit::getSourceFiles().
//
// The $unit items are looked up through the database rather than through
// Design::getParameters(), so these tests do not depend on which object they
// are parented to (Design today; see hlc issue #41, "Out of scope").
//
// Time units are base-10 exponents of one second: 1us == -6, 1ns == -9.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/unit.h>
#include <hldb/variable.h>

#include <string>
#include <string_view>
#include <vector>

namespace hlc {

// Shared helpers. Each derived suite compiles once in its own SetUpTestSuite().
class CompilationUnitScopeTest : public Test {
 protected:
  static std::string_view baseName(std::string_view path) {
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
  }

  // The file names of every compilation unit, in Design::getUnits() order.
  static std::vector<std::vector<std::string>> unitFileNames() {
    std::vector<std::vector<std::string>> result;
    if (const hldb::UnitCollection *const units = m_design->getUnits()) {
      for (const hldb::Unit *const unit : *units) {
        std::vector<std::string> &names = result.emplace_back();
        if (const hldb::SourceFileCollection *const files = unit->getSourceFiles()) {
          for (const hldb::SourceFile *const file : *files) {
            names.emplace_back(baseName(file->getFile()));
          }
        }
      }
    }
    return result;
  }

  // The unique Parameter named 'name', wherever it is parented.
  static const hldb::Parameter *findParameter(std::string_view name) {
    const hldb::Parameter *found = nullptr;
    for (const hldb::Any *const any : m_session->getDatabase().getObjects<hldb::Parameter>()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(any);
      if ((param == nullptr) || (param->getName() != name)) continue;
      EXPECT_EQ(found, nullptr) << "more than one parameter named '" << name << "'";
      found = param;
    }
    return found;
  }

  // The right-hand side of the parameter assignment to 'lhsName', when it is a plain reference.
  static const hldb::RefObj *findParamAssignRhs(std::string_view lhsName) {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects<hldb::ParamAssign>()) {
      const hldb::ParamAssign *const pa = any_cast<hldb::ParamAssign>(any);
      if (pa == nullptr) continue;
      const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(pa->getLhs());
      if ((lhs == nullptr) || (lhs->getName() != lhsName)) continue;
      return any_cast<hldb::RefObj>(pa->getRhs());
    }
    return nullptr;
  }

  // The right-hand side of b.h's "parameter x = a;", i.e. the reference to 'a'.
  static const hldb::RefObj *findReferenceToA() { return findParamAssignRhs("x"); }

  static const hldb::Module *getMUse() { return hldb::findByName<hldb::Module>("m_use", m_design->getAllModules()); }

  // The type reference of "C h;" in m_use.
  static const hldb::RefTypespec *getHTypespec() {
    const hldb::Module *const m = getMUse();
    if (m == nullptr) return nullptr;
    const hldb::Variable *const h = hldb::findByName<hldb::Variable>("h", m->getVariables());
    return (h == nullptr) ? nullptr : h->getTypespec();
  }

  // The type reference of "chk u_chk();" in m_use.
  static const hldb::RefTypespec *getChkTypespec() {
    const hldb::Module *const m = getMUse();
    if (m == nullptr) return nullptr;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_chk", m->getRefInstances());
    return (inst == nullptr) ? nullptr : inst->getTypespec();
  }

  // The compilation unit whose members include 'any', or nullptr.
  static const hldb::Unit *unitOf(const hldb::Any *any) {
    if (const hldb::UnitCollection *const units = m_design->getUnits()) {
      for (const hldb::Unit *const unit : *units) {
        if (const hldb::AnyCollection *const members = unit->getMembers()) {
          for (const hldb::Any *const member : *members) {
            if (member == any) return unit;
          }
        }
      }
    }
    return nullptr;
  }

  static const hldb::Module *getMTs() { return hldb::findByName<hldb::Module>("m_ts", m_design->getAllModules()); }
};

// ----
// SingleUnit.f -- x.h b.h ts_set.sv ts_use.sv, one compilation unit.

class SingleUnitTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SingleUnit.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(SingleUnitTest, AllFilesFormOneUnitInCommandLineOrder) {
  // Sec 3.12.1 (a): all files on the command line make a single compilation unit.
  const std::vector<std::vector<std::string>> expected = {
      {"x.h", "b.h", "ts_set.sv", "ts_use.sv", "pkg.sv", "unit_decl.sv", "unit_use.sv"}};
  EXPECT_EQ(unitFileNames(), expected);
}

TEST_F(SingleUnitTest, ClassFromEarlierFileIsVisible) {
  // Sec 3.12.1 (a): unit_decl.sv's class C is visible to unit_use.sv.
  const hldb::RefTypespec *const rt = getHTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  const hldb::ClassTypespec *const cts = rt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(cts, nullptr);
  ASSERT_NE(cts->getClassDefn(), nullptr);
  EXPECT_EQ(cts->getClassDefn()->getName(), "C");
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "C"), nullptr);
}

TEST_F(SingleUnitTest, CheckerFromEarlierFileIsVisible) {
  const hldb::RefTypespec *const rt = getChkTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  EXPECT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::CheckerTypespec);
}

TEST_F(SingleUnitTest, ImportFromEarlierFileIsVisible) {
  // unit_decl.sv's "import P::*;" is in the compilation-unit scope, so it also
  // makes P's PV visible to unit_use.sv (Sec 3.12.1 (a), Sec 26.3).
  const hldb::RefObj *const ref = findParamAssignRhs("q");
  ASSERT_NE(ref, nullptr);
  ASSERT_NE(ref->getActual(), nullptr);
  const hldb::Parameter *const pv = ref->getActual<hldb::Parameter>();
  ASSERT_NE(pv, nullptr);
  EXPECT_EQ(pv->getName(), "PV");
  ASSERT_NE(pv->getParent(), nullptr);
  EXPECT_EQ(pv->getParent()->getAnyType(), hldb::AnyType::Package);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "PV"), nullptr);
}

TEST_F(SingleUnitTest, ReferenceBindsToEarlierFile) {
  // Sec 3.12.1 (a): x.h's 'a' is "already defined" when b.h refers to it.
  const hldb::Parameter *const a = findParameter("a");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(baseName(a->getFile()), "x.h");

  const hldb::RefObj *const ref = findReferenceToA();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(baseName(ref->getFile()), "b.h");
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual(), a);

  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "a"), nullptr);
}

TEST_F(SingleUnitTest, BothParametersBelongToTheSameUnit) {
  const hldb::Parameter *const a = findParameter("a");
  const hldb::Parameter *const x = findParameter("x");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(x, nullptr);
  ASSERT_NE(unitOf(a), nullptr);
  EXPECT_EQ(unitOf(a), unitOf(x));
}

TEST_F(SingleUnitTest, TimescaleFromEarlierFileApplies) {
  // Sec 3.12.1: directives "once seen by a tool apply to all subsequent source
  // text" of the unit; Sec 3.14.2.3 (b): the last `timescale in the unit.
  const hldb::Module *const m = getMTs();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -6);
  EXPECT_EQ(m->getTimePrecision(), -9);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_ts"), nullptr);
}

// ----
// SingleUnitReversed.f -- b.h x.h ts_use.sv ts_set.sv, one compilation unit.

class SingleUnitReversedTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SingleUnitReversed.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(SingleUnitReversedTest, AllFilesFormOneUnitInCommandLineOrder) {
  const std::vector<std::vector<std::string>> expected = {{"b.h", "x.h", "ts_use.sv", "ts_set.sv"}};
  EXPECT_EQ(unitFileNames(), expected);
}

TEST_F(SingleUnitReversedTest, ReferenceToLaterFileIsAForwardReference) {
  // Sec 3.12.1: "references shall only be made to names already defined in the
  // compilation unit." 'a' is defined in x.h, compiled after b.h, so b.h's
  // reference (b.h:1:16) is to a later compilation-unit item.
  EXPECT_NE(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "a", 1, 16), nullptr);
}

TEST_F(SingleUnitReversedTest, TimescaleFromLaterFileDoesNotApply) {
  // The `timescale in ts_set.sv comes after m_ts, so nothing gives m_ts a time
  // scope; with no -timescale on the command line it is reported.
  const hldb::Module *const m = getMTs();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), 0);
  EXPECT_EQ(m->getTimePrecision(), 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_ts"), nullptr);
}

// ----
// FileUnit.f -- x.h b.h ts_set.sv ts_use.sv, -fileunit.

class FileUnitTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileUnit.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(FileUnitTest, EachFileIsItsOwnUnit) {
  // Sec 3.12.1 (b): each file is a separate compilation unit.
  const std::vector<std::vector<std::string>> expected = {{"x.h"},    {"b.h"},          {"ts_set.sv"},  {"ts_use.sv"},
                                                          {"pkg.sv"}, {"unit_decl.sv"}, {"unit_use.sv"}};
  EXPECT_EQ(unitFileNames(), expected);
}

TEST_F(FileUnitTest, ClassFromAnotherUnitIsNotVisible) {
  // Sec 3.12.1 (b): class C in unit_decl.sv's compilation-unit scope is
  // "accessible only within its corresponding file".
  const hldb::RefTypespec *const rt = getHTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getActual(), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "C", 2, 3), nullptr);
}

TEST_F(FileUnitTest, CheckerFromAnotherUnitIsNotVisible) {
  // The checker chk is a compilation-unit item of unit_decl.sv (Sec 3.12.1,
  // Sec 3.13 c), so u_chk names no checker.
  const hldb::RefTypespec *const rt = getChkTypespec();
  ASSERT_NE(rt, nullptr);
  if (rt->getActual() != nullptr) {
    EXPECT_NE(rt->getActual()->getAnyType(), hldb::AnyType::CheckerTypespec);
  }
}

TEST_F(FileUnitTest, ImportInAnotherUnitIsNotVisible) {
  // unit_decl.sv's "import P::*;" only affects its own compilation-unit scope;
  // unit_use.sv neither imports P nor qualifies PV.
  const hldb::RefObj *const ref = findParamAssignRhs("q");
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "PV", 3, 22), nullptr);
}

TEST_F(FileUnitTest, ReferenceToAnotherUnitDoesNotBind) {
  // Sec 3.12.1 (b): x.h's 'a' is "accessible only within its corresponding
  // file", and "Items defined in the compilation-unit scope cannot be accessed
  // by name from outside the compilation unit." b.h's 'a' names nothing.
  const hldb::RefObj *const ref = findReferenceToA();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a", 1, 16), nullptr);
  // Not a forward reference: no unit holds both the reference and 'a'.
  EXPECT_EQ(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "a"), nullptr);
}

TEST_F(FileUnitTest, EachParameterBelongsToItsOwnFilesUnit) {
  const hldb::Parameter *const a = findParameter("a");
  const hldb::Parameter *const x = findParameter("x");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(x, nullptr);
  ASSERT_NE(unitOf(a), nullptr);
  ASSERT_NE(unitOf(x), nullptr);
  EXPECT_NE(unitOf(a), unitOf(x));
}

TEST_F(FileUnitTest, TimescaleDoesNotCrossUnits) {
  // Sec 3.12.1: "compiler directives from one separately compiled unit shall not
  // affect other compilation units" (also Sec 3.14.2.1).
  const hldb::Module *const m = getMTs();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), 0);
  EXPECT_EQ(m->getTimePrecision(), 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_ts"), nullptr);
}

// ----
// FileUnitReversed.f -- b.h x.h ts_use.sv ts_set.sv, -fileunit. With separate
// units the order changes nothing about what each file can see.

class FileUnitReversedTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileUnitReversed.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(FileUnitReversedTest, EachFileIsItsOwnUnit) {
  const std::vector<std::vector<std::string>> expected = {{"b.h"}, {"x.h"}, {"ts_use.sv"}, {"ts_set.sv"}};
  EXPECT_EQ(unitFileNames(), expected);
}

TEST_F(FileUnitReversedTest, ReferenceToAnotherUnitDoesNotBind) {
  const hldb::RefObj *const ref = findReferenceToA();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a", 1, 16), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "a"), nullptr);
}

TEST_F(FileUnitReversedTest, TimescaleDoesNotCrossUnits) {
  const hldb::Module *const m = getMTs();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), 0);
  EXPECT_EQ(m->getTimePrecision(), 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_ts"), nullptr);
}

// ----
// CuTimeunit.f -- cu_tu.sv cu_use.sv, one compilation unit. 100ps == -10, 10ps == -11.

class CuTimeunitTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CuTimeunit.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CuTimeunitTest, CompilationUnitTimeunitAppliesToLaterFile) {
  // Sec 3.14.2.3 (c): no own declaration, no enclosing element, no `timescale,
  // so m_cu takes "the time units of the compilation unit"; precision likewise.
  const hldb::Module *const m = hldb::findByName<hldb::Module>("m_cu", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -10);
  EXPECT_EQ(m->getTimePrecision(), -11);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_cu"), nullptr);
}

// ----
// CuTimeunitFileUnit.f -- cu_tu.sv cu_use.sv, -fileunit.

class CuTimeunitFileUnitTest : public CompilationUnitScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CuTimeunitFileUnit.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CuTimeunitFileUnitTest, CompilationUnitTimeunitDoesNotCrossUnits) {
  // cu_use.sv is its own compilation unit, whose scope specifies no time unit
  // (Sec 3.14.2.3 (d): the default applies).
  const hldb::Module *const m = hldb::findByName<hldb::Module>("m_cu", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), 0);
  EXPECT_EQ(m->getTimePrecision(), 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_cu"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
