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

// Validates the HLDB model built for tests/PackedArrayHighConn/dut.sv. With
// the block comments removed, the design is:
//
//   package prim_pad_wrapper_pkg;
//     typedef enum logic [2:0] {
//       BidirStd = 3'h0
//     } pad_type_e;
//     typedef logic [7:0] pad_pok_t;
//   endpackage : prim_pad_wrapper_pkg
//
//   module prim_pad_wrapper
//     import prim_pad_wrapper_pkg::*; (input pad_pok_t pok_i);
//   endmodule : prim_pad_wrapper
//
//   module dut
//     import prim_pad_wrapper_pkg::*; #(parameter logic [0:0][1:0] DioPadBank = '0) ();
//     pad_pok_t [3:0] pad_pok;
//     prim_pad_wrapper u_dio_pad (
//       .pok_i      ( pad_pok[DioPadBank[0]]   )
//     );
//   endmodule : dut
//
// Three further designs -- a second 'module dut', a 'module top' and a
// package 'pack_pkg' with another 'module dut' -- are inside /* ... */ block
// comments, so they do not exist (5.4).
//
// The point of the fixture is the high connection of an instance port
// (IEEE 1800-2023 37.14 detail 3): the port pok_i of u_dio_pad is connected
// by name (23.3.2.2) to an element of a packed array of a typedef imported
// in the module header (26.4), selected by an element of a parameter. The
// regression this file exists to catch is HLC losing the high connection or
// the bindings inside it.
//
// What is checked, and why:
//   Package prim_pad_wrapper_pkg (26.2)
//     - end label "prim_pad_wrapper_pkg"; exactly 2 typedefs
//     - pad_type_e: an enum (6.19) with base type logic [2:0] and the single
//       name BidirStd with the value Constant "3'h0"
//     - pad_pok_t: an alias of logic [7:0]
//   Module prim_pad_wrapper
//     - end label "prim_pad_wrapper"; exactly 1 port, pok_i, an input. An
//       input port with no port kind is a net (23.2.2.3): the module's Net
//       pok_i is the port's low connection
//     - pok_i's type pad_pok_t resolves through the header import (26.4) to
//       the package's typedef
//   Module dut
//     - end label "dut"
//     - exactly 1 parameter, DioPadBank, declared in the parameter port list
//       so not local (6.20.1), of type logic [0:0][1:0]: a LogicTypespec
//       with exactly 2 packed ranges, [0:0] then [1:0]; its default is the
//       unbased unsized literal '0 (5.7.1)
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - exactly 1 variable, pad_pok, a packed array (7.4.1) with the single
//       packed dimension [3:0] whose element type is pad_pok_t
//     - exactly 1 instance, u_dio_pad, of the module prim_pad_wrapper
//   .pok_i ( pad_pok[DioPadBank[0]] ) (23.3.2.2)
//     - the instance connects exactly 1 port, pok_i, whose high connection is
//       a bit-select (11.5.1) with the prefix bound to dut's variable pad_pok
//       and the index another bit-select, with the prefix bound to the
//       parameter DioPadBank and the index Constant "0"
//   Elaboration (23.3.1)
//     - prim_pad_wrapper is instantiated, so dut is the only top-level
//       instance; it holds exactly 1 module instance, u_dio_pad, of the
//       module prim_pad_wrapper, whose port pok_i has a high connection
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: DioPadBank[0] is a constant select of a
// parameter, so after elaboration it is the index 0; that the elaborated
// port keeps a high connection is checked under getElaborated().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module dut ... ();' with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. DutHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - How HLC models a port connection of an unelaborated instance: the
//     connection record carries no name of its own and identifies its formal
//     through a low connection bound to the instantiated module's port. The
//     test follows that shape and asserts the binding.
//   - The value pok_i receives only exists while simulation runs.
//   - The name HLC gives a parameterized module definition: modules are
//     looked up by their definition name.
//   - Where HLC records the header imports is a model convention. Their
//     effect is asserted through the resolution of pad_pok_t.
//   - How a RefTypespec refers to pad_pok_t: it may resolve to the
//     TypedefTypespec or to the LogicTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
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

class PackedArrayHighConnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayHighConn.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_pad_wrapper_pkg", m_design->getAllPackages());
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

  static const hldb::RefInstance *getUDioPad() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_dio_pad", dut->getRefInstances());
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
// package prim_pad_wrapper_pkg; ... endpackage : prim_pad_wrapper_pkg
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHighConnTest, PackageHasEndLabelAndTwoTypedefs) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'prim_pad_wrapper_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "prim_pad_wrapper_pkg");
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 2u) << "'pad_type_e' and 'pad_pok_t'";
}

TEST_F(PackedArrayHighConnTest, PadTypeEIsLogic2To0EnumWithBidirStd) {
  const hldb::Typedef *const td = getTypedef("pad_type_e");
  ASSERT_NE(td, nullptr) << "typedef 'pad_type_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'pad_type_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic [2:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ExpectConstRange(base->getRanges()->at(0), "2", "0");
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 1u);
  const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "BidirStd");
  const hldb::Constant *const value = ec->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "3'h0");
}

TEST_F(PackedArrayHighConnTest, PadPokTIsLogic7To0) {
  const hldb::Typedef *const td = getTypedef("pad_pok_t");
  ASSERT_NE(td, nullptr) << "typedef 'pad_pok_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = td->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.18: 'pad_pok_t' names 'logic [7:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "7", "0");
}

// ---------------------------------------------------------------------------
// module prim_pad_wrapper import prim_pad_wrapper_pkg::*; (input pad_pok_t pok_i);
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHighConnTest, PadWrapperHasInputNetPortPokI) {
  const hldb::Module *const m = getModule("prim_pad_wrapper");
  ASSERT_NE(m, nullptr) << "module 'prim_pad_wrapper' not found";
  EXPECT_EQ(m->getEndLabel(), "prim_pad_wrapper");
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const port = m->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "pok_i");
  EXPECT_EQ(port->getDirection(), vpiInput);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("pok_i", m->getNets());
  ASSERT_NE(net, nullptr) << "23.2.2.3: an input port with no port kind is a net";
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), net) << "37.14 detail 4: the port connects to its own net";
}

TEST_F(PackedArrayHighConnTest, PokIIsTypedByImportedPadPokT) {
  const hldb::Module *const m = getModule("prim_pad_wrapper");
  ASSERT_NE(m, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("pok_i", m->getNets());
  ASSERT_NE(net, nullptr);
  EXPECT_TRUE(namesTypedef(net->getTypespec(), getTypedef("pad_pok_t")))
      << "26.4: the header import makes the package's pad_pok_t visible to the port list";
}

// ---------------------------------------------------------------------------
// module dut import prim_pad_wrapper_pkg::*; #(...) ();
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHighConnTest, DutHasEndLabelAndOverridableDioPadBank) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  EXPECT_EQ(dut->getEndLabel(), "dut");
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("DioPadBank", dut->getParameters());
  ASSERT_NE(p, nullptr) << "parameter 'DioPadBank' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: a parameter in the parameter port list can be overridden";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'DioPadBank' is declared 'logic [0:0][1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "two packed dimensions";
  ExpectConstRange(lt->getRanges()->at(0), "0", "0");
  ExpectConstRange(lt->getRanges()->at(1), "1", "0");
}

TEST_F(PackedArrayHighConnTest, DioPadBankDefaultsToUnbasedUnsizedZero) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParamAssigns(), nullptr);
  ASSERT_EQ(dut->getParamAssigns()->size(), 1u);
  const hldb::Constant *const def = dut->getParamAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(def, nullptr) << "'DioPadBank' defaults to a literal";
  EXPECT_EQ(def->getDecompile(), "'0") << "5.7.1: '0 is the unbased unsized literal that sets all bits to 0";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(PackedArrayHighConnTest, DutHasOneNullPort) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(dut->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  const hldb::Port *const port = dut->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
}

TEST_F(PackedArrayHighConnTest, PadPokIsPackedArray3To0OfPadPokT) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getVariables(), nullptr);
  ASSERT_EQ(dut->getVariables()->size(), 1u) << "'pad_pok' is the only variable";
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("pad_pok", dut->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'pad_pok_t [3:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[3:0]' is written before the name, so it is packed";
  ExpectConstRange(at->getRange(), "3", "0");
  EXPECT_TRUE(namesTypedef(at->getElemTypespec(), getTypedef("pad_pok_t"))) << "the element type is pad_pok_t";
}

TEST_F(PackedArrayHighConnTest, DutInstantiatesPadWrapperOnce) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getRefInstances(), nullptr);
  ASSERT_EQ(dut->getRefInstances()->size(), 1u) << "'u_dio_pad' is the only instance";
  const hldb::RefInstance *const inst = getUDioPad();
  ASSERT_NE(inst, nullptr) << "instance 'u_dio_pad' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "23.3.2: 'prim_pad_wrapper u_dio_pad (...)' instantiates a module";
  EXPECT_EQ(mt->getModule(), getModule("prim_pad_wrapper"));
}

// ---------------------------------------------------------------------------
// .pok_i ( pad_pok[DioPadBank[0]] )
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHighConnTest, PokIHighConnSelectsPadPokByDioPadBankElement) {
  const hldb::RefInstance *const inst = getUDioPad();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 1u) << "the instance connects exactly one port";
  const hldb::Port *const port = any_cast<hldb::Port>(inst->getPorts()->at(0));
  ASSERT_NE(port, nullptr);
  // The connection identifies its formal through its low connection, a
  // reference to the port of prim_pad_wrapper that '.pok_i' names.
  const hldb::RefObj *const formal = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(formal, nullptr) << "23.3.2.2: '.pok_i(...)' names the port it connects";
  EXPECT_EQ(formal->getName(), "pok_i");
  const hldb::Module *const wrapper = getModule("prim_pad_wrapper");
  ASSERT_NE(wrapper, nullptr);
  ASSERT_NE(wrapper->getPorts(), nullptr);
  ASSERT_FALSE(wrapper->getPorts()->empty());
  EXPECT_EQ(formal->getActual(), wrapper->getPorts()->at(0))
      << "23.3.2.2: the connection is to prim_pad_wrapper's pok_i";
  const hldb::BitSelect *const outer = port->getHighConn<hldb::BitSelect>();
  ASSERT_NE(outer, nullptr) << "37.14 detail 3: the high connection is 'pad_pok[DioPadBank[0]]'";
  const hldb::RefObj *const padPok = outer->getPrefix<hldb::RefObj>();
  ASSERT_NE(padPok, nullptr);
  EXPECT_EQ(padPok->getName(), "pad_pok");
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  EXPECT_EQ(padPok->getActual(), hldb::findByName<hldb::Variable>("pad_pok", dut->getVariables()))
      << "'pad_pok' is dut's variable";
  const hldb::BitSelect *const inner = outer->getIndex<hldb::BitSelect>();
  ASSERT_NE(inner, nullptr) << "the index 'DioPadBank[0]' is itself a bit-select";
  const hldb::RefObj *const bank = inner->getPrefix<hldb::RefObj>();
  ASSERT_NE(bank, nullptr);
  EXPECT_EQ(bank->getName(), "DioPadBank");
  EXPECT_EQ(bank->getActual(), hldb::findByName<hldb::Parameter>("DioPadBank", dut->getParameters()))
      << "'DioPadBank' is dut's parameter";
  const hldb::Constant *const zero = inner->getIndex<hldb::Constant>();
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// Commented-out designs, elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHighConnTest, CommentedOutDesignsDoNotExist) {
  EXPECT_EQ(getModule("top"), nullptr) << "5.4: module 'top' is inside a block comment";
  EXPECT_EQ(hldb::findByName<hldb::Package>("pack_pkg", m_design->getAllPackages()), nullptr)
      << "5.4: package 'pack_pkg' is inside a block comment";
  ASSERT_NE(m_design->getAllModules(), nullptr);
  size_t duts = 0;
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    if (m->getDefName() == "dut") ++duts;
  }
  EXPECT_EQ(duts, 1u) << "5.4: the other two 'module dut' declarations are inside block comments";
}

TEST_F(PackedArrayHighConnTest, ElaboratedDutConnectsUDioPad) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: prim_pad_wrapper is instantiated, dut is not";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getDefName(), "dut");
    ASSERT_NE(top->getModules(), nullptr);
    ASSERT_EQ(top->getModules()->size(), 1u);
    const hldb::Module *const inst = top->getModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "u_dio_pad");
    EXPECT_EQ(inst->getDefName(), "prim_pad_wrapper");
    ASSERT_NE(inst->getPorts(), nullptr);
    ASSERT_EQ(inst->getPorts()->size(), 1u);
    EXPECT_NE(inst->getPorts()->at(0)->getHighConn(), nullptr) << "37.14 detail 3: pok_i is connected";
  }
}

TEST_F(PackedArrayHighConnTest, NoFatalSyntaxOrErrorDiagnostics) {
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
