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

// Validates the HLDB model built for tests/PackageNet/dut.sv:
//
//   package my_package1;
//   parameter  p1 = 1;
//   localparam p2 = 2;
//   typedef logic [1:0] word;
//   word v;
//   endpackage
//
//   module test();
//   import my_package1::*;
//   initial begin
//     v = p1 + p2;
//   end
//   endmodule
//
// The point of the fixture is a module that writes a package variable and
// reads two package parameters through a wildcard import (IEEE 1800-2023
// 26.3). 'v' is declared in the package with a user-defined data type, so it
// is a variable (6.8), and the reference in the module must bind to that
// package variable. The regression this file exists to catch is HLC treating
// the imported name 'v' as something local to the module -- an implicit net
// or a module variable -- instead of binding it to my_package1::v.
//
// What is checked, and why:
//   Package my_package1 (26.2)
//     - exists; no ': label' after endpackage
//     - exactly 2 parameters, p1 and p2. Both are local parameters: p2 is
//       declared 'localparam', and in a package the keyword 'parameter' is a
//       synonym for 'localparam' (6.20.4)
//     - their ParamAssigns bind the LHS to the Parameter and have the
//       Constant RHS "1" and "2"
//     - typedef logic [1:0] word; (6.18): exactly 1 Typedef, 'word', whose
//       alias is a LogicTypespec with exactly 1 packed range [1:0]
//     - word v; is a data declaration with no net type, so it declares a
//       variable (6.8): the package has exactly 1 Variable 'v', typed by
//       'word', with no initializer, and the package has no nets
//     - v is a package variable, so its lifetime is static (6.21)
//   Module test (23.2)
//     - no ports, and no Nets or Variables of its own. The wildcard import
//       makes 'v' locally visible but declares nothing in 'test' (26.3), and
//       since 'v' is declared no implicit net is created (6.10)
//     - exactly 1 process, an Initial whose begin-end holds exactly 1
//       statement
//   v = p1 + p2;
//     - a blocking Assignment (10.4.1)
//     - LHS RefObj 'v' bound by object identity to my_package1::v (26.3)
//     - RHS Operation vpiAddOp with exactly 2 operands, RefObj 'p1' and
//       RefObj 'p2', each bound to the package's Parameter of that name
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics, and no
//       COMP_UNDEFINED_VARIABLE or COMP_FAILED_TO_BIND for v, p1 or p2
//
// Reduction and elaboration: nothing here is required to reduce. The
// parameter values are already literals, and the RHS of a procedural
// assignment is not a constant-expression context, so the standard does not
// require HLC to fold 'p1 + p2'. No check is gated on getElaborated().
//
// KNOWN COMPILER BUG (package parameter is not a localparam), not a defect
// in this test: 6.20.4 says that in a package "the parameter keyword shall
// be a synonym for the localparam keyword", but HLC reports
// Parameter::getLocalParam() false for 'parameter p1 = 1;'.
// BothParametersAreLocalParams asserts the LRM value and is expected to fail
// until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value 3 that v holds after the initial procedure runs only exists
//     while simulation runs. Permanently out of scope; the static half, that
//     the assignment binds v, p1 and p2 to the package's declarations, is
//     covered by AssignmentLhsIsPackageVariableV and
//     AssignmentRhsAddsPackageParameters.
//   - The parameters' types: 'parameter p1 = 1;' writes no data type, so its
//     type comes from its value (6.20.2). Whether HLC records a typespec for
//     it, and which, is a tool convention.
//   - How 'v' refers to 'word': its RefTypespec may resolve to the
//     TypedefTypespec or to the LogicTypespec that 'word' aliases. Both are
//     that type (6.18), so either is accepted.
//   - Where HLC records 'import my_package1::*;' is a model convention. Its
//     effect is asserted through the bindings above.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackageNetTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageNet.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("my_package1", m_design->getAllPackages());
  }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  // The ParamAssign whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getWord() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("word", pkg->getTypedefs());
  }

  static const hldb::Variable *getV() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("v", pkg->getVariables());
  }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("test", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBlock() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Begin *const block = getInitialBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Assignment>(block->getStmts()->at(0));
  }

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "26.3: '" << name << "' must bind to my_package1's declaration";
  }

  // Verifies the ParamAssign of 'name' binds its LHS to the Parameter and has
  // the Constant RHS 'value'.
  static void ExpectParamAssign(std::string_view name, std::string_view value) {
    const hldb::ParamAssign *const pa = getParamAssign(name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(getParam(name), nullptr);
    EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getParam(name));
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "'" << name << "' is assigned a literal";
    EXPECT_EQ(rhs->getDecompile(), value);
  }
};

// ---------------------------------------------------------------------------
// package my_package1; ... endpackage
// ---------------------------------------------------------------------------

TEST_F(PackageNetTest, PackageExistsWithoutEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'my_package1' not found";
  EXPECT_EQ(pkg->getName(), "my_package1");
  EXPECT_EQ(pkg->getEndLabel(), "") << "'endpackage' is written without ': my_package1'";
}

TEST_F(PackageNetTest, PackageHasExactlyParametersP1AndP2) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u) << "'p1' and 'p2'";
  EXPECT_NE(getParam("p1"), nullptr);
  EXPECT_NE(getParam("p2"), nullptr);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (package
// parameter is not a localparam) in the file header.
TEST_F(PackageNetTest, BothParametersAreLocalParams) {
  ASSERT_NE(getParam("p1"), nullptr);
  ASSERT_NE(getParam("p2"), nullptr);
  EXPECT_TRUE(getParam("p1")->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  EXPECT_TRUE(getParam("p2")->getLocalParam()) << "6.20.4: 'p2' is declared 'localparam'";
}

TEST_F(PackageNetTest, ParamAssignsAreOneAndTwo) {
  ExpectParamAssign("p1", "1");
  ExpectParamAssign("p2", "2");
}

TEST_F(PackageNetTest, TypedefWordIsLogic1To0) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'word' is the package's only typedef";
  const hldb::Typedef *const word = getWord();
  ASSERT_NE(word, nullptr) << "typedef 'word' not found";
  ASSERT_NE(word->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = word->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.18: 'word' names the type 'logic [1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "'logic [1:0]' has exactly one packed dimension";
  const hldb::Range *const range = lt->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(PackageNetTest, PackageDeclaresVariableVAndNoNets) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getVariables(), nullptr);
  EXPECT_EQ(pkg->getVariables()->size(), 1u) << "'word v;' is the package's only data declaration";
  EXPECT_NE(getV(), nullptr) << "6.8: 'word v;' has no net type, so it declares a variable";
  EXPECT_TRUE(pkg->getNets() == nullptr || pkg->getNets()->empty()) << "the package declares no net";
}

TEST_F(PackageNetTest, VariableVIsTypedByWord) {
  const hldb::Variable *const v = getV();
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr) << "'v' is declared with the type 'word'";
  const hldb::Typedef *const word = getWord();
  ASSERT_NE(word, nullptr);
  ASSERT_NE(word->getAlias(), nullptr);
  const hldb::Typespec *const actual = rts->getActual();
  ASSERT_NE(actual, nullptr) << "'v's type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  const bool isWord =
      ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == word)) || (actual == word->getAlias()->getActual());
  EXPECT_TRUE(isWord) << "6.18: 'v' must be typed by my_package1's 'word'";
}

TEST_F(PackageNetTest, VariableVIsStaticWithoutInitializer) {
  const hldb::Variable *const v = getV();
  ASSERT_NE(v, nullptr);
  EXPECT_FALSE(v->getAutomatic()) << "6.21: a variable declared in a package is static";
  EXPECT_EQ(v->getValue(), nullptr) << "'word v;' has no initializer";
}

// ---------------------------------------------------------------------------
// module test(); import my_package1::*; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(PackageNetTest, ModuleTestHasNoPortsNetsOrVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'test' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module test();' has an empty port list";
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "6.10: 'v' is declared in my_package1, so no implicit net is created for it";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "26.3: the import makes 'v' visible in 'test' but does not declare a variable there";
}

TEST_F(PackageNetTest, ModuleTestHasOneInitialWithOneStatement) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "9.2.1: the only process is an initial";
  const hldb::Begin *const block = getInitialBlock();
  ASSERT_NE(block, nullptr) << "the initial's statement is a begin-end";
  ASSERT_NE(block->getStmts(), nullptr);
  EXPECT_EQ(block->getStmts()->size(), 1u) << "'v = p1 + p2;' is the only statement";
}

// ---------------------------------------------------------------------------
// v = p1 + p2;
// ---------------------------------------------------------------------------

TEST_F(PackageNetTest, AssignmentIsBlocking) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr) << "'v = p1 + p2;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
}

TEST_F(PackageNetTest, AssignmentLhsIsPackageVariableV) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  ExpectBoundRef(assign->getLhs(), "v", getV());
}

TEST_F(PackageNetTest, AssignmentRhsAddsPackageParameters) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const add = assign->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "'p1 + p2' should be an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  ExpectBoundRef(add->getOperands()->at(0), "p1", getParam("p1"));
  ExpectBoundRef(add->getOperands()->at(1), "p2", getParam("p2"));
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageNetTest, PackageNamesAreNotReported) {
  for (std::string_view name : {"v", "p1", "p2"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "26.3: '" << name << "' is declared in my_package1 and imported by 'test'";
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "26.3: '" << name << "' is declared in my_package1 and imported by 'test'";
  }
}

TEST_F(PackageNetTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
