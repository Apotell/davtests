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

// Validates the HLDB model and the diagnostics built for
// tests/Google/chapter-18/18.8--disabling-random-variables-with-rand_mode_4.sv.
// Lines 1-16 are the license and the suite's metadata comment, which says:
//
//   :should_fail_because: The rand_mode() method is built-in and cannot be
//                         overridden.
//   :tags: 18.8
//   :type: simulation
//
// The rest is:
//
//   18  class a1;
//   19      rand int x;
//   20      function int rand_mode();
//   21          return 1;
//   22      endfunction
//   23  endclass
//
// IEEE 1800-2023 18.8: "The rand_mode() method is built-in and cannot be
// overridden." Class a1 declares its own method named rand_mode, so the file
// is illegal. The rule is about a declaration, not about anything that
// happens at run time, so a tool can and must reject it while compiling,
// even though the suite tags the file ':type: simulation'. HLC defines a
// dedicated diagnostic for exactly this case,
// COMP_CANNOT_REDEFINE_BUILTIN_METHOD, and the test expects it.
//
// The .hlc passes -nobuiltin, which stops HLC from loading its built-in
// package. That is a tool option; 18.8 makes rand_mode() built into every
// class with random variables regardless, so it does not change what is
// expected.
//
// What is checked, and why:
//   class a1 (8.3)
//     - exactly one class named "a1"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label; declares no
//       constraint block (18.5)
//     - 'rand int x;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//   function int rand_mode(); return 1; endfunction (13.4)
//     - the class body declares exactly one method: the Function
//       'rand_mode'; a class method (getMethod() true), automatic (8.6),
//       public (8.18), not virtual, no end label
//     - empty formal list; return type 'int', a signed IntTypespec (6.11)
//     - the body is exactly one statement, 'return 1;': a ReturnStmt whose
//       value is the literal 1 (13.4.1)
//   Diagnostics (18.8)
//     - COMP_CANNOT_REDEFINE_BUILTIN_METHOD is reported for 'rand_mode', in
//       this file, on line 20 (the declaration), and HLC classifies it as an
//       error (not a warning or note)
//     - the compilation therefore reports at least one error
//     - every error that points into this file points into the rand_mode
//       declaration (lines 20-22): the rest of the file is legal
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no module, no parameter and no class specialization. No check is
// gated on getElaborated().
//
// KNOWN FAILURE (illegal override of rand_mode() accepted), not a defect in
// this test: RedefiningRandModeIsReportedOnItsDeclaration and
// CompilationReportsAtLeastOneError fail on HLC 577. HLC compiles this file
// with zero fatal, syntax and error diagnostics and never reports
// COMP_CANNOT_REDEFINE_BUILTIN_METHOD for 'rand_mode', although 18.8
// forbids overriding it and HLC defines that diagnostic as an error. With
// no error reported, EveryErrorInThisFileIsOnTheRandModeDeclaration passes
// trivially. Both checks are expected to fail until HLC is fixed and are
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The column of the diagnostic: whether HLC points at 'function', at
//     the return type or at the name is a tool convention, so only the line
//     is asserted.
//   - Whether HLC still records the illegal rand_mode as a method of a1
//     after diagnosing it is a tool convention. The structural checks above
//     describe the source as written; if a tool drops the method after the
//     error, those checks fail and say so.
//   - Whether the function's return value is modeled as an implicit
//     Variable named 'rand_mode' (13.4.1) is a model convention.
//   - Whether HLC stores the one-statement body as the bare statement or
//     as a Begin holding it is a model convention; both are accepted, and
//     in both the ReturnStmt must be the only statement.
//   - Warnings, notes and the license/metadata comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class DisablingRandomVariablesWithRandMode4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.8--disabling-random-variables-with-rand_mode_4.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static constexpr std::string_view kSourceName = "18.8--disabling-random-variables-with-rand_mode_4.sv";

  // --- class a1 -----------------------------------------------------------

  static const hldb::ClassDefn *getClassA1() {
    return hldb::findByName<hldb::ClassDefn>("a1", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassA1X() {
    const hldb::ClassDefn *const cls = getClassA1();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  static const hldb::Function *getRandMode() {
    const hldb::ClassDefn *const cls = getClassA1();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("rand_mode", cls->getMethods());
  }

  // The statements of the rand_mode body: the bare statement, or the
  // statements of a Begin wrapping it.
  static std::vector<const hldb::Any *> getBodyStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Function *const fn = getRandMode();
    if (fn == nullptr || fn->getStmt() == nullptr) {
      return stmts;
    }
    if (const hldb::Begin *const block = fn->getStmt<hldb::Begin>()) {
      if (block->getStmts() == nullptr) {
        return stmts;
      }
      for (const hldb::Any *const stmt : *block->getStmts()) {
        stmts.emplace_back(stmt);
      }
      return stmts;
    }
    stmts.emplace_back(fn->getStmt());
    return stmts;
  }

  // --- diagnostics --------------------------------------------------------

  // True if 'type' has severity error or worse.
  static bool isErrorSeverity(ErrorDefinition::ErrorType type) {
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(type);
    if (info == infos.cend()) {
      return false;
    }
    return info->second.m_severity <= ErrorDefinition::ERROR;
  }

  // The source lines of this file that diagnostics of 'type' point to. When
  // 'errorsOnly' is set, 'type' is ignored and every diagnostic of severity
  // error or worse is collected instead.
  static std::vector<uint32_t> getDiagnosticLines(ErrorDefinition::ErrorType type, bool errorsOnly) {
    std::vector<uint32_t> lines;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (errorsOnly) {
        if (!isErrorSeverity(error.getType())) {
          continue;
        }
      } else if (error.getType() != type) {
        continue;
      }
      for (const Location &loc : error.getLocations()) {
        const std::string_view path = FileSystem::toPath(loc.m_fileId);
        if (path.find(kSourceName) == std::string_view::npos) {
          continue;
        }
        lines.emplace_back(loc.m_line);
      }
    }
    return lines;
  }

  // --- expectations -------------------------------------------------------

  // Verifies 'type' is 'int': an IntTypespec that is signed.
  static void ExpectSignedInt(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << what << " is declared 'int'";
    EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type, so " << what << " is signed";
  }
};

// ---------------------------------------------------------------------------
// class a1; rand int x; ... endclass
// ---------------------------------------------------------------------------

TEST_F(DisablingRandomVariablesWithRandMode4Test, ClassA1IsPlainUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const c : *m_design->getAllClasses()) {
    if (c->getName() == "a1") {
      ++count;
    }
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'a1' exactly once";
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr) << "class 'a1' not found";
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class a1;' is a user-defined class";
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'a1' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a1;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a1;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a1'";
  EXPECT_TRUE(cls->getConstraints() == nullptr || cls->getConstraints()->empty())
      << "18.5: class a1 declares no constraint block";
}

TEST_F(DisablingRandomVariablesWithRandMode4Test, ClassA1HasOneRandIntPropertyX) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property";
  const hldb::Variable *const x = getClassA1X();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(cls->getVariables()->at(0), x);
  ExpectSignedInt(x->getTypespec(), "class a1's 'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
}

// ---------------------------------------------------------------------------
// function int rand_mode(); return 1; endfunction
// ---------------------------------------------------------------------------

TEST_F(DisablingRandomVariablesWithRandMode4Test, ClassA1DeclaresOneFunctionRandMode) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr) << "the class body declares 'function int rand_mode()'";
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'rand_mode' is the only method written in class a1";
  const hldb::Function *const fn = getRandMode();
  ASSERT_NE(fn, nullptr) << "13.4: 'function int rand_mode()' is a Function";
  EXPECT_TRUE(fn->getMethod()) << "8.6: a function declared in a class is a method";
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.18: 'rand_mode' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(fn->getVirtual()) << "8.20: 'rand_mode' is not declared 'virtual'";
  EXPECT_EQ(fn->getEndLabelObj(), nullptr) << "'endfunction' is written without ': rand_mode'";
}

TEST_F(DisablingRandomVariablesWithRandMode4Test, RandModeHasNoFormalsAndReturnsSignedInt) {
  const hldb::Function *const fn = getRandMode();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty()) << "'rand_mode()' has an empty port list";
  ExpectSignedInt(fn->getReturn(), "the return type of 'rand_mode'");
}

TEST_F(DisablingRandomVariablesWithRandMode4Test, RandModeBodyIsReturnOne) {
  ASSERT_NE(getRandMode(), nullptr);
  const std::vector<const hldb::Any *> stmts = getBodyStmts();
  ASSERT_EQ(stmts.size(), 1u) << "'return 1;' is the function's only statement";
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[0]);
  ASSERT_NE(ret, nullptr) << "13.4.1: 'return 1;' should be a ReturnStmt";
  const hldb::Constant *const value = ret->getCondition<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the returned value is the literal 1";
  EXPECT_EQ(value->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Diagnostics: rand_mode() is built in and cannot be overridden (18.8)
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (illegal override
// of rand_mode() accepted) in the file header.
TEST_F(DisablingRandomVariablesWithRandMode4Test, RedefiningRandModeIsReportedOnItsDeclaration) {
  EXPECT_NE(findError(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD, "rand_mode"), nullptr)
      << "18.8: 'The rand_mode() method is built-in and cannot be overridden.'";
  const std::vector<uint32_t> lines = getDiagnosticLines(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD, false);
  ASSERT_FALSE(lines.empty()) << "the redefinition must be reported in this file";
  size_t onDeclaration = 0;
  for (const uint32_t line : lines) {
    if (line == 20) {
      ++onDeclaration;
    }
  }
  EXPECT_GE(onDeclaration, 1u) << "line 20 declares 'function int rand_mode();'";
}

TEST_F(DisablingRandomVariablesWithRandMode4Test, RedefinitionDiagnosticIsAnError) {
  EXPECT_TRUE(isErrorSeverity(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD))
      << "18.8: overriding rand_mode() is illegal, so it is an error, not a warning";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (illegal override
// of rand_mode() accepted) in the file header.
TEST_F(DisablingRandomVariablesWithRandMode4Test, CompilationReportsAtLeastOneError) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbFatal + stats.nbSyntax + stats.nbError, 1)
      << "the source is illegal (18.8), so the compilation cannot be clean";
}

TEST_F(DisablingRandomVariablesWithRandMode4Test, EveryErrorInThisFileIsOnTheRandModeDeclaration) {
  const std::vector<uint32_t> lines = getDiagnosticLines(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD, true);
  for (const uint32_t line : lines) {
    EXPECT_TRUE(line >= 20 && line <= 22)
        << "error at line " << line << ": only the rand_mode declaration (lines 20-22) is illegal";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
