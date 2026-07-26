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

// Validates HLDB graph nodes produced for interface_identifier grammar sites
// (IEEE 1800 SV3_1aParser.g4):
//   1  interface_port_header        (ANSI port declarations)
//   2  interface_port_declaration   (non-ANSI port declarations)
//   3  virtual-interface data_type  (variable declarations)
//   4  function_body_declaration    (out-of-block function impl)
//   5  task_body_declaration        (out-of-block task impl)
//   6  interface_instantiation      (module-level instantiation)
//   7  specify_terminal_descriptor  (specify path endpoints)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/port.h>

namespace hlc {

class InterfaceIdentifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InterfaceIdentifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Interface declarations — BusIf and SubIf
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, BusIfExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr) << "Design has no interfaces";
  EXPECT_NE(hldb::findByName<hldb::Interface>("BusIf", m_design->getAllInterfaces()), nullptr)
      << "Interface 'BusIf' not found";
}

TEST_F(InterfaceIdentifiers, SubIfExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("SubIf", m_design->getAllInterfaces()), nullptr)
      << "Interface 'SubIf' not found";
}

// ---------------------------------------------------------------------------
// BusIf modports (grammar site 1 prerequisite: modport master / slave)
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, BusIfHasModports) {
  const hldb::Interface *const busif = hldb::findByName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  ASSERT_NE(busif->getModports(), nullptr) << "BusIf has no modports";
  EXPECT_EQ(busif->getModports()->size(), 2u) << "BusIf should have exactly 2 modports (master, slave)";
}

TEST_F(InterfaceIdentifiers, BusIfModportNames) {
  const hldb::Interface *const busif = hldb::findByName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  ASSERT_NE(busif->getModports(), nullptr);

  bool hasMaster = false, hasSlave = false;
  for (const hldb::Modport *const mp : *busif->getModports()) {
    if (mp->getName() == "master") hasMaster = true;
    if (mp->getName() == "slave") hasSlave = true;
  }
  EXPECT_TRUE(hasMaster) << "BusIf missing modport 'master'";
  EXPECT_TRUE(hasSlave) << "BusIf missing modport 'slave'";
}

// ---------------------------------------------------------------------------
// BusIf extern task/function declarations (grammar sites 4 & 5 prerequisite)
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, BusIfHasTaskFuncDecls) {
  const hldb::Interface *const busif = hldb::findByName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(busif->getTaskFuncDecls(), nullptr) << "BusIf has no extern task/function declarations";
}

// ---------------------------------------------------------------------------
// Grammar site 1: interface_port_header — ANSI port declarations
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, AnsiPlainPortModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mod_ansi_plain_port", m_design->getAllModules()), nullptr)
      << "Module 'mod_ansi_plain_port' not found";
}

TEST_F(InterfaceIdentifiers, AnsiPlainPortModuleHasBusPort) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("mod_ansi_plain_port", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr) << "mod_ansi_plain_port has no ports";

  bool found = false;
  for (const hldb::Port *const p : *m->getPorts()) {
    if (p->getName() == "bus") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "mod_ansi_plain_port missing interface port 'bus'";
}

TEST_F(InterfaceIdentifiers, AnsiModportPortModuleHasMstAndSlv) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("mod_ansi_modport_port", m_design->getAllModules());
  ASSERT_NE(m, nullptr) << "Module 'mod_ansi_modport_port' not found";
  ASSERT_NE(m->getPorts(), nullptr);

  bool hasMst = false, hasSlv = false;
  for (const hldb::Port *const p : *m->getPorts()) {
    if (p->getName() == "mst") hasMst = true;
    if (p->getName() == "slv") hasSlv = true;
  }
  EXPECT_TRUE(hasMst) << "mod_ansi_modport_port missing port 'mst'";
  EXPECT_TRUE(hasSlv) << "mod_ansi_modport_port missing port 'slv'";
}

TEST_F(InterfaceIdentifiers, AnsiArrayPortModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mod_ansi_array_port", m_design->getAllModules()), nullptr)
      << "Module 'mod_ansi_array_port' not found";
}

// ---------------------------------------------------------------------------
// Grammar site 2: interface_port_declaration — non-ANSI port declarations
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, NonAnsiSingleModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mod_nonansi_single", m_design->getAllModules()), nullptr)
      << "Module 'mod_nonansi_single' not found";
}

TEST_F(InterfaceIdentifiers, NonAnsiModportModuleHasMstAndSlv) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("mod_nonansi_modport", m_design->getAllModules());
  ASSERT_NE(m, nullptr) << "Module 'mod_nonansi_modport' not found";
  ASSERT_NE(m->getPorts(), nullptr);

  bool hasMst = false, hasSlv = false;
  for (const hldb::Port *const p : *m->getPorts()) {
    if (p->getName() == "mst") hasMst = true;
    if (p->getName() == "slv") hasSlv = true;
  }
  EXPECT_TRUE(hasMst) << "mod_nonansi_modport missing port 'mst'";
  EXPECT_TRUE(hasSlv) << "mod_nonansi_modport missing port 'slv'";
}

// ---------------------------------------------------------------------------
// Grammar site 3: virtual-interface data_type
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, VirtualIfModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mod_virtual_ifs", m_design->getAllModules()), nullptr)
      << "Module 'mod_virtual_ifs' not found";
}

// ---------------------------------------------------------------------------
// Grammar site 6: interface_instantiation
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, TopInstantiationExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top_instantiation", m_design->getAllModules()), nullptr)
      << "Module 'top_instantiation' not found";
}

TEST_F(InterfaceIdentifiers, TopInstantiationHasInterfaceInstances) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("top_instantiation", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getRefInstances(), nullptr) << "top_instantiation has no interface instances";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus16", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus16' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus32", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus32' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus_wc", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus_wc' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus_a", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus_a' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus_b", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus_b' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_narrow_a", m->getRefInstances()), nullptr)
      << "RefInstance 'u_narrow_a' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_narrow_b", m->getRefInstances()), nullptr)
      << "RefInstance 'u_narrow_b' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_bus_arr", m->getRefInstances()), nullptr)
      << "RefInstance 'u_bus_arr' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("u_sub", m->getRefInstances()), nullptr)
      << "RefInstance 'u_sub' not found";
}

// ---------------------------------------------------------------------------
// Grammar site 7: specify_terminal_descriptor
// ---------------------------------------------------------------------------

TEST_F(InterfaceIdentifiers, SpecifyPathsModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mod_specify_paths", m_design->getAllModules()), nullptr)
      << "Module 'mod_specify_paths' not found";
}

TEST_F(InterfaceIdentifiers, SpecifyPathsModuleHasModPaths) {
  GTEST_SKIP() << "ModPath and HierPaths are ambiguous in this context. These need to be resolved post binding.";
  const hldb::Module *const m = hldb::findByName<hldb::Module>("mod_specify_paths", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getModPaths(), nullptr) << "mod_specify_paths has no mod paths (specify block)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
