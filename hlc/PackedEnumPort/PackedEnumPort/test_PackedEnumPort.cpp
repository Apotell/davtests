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

// Validates the HLDB model built for tests/PackedEnumPort/dut.sv. With the
// comments removed, the design is:
//
//   package lc_ctrl_pkg;
//      typedef enum logic [3:0] {
//         On = 4'b1010
//      } lc_tx_e;
//      typedef lc_tx_e lc_tx_t;
//   endpackage : lc_ctrl_pkg
//
//   module lc_ctrl_fsm(
//     input logic lc_clk_byp_ack_i
//   );
//   endmodule : lc_ctrl_fsm
//
//   module top(output int o, output int p);
//     lc_ctrl_pkg::lc_tx_t [0:0] lc_clk_byp_ack;
//      lc_ctrl_fsm u_lc_ctrl_fsm (
//         .lc_clk_byp_ack_i( lc_clk_byp_ack[0])
//      );
//   endmodule : top
//
// A module 'prim_lc_sync' and several statements are commented out, so they
// do not exist (5.4).
//
// The point of the fixture is a port of an instance connected to an element
// of a packed array whose element type is a package-scoped enum typedef
// (IEEE 1800-2023 26.3, 7.4.1). Connecting the 4-bit enum element to the
// 1-bit logic port is legal: an enum converts to an integral type without a
// cast (6.22.3). The regression this file exists to catch is HLC failing to
// resolve the package-scoped element type, or losing the port connection.
//
// What is checked, and why:
//   Package lc_ctrl_pkg (26.2)
//     - end label "lc_ctrl_pkg"; exactly 2 typedefs
//     - lc_tx_e: an enum (6.19) with base type logic [3:0] and the single
//       name On with the value Constant "4'b1010"
//     - lc_tx_t: a typedef of lc_tx_e (6.18)
//   Module lc_ctrl_fsm
//     - end label "lc_ctrl_fsm"; exactly 1 port, lc_clk_byp_ack_i, an input.
//       An input port with no port kind is a net (23.2.2.3): the module's Net
//       lc_clk_byp_ack_i, a LogicTypespec with no packed range, is the
//       port's low connection
//   Module top
//     - end label "top"; exactly 2 ports, o then p, both outputs. Each is
//       declared 'int', an explicit data type, so each is a variable
//       (23.2.2.3) of the signed type int (6.11)
//     - port indexes 0 and 1 (37.14 detail 9)
//     - exactly 3 variables: o, p and lc_clk_byp_ack
//     - lc_clk_byp_ack is a packed array with the single packed dimension
//       [0:0] whose element type is the package-scoped name
//       lc_ctrl_pkg::lc_tx_t: a RefTypespec path whose prefix is bound to
//       lc_ctrl_pkg and whose last element resolves to the package's lc_tx_t
//     - exactly 1 instance, u_lc_ctrl_fsm, of the module lc_ctrl_fsm, which
//       connects exactly 1 port, lc_clk_byp_ack_i, by name (23.3.2.2); its
//       high connection (37.14 detail 3) is a bit-select whose prefix is
//       bound to top's variable lc_clk_byp_ack and whose index is the
//       Constant "0"
//   Elaboration (23.3.1)
//     - lc_ctrl_fsm is instantiated, so top is the only top-level instance;
//       it holds exactly 1 module instance, u_lc_ctrl_fsm, of lc_ctrl_fsm
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: nothing reduces; only the instance tree is
// checked under getElaborated().
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test: HLC
// leaves vpiPortIndex at 0 for every port, although 37.14 detail 9 says the
// port index gives the port order. TopPortIndexesFollowDeclarationOrder is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - How HLC models a port connection of an unelaborated instance: the
//     connection record carries no name of its own and identifies its formal
//     through a low connection bound to the instantiated module's port. The
//     test follows that shape and asserts the binding.
//   - The values the ports carry only exist while simulation runs.
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - How a RefTypespec refers to a typedef: it may resolve to the
//     TypedefTypespec or to the typespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; lc_ctrl_pkg is looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackedEnumPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedEnumPort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("lc_ctrl_pkg", m_design->getAllPackages());
  }

  static const hldb::Typedef *getTypedef(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, pkg->getTypedefs());
  }

  // A module definition, found by its definition name.
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Variable *getTopVar(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // Whether 'rts' names the type of 'td': it may resolve to the
  // TypedefTypespec or to the typespec the typedef aliases (6.18).
  static bool namesTypedef(const hldb::RefTypespec *rts, const hldb::Typedef *td) {
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *const actual = rts->getActual();
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  }

  // The reference to the package in the first element of a package-scoped
  // type path. HLC records it either as a RefObj or as a RefTypespec whose own
  // path holds that RefObj; which one is a model convention.
  static const hldb::RefObj *getScopePackageRef(const hldb::Any *elem) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem)) return ref;
    const hldb::RefTypespec *const rts = any_cast<hldb::RefTypespec>(elem);
    if (rts == nullptr || rts->getPathElems() == nullptr || rts->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(rts->getPathElems()->at(0));
  }

  // Verifies 'range' is [left:right] with Constant bounds.
  static void ExpectConstRange(const hldb::Range *range, std::string_view left, std::string_view right) {
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(r->getDecompile(), right);
  }
};

// ---------------------------------------------------------------------------
// package lc_ctrl_pkg; ... endpackage : lc_ctrl_pkg
// ---------------------------------------------------------------------------

TEST_F(PackedEnumPortTest, PackageHasEndLabelAndTwoTypedefs) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'lc_ctrl_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "lc_ctrl_pkg");
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 2u) << "'lc_tx_e' and 'lc_tx_t'";
}

TEST_F(PackedEnumPortTest, LcTxEIsLogic3To0EnumWithOn) {
  const hldb::Typedef *const td = getTypedef("lc_tx_e");
  ASSERT_NE(td, nullptr) << "typedef 'lc_tx_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'lc_tx_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic [3:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ExpectConstRange(base->getRanges()->at(0), "3", "0");
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 1u);
  const hldb::EnumConst *const on = et->getEnum()->getEnumConsts()->at(0);
  ASSERT_NE(on, nullptr);
  EXPECT_EQ(on->getName(), "On");
  const hldb::Constant *const value = on->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "4'b1010");
}

TEST_F(PackedEnumPortTest, LcTxTIsTypedefOfLcTxE) {
  const hldb::Typedef *const td = getTypedef("lc_tx_t");
  ASSERT_NE(td, nullptr) << "typedef 'lc_tx_t' not found";
  EXPECT_TRUE(namesTypedef(td->getAlias(), getTypedef("lc_tx_e"))) << "6.18: 'lc_tx_t' names the type lc_tx_e";
}

// ---------------------------------------------------------------------------
// module lc_ctrl_fsm(input logic lc_clk_byp_ack_i);
// ---------------------------------------------------------------------------

TEST_F(PackedEnumPortTest, FsmHasOneInputNetPort) {
  const hldb::Module *const m = getModule("lc_ctrl_fsm");
  ASSERT_NE(m, nullptr) << "module 'lc_ctrl_fsm' not found";
  EXPECT_EQ(m->getEndLabel(), "lc_ctrl_fsm");
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u) << "the commented-out port declaration is not a port";
  const hldb::Port *const port = m->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "lc_clk_byp_ack_i");
  EXPECT_EQ(port->getDirection(), vpiInput);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("lc_clk_byp_ack_i", m->getNets());
  ASSERT_NE(net, nullptr) << "23.2.2.3: an input port with no port kind is a net";
  ASSERT_NE(net->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "the port is declared 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'logic' with no dimension is one bit";
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), net) << "37.14 detail 4: the port connects to its own net";
}

// ---------------------------------------------------------------------------
// module top(output int o, output int p);
// ---------------------------------------------------------------------------

TEST_F(PackedEnumPortTest, TopHasTwoOutputIntVariablePorts) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_EQ(top->getEndLabel(), "top");
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const char *const names[] = {"o", "p"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const port = top->getPorts()->at(i);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), names[i]) << "port " << i << ", in source order";
    EXPECT_EQ(port->getDirection(), vpiOutput);
    const hldb::Variable *const v = getTopVar(names[i]);
    ASSERT_NE(v, nullptr) << "23.2.2.3: an output port declared 'int' is a variable";
    ASSERT_NE(v->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = v->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "'" << names[i] << "' is declared 'int'";
    EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
    const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr);
    EXPECT_EQ(low->getActual(), v) << "37.14 detail 4: the port connects to its own variable";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (port index
// not set) in the file header.
TEST_F(PackedEnumPortTest, TopPortIndexesFollowDeclarationOrder) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_NE(top->getPorts()->at(i), nullptr);
    EXPECT_EQ(top->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: vpiPortIndex gives the port order, and the first port has index 0";
  }
}

TEST_F(PackedEnumPortTest, TopHasThreeVariables) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u) << "'o', 'p' and 'lc_clk_byp_ack'";
  EXPECT_NE(getTopVar("lc_clk_byp_ack"), nullptr);
}

TEST_F(PackedEnumPortTest, LcClkBypAckIsPackedArrayOfScopedLcTxT) {
  const hldb::Variable *const v = getTopVar("lc_clk_byp_ack");
  ASSERT_NE(v, nullptr) << "variable 'lc_clk_byp_ack' not found";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'lc_ctrl_pkg::lc_tx_t [0:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[0:0]' is written before the name, so it is packed";
  ExpectConstRange(at->getRange(), "0", "0");
  const hldb::RefTypespec *const elem = at->getElemTypespec();
  ASSERT_NE(elem, nullptr);
  ASSERT_NE(elem->getPathElems(), nullptr) << "'lc_ctrl_pkg::lc_tx_t' should be a package-scoped path";
  ASSERT_EQ(elem->getPathElems()->size(), 2u) << "the package, then the type";
  const hldb::RefObj *const pkgRef = getScopePackageRef(elem->getPathElems()->at(0));
  ASSERT_NE(pkgRef, nullptr) << "the path starts with the package";
  EXPECT_EQ(pkgRef->getName(), "lc_ctrl_pkg");
  EXPECT_EQ(pkgRef->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names lc_ctrl_pkg";
  const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(elem->getPathElems()->at(1));
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->getName(), "lc_tx_t");
  EXPECT_TRUE(namesTypedef(last, getTypedef("lc_tx_t")))
      << "26.3: 'lc_tx_t' is declared in lc_ctrl_pkg, so the scoped type name must resolve to it";
}

TEST_F(PackedEnumPortTest, TopInstantiatesFsmConnectedToElementZero) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u) << "'u_lc_ctrl_fsm' is the only instance";
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "u_lc_ctrl_fsm");
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "23.3.2: the instance names a module";
  EXPECT_EQ(mt->getModule(), getModule("lc_ctrl_fsm"));
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 1u);
  const hldb::Port *const port = any_cast<hldb::Port>(inst->getPorts()->at(0));
  ASSERT_NE(port, nullptr);
  // The connection identifies its formal through its low connection, a
  // reference to the port of lc_ctrl_fsm that '.lc_clk_byp_ack_i' names.
  const hldb::RefObj *const formal = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(formal, nullptr) << "23.3.2.2: '.lc_clk_byp_ack_i(...)' names the port it connects";
  EXPECT_EQ(formal->getName(), "lc_clk_byp_ack_i");
  const hldb::Module *const fsm = getModule("lc_ctrl_fsm");
  ASSERT_NE(fsm, nullptr);
  ASSERT_NE(fsm->getPorts(), nullptr);
  ASSERT_FALSE(fsm->getPorts()->empty());
  EXPECT_EQ(formal->getActual(), fsm->getPorts()->at(0)) << "23.3.2.2: the connection is to lc_ctrl_fsm's port";
  const hldb::BitSelect *const sel = port->getHighConn<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr) << "37.14 detail 3: the high connection is 'lc_clk_byp_ack[0]'";
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "lc_clk_byp_ack");
  EXPECT_EQ(prefix->getActual(), getTopVar("lc_clk_byp_ack")) << "'lc_clk_byp_ack' is top's variable";
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// Commented-out module, elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackedEnumPortTest, CommentedOutModuleDoesNotExist) {
  EXPECT_EQ(getModule("prim_lc_sync"), nullptr) << "5.4: module 'prim_lc_sync' is inside a block comment";
}

TEST_F(PackedEnumPortTest, ElaboratedTopInstantiatesFsm) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: lc_ctrl_fsm is instantiated, top is not";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getName(), "top");
    ASSERT_NE(top->getModules(), nullptr);
    ASSERT_EQ(top->getModules()->size(), 1u);
    EXPECT_EQ(top->getModules()->at(0)->getName(), "u_lc_ctrl_fsm");
    EXPECT_EQ(top->getModules()->at(0)->getDefName(), "lc_ctrl_fsm");
  }
}

TEST_F(PackedEnumPortTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal: an enum converts to logic without a cast (6.22.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
