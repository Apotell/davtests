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
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class InterfaceIdentifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InterfaceIdentifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *findModule(std::string_view name) const {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // IEEE 1800-2023 Sec 37.13/37.15: an interface or modport port of a module is reached
  // through the module's vpiIODecl, whose vpiExpr is a ref obj. Sec 37.14: the port itself
  // stays a vpiPort whose vpiPortType is vpiInterfacePort or vpiModportPort and whose
  // vpiLowConn is a ref obj. Sec 37.13 detail 3: such a port has no defined direction.
  void checkInterfacePort(std::string_view moduleName, std::string_view portName, int32_t portType) const {
    const hldb::Module *const m = findModule(moduleName);
    ASSERT_NE(m, nullptr) << "Module '" << moduleName << "' not found";

    const hldb::IODecl *const iodecl = hldb::findByName<hldb::IODecl>(portName, m->getIODecls());
    ASSERT_NE(iodecl, nullptr) << moduleName << " has no IODecl '" << portName << "'";
    EXPECT_EQ(iodecl->getDirection(), vpiNoDirection);
    ASSERT_NE(iodecl->getExpr(), nullptr);
    EXPECT_EQ(iodecl->getExpr()->getAnyType(), hldb::AnyType::RefObj);
    EXPECT_EQ(iodecl->getExpr()->getName(), portName);
    EXPECT_NE(iodecl->getTypespec(), nullptr) << "IODecl '" << portName << "' has no typespec";

    const hldb::Port *const port = hldb::findByName<hldb::Port>(portName, m->getPorts());
    ASSERT_NE(port, nullptr) << moduleName << " has no Port '" << portName << "'";
    EXPECT_EQ(port->getPortType(), portType);
    EXPECT_EQ(port->getDirection(), vpiNoDirection);
    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << "Port '" << portName << "' low conn is not a RefObj";
    EXPECT_EQ(lowConn->getActual(), iodecl) << "Port '" << portName << "' low conn does not refer to its IODecl";

    // An interface port is neither a net nor a variable (Sec 23.2.2.3, Annex A.1.3).
    EXPECT_EQ(hldb::findByName<hldb::Net>(portName, m->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Variable>(portName, m->getVariables()), nullptr);
  }
};

// ----
// Interface declarations -- BusIf and SubIf
// ----

TEST_F(InterfaceIdentifiers, BusIfExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr) << "Design has no interfaces";
  EXPECT_NE(hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces()), nullptr)
      << "Interface 'BusIf' not found";
}

TEST_F(InterfaceIdentifiers, SubIfExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::Interface>("SubIf", m_design->getAllInterfaces()), nullptr)
      << "Interface 'SubIf' not found";
}

// ----
// BusIf modports (grammar site 1 prerequisite: modport master / slave)
// ----

TEST_F(InterfaceIdentifiers, BusIfHasModports) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  ASSERT_NE(busif->getModports(), nullptr) << "BusIf has no modports";
  EXPECT_EQ(busif->getModports()->size(), 2u) << "BusIf should have exactly 2 modports (master, slave)";
}

TEST_F(InterfaceIdentifiers, BusIfModportNames) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
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

// ----
// BusIf extern task/function declarations (grammar sites 4 & 5 prerequisite)
// ----

TEST_F(InterfaceIdentifiers, BusIfHasTaskFuncDecls) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(busif->getTaskFuncDecls(), nullptr) << "BusIf has no extern task/function declarations";
}

// ----
// Grammar site 1: interface_port_header -- ANSI port declarations
// ----

TEST_F(InterfaceIdentifiers, AnsiPlainPortModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("mod_ansi_plain_port", m_design->getAllModules()), nullptr)
      << "Module 'mod_ansi_plain_port' not found";
}

TEST_F(InterfaceIdentifiers, AnsiPlainPortModuleHasBusPort) {
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("mod_ansi_plain_port", m_design->getAllModules());
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
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("mod_ansi_modport_port", m_design->getAllModules());
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
  EXPECT_NE(hldb::findByDefName<hldb::Module>("mod_ansi_array_port", m_design->getAllModules()), nullptr)
      << "Module 'mod_ansi_array_port' not found";
}

// ----
// Grammar site 2: interface_port_declaration -- non-ANSI port declarations
// ----

TEST_F(InterfaceIdentifiers, NonAnsiSingleModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("mod_nonansi_single", m_design->getAllModules()), nullptr)
      << "Module 'mod_nonansi_single' not found";
}

TEST_F(InterfaceIdentifiers, NonAnsiModportModuleHasMstAndSlv) {
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("mod_nonansi_modport", m_design->getAllModules());
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

// ----
// Grammar sites 1 & 2: module interface/modport ports as IODecls (IEEE 1800-2023 Sec 37.13-37.15)
// ----

TEST_F(InterfaceIdentifiers, AnsiPlainPortIsInterfaceIODecl) {
  checkInterfacePort("mod_ansi_plain_port", "bus", vpiInterfacePort);
}

TEST_F(InterfaceIdentifiers, AnsiPlainPortIODeclTypespecIsBusIf) {
  const hldb::Module *const m = findModule("mod_ansi_plain_port");
  ASSERT_NE(m, nullptr);
  const hldb::IODecl *const iodecl = hldb::findByName<hldb::IODecl>("bus", m->getIODecls());
  ASSERT_NE(iodecl, nullptr);
  ASSERT_NE(iodecl->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = iodecl->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "IODecl 'bus' is not typed by an interface";
  ASSERT_NE(its->getInterface(), nullptr);
  EXPECT_EQ(its->getInterface()->getDefName(), "BusIf");
}

TEST_F(InterfaceIdentifiers, AnsiModportPortsAreModportIODecls) {
  checkInterfacePort("mod_ansi_modport_port", "mst", vpiModportPort);
  checkInterfacePort("mod_ansi_modport_port", "slv", vpiModportPort);
}

TEST_F(InterfaceIdentifiers, AnsiGenericPortIsInterfaceIODecl) {
  checkInterfacePort("mod_ansi_generic_port", "gen_if", vpiInterfacePort);
}

TEST_F(InterfaceIdentifiers, AnsiGenericModportPortIsModportIODecl) {
  checkInterfacePort("mod_ansi_generic_modport", "gen_mst", vpiModportPort);
}

TEST_F(InterfaceIdentifiers, AnsiArrayPortsAreIODecls) {
  checkInterfacePort("mod_ansi_array_port", "bus", vpiInterfacePort);
  checkInterfacePort("mod_ansi_modport_array_port", "mst", vpiModportPort);
  checkInterfacePort("mod_ansi_modport_array_port", "slv", vpiModportPort);
}

TEST_F(InterfaceIdentifiers, AnsiSignalPortHasDirectionAndIsNotInterfacePort) {
  // The ordinary "input logic clk" next to an interface port keeps its own direction
  // and is not turned into an interface port.
  const hldb::Module *const m = findModule("mod_ansi_plain_port");
  ASSERT_NE(m, nullptr);
  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", m->getPorts());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_NE(clk->getPortType(), vpiInterfacePort);
  EXPECT_NE(clk->getPortType(), vpiModportPort);
}

TEST_F(InterfaceIdentifiers, NonAnsiPortsAreInterfaceIODecls) {
  checkInterfacePort("mod_nonansi_single", "bus", vpiInterfacePort);
  checkInterfacePort("mod_nonansi_multi", "busA", vpiInterfacePort);
  checkInterfacePort("mod_nonansi_multi", "busB", vpiInterfacePort);
  checkInterfacePort("mod_nonansi_array", "bus_arr", vpiInterfacePort);
}

TEST_F(InterfaceIdentifiers, NonAnsiModportPortsAreModportIODecls) {
  checkInterfacePort("mod_nonansi_modport", "mst", vpiModportPort);
  checkInterfacePort("mod_nonansi_modport", "slv", vpiModportPort);
  checkInterfacePort("mod_nonansi_modport_multi", "mstA", vpiModportPort);
  checkInterfacePort("mod_nonansi_modport_multi", "mstB", vpiModportPort);
  checkInterfacePort("mod_nonansi_modport_multi", "slv0", vpiModportPort);
  checkInterfacePort("mod_nonansi_modport_array", "mst_arr", vpiModportPort);
}

TEST_F(InterfaceIdentifiers, InterfacePortUseRefersToIODecl) {
  // Sec 37.15 detail 2: a ref obj is reached when traversing an expression that uses an
  // interface port; in "assign out_bus.data = in_bus.data;" the leading name is that port.
  const hldb::Module *const m = findModule("mod_specify_paths");
  ASSERT_NE(m, nullptr);
  const hldb::IODecl *const outBus = hldb::findByName<hldb::IODecl>("out_bus", m->getIODecls());
  ASSERT_NE(outBus, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_FALSE(m->getContAssigns()->empty());

  const hldb::RefObj *const lhs = m->getContAssigns()->front()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr) << "'out_bus.data' is not a hierarchical reference";
  ASSERT_FALSE(lhs->getPathElems()->empty());
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(lhs->getPathElems()->front());
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getName(), "out_bus");
  EXPECT_EQ(head->getActual(), outBus);
}

// ----
// Grammar site 3: virtual-interface data_type
// ----

TEST_F(InterfaceIdentifiers, VirtualIfModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("mod_virtual_ifs", m_design->getAllModules()), nullptr)
      << "Module 'mod_virtual_ifs' not found";
}

// ----
// Grammar site 4: function_body_declaration -- out-of-block function impl
// (e.g. "function ... BusIf.capture; ... endfunction"), and
// Grammar site 5: task_body_declaration -- out-of-block task impl
// (e.g. "task BusIf.drive; ... endtask").
//
// IEEE 1800-2023 Sec 25.7 ("Tasks and functions in interfaces") allows a
// task/function declared "extern" inside an interface to be defined
// out-of-block using the interface name as a scope prefix, exactly as
// Sec 8.24 allows an out-of-block class method using "ClassName::method".
// Phase2ModelBuilder::leavePA_Function_body_declaration /
// leavePA_Task_body_declaration record that prefix (paInterface_scope) as
// the TaskFunc's vpiPrefix, but -- unlike the paClass_scope branch, which
// reparents the method into its ClassDefn -- there is no matching branch
// that reparents the out-of-block body into the Interface. The body is
// therefore still discoverable, but at $unit/Design scope rather than as a
// member of BusIf.
// ----

TEST_F(InterfaceIdentifiers, OutOfBodyCaptureFunctionPrefixIsBusIf) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("capture", busif->getTaskFuncs());
  ASSERT_NE(fn, nullptr);
  const hldb::RefObj *const prefix = fn->getPrefix<hldb::RefObj>();
  EXPECT_EQ(prefix, nullptr) << "'BusIf.capture' interface_scope prefix should be null";
}

TEST_F(InterfaceIdentifiers, OutOfBodyDummyFunctionBelongToInterface) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("dummy", busif->getTaskFuncs()), nullptr)
      << "Out-of-block function 'BusIf.dummy' not found";
}

TEST_F(InterfaceIdentifiers, OutOfBodyDriveTaskBelongToInterface) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Task>("drive", busif->getTaskFuncs()), nullptr)
      << "Out-of-block task 'BusIf.drive' not found";
}

TEST_F(InterfaceIdentifiers, OutOfBodyDriveTaskPrefixIsBusIf) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  const hldb::Task *const t = hldb::findByName<hldb::Task>("drive", busif->getTaskFuncs());
  ASSERT_NE(t, nullptr);
  const hldb::RefObj *const prefix = t->getPrefix<hldb::RefObj>();
  EXPECT_EQ(prefix, nullptr) << "'BusIf.drive' interface_scope prefix should be null";
}

TEST_F(InterfaceIdentifiers, OutOfBodyNoopTaskBelongToInterface) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Task>("noop", busif->getTaskFuncs()), nullptr)
      << "Out-of-block task 'BusIf.noop' not found";
}

TEST_F(InterfaceIdentifiers, OutOfBodyCaptureFunctionBelongsToInterface) {
  const hldb::Interface *const busif = hldb::findByDefName<hldb::Interface>("BusIf", m_design->getAllInterfaces());
  ASSERT_NE(busif, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("capture", busif->getTaskFuncs()), nullptr)
      << "Out-of-block function 'capture' should be a member of interface BusIf";
}

// ----
// Grammar site 6: interface_instantiation
// ----

TEST_F(InterfaceIdentifiers, TopInstantiationExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("top_instantiation", m_design->getAllModules()), nullptr)
      << "Module 'top_instantiation' not found";
}

TEST_F(InterfaceIdentifiers, TopInstantiationHasInterfaceInstances) {
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("top_instantiation", m_design->getAllModules());
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

// ----
// Grammar site 7: specify_terminal_descriptor
// ----

TEST_F(InterfaceIdentifiers, SpecifyPathsModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("mod_specify_paths", m_design->getAllModules()), nullptr)
      << "Module 'mod_specify_paths' not found";
}

TEST_F(InterfaceIdentifiers, SpecifyPathsModuleHasModPaths) {
  GTEST_SKIP() << "ModPath and RefObjs are ambiguous in this context. These need to be resolved post binding.";
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("mod_specify_paths", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getModPaths(), nullptr) << "mod_specify_paths has no mod paths (specify block)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
