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

// Validates the UHDM graph produced for tests/NetsAndVariables/NetsAndVariablesNonAnsi.sv,
// a non-ANSI-style exercise of net and variable declarations under
// `default_nettype wire`:
//   - module nets_and_variables_nonansi: non-ANSI ports 'a'/'b' (implicit net
//     inputs) and 'y' (implicit net output), an internal wire, a wire bus, a
//     tri, a logic and a reg, an implicit net created by a continuous
//     assignment to an undeclared identifier, and an always_comb process
//   - program nets_and_variables_program: only bare existence is checked (see
//     note below)
//   - a file-scope package, interface, checker and class, mirroring the ANSI
//     counterpart file
//   - module top_nonansi: instantiates the module, the program, the
//     interface, the checker, and a class-handle variable
//
// Checked (see per-TEST_F comments for the exact item):
//   - work@nets_and_variables_nonansi exists with 3 ports (a, b input; y
//     output), each an implicit wire under `default_nettype wire`, matching
//     the pattern established by 6.10--implicit_port in this suite
//   - w0 (wire), w_bus (wire [3:0]), t0 (tri, existence only -- see the ANSI
//     test file for why no getNetType() assertion is made for plain tri),
//     var_logic / var_reg (logic typespec)
//   - implicit_net_nonansi (undeclared identifier used as the LHS of a
//     continuous assignment, under `default_nettype wire) IS materialized
//     as a real vpiWire Net -- a legal implicit declaration per IEEE 1800
//     clause 6.10, not an error
//   - 4 continuous assignments and 1 always_comb process
//   - program nets_and_variables_program exists via m_design->getAllPrograms();
//     no test anywhere in this suite exercises Program ports/variables/body,
//     so nothing beyond existence is asserted
//   - the file-scope interface's modport and the file-scope checker's ports
//     and variables, using the same patterns as the ANSI test file
//   - the file-scope package and class are left as documented GTEST_SKIP
//     placeholders, for the same reasons as the ANSI test file
//   - top_nonansi instantiates nets_and_variables_nonansi, the program, and
//     the interface (checked via getRefInstances()), and declares a
//     class-handle variable 'cls0' (checked via getVariables())

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/program.h>
#include <hldb/property_decl.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NetsAndVariablesNonAnsi : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetsAndVariablesNonAnsi.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_nonansi", m_design->getAllModules());
  }

  static const hldb::Module *getTopNonansi() {
    return hldb::findByName<hldb::Module>("work@top_nonansi", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// Module + non-ANSI ports
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, ModuleExists) { ASSERT_NE(getMod(), nullptr); }

TEST_F(NetsAndVariablesNonAnsi, ThreePortsExist) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 3u) << "expected ports a, b, y";
}

TEST_F(NetsAndVariablesNonAnsi, PortDirections) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);

  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", mod->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", mod->getPorts());
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", mod->getPorts());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(y, nullptr);

  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
}

TEST_F(NetsAndVariablesNonAnsi, PortsResolveToWireNets) {
  // Matches 6.10--implicit_port: non-ANSI ports with no type keyword default
  // to a net of `default_nettype wire`, for both input and output ports.
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);

  for (const char *const name : {"a", "b", "y"}) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(p, nullptr) << "port '" << name << "' not found";
    const hldb::RefObj *const lc = p->getLowConn<hldb::RefObj>();
    ASSERT_NE(lc, nullptr) << "port '" << name << "' has no lowConn RefObj";
    const hldb::Net *const net = lc->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << "port '" << name << "' lowConn does not resolve to a Net";
    EXPECT_EQ(net->getNetType(), vpiWire) << "port '" << name << "' should default to wire";
  }
}

// ---------------------------------------------------------------------------
// Internal nets -- w0, w_bus, t0
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, W0IsWire) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const w0 = hldb::findByName<hldb::Net>("w0", mod->getNets());
  ASSERT_NE(w0, nullptr);
  EXPECT_EQ(w0->getNetType(), vpiWire);
}

TEST_F(NetsAndVariablesNonAnsi, WBusIsVectorThreeToZero) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const wBus = hldb::findByName<hldb::Net>("w_bus", mod->getNets());
  ASSERT_NE(wBus, nullptr);
  EXPECT_EQ(wBus->getNetType(), vpiWire);

  const hldb::RefTypespec *const rts = wBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(NetsAndVariablesNonAnsi, T0Exists) {
  // Plain 'tri': no test anywhere in this suite exercises getNetType() for
  // it (see the ANSI test file), so only existence is checked here.
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("t0", mod->getNets()), nullptr);
}

// ---------------------------------------------------------------------------
// var_logic / var_reg
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, VarLogicIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_logic", mod->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, VarRegIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("var_reg", mod->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr) << "reg is an alias of logic";
}

// ---------------------------------------------------------------------------
// True implicit net -- implicit_net_nonansi is only ever used as the LHS of
// a continuous assignment and was never declared. Per IEEE 1800 clause
// 6.10, under `default_nettype wire (in effect here) this is a legal
// implicit net declaration, not an error, and a real vpiWire Net node is
// materialized for it.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, ImplicitNetNonansiIsDeclaredWire) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_nonansi", mod->getNets());
  ASSERT_NE(net, nullptr)
      << "'implicit_net_nonansi' is a legally-implicit wire net and should be materialized";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(NetsAndVariablesNonAnsi, ImplicitNetNonansiContAssignHasActual) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_FALSE(mod->getContAssigns()->empty());
  const hldb::RefObj *const lhs = mod->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_nonansi");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'implicit_net_nonansi' is a legally-implicit wire net";
}

// ---------------------------------------------------------------------------
// Continuous assignments -- 4 total, in source order:
//   implicit_net_nonansi = a & b; w0 = a | b; w_bus[0] = a; y = w0
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, FourContAssignsExist) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 4u);
}

TEST_F(NetsAndVariablesNonAnsi, LastContAssignDrivesY) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_FALSE(mod->getContAssigns()->empty());
  const hldb::RefObj *const lhs = mod->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "y");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

// ---------------------------------------------------------------------------
// Processes -- one always_comb block
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, OneProcessExists) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(mod->getProcesses()->at(0)), nullptr)
      << "'always_comb' should be modeled as an Always process";
}

// ---------------------------------------------------------------------------
// Program -- only bare existence is exercised anywhere in this suite
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, ProgramExists) {
  ASSERT_NE(m_design->getAllPrograms(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Program>("work@nets_and_variables_program", m_design->getAllPrograms()), nullptr);
}

// ---------------------------------------------------------------------------
// Package -- left unchecked; see the ANSI test file for rationale
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, PackageDeclarationNotStructurallyChecked) {
  GTEST_SKIP() << "No hldb::Package (or equivalent design-level lookup) is exercised anywhere in this test "
                  "suite; nets_and_variables_pkg_nonansi is left unchecked rather than guessing an unverified API.";
}

// ---------------------------------------------------------------------------
// Interface -- nets_and_variables_if_nonansi with a single modport "mp"
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, InterfaceExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if_nonansi", m_design->getAllInterfaces()),
      nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, InterfaceHasOneModport) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp");
}

TEST_F(NetsAndVariablesNonAnsi, InterfaceHasOneContAssign) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  EXPECT_EQ(iface->getContAssigns()->size(), 1u);
}

TEST_F(NetsAndVariablesNonAnsi, InterfaceContAssignDrivesImplicitNet) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_FALSE(iface->getContAssigns()->empty());
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "if_implicit_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'if_implicit_net' is a legally-implicit wire net";
}

// ---------------------------------------------------------------------------
// Modport -- nets_and_variables_modport_if_nonansi with a single modport
// "mp_basic" exposing a net port, a variable port, and an implicit net port
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, ModportInterfaceExists) {
  EXPECT_NE(
      hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if_nonansi", m_design->getAllInterfaces()),
      nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, ModportInterfaceHasNetAndVariable) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>(
      "work@nets_and_variables_modport_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("mp_net", iface->getNets()), nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("mp_var", iface->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, ModportInterfaceContAssignDrivesImplicitNet) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>(
      "work@nets_and_variables_modport_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_EQ(iface->getContAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "mp_implicit");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'mp_implicit' is a legally-implicit wire net";
}

TEST_F(NetsAndVariablesNonAnsi, ModportHasThreeIODecls) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>(
      "work@nets_and_variables_modport_if_nonansi", m_design->getAllInterfaces());
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
// Checker -- nets_and_variables_checker_nonansi(clk, a)
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, CheckerExists) {
  ASSERT_NE(m_design->getCheckerDecls(), nullptr);
  EXPECT_NE(
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker_nonansi", m_design->getCheckerDecls()),
      nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, CheckerHasTwoInputPorts) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker_nonansi", m_design->getCheckerDecls());
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

TEST_F(NetsAndVariablesNonAnsi, CheckerHasLogicVariables) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker_nonansi", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_logic", checker->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_vec", checker->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, CheckerHasRandVariables) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker_nonansi", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);

  const hldb::Variable *const ckRand = hldb::findByName<hldb::Variable>("ck_rand", checker->getVariables());
  ASSERT_NE(ckRand, nullptr);
  EXPECT_EQ(ckRand->getRandType(), vpiRand);

  const hldb::Variable *const ckRandc = hldb::findByName<hldb::Variable>("ck_randc", checker->getVariables());
  ASSERT_NE(ckRandc, nullptr);
  EXPECT_EQ(ckRandc->getRandType(), vpiRandC);
}

TEST_F(NetsAndVariablesNonAnsi, CheckerHasOneProcess) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker_nonansi", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getProcesses(), nullptr);
  ASSERT_EQ(checker->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(checker->getProcesses()->at(0)), nullptr)
      << "'always_ff @(posedge clk)' should be modeled as an Always process";
}

// ---------------------------------------------------------------------------
// Class -- nets_and_variables_class_nonansi (file scope)
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, ClassExists) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class_nonansi", m_design->getAllClasses()),
            nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, ClassHasVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class_nonansi", m_design->getAllClasses());
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_logic", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_reg", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_bits", cls->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, ClassHasRandVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class_nonansi", m_design->getAllClasses());
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
// Assertion -- nets_and_variables_assertion_nonansi(clk, a, b)
//
// Not checked: the internal PropertySpec/ClockedProperty expression tree of
// the property body; no test anywhere in this codebase exercises a named
// property declaration with a local variable, so that deeper shape is left
// undocumented rather than guessed.
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, AssertionModuleExists) {
  EXPECT_NE(
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion_nonansi", m_design->getAllModules()),
      nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, AssertionModuleHasThreeInputPorts) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion_nonansi", m_design->getAllModules());
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

TEST_F(NetsAndVariablesNonAnsi, AssertionModuleHasOnePropertyDecl) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion_nonansi", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPropertyDecls(), nullptr);
  EXPECT_EQ(mod->getPropertyDecls()->size(), 1u);
}

TEST_F(NetsAndVariablesNonAnsi, AssertionPropertyDeclHasLocalVariable) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion_nonansi", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPropertyDecls(), nullptr);
  ASSERT_EQ(mod->getPropertyDecls()->size(), 1u);
  const hldb::PropertyDecl *const prop = mod->getPropertyDecls()->at(0);
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("local_val", prop->getVariables()), nullptr);
}

TEST_F(NetsAndVariablesNonAnsi, AssertionModuleHasOneConcurrentAssertion) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("work@nets_and_variables_assertion_nonansi", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(mod->getConcurrentAssertions()->size(), 1u);
}

// ---------------------------------------------------------------------------
// top_nonansi -- instantiates the module, the program, the interface, and
// the checker, plus a class-handle variable
// ---------------------------------------------------------------------------
TEST_F(NetsAndVariablesNonAnsi, TopNonansiExists) { ASSERT_NE(getTopNonansi(), nullptr); }

TEST_F(NetsAndVariablesNonAnsi, TopNonansiHasFourWireNets) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 4u) << "expected nets a, b, y1, y2";
}

TEST_F(NetsAndVariablesNonAnsi, TopNonansiAInitialValueIsOne) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'wire a = 1'b1;' should have an initial value";
}

TEST_F(NetsAndVariablesNonAnsi, TopNonansiBInitialValueIsZero) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'wire b = 1'b0;' should have an initial value";
}

TEST_F(NetsAndVariablesNonAnsi, TopNonansiInstantiatesModuleAndProgramAndInterface) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("mod_nonansi", top->getRefInstances()), nullptr)
      << "module instance 'mod_nonansi' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("prog_inst", top->getRefInstances()), nullptr)
      << "program instance 'prog_inst' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("if0", top->getRefInstances()), nullptr)
      << "interface instance 'if0' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("mpif0", top->getRefInstances()), nullptr)
      << "modport interface instance 'mpif0' not found";
}

TEST_F(NetsAndVariablesNonAnsi, TopNonansiHasCls0Variable) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls0", top->getVariables()), nullptr)
      << "class-handle variable 'cls0' not found";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
