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

// Validates the HLDB model built for tests/PackEnumVal/dut.sv:
//
//   package test_package;
//     typedef enum logic {
//       On  = 1,
//       Off = 0
//     } lc_tx_t;
//     function automatic logic test_true(lc_tx_t val);
//       return On == val;
//     endfunction : test_true
//   endpackage
//
//   module top(input logic i, output logic o);
//     assign o = test_package::test_true(i);
//   endmodule
//
// The point of the fixture is a package function with an enum-typed formal,
// called through the package scope resolution operator (IEEE 1800-2023 26.3)
// from a continuous assignment. The argument 'i' is a logic, not a value of
// the enumerated type: "Enumerated variables are type-checked in
// assignments, arguments, and relational operators" (6.19.3), and "an
// integral type requires a cast to be assigned to an enum" (6.22.4). Passing
// 'i' to 'val' without a cast is therefore an error. The regression this
// file exists to catch is HLC losing the call's bindings, or not checking
// the argument against the enum formal.
//
// What is checked, and why:
//   Package test_package (26.2)
//     - exactly 1 Typedef, 'lc_tx_t', whose alias is an EnumTypespec (6.19)
//       with base type 'logic' (no packed range) and exactly 2 names in
//       source order: On with the value Constant "1", Off with "0"
//     - exactly 1 subroutine, the automatic (13.4.2) Function test_true with
//       the end label "test_true", returning a 1-bit logic
//     - exactly 1 formal, 'val', an input typed by lc_tx_t
//     - the body is 'return On == val;': a ReturnStmt whose expression is
//       Operation vpiEqOp over RefObj 'On', bound to the EnumConst, and
//       RefObj 'val', bound to the formal
//   Module top (23.2)
//     - exactly 2 ports, i (input) then o (output), with port indexes 0 and
//       1 (37.14 detail 9). Following 23.2.2.3, i is an input so it is a
//       net, and o is an output with the explicit data type logic so it is a
//       variable; each is its port's low connection
//     - exactly 1 ContAssign: LHS bound to the variable o; RHS a
//       package-scoped RefObj path whose prefix is bound to test_package and
//       whose last element is a call of test_true bound to the package's
//       Function, with exactly 1 argument, RefObj 'i' bound to the net i
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - passing the logic 'i' to the enum formal 'val' without a cast is
//       illegal (6.19.3, 6.22.4), so at least one error is reported
//     - the scoped call binds: no COMP_FAILED_TO_BIND for test_true
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: the RHS of a continuous assignment is not a
// constant-expression context; only the instance tree is checked under
// getElaborated().
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test: HLC
// leaves vpiPortIndex at 0 for every port, although 37.14 detail 9 says the
// port index gives the port order. TopPortIndexesFollowDeclarationOrder is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (uncast argument to an enum formal not reported), not
// a defect in this test: HLC reports no diagnostic at all for passing the
// logic 'i' to the enum formal 'val', although "Enumerated variables are
// type-checked in assignments, arguments, and relational operators" (6.19.3)
// and "an integral type requires a cast to be assigned to an enum" (6.22.4).
// UncastLogicArgumentToEnumFormalIsAnError is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Which diagnostic HLC gives for the uncast argument. The standard makes
//     it an error but names no specific diagnostic and no symbol to key it
//     on, so the error count is checked.
//   - The value o carries only exists while simulation runs.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the call). That shape is a model convention; the test follows it
//     and asserts both the package and the call binding.
//   - How a RefTypespec refers to lc_tx_t: it may resolve to the
//     TypedefTypespec or to the EnumTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; test_package is looked
//     up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/tf_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackEnumValTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackEnumVal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("test_package", m_design->getAllPackages());
  }

  static const hldb::Typedef *getLcTxT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("lc_tx_t", pkg->getTypedefs());
  }

  static const hldb::EnumConst *getEnumConst(std::string_view name) {
    const hldb::Typedef *const td = getLcTxT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr || et->getEnum() == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>(name, et->getEnum()->getEnumConsts());
  }

  static const hldb::Function *getTestTrue() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("test_true", pkg->getTaskFuncs());
  }

  static const hldb::IODecl *getVal() {
    const hldb::Function *const fn = getTestTrue();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>("val", fn->getIODecls());
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

// ---------------------------------------------------------------------------
// package test_package; ... endpackage
// ---------------------------------------------------------------------------

TEST_F(PackEnumValTest, LcTxTIsOneBitEnumWithOnAndOff) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'test_package' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'lc_tx_t' is the package's only typedef";
  const hldb::Typedef *const td = getLcTxT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'lc_tx_t' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic'";
  EXPECT_TRUE(base->getRanges() == nullptr || base->getRanges()->empty()) << "'logic' with no dimension is one bit";
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"On", "Off"};
  const char *const values[] = {"1", "0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

TEST_F(PackEnumValTest, TestTrueIsAutomaticReturningOneBitLogic) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << "'test_true' is the package's only subroutine";
  const hldb::Function *const fn = getTestTrue();
  ASSERT_NE(fn, nullptr) << "function 'test_true' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
  EXPECT_EQ(fn->getEndLabel(), "test_true") << "'endfunction : test_true' carries an end label";
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::LogicTypespec *const lt = fn->getReturn()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "the return type is 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty());
}

TEST_F(PackEnumValTest, FormalValIsInputOfLcTxT) {
  const hldb::Function *const fn = getTestTrue();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u) << "'val' is the only formal";
  const hldb::IODecl *const val = getVal();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDirection(), vpiInput) << "13.4: a formal with no direction is an input";
  const hldb::Typedef *const td = getLcTxT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(val->getTypespec(), nullptr);
  const hldb::Typespec *const actual = val->getTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "'val''s type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual()))
      << "'val' is declared with the type lc_tx_t";
}

TEST_F(PackEnumValTest, BodyReturnsOnEqualsVal) {
  const hldb::Function *const fn = getTestTrue();
  ASSERT_NE(fn, nullptr);
  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "the body is the single return statement";
  const hldb::Operation *const eq = ret->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "'On == val' is an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const on = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(on, nullptr);
  EXPECT_EQ(on->getName(), "On");
  ASSERT_NE(getEnumConst("On"), nullptr);
  EXPECT_EQ(on->getActual(), getEnumConst("On")) << "'On' is a name of lc_tx_t";
  const hldb::RefObj *const val = any_cast<hldb::RefObj>(eq->getOperands()->at(1));
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  ASSERT_NE(getVal(), nullptr);
  EXPECT_EQ(val->getActual(), getVal()) << "'val' is the function's formal";
}

// ---------------------------------------------------------------------------
// module top(input logic i, output logic o);
// ---------------------------------------------------------------------------

TEST_F(PackEnumValTest, TopHasInputNetIAndOutputVariableO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const hldb::Port *const i = top->getPorts()->at(0);
  const hldb::Port *const o = top->getPorts()->at(1);
  ASSERT_NE(i, nullptr);
  ASSERT_NE(o, nullptr);
  EXPECT_EQ(i->getName(), "i");
  EXPECT_EQ(i->getDirection(), vpiInput);
  EXPECT_EQ(o->getName(), "o");
  EXPECT_EQ(o->getDirection(), vpiOutput);
  const hldb::Net *const netI = hldb::findByName<hldb::Net>("i", top->getNets());
  ASSERT_NE(netI, nullptr) << "23.2.2.3: an input port with no port kind is a net";
  const hldb::Variable *const varO = hldb::findByName<hldb::Variable>("o", top->getVariables());
  ASSERT_NE(varO, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
  ASSERT_NE(i->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(i->getLowConn<hldb::RefObj>()->getActual(), netI);
  ASSERT_NE(o->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(o->getLowConn<hldb::RefObj>()->getActual(), varO);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (port index
// not set) in the file header.
TEST_F(PackEnumValTest, TopPortIndexesFollowDeclarationOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_NE(top->getPorts()->at(i), nullptr);
    EXPECT_EQ(top->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: vpiPortIndex gives the port order, and the first port has index 0";
  }
}

TEST_F(PackEnumValTest, ContAssignCallsScopedTestTrueWithI) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", top->getVariables()));
  const hldb::RefObj *const path = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'test_package::test_true(i)' should be a package-scoped RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the call";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "test_package");
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names test_package";
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "the last path element is the call 'test_true(i)'";
  EXPECT_EQ(call->getName(), "test_true");
  ASSERT_NE(getTestTrue(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getTestTrue()) << "26.3: the call binds to the package's test_true";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "i");
  EXPECT_EQ(arg->getActual(), hldb::findByName<hldb::Net>("i", top->getNets())) << "'i' is top's input net";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackEnumValTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (uncast
// argument to an enum formal not reported) in the file header.
TEST_F(PackEnumValTest, UncastLogicArgumentToEnumFormalIsAnError) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "6.19.3, 6.22.4: passing the logic 'i' to the enum formal 'val' requires a cast";
}

TEST_F(PackEnumValTest, ScopedCallIsNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "test_true"), nullptr)
      << "26.3: test_true is declared in test_package";
}

TEST_F(PackEnumValTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
