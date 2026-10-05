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

// Tests for dut.sv (tags: NameCollisionBind)
//   typedef int DataPath;
//   typedef struct packed {
//     DataPath numLoadMiss;
//   } PerfCounterPath;
//
//   interface PerformanceCounterIF( );
//     PerfCounterPath perfCounter;
//     modport CSR ( input perfCounter );
//   endinterface
//
//   module CSR_Unit(
//     PerformanceCounterIF.CSR perfCounter
//   );
//   assign mshrID = perfCounter.perfCounter.numLoadMiss;
//   endmodule
//
// The interesting part is the name collision: the module's interface port
// is named 'perfCounter', and so is the variable inside the interface that
// the modport exposes. In 'perfCounter.perfCounter.numLoadMiss' the first
// 'perfCounter' is the port (resolved in CSR_Unit's scope), the second is
// the interface item reached through the modport, and 'numLoadMiss' is a
// member of the packed struct type of that item.
//
// What is checked (IEEE 1800-2023):
//   - 6.18 / 7.2.1: $unit typedef 'DataPath' aliases 'int'; 'PerfCounterPath'
//     is a packed struct with a single member 'numLoadMiss' of type
//     'DataPath' (int is an integral type, legal in a packed struct).
//   - 25.3 / 6.8: interface 'PerformanceCounterIF' declares variable
//     'perfCounter' of type 'PerfCounterPath'.
//   - 25.5: modport 'CSR' has exactly one port, 'perfCounter', direction
//     input, whose expression refers to the interface variable
//     'perfCounter'.
//   - 25.3 / 25.5 / 23.2.2: the module port 'PerformanceCounterIF.CSR
//     perfCounter' is an interface port restricted to modport CSR: its type
//     resolves to interface 'PerformanceCounterIF' with modport 'CSR' (no
//     COMP_FAILED_TO_BIND for 'CSR'), its low connection binds to the
//     modport, and -- since interface_port_header has no port_direction --
//     no "missing direction" diagnostic is raised for it.
//   - 25.5 / 23.6: the RHS hierarchical reference resolves step by step:
//     'perfCounter' (1st) -> the interface port (modport CSR),
//     'perfCounter' (2nd) -> the interface variable, 'numLoadMiss' ->
//     the struct member. No COMP_FAILED_TO_BIND for any of these names.
//   - 6.10: 'mshrID' is undeclared and is the LHS of a continuous
//     assignment, so it is an implicit scalar 'wire' net and the LHS binds
//     to it.
//
// What is NOT checked and why:
//   - the width truncation of a 32-bit value into the 1-bit implicit net
//     (10.3 / 11.6 assignment semantics; no diagnostic is required).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NameCollisionBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NameCollisionBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface() {
    return hldb::findByName<hldb::Interface>("PerformanceCounterIF", m_design->getAllInterfaces());
  }

  static const hldb::Module *getMod() { return hldb::findByName<hldb::Module>("CSR_Unit", m_design->getAllModules()); }

  static const hldb::Variable *getIfaceVar() {
    const hldb::Interface *const i = getIface();
    return (i == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("perfCounter", i->getVariables());
  }

  static const hldb::Modport *getModport() {
    const hldb::Interface *const i = getIface();
    return (i == nullptr) ? nullptr : hldb::findByName<hldb::Modport>("CSR", i->getModports());
  }

  static const hldb::Struct *getPerfStruct() {
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("PerfCounterPath", m_design->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const sts = any_cast<hldb::StructTypespec>(td->getAlias()->getActual());
    return (sts == nullptr) ? nullptr : sts->getStruct();
  }

  static const hldb::Port *getPort() {
    const hldb::Module *const m = getMod();
    return (m == nullptr) ? nullptr : hldb::findByName<hldb::Port>("perfCounter", m->getPorts());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getMod();
    if (m == nullptr || m->getContAssigns() == nullptr || m->getContAssigns()->empty()) return nullptr;
    return m->getContAssigns()->at(0);
  }

  static const hldb::RefObj *getRhsElem(size_t index) {
    const hldb::ContAssign *const ca = getAssign();
    if (ca == nullptr) return nullptr;
    const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(ca->getRhs());
    if (rhs == nullptr || rhs->getPathElems() == nullptr || rhs->getPathElems()->size() <= index) return nullptr;
    return any_cast<hldb::RefObj>(rhs->getPathElems()->at(index));
  }
};

// ===========================================================================
// $unit typedefs
// ===========================================================================

TEST_F(NameCollisionBindTest, DataPathAliasesInt) {
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("DataPath", m_design->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  EXPECT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
}

TEST_F(NameCollisionBindTest, PerfCounterPathIsPackedStructWithNumLoadMiss) {
  const hldb::Struct *const st = getPerfStruct();
  ASSERT_NE(st, nullptr);
  EXPECT_TRUE(st->getPacked());
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = st->getMembers()->at(0);
  EXPECT_EQ(m->getName(), "numLoadMiss");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(m->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr) << "'numLoadMiss' is typed by typedef 'DataPath'";
  EXPECT_EQ(tts->getName(), "DataPath");
}

// ===========================================================================
// Interface and modport
// ===========================================================================

TEST_F(NameCollisionBindTest, InterfaceVariablePerfCounter) {
  ASSERT_NE(getIface(), nullptr);
  const hldb::Variable *const v = getIfaceVar();
  ASSERT_NE(v, nullptr) << "6.8: 'PerfCounterPath perfCounter;' is a variable";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(v->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr);
  EXPECT_EQ(tts->getName(), "PerfCounterPath");
}

TEST_F(NameCollisionBindTest, ModportCsrHasInputPerfCounter) {
  const hldb::Modport *const mp = getModport();
  ASSERT_NE(mp, nullptr) << "modport 'CSR' not found";
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = mp->getIODecls()->at(0);
  EXPECT_EQ(io->getName(), "perfCounter");
  EXPECT_EQ(io->getDirection(), vpiInput);
}

TEST_F(NameCollisionBindTest, ModportPortRefersToInterfaceVariable) {
  const hldb::Modport *const mp = getModport();
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = mp->getIODecls()->at(0);
  ASSERT_NE(io->getExpr(), nullptr) << "25.5: modport port 'perfCounter' must refer to the interface item";
  const hldb::Any *resolved = io->getExpr();
  if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(io->getExpr())) resolved = ref->getActual();
  ASSERT_NE(getIfaceVar(), nullptr);
  EXPECT_EQ(resolved, getIfaceVar());
}

// ===========================================================================
// Interface port 'PerformanceCounterIF.CSR perfCounter'
// ===========================================================================

TEST_F(NameCollisionBindTest, PortTypeResolvesToInterfaceAndModport) {
  const hldb::Port *const p = getPort();
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  ASSERT_NE(p->getTypespec()->getActual(), nullptr) << "'PerformanceCounterIF.CSR' must resolve";
  const hldb::InterfaceTypespec *const its = any_cast<hldb::InterfaceTypespec>(p->getTypespec()->getActual());
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getIface());
  ASSERT_NE(its->getModport(), nullptr);
  EXPECT_EQ(its->getModport(), getModport());
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "CSR"), nullptr);
}

TEST_F(NameCollisionBindTest, PortLowConnBindsToModport) {
  const hldb::Port *const p = getPort();
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const lc = any_cast<hldb::RefObj>(p->getLowConn());
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getName(), "perfCounter");
  ASSERT_NE(lc->getActual(), nullptr);
  EXPECT_EQ(lc->getActual(), getModport());
}

TEST_F(NameCollisionBindTest, InterfacePortNotWarnedMissingDirection) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, "perfCounter"), nullptr)
      << "23.2.2: interface_port_header has no port_direction; none is missing";
}

// ===========================================================================
// RHS perfCounter.perfCounter.numLoadMiss
// ===========================================================================

TEST_F(NameCollisionBindTest, RhsIsThreeElementPath) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(ca->getRhs());
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  EXPECT_EQ(rhs->getPathElems()->size(), 3u);
}

TEST_F(NameCollisionBindTest, FirstPerfCounterBindsToPort) {
  const hldb::RefObj *const e = getRhsElem(0);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->getName(), "perfCounter");
  ASSERT_NE(e->getActual(), nullptr) << "first 'perfCounter' is the interface port of CSR_Unit";
  EXPECT_EQ(e->getActual(), getModport()) << "the port is restricted to modport CSR (25.5)";
}

TEST_F(NameCollisionBindTest, SecondPerfCounterBindsToInterfaceVariable) {
  const hldb::RefObj *const e = getRhsElem(1);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->getName(), "perfCounter");
  ASSERT_NE(e->getActual(), nullptr) << "second 'perfCounter' is the interface item exposed by modport CSR";
  ASSERT_NE(getIfaceVar(), nullptr);
  EXPECT_EQ(e->getActual(), getIfaceVar());
}

TEST_F(NameCollisionBindTest, NumLoadMissBindsToStructMember) {
  const hldb::RefObj *const e = getRhsElem(2);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->getName(), "numLoadMiss");
  ASSERT_NE(e->getActual(), nullptr);
  const hldb::Struct *const st = getPerfStruct();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_FALSE(st->getMembers()->empty());
  EXPECT_EQ(e->getActual(), st->getMembers()->at(0));
}

TEST_F(NameCollisionBindTest, NoFailedToBindOnRhsNames) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "perfCounter"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "numLoadMiss"), nullptr);
}

// ===========================================================================
// 6.10: implicit net mshrID
// ===========================================================================

TEST_F(NameCollisionBindTest, MshrIDIsImplicitScalarWire) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("mshrID", m->getNets());
  ASSERT_NE(n, nullptr) << "6.10: undeclared LHS of a continuous assignment is an implicit net";
  EXPECT_TRUE(n->getImplicitDecl());
  EXPECT_EQ(n->getNetType(), vpiWire);
  EXPECT_TRUE(n->getScalar());

  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), n) << "LHS 'mshrID' binds to its implicit net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
