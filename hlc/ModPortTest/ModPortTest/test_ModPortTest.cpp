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

// Tests for tests/ModPortTest/top.v (tags: ModPortTest)
//   module dff0_test(n1);
//     (* init = 32'd1 *)
//     output n1;
//     reg n1 = 32'd0;
//   endmodule
//
//   interface AXI_BUS #( parameter AXI_ID_WIDTH = -1 );
//     typedef logic [AXI_ID_WIDTH-1:0]   id_t;
//     id_t1       aw_id;
//     modport Master ( output aw_id );
//     id_t       rw_id;
//     modport Slave ( output ww_id );
//   endinterface
//
//   interface mem_if (input wire clk);
//     modport  system (input clk);
//     modport  memory (output clk);
//   endinterface
//
//   module memory_ctrl1 (mem_if.system1 sif);
//     typedef  enum {IDLE,WRITE,READ,DONE} fsm_t;
//     fsm_t state;
//   endmodule
//
//   module memory_ctrl2 (mem_if.system sif);
//     typedef  enum {IDLE,WRITE,READ,DONE} fsm_t;
//     fsm_t state;
//     DD t;
//   endmodule
//
// (The .hlc also adds the UVM include directory, but top.v includes nothing
// from it; only the constructs in top.v are tested.)
//
// What is checked (IEEE 1800-2023):
//   - 25.5: AXI_BUS has modports Master (output aw_id) and Slave
//     (output ww_id).
//   - 25.5: "All of the names used in a modport declaration shall be
//     declared by the same interface as the modport itself." ww_id is never
//     declared in AXI_BUS -> COMP_MODPORT_UNDEFINED_PORT "ww_id" at 23:12;
//     aw_id IS declared (even though its type is bad), so no such error is
//     raised for aw_id.
//   - 6.18 / 6.11: "typedef logic [AXI_ID_WIDTH-1:0] id_t;" -- "logic" is a
//     built-in type keyword, not an identifier to bind; no
//     COMP_FAILED_TO_BIND may be reported for "logic". rw_id is a variable of
//     type id_t.
//   - 6.18: "id_t1 aw_id;" -- id_t1 is not a declared type; an error is
//     required (accepted as either COMP_UNDEFINED_TYPE or
//     COMP_FAILED_TO_BIND for "id_t1"). Likewise for "DD t;" in
//     memory_ctrl2.
//   - 6.20.1: AXI_ID_WIDTH is a (non-local) parameter of AXI_BUS (declared
//     in the parameter_port_list).
//   - 25.5 / 23.2.2: memory_ctrl1's port "mem_if.system1 sif" names a
//     modport that does not exist in mem_if (only system and memory) -> an
//     unresolved-name error for "system1" (COMP_FAILED_TO_BIND).
//     memory_ctrl2's "mem_if.system sif" names an existing modport, so no
//     COMP_FAILED_TO_BIND for "system"; its port is a modport port
//     (37.14 detail 1: vpiPortType vpiModportPort) whose typespec resolves
//     to mem_if restricted to modport system.
//   - 23.2.2: interface_port_header carries no direction -> no
//     COMP_PORT_MISSING_DIRECTION for "sif".
//   - 6.19: each memory_ctrl module has its own enum typedef fsm_t with
//     constants IDLE, WRITE, READ, DONE (implicit values 0..3) and a variable
//     state of that type. The labels being repeated across two modules is
//     legal (separate scopes).
//   - 23.2.2.1 / 5.12: dff0_test is a non-ANSI module whose port n1 is
//     declared "output" and then completed by "reg n1 = 32'd0;" (a variable
//     output port with an initializer, which 23.2.2 allows). The port's
//     direction is output, n1 is a variable (reg, 6.8) initialized with
//     32'd0, and the attribute instance (* init = 32'd1 *) preceding the
//     port_declaration attaches to the port (37.83: ports carry attributes).
//
// What is NOT checked and why:
//   - the numeric value of AXI_ID_WIDTH (-1) and the resulting zero/negative
//     width of id_t: the parameter is never overridden and the standard does
//     not require a diagnostic for an unused parameterized typedef.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/attribute.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ModPortTestTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModPortTest.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface(std::string_view name) {
    // Interfaces are looked up by vpiDefName: the definition name of a
    // parameterized interface is "AXI_BUS" regardless of how vpiName is
    // decorated with its default parameter values.
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Modport *getModport(std::string_view iface, std::string_view name) {
    const hldb::Interface *const i = getIface(iface);
    if (i == nullptr) return nullptr;
    return hldb::findByName<hldb::Modport>(name, i->getModports());
  }

  static void expectFsmEnum(std::string_view module) {
    const hldb::Module *const m = getModule(module);
    ASSERT_NE(m, nullptr) << module;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("fsm_t", m->getTypedefs());
    ASSERT_NE(td, nullptr) << module << ": typedef fsm_t";
    ASSERT_NE(td->getAlias(), nullptr) << module;
    const hldb::EnumTypespec *const ets = td->getAlias()->getActual<hldb::EnumTypespec>();
    ASSERT_NE(ets, nullptr) << module << ": fsm_t aliases an enum";
    const hldb::Enum *const en = ets->getEnum();
    ASSERT_NE(en, nullptr) << module;
    ASSERT_NE(en->getEnumConsts(), nullptr) << module;
    ASSERT_EQ(en->getEnumConsts()->size(), 4u) << module;
    const char *const names[] = {"IDLE", "WRITE", "READ", "DONE"};
    for (size_t i = 0; i < 4; ++i) {
      EXPECT_EQ(en->getEnumConsts()->at(i)->getName(), names[i]) << module;
    }

    const hldb::Variable *const state = hldb::findByName<hldb::Variable>("state", m->getVariables());
    ASSERT_NE(state, nullptr) << module << ": variable state";
    ASSERT_NE(state->getTypespec(), nullptr) << module;
    const hldb::Typespec *const ts = state->getTypespec()->getActual();
    ASSERT_NE(ts, nullptr) << module << ": state's type fsm_t must resolve";
    const hldb::TypedefTypespec *const tdts = any_cast<hldb::TypedefTypespec>(ts);
    if (tdts != nullptr) {
      EXPECT_EQ(tdts->getTypedef(), td) << module;
    } else {
      EXPECT_EQ(ts, ets) << module;
    }
  }
};

// ===========================================================================
// interface AXI_BUS
// ===========================================================================

TEST_F(ModPortTestTest, AxiBusHasMasterAndSlaveModports) {
  const hldb::Interface *const iface = getIface("AXI_BUS");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 2u);

  const hldb::Modport *const master = getModport("AXI_BUS", "Master");
  ASSERT_NE(master, nullptr);
  ASSERT_NE(master->getIODecls(), nullptr);
  ASSERT_EQ(master->getIODecls()->size(), 1u);
  EXPECT_EQ(master->getIODecls()->at(0)->getName(), "aw_id");
  EXPECT_EQ(master->getIODecls()->at(0)->getDirection(), vpiOutput);

  const hldb::Modport *const slave = getModport("AXI_BUS", "Slave");
  ASSERT_NE(slave, nullptr);
  ASSERT_NE(slave->getIODecls(), nullptr);
  ASSERT_EQ(slave->getIODecls()->size(), 1u);
  EXPECT_EQ(slave->getIODecls()->at(0)->getName(), "ww_id");
  EXPECT_EQ(slave->getIODecls()->at(0)->getDirection(), vpiOutput);
}

TEST_F(ModPortTestTest, UndeclaredModportNameIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_MODPORT_UNDEFINED_PORT, "ww_id", 23, 12), nullptr)
      << "25.5: ww_id is not declared by AXI_BUS";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MODPORT_UNDEFINED_PORT, "aw_id"), nullptr)
      << "25.5: aw_id is declared by AXI_BUS";
}

TEST_F(ModPortTestTest, AxiBusParameterIsNonLocal) {
  const hldb::Interface *const iface = getIface("AXI_BUS");
  ASSERT_NE(iface, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("AXI_ID_WIDTH", iface->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: declared in the parameter_port_list with 'parameter'";
}

TEST_F(ModPortTestTest, LogicKeywordInTypedefIsNotBound) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "logic"), nullptr)
      << "6.11: 'logic' is a built-in type keyword, not an identifier";
}

TEST_F(ModPortTestTest, RwIdIsVariableOfTypeIdT) {
  const hldb::Interface *const iface = getIface("AXI_BUS");
  ASSERT_NE(iface, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("id_t", iface->getTypedefs());
  ASSERT_NE(td, nullptr);
  const hldb::Variable *const rw = hldb::findByName<hldb::Variable>("rw_id", iface->getVariables());
  ASSERT_NE(rw, nullptr);
  ASSERT_NE(rw->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tdts = rw->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tdts, nullptr) << "rw_id's type is the typedef id_t";
  EXPECT_EQ(tdts->getTypedef(), td);
}

TEST_F(ModPortTestTest, UndeclaredTypesAreDiagnosed) {
  for (std::string_view name : {"id_t1", "DD"}) {
    const bool found = (findError(ErrorDefinition::COMP_UNDEFINED_TYPE, name) != nullptr) ||
                       (findError(ErrorDefinition::COMP_FAILED_TO_BIND, name) != nullptr);
    EXPECT_TRUE(found) << "6.18: '" << name << "' is not a declared type";
  }
}

// ===========================================================================
// mem_if / memory_ctrl1 / memory_ctrl2
// ===========================================================================

TEST_F(ModPortTestTest, MemIfHasSystemAndMemoryModports) {
  const hldb::Interface *const iface = getIface("mem_if");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 2u);
  const hldb::Modport *const sys = getModport("mem_if", "system");
  ASSERT_NE(sys, nullptr);
  ASSERT_NE(sys->getIODecls(), nullptr);
  ASSERT_EQ(sys->getIODecls()->size(), 1u);
  EXPECT_EQ(sys->getIODecls()->at(0)->getDirection(), vpiInput);
  const hldb::Modport *const mem = getModport("mem_if", "memory");
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(mem->getIODecls(), nullptr);
  ASSERT_EQ(mem->getIODecls()->size(), 1u);
  EXPECT_EQ(mem->getIODecls()->at(0)->getDirection(), vpiOutput);
}

TEST_F(ModPortTestTest, NonexistentModportInPortHeaderIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "system1"), nullptr)
      << "25.5: mem_if has no modport named system1";
}

TEST_F(ModPortTestTest, ExistingModportInPortHeaderBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "system"), nullptr)
      << "25.5: system is a modport of mem_if";
}

TEST_F(ModPortTestTest, MemoryCtrl2PortIsModportPortOfMemIfSystem) {
  const hldb::Module *const m = getModule("memory_ctrl2");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const p = m->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "sif");
  EXPECT_EQ(p->getPortType(), vpiModportPort);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getIface("mem_if"));
  EXPECT_EQ(its->getModport(), getModport("mem_if", "system"));
}

TEST_F(ModPortTestTest, InterfacePortsNotWarnedMissingDirection) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, "sif"), nullptr)
      << "23.2.2: interface_port_header has no port_direction";
}

TEST_F(ModPortTestTest, MemoryCtrl1HasFsmEnumAndState) { expectFsmEnum("memory_ctrl1"); }

TEST_F(ModPortTestTest, MemoryCtrl2HasFsmEnumAndState) { expectFsmEnum("memory_ctrl2"); }

// ===========================================================================
// module dff0_test
// ===========================================================================

TEST_F(ModPortTestTest, Dff0PortN1IsOutputWithInitAttribute) {
  const hldb::Module *const m = getModule("dff0_test");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const p = m->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "n1");
  EXPECT_EQ(p->getDirection(), vpiOutput);
  ASSERT_NE(p->getAttributes(), nullptr) << "5.12: attribute instance before the port_declaration attaches to it";
  const hldb::Attribute *const attr = hldb::findByName<hldb::Attribute>("init", p->getAttributes());
  ASSERT_NE(attr, nullptr);
  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), "32'd1");
}

TEST_F(ModPortTestTest, Dff0N1IsInitializedRegVariable) {
  const hldb::Module *const m = getModule("dff0_test");
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const n1 = hldb::findByName<hldb::Variable>("n1", m->getVariables());
  ASSERT_NE(n1, nullptr) << "6.8: 'reg n1' declares a variable";
  const hldb::Any *const init = (n1->getExpr() != nullptr) ? n1->getExpr() : n1->getValue();
  ASSERT_NE(init, nullptr) << "variable output port initializer '= 32'd0' must be kept";
  const hldb::Constant *const c = any_cast<hldb::Constant>(init);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "32'd0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
