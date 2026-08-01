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

// Validates the UHDM graph produced for tests/NetsAndVariables/NetsAndVariablesAnsi.sv,
// an ANSI-style exercise of net and variable declarations across scopes:
//   - module nets_and_variables_test: every standard net keyword (wire, tri,
//     tri0, tri1, wand, wor, triand, trior, supply0, supply1, uwire), packed
//     net buses, every standard variable keyword (logic, reg, bit, int,
//     integer, shortint, longint, byte, time, real, realtime, string,
//     chandle, event, enum, packed vectors), implicit nets created by a
//     continuous assignment to an undeclared identifier, "implicit variable"
//     attempts created by a procedural assignment to an undeclared
//     identifier, and always_comb/always_ff/always_latch/always/initial
//     processes
//   - package, interface, checker and class declarations at file scope
//   - a second module instantiating the interface and the class
//
// Checked (see per-TEST_F comments for the exact item):
//   - work@nets_and_variables_test exists with ports clk, a, b (input) and y (output)
//   - every explicitly-declared net/variable in the module is present, with
//     net-type / typespec assertions wherever an existing test in this suite
//     already establishes the pattern (wire -> vpiWire, tri1 -> vpiTri1,
//     int -> IntTypespec, real -> RealTypespec, string -> StringTypespec,
//     event -> EventTypespec, enum -> EnumTypespec, bit -> BitTypespec,
//     logic/reg (+ vectors) -> LogicTypespec, integer -> IntegerTypespec,
//     longint -> LongIntTypespec, byte -> ByteTypespec, time -> TimeTypespec)
//   - realtime and chandle have a RefTypespec node whose vpiActual is
//     unresolved (matches 6.12--realtime / 6.14--chandle in this suite)
//   - tri, tri0, wand, wor, triand, trior, supply0, supply1, uwire and
//     shortint are checked for existence only: no test anywhere in this
//     suite exercises getNetType() for those net kinds, or a dedicated
//     typespec for shortint, so no exact-value assertion is made for them
//   - implicit_net_a / implicit_net_b (undeclared identifiers used as the
//     LHS of a continuous assignment, with plain 'wire' in effect) ARE
//     materialized as real vpiWire nets -- per IEEE 1800 clause 6.10, an
//     implicit net is only an error under `default_nettype none; otherwise
//     it is a legal implicit declaration of the current default net type
//   - continuous assignments and processes are counted
//   - the file-scope interface's modport and the file-scope checker's ports
//     and variables are checked using the patterns established by the
//     InterfaceIdentifiers and CheckerDecl test suites
//   - the second module's interface instantiation is checked
//   - the file-scope package and class are NOT checked structurally: no test
//     anywhere in this suite demonstrates a design-level (non-nested)
//     hldb::Package or hldb::ClassDefn lookup, so those are left as
//     documented GTEST_SKIP placeholders rather than guessed API calls

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/assignment.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/event_control.h>
#include <hldb/event_typespec.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/property_decl.h>
#include <hldb/range.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/time_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NetsAndVariablesAnsi : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetsAndVariablesAnsi.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_test", m_design->getAllModules());
  }

  static const hldb::Module *getSecond() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_second", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// Module + ports
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(NetsAndVariablesAnsi, FourPortsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(top->getPorts()->size(), 4u) << "expected ports clk, a, b, y";
}

TEST_F(NetsAndVariablesAnsi, PortDirections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);

  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", top->getPorts());
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", top->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", top->getPorts());
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", top->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(y, nullptr);

  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
}

// ---------------------------------------------------------------------------
// Net keyword coverage: wire, tri, tri0, tri1, wand, wor, triand, trior,
// supply0, supply1, uwire
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, W0IsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w0 = hldb::findByName<hldb::Net>("w0", top->getNets());
  ASSERT_NE(w0, nullptr);
  EXPECT_EQ(w0->getNetType(), vpiWire);
}

TEST_F(NetsAndVariablesAnsi, T1zIsTri1) {
  // tri1 is the only non-wire net kind with an established getNetType() precedent
  // in this suite (6.9.2--vector_scalared / 6.9.2--vector_vectored).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const t1z = hldb::findByName<hldb::Net>("t1z", top->getNets());
  ASSERT_NE(t1z, nullptr);
  EXPECT_EQ(t1z->getNetType(), vpiTri1);
}

TEST_F(NetsAndVariablesAnsi, RemainingNetKindsExist) {
  // tri, tri0, wand, wor, triand, trior, supply0, supply1, uwire: no test in
  // this suite exercises getNetType() for these, so only existence is checked.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  EXPECT_NE(hldb::findByName<hldb::Net>("t0", top->getNets()), nullptr) << "net 't0' (tri) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("t0z", top->getNets()), nullptr) << "net 't0z' (tri0) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("wand_net", top->getNets()), nullptr) << "net 'wand_net' (wand) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("wor_net", top->getNets()), nullptr) << "net 'wor_net' (wor) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("triand_net", top->getNets()), nullptr)
      << "net 'triand_net' (triand) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("trior_net", top->getNets()), nullptr) << "net 'trior_net' (trior) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("supply0_net", top->getNets()), nullptr)
      << "net 'supply0_net' (supply0) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("supply1_net", top->getNets()), nullptr)
      << "net 'supply1_net' (supply1) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("uwire_net", top->getNets()), nullptr) << "net 'uwire_net' (uwire) not found";
}

// ---------------------------------------------------------------------------
// Net buses -- w_bus (wire [7:0]) and tri_bus (tri [3:0])
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, WBusIsVectorSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const wBus = hldb::findByName<hldb::Net>("w_bus", top->getNets());
  ASSERT_NE(wBus, nullptr);
  EXPECT_EQ(wBus->getNetType(), vpiWire);

  const hldb::RefTypespec *const rts = wBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(NetsAndVariablesAnsi, TriBusIsVectorThreeToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const triBus = hldb::findByName<hldb::Net>("tri_bus", top->getNets());
  ASSERT_NE(triBus, nullptr);

  const hldb::RefTypespec *const rts = triBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// var_logic / var_reg -- plain logic/reg declarations
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, VarLogicIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_logic", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getNetType(), 0) << "logic keyword does not set vpiNetType";
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarRegIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_reg", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr) << "reg is an alias of logic";
}

// ---------------------------------------------------------------------------
// Explicit "implicit_wire" / "implicit_logic" -- these are formally declared
// despite their names; only the true implicit identifiers below are not.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, ImplicitWireIsFormallyDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("implicit_wire", top->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
}

TEST_F(NetsAndVariablesAnsi, ImplicitLogicIsFormallyDeclaredVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const l = hldb::findByName<hldb::Net>("implicit_logic", top->getNets());
  ASSERT_NE(l, nullptr);
  const hldb::RefTypespec *const rts = l->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// True implicit nets -- implicit_net_a / implicit_net_b are only ever used as
// the LHS (or RHS) of a continuous assignment and were never declared. Per
// 6.10--implicit_continuous_assignment, HLC does not materialize a Net node
// for these; the ContAssign LHS RefObj has no vpiActual.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, ImplicitNetAIsDeclaredWire) {
  // Per IEEE 1800 clause 6.10: with no `default_nettype override in this
  // file (plain 'wire' applies), an undeclared identifier used as a
  // continuous-assignment LHS is a legally-implicit wire net, not an error.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_a", top->getNets());
  ASSERT_NE(net, nullptr) << "'implicit_net_a' is a legally-implicit wire net and should be materialized";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(NetsAndVariablesAnsi, ImplicitNetBIsDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_b", top->getNets());
  ASSERT_NE(net, nullptr) << "'implicit_net_b' is a legally-implicit wire net and should be materialized";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Numeric / misc variable keyword coverage
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, VarBitIsBitTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_bit", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarIntIsIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_int", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarIntegerIsIntegerTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_integer", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarShortintExists) {
  // No dedicated typespec class for shortint has been found anywhere in this
  // suite, so only existence is checked.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("var_shortint", top->getNets()), nullptr)
      << "'var_shortint' not found among the module's nets";
}

TEST_F(NetsAndVariablesAnsi, VarLongintIsLongIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_longint", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LongIntTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarByteIsByteTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_byte", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::ByteTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarTimeIsTimeTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_time", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::TimeTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarRealIsRealTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_real", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarRealtimeTypespecActualIsNull) {
  // Matches 6.12--realtime: realtime has a RefTypespec node but vpiActual is
  // unresolved in this compiler version.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_realtime", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarStringIsStringTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_string", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarChandleTypespecActualIsNull) {
  // Matches 6.14--chandle: chandle has a RefTypespec node but vpiActual is
  // unresolved in this compiler version.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_chandle", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarEventIsEventTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_event", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EventTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// var_enum -- enum logic [1:0] {IDLE, BUSY} var_enum
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, VarEnumIsEnumTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_enum", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesAnsi, VarEnumHasIdleAndBusyConsts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_enum", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::EnumTypespec *const enumTs = rts->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  ASSERT_EQ(enumTs->getEnumConsts()->size(), 2u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "IDLE");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "BUSY");
}

TEST_F(NetsAndVariablesAnsi, VarEnumHasExplicitBaseTypespec) {
  // Contrast with 6.19--enum_anon: our enum has an explicit "logic [1:0]"
  // base type, so getBaseTypespec() should be set (unlike the anonymous case).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_enum", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::EnumTypespec *const enumTs = rts->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  EXPECT_NE(enumTs->getBaseTypespec(), nullptr) << "enum logic [1:0] {...} has an explicit base type";
}

// ---------------------------------------------------------------------------
// var_vector / var_reg_vector -- packed vectors of logic/reg
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, VarVectorIsThreeToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_vector", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(NetsAndVariablesAnsi, VarRegVectorIsSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_reg_vector", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// Aggregate net count -- 4 ports + 32 body declarations = 36. This is a
// coarse sanity check in addition to the individual existence checks above.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, TotalNetCount) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 36u);
}

// ---------------------------------------------------------------------------
// Continuous assignments -- 6 total, in source order:
//   implicit_wire = a | b; implicit_net_a = a & b;
//   implicit_net_b = implicit_net_a | a; w0 = a & b; w_bus[0] = a;
//   uwire_net = a ^ b
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, SixContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 6u);
}

TEST_F(NetsAndVariablesAnsi, FirstContAssignDrivesImplicitWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_wire");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'implicit_wire' is formally declared";
}

TEST_F(NetsAndVariablesAnsi, SecondContAssignDrivesImplicitNetA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(1)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_a");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'implicit_net_a' is a legally-implicit wire net";
}

TEST_F(NetsAndVariablesAnsi, LastContAssignDrivesUwireNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "uwire_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

// ---------------------------------------------------------------------------
// Processes -- always_comb, always_ff, always_latch, initial, always = 5
// total.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, FiveProcessesExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 5u);
}

TEST_F(NetsAndVariablesAnsi, ProcessTypeCounts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  int initialCount = 0;
  int alwaysCount = 0;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (any_cast<hldb::Initial>(p) != nullptr) initialCount++;
    if (any_cast<hldb::Always>(p) != nullptr) alwaysCount++;
  }
  EXPECT_EQ(initialCount, 1) << "one initial block (var_real/var_string)";
  EXPECT_EQ(alwaysCount, 4) << "always_comb, always_ff, always_latch, always";
}

TEST_F(NetsAndVariablesAnsi, PlainAlwaysDrivesYFromVarLogic) {
  // Identify the final "always @(posedge clk) y <= var_logic;" block
  // structurally (by its Assignment LHS), rather than assuming process order.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Always *found = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    const hldb::Always *const a = any_cast<hldb::Always>(p);
    if (a == nullptr) continue;
    const hldb::EventControl *const ec = a->getStmt<hldb::EventControl>();
    if (ec == nullptr) continue;
    const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
    if (assign == nullptr) continue;
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    if (lhs != nullptr && lhs->getName() == "y") {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr) << "could not find 'always @(posedge clk) y <= var_logic;'";
  EXPECT_EQ(found->getAlwaysType(), vpiAlways);
}

// ---------------------------------------------------------------------------
// Package -- no test anywhere in this suite demonstrates a design-level
// hldb::Package lookup; left as a documented placeholder rather than a
// guessed API call.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, PackageDeclarationNotStructurallyChecked) {
  GTEST_SKIP() << "No hldb::Package (or equivalent design-level lookup) is exercised anywhere in this test "
                  "suite; nets_and_variables_pkg is left unchecked rather than guessing an unverified API.";
}

// ---------------------------------------------------------------------------
// Interface -- nets_and_variables_if with a single modport "mp"
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, InterfaceExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, InterfaceHasOneModport) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp");
}

TEST_F(NetsAndVariablesAnsi, InterfaceHasOneContAssign) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  EXPECT_EQ(iface->getContAssigns()->size(), 1u);
}

TEST_F(NetsAndVariablesAnsi, InterfaceContAssignDrivesImplicitNet) {
  // Per IEEE 1800 clause 6.10, 'if_implicit_net' (no `default_nettype
  // override, plain 'wire' applies) is a legally-implicit wire net.
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_FALSE(iface->getContAssigns()->empty());
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "if_implicit_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'if_implicit_net' is a legally-implicit wire net";
}

// ---------------------------------------------------------------------------
// Modport -- nets_and_variables_modport_if with a single modport "mp_basic"
// exposing a net port, a variable port, and an implicit net port
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, ModportInterfaceExists) {
  EXPECT_NE(hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if", m_design->getAllInterfaces()),
            nullptr);
}

TEST_F(NetsAndVariablesAnsi, ModportInterfaceHasNetAndVariable) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("mp_net", iface->getNets()), nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("mp_var", iface->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, ModportInterfaceContAssignDrivesImplicitNet) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_EQ(iface->getContAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "mp_implicit");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'mp_implicit' is a legally-implicit wire net";
}

TEST_F(NetsAndVariablesAnsi, ModportHasThreeIODecls) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  const hldb::Modport *const mp = iface->getModports()->at(0);
  ASSERT_NE(mp, nullptr);
  EXPECT_EQ(mp->getName(), "mp_basic");
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 3u);

  const hldb::IODecl *const netPort = hldb::findByName<hldb::IODecl>("mp_net", mp->getIODecls());
  ASSERT_NE(netPort, nullptr);
  EXPECT_EQ(netPort->getDirection(), vpiInput);

  const hldb::IODecl *const varPort = hldb::findByName<hldb::IODecl>("mp_var", mp->getIODecls());
  ASSERT_NE(varPort, nullptr);
  EXPECT_EQ(varPort->getDirection(), vpiOutput);

  const hldb::IODecl *const implicitPort = hldb::findByName<hldb::IODecl>("mp_implicit", mp->getIODecls());
  ASSERT_NE(implicitPort, nullptr);
  EXPECT_EQ(implicitPort->getDirection(), vpiInput);
}

// ---------------------------------------------------------------------------
// Checker -- nets_and_variables_checker(clk, a)
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, CheckerExists) {
  ASSERT_NE(m_design->getCheckerDecls(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls()),
            nullptr);
}

TEST_F(NetsAndVariablesAnsi, CheckerHasTwoInputPorts) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getPorts(), nullptr);
  ASSERT_EQ(checker->getPorts()->size(), 2u);

  const hldb::CheckerPort *const clk = hldb::findByName<hldb::CheckerPort>("clk", checker->getPorts());
  const hldb::CheckerPort *const a = hldb::findByName<hldb::CheckerPort>("a", checker->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
}

TEST_F(NetsAndVariablesAnsi, CheckerHasLogicVariables) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_logic", checker->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_vec", checker->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, CheckerHasRandVariables) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);

  const hldb::Variable *const ckRand = hldb::findByName<hldb::Variable>("ck_rand", checker->getVariables());
  ASSERT_NE(ckRand, nullptr);
  EXPECT_EQ(ckRand->getRandType(), vpiRand);

  const hldb::Variable *const ckRandc = hldb::findByName<hldb::Variable>("ck_randc", checker->getVariables());
  ASSERT_NE(ckRandc, nullptr);
  EXPECT_EQ(ckRandc->getRandType(), vpiRandC);
}

TEST_F(NetsAndVariablesAnsi, CheckerHasOneProcess) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getProcesses(), nullptr);
  ASSERT_EQ(checker->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(checker->getProcesses()->at(0)), nullptr)
      << "'always_ff @(posedge clk)' should be modeled as an Always process";
}

// ---------------------------------------------------------------------------
// Class -- nets_and_variables_class(file scope)
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, ClassExists) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, ClassHasVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses());
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_logic", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_reg", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_bits", cls->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, ClassHasRandVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses());
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);

  const hldb::Variable *const clsRand = hldb::findByName<hldb::Variable>("cls_rand", cls->getVariables());
  ASSERT_NE(clsRand, nullptr);
  EXPECT_EQ(clsRand->getRandType(), vpiRand);

  const hldb::Variable *const clsRandc = hldb::findByName<hldb::Variable>("cls_randc", cls->getVariables());
  ASSERT_NE(clsRandc, nullptr);
  EXPECT_EQ(clsRandc->getRandType(), vpiRandC);
}

// ---------------------------------------------------------------------------
// Second module -- instantiates the interface and a class-handle variable
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, SecondModuleExists) { ASSERT_NE(getSecond(), nullptr); }

TEST_F(NetsAndVariablesAnsi, SecondModuleInstantiatesInterface) {
  const hldb::Module *const second = getSecond();
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getRefInstances(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("if0", second->getRefInstances()), nullptr)
      << "interface instance 'if0' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("mpif0", second->getRefInstances()), nullptr)
      << "modport interface instance 'mpif0' not found";
}

TEST_F(NetsAndVariablesAnsi, SecondModuleHasCls0Variable) {
  const hldb::Module *const second = getSecond();
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls0", second->getVariables()), nullptr)
      << "class-handle variable 'cls0' not found";
}

// ---------------------------------------------------------------------------
// Assertion -- nets_and_variables_assertion(clk, a, b)
//
// Not checked: the internal PropertySpec/ClockedProperty expression tree of
// the property body; no test anywhere in this codebase exercises a named
// property declaration with a local variable, so that deeper shape is left
// undocumented rather than guessed.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesAnsi, AssertionModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("work@nets_and_variables_assertion", m_design->getAllModules()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, AssertionModuleHasThreeInputPorts) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  ASSERT_EQ(mod->getPorts()->size(), 3u);

  const char *const names[3] = {"clk", "a", "b"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(names[i], mod->getPorts());
    ASSERT_NE(port, nullptr) << "port " << names[i];
    EXPECT_EQ(port->getDirection(), vpiInput);
  }
}

TEST_F(NetsAndVariablesAnsi, AssertionModuleHasOnePropertyDecl) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPropertyDecls(), nullptr);
  EXPECT_EQ(mod->getPropertyDecls()->size(), 1u);
}

TEST_F(NetsAndVariablesAnsi, AssertionPropertyDeclHasLocalVariable) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPropertyDecls(), nullptr);
  ASSERT_EQ(mod->getPropertyDecls()->size(), 1u);
  const hldb::PropertyDecl *const prop = mod->getPropertyDecls()->at(0);
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("local_val", prop->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesAnsi, AssertionModuleHasOneConcurrentAssertion) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(mod->getConcurrentAssertions()->size(), 1u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
