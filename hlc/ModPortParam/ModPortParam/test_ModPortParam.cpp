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

// Tests for tests/ModPortParam/dut.sv (tags: ModPortParam)
//   interface PerformanceCounterIF( );
//       modport CSR (
//       input
//           perfCounter
//       );
//   endinterface
//
//   module CSR_Unit(
//       PerformanceCounterIF.CSR perfCounter
//   );
//   endmodule
//
//   module Core();
//    PerformanceCounterIF perfCounterIF( );
//    CSR_Unit csrUnit(perfCounterIF);
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 25.5: interface PerformanceCounterIF has exactly one modport "CSR"
//     listing a single "input perfCounter" modport port.
//   - 25.5: "All of the names used in a modport declaration shall be
//     declared by the same interface as the modport itself ... a modport
//     declaration shall not implicitly declare new ports." perfCounter is
//     never declared in PerformanceCounterIF, so the modport is illegal and
//     must be diagnosed: COMP_MODPORT_UNDEFINED_PORT for "perfCounter" at
//     4:9.
//   - 23.2.2 / 25.5: CSR_Unit's single ANSI port "PerformanceCounterIF.CSR
//     perfCounter" is an interface port restricted to modport CSR
//     (interface_port_header ::= interface_identifier [. modport_identifier]).
//       * 37.14 detail 1: its vpiPortType is vpiModportPort.
//       * the modport name "CSR" must resolve (no COMP_FAILED_TO_BIND), and
//         the port's typespec resolves to an InterfaceTypespec whose
//         interface is PerformanceCounterIF and whose modport is CSR.
//       * 37.14 detail 5: the low conn of an interface port is a RefObj;
//         it binds to the CSR modport (see hldb_model_gaps.md Sec 2).
//       * interface_port_header carries no port_direction, so HLC must not
//         warn that the port is "missing its direction"
//         (COMP_PORT_MISSING_DIRECTION).
//       * no net or variable named perfCounter is materialized in CSR_Unit
//         (an interface port is neither a net nor a variable port).
//   - 25.3 / 23.3.2.1: module Core instantiates PerformanceCounterIF as
//     perfCounterIF, and CSR_Unit as csrUnit with a single ordered port
//     connection whose high conn is a RefObj "perfCounterIF" bound to that
//     interface instance and whose low conn binds to CSR_Unit's port.
//
// What is NOT checked and why:
//   - the name "ModPortParam" suggests modport/parameter interplay, but the
//     source contains no parameters at all; nothing parameter-related is
//     asserted.
//   - the vpiDirection reported for an interface port: 37.14 lists
//     vpiDirection on ports but the standard does not define a value for
//     interface/modport ports (the direction lives in the modport's
//     io decls), so no expectation is encoded.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ModPortParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModPortParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface() {
    return hldb::findByName<hldb::Interface>("PerformanceCounterIF", m_design->getAllInterfaces());
  }

  static const hldb::Modport *getCsrModport() {
    const hldb::Interface *const iface = getIface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Modport>("CSR", iface->getModports());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Port *getCsrUnitPort() {
    const hldb::Module *const m = getModule("CSR_Unit");
    if ((m == nullptr) || (m->getPorts() == nullptr) || m->getPorts()->empty()) return nullptr;
    return m->getPorts()->at(0);
  }
};

// ===========================================================================
// interface PerformanceCounterIF / modport CSR
// ===========================================================================

TEST_F(ModPortParamTest, InterfaceHasSingleModportCsr) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "CSR");
}

TEST_F(ModPortParamTest, ModportCsrHasInputPerfCounter) {
  const hldb::Modport *const mp = getCsrModport();
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = mp->getIODecls()->at(0);
  EXPECT_EQ(io->getName(), "perfCounter");
  EXPECT_EQ(io->getDirection(), vpiInput);
}

TEST_F(ModPortParamTest, ModportNameNotDeclaredInInterfaceIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_MODPORT_UNDEFINED_PORT, "perfCounter", 4, 9), nullptr)
      << "25.5: modport names shall be declared by the same interface; 'perfCounter' is not";
}

// ===========================================================================
// module CSR_Unit(PerformanceCounterIF.CSR perfCounter)
// ===========================================================================

TEST_F(ModPortParamTest, CsrUnitHasSingleModportPort) {
  const hldb::Module *const m = getModule("CSR_Unit");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const p = getCsrUnitPort();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getName(), "perfCounter");
  EXPECT_EQ(p->getPortType(), vpiModportPort) << "37.14: port declared as interface.modport is a modport port";
}

TEST_F(ModPortParamTest, ModportNameInPortHeaderBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "CSR"), nullptr)
      << "25.5: 'CSR' in 'PerformanceCounterIF.CSR' names a modport of that interface";
}

TEST_F(ModPortParamTest, PortTypespecResolvesToInterfaceAndModport) {
  const hldb::Port *const p = getCsrUnitPort();
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "port typespec should resolve to an InterfaceTypespec";
  EXPECT_EQ(its->getInterface(), getIface());
  ASSERT_NE(its->getModport(), nullptr);
  EXPECT_EQ(its->getModport(), getCsrModport());
}

TEST_F(ModPortParamTest, PortLowConnBindsToModport) {
  const hldb::Port *const p = getCsrUnitPort();
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const lc = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "37.14 detail 5: low conn of an interface port is a RefObj";
  EXPECT_EQ(lc->getName(), "perfCounter");
  ASSERT_NE(lc->getActual(), nullptr);
  EXPECT_EQ(lc->getActual(), getCsrModport());
}

TEST_F(ModPortParamTest, InterfacePortNotWarnedMissingDirection) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, "perfCounter"), nullptr)
      << "23.2.2: interface_port_header has no port_direction; none is missing";
}

TEST_F(ModPortParamTest, InterfacePortIsNotANetOrVariable) {
  const hldb::Module *const m = getModule("CSR_Unit");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("perfCounter", m->getNets()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("perfCounter", m->getVariables()), nullptr);
}

// ===========================================================================
// module Core: interface instance + module instance
// ===========================================================================

TEST_F(ModPortParamTest, CoreInstantiatesInterfaceAndCsrUnit) {
  const hldb::Module *const core = getModule("Core");
  ASSERT_NE(core, nullptr);
  ASSERT_NE(core->getRefInstances(), nullptr);
  ASSERT_EQ(core->getRefInstances()->size(), 2u);

  const hldb::RefInstance *const ifInst = hldb::findByName<hldb::RefInstance>("perfCounterIF", core->getRefInstances());
  ASSERT_NE(ifInst, nullptr);
  ASSERT_NE(ifInst->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = ifInst->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getIface());

  const hldb::RefInstance *const modInst = hldb::findByName<hldb::RefInstance>("csrUnit", core->getRefInstances());
  ASSERT_NE(modInst, nullptr);
}

TEST_F(ModPortParamTest, CsrUnitConnectionBindsInterfaceInstanceToPort) {
  const hldb::Module *const core = getModule("Core");
  ASSERT_NE(core, nullptr);
  const hldb::RefInstance *const ifInst = hldb::findByName<hldb::RefInstance>("perfCounterIF", core->getRefInstances());
  const hldb::RefInstance *const modInst = hldb::findByName<hldb::RefInstance>("csrUnit", core->getRefInstances());
  ASSERT_NE(ifInst, nullptr);
  ASSERT_NE(modInst, nullptr);
  ASSERT_NE(modInst->getPorts(), nullptr);
  ASSERT_EQ(modInst->getPorts()->size(), 1u);
  const hldb::Port *const conn = any_cast<hldb::Port>(modInst->getPorts()->at(0));
  ASSERT_NE(conn, nullptr);

  const hldb::RefObj *const hi = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getName(), "perfCounterIF");
  EXPECT_EQ(hi->getActual(), ifInst);

  const hldb::RefObj *const lo = conn->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(lo->getActual(), getCsrUnitPort()) << "23.3.2.1: first ordered connection binds to the first port";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
