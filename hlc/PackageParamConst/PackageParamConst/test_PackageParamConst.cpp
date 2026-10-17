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

// Validates the HLDB model built for tests/PackageParamConst/dut.sv:
//
//   package pkg;
//      parameter logic [3:0] A = 4'hF;
//   endpackage
//
//   module top(output a);
//      assign a = pkg::A[0];
//   endmodule
//
// The point of the fixture is a bit-select applied to a package parameter
// named with the package scope resolution operator (IEEE 1800-2023 26.3).
// The regression this file exists to catch is HLC losing the binding of the
// parameter once a select is applied to the scoped name: 'pkg::A[0]' must
// still bind 'A' to pkg's Parameter.
//
// What is checked, and why:
//   Package pkg (26.2)
//     - exists; no ': label' after endpackage
//     - exactly 1 parameter, 'A', of type 'logic [3:0]': a LogicTypespec with
//       exactly 1 packed range [3:0]
//     - A is a local parameter: in a package the keyword 'parameter' is a
//       synonym for 'localparam' (6.20.4)
//     - its ParamAssign binds the LHS to the Parameter and has the Constant
//       RHS "4'hF"
//   Module top (23.2)
//     - exactly 1 port, 'a', with direction output and port index 0
//       (37.14 detail 9)
//     - 'output a' writes neither a data type nor a port kind, so the port
//       is a net of the default net type, wire (23.2.2.3, 22.8): the module
//       has exactly 1 Net 'a' of net type vpiWire and no Variables, and the
//       port's low connection is bound to that Net
//   assign a = pkg::A[0]; (10.3)
//     - exactly 1 ContAssign whose LHS is bound to the Net 'a'
//     - its RHS is a package-scoped RefObj path: the prefix 'pkg' bound to
//       the package, then a BitSelect (11.5.1) whose index is the Constant
//       "0" and whose prefix 'A' is bound to pkg's Parameter A
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the scoped name binds: no COMP_FAILED_TO_BIND or
//       COMP_UNDEFINED_VARIABLE for 'A', and no COMP_UNDEFINED_PACKAGE for
//       'pkg'
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the RHS of a continuous assignment is not a
// constant-expression context, so the standard does not require HLC to fold
// 'pkg::A[0]'; only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The value 1'b1 that 'a' carries (bit 0 of 4'hF) only exists while
//     simulation runs, since the RHS is not required to be folded.
//     Permanently out of scope; the static half, that the select reads bit 0
//     of pkg::A, is covered by ContAssignRhsSelectsBitZeroOfPkgA.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the named item or select). That shape is a model convention; the
//     test follows it and asserts both the package and the item binding.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; the package is looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackageParamConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageParamConst.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()); }

  static const hldb::Parameter *getA() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("A", pkg->getParameters());
  }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Net *getNetA() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("a", top->getNets());
  }

  static const hldb::ContAssign *getContAssign() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->empty()) return nullptr;
    return top->getContAssigns()->at(0);
  }
};

// ---------------------------------------------------------------------------
// package pkg; parameter logic [3:0] A = 4'hF; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackageParamConstTest, PackageExistsWithoutEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "") << "'endpackage' is written without ': pkg'";
}

TEST_F(PackageParamConstTest, PackageHasExactlyParameterA) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u) << "'A' is pkg's only parameter";
  EXPECT_NE(getA(), nullptr) << "parameter 'A' not found";
}

TEST_F(PackageParamConstTest, ParameterAIsLogic3To0) {
  const hldb::Parameter *const a = getA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = a->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'A' is declared 'logic [3:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "3");
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(PackageParamConstTest, ParameterAIsLocalParam) {
  const hldb::Parameter *const a = getA();
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
}

TEST_F(PackageParamConstTest, ParameterAIsAssignedFourBitHexF) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParamAssigns(), nullptr);
  ASSERT_EQ(pkg->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = pkg->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(getA(), nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getA());
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'A' is assigned a literal";
  EXPECT_EQ(rhs->getDecompile(), "4'hF");
}

// ---------------------------------------------------------------------------
// module top(output a);
// ---------------------------------------------------------------------------

TEST_F(PackageParamConstTest, ModuleTopHasOneOutputPortA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u) << "'module top(output a)' declares exactly one port";
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "a");
  EXPECT_EQ(port->getDirection(), vpiOutput) << "'a' is declared 'output'";
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
}

TEST_F(PackageParamConstTest, PortAIsImplicitWireNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "'a' is the module's only net";
  const hldb::Net *const net = getNetA();
  ASSERT_NE(net, nullptr) << "23.2.2.3: an output port with no data type and no port kind is a net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "22.8: with no `default_nettype, the default net type is wire";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty()) << "'a' is a net, not a variable";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_FALSE(top->getPorts()->empty());
  const hldb::RefObj *const low = top->getPorts()->at(0)->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "the port's low connection is a reference to 'a'";
  EXPECT_EQ(low->getActual(), net) << "37.14 detail 4: the low connection is the module's own net 'a'";
}

// ---------------------------------------------------------------------------
// assign a = pkg::A[0];
// ---------------------------------------------------------------------------

TEST_F(PackageParamConstTest, ModuleTopHasOneContAssignDrivingA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  ASSERT_NE(getNetA(), nullptr);
  EXPECT_EQ(lhs->getActual(), getNetA()) << "10.3: the continuous assignment drives the port net 'a'";
}

TEST_F(PackageParamConstTest, ContAssignRhsSelectsBitZeroOfPkgA) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const path = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'pkg::A[0]' should be a package-scoped RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the select";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "pkg");
  ASSERT_NE(getPkg(), nullptr);
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names package pkg";

  const hldb::BitSelect *const select = any_cast<hldb::BitSelect>(path->getPathElems()->at(1));
  ASSERT_NE(select, nullptr) << "11.5.1: 'A[0]' is a bit-select";
  const hldb::Constant *const index = select->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
  const hldb::RefObj *const param = select->getPrefix<hldb::RefObj>();
  ASSERT_NE(param, nullptr) << "the select applies to 'A'";
  EXPECT_EQ(param->getName(), "A");
  ASSERT_NE(getA(), nullptr);
  EXPECT_EQ(param->getActual(), getA()) << "26.3: 'pkg::A' must bind to the Parameter A declared in pkg";
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(PackageParamConstTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top")
        << "23.3.1: a top-level instance is named after its module";
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageParamConstTest, ScopedNameIsNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "A"), nullptr) << "26.3: 'A' is declared in pkg";
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "A"), nullptr) << "26.3: 'A' is declared in pkg";
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_PACKAGE, "pkg"), nullptr)
      << "26.3: pkg is compiled before top references it";
}

TEST_F(PackageParamConstTest, NoFatalSyntaxOrErrorDiagnostics) {
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
