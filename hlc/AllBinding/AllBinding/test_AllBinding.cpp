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

// Tests for dut.sv + enum.sv, compiled together as "enum.sv dut.sv" (tags: AllBinding)
//   enum.sv:
//     typedef enum logic[1:0] {
//         SCR1_MEM_RESP_NOTRDY    = 2'b00,
//         SCR1_MEM_RESP_RDY_OK    = 2'b01,
//         SCR1_MEM_RESP_RDY_ER    = 2'b10
//     } type_scr1_mem_resp_e;
//   dut.sv:
//     module top( input   type_scr1_mem_resp_e                dmem2exu_resp_i);
//     type_scr1_mem_resp_e                dmem2exu_resp_ii;
//     endmodule
//
// "type_scr1_mem_resp_e" is declared in enum.sv with no package wrapper, so
// it lives at compilation-unit ($unit) scope: Design::getTypedefs() holds it
// directly, not any Package. dut.sv's port and internal variable both use
// that cross-file type with no import -- this test exists to confirm that
// binding resolves across files in the same compile, not that any
// particular syntax was used to reach it.
//
// Checked:
//   - module "top" has exactly 1 port, "dmem2exu_resp_i": an input,
//     net-typed (vpiWire) since it has no explicit "output"/variable data
//     type (IEEE 1800-2023 23.2.2.3), whose typespec resolves to the
//     TypedefTypespec "type_scr1_mem_resp_e" declared in enum.sv
//   - module "top" has exactly 1 variable, "dmem2exu_resp_ii" (plain
//     non-port declaration), whose typespec also resolves to that same
//     cross-file TypedefTypespec
//   - the design's single Typedef "type_scr1_mem_resp_e" aliases an
//     EnumTypespec whose base type is "logic [1:0]" and which has exactly
//     3 enum constants, in declaration order, with their 2-bit binary
//     values
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AllBindingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AllBinding.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *getCrossFileTypedef() {
    return hldb::findByName<hldb::Typedef>("type_scr1_mem_resp_e", m_design->getTypedefs());
  }

  static const hldb::EnumTypespec *getCrossFileEnumTypespec() {
    const hldb::Typedef *const td = getCrossFileTypedef();
    if (td == nullptr || td->getAlias() == nullptr) {
      return nullptr;
    }
    return td->getAlias()->getActual<hldb::EnumTypespec>();
  }
};

// --- port / variable binding to the cross-file enum typedef ---------------

TEST_F(AllBindingTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AllBindingTest, PortResolvesToCrossFileEnumTypedef) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 1u);

  const hldb::Port *const port = hldb::findByName<hldb::Port>("dmem2exu_resp_i", mod->getPorts());
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
  ASSERT_NE(port->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tt = port->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tt, nullptr) << "port's typespec should resolve to the TypedefTypespec declared in enum.sv";
  EXPECT_EQ(tt->getName(), "type_scr1_mem_resp_e");

  const hldb::Net *const net = hldb::findByName<hldb::Net>("dmem2exu_resp_i", mod->getNets());
  ASSERT_NE(net, nullptr) << "an ANSI input port with no explicit variable data type defaults to a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
}

TEST_F(AllBindingTest, InternalVariableResolvesToCrossFileEnumTypedef) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  ASSERT_EQ(mod->getVariables()->size(), 1u);

  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("dmem2exu_resp_ii", mod->getVariables());
  ASSERT_NE(var, nullptr);
  ASSERT_NE(var->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tt = var->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tt, nullptr) << "variable's typespec should resolve to the TypedefTypespec declared in enum.sv";
  EXPECT_EQ(tt->getName(), "type_scr1_mem_resp_e");
}

// --- type_scr1_mem_resp_e itself --------------------------------------------

TEST_F(AllBindingTest, TypedefAliasesLogicBackedEnum) {
  const hldb::Typedef *const td = getCrossFileTypedef();
  ASSERT_NE(td, nullptr) << "design should hold 'type_scr1_mem_resp_e' as a $unit-scope Typedef";
  ASSERT_NE(td->getAlias(), nullptr);

  const hldb::EnumTypespec *const enumTypespec = getCrossFileEnumTypespec();
  ASSERT_NE(enumTypespec, nullptr);
  const hldb::Enum *const e = enumTypespec->getEnum();
  ASSERT_NE(e, nullptr);

  ASSERT_NE(e->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const baseType = e->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(baseType, nullptr) << "'enum logic[1:0] {...}' base type should be a LogicTypespec";
  ASSERT_NE(baseType->getRanges(), nullptr);
  ASSERT_EQ(baseType->getRanges()->size(), 1u);
  const hldb::Range *const range = baseType->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const leftExpr = range->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const rightExpr = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(leftExpr, nullptr);
  ASSERT_NE(rightExpr, nullptr);
  EXPECT_EQ(leftExpr->getDecompile(), "1");
  EXPECT_EQ(rightExpr->getDecompile(), "0");
}

TEST_F(AllBindingTest, EnumHasThreeConstsInDeclarationOrder) {
  const hldb::EnumTypespec *const enumTypespec = getCrossFileEnumTypespec();
  ASSERT_NE(enumTypespec, nullptr);
  const hldb::Enum *const e = enumTypespec->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 3u);

  static constexpr std::string_view kNames[3] = {"SCR1_MEM_RESP_NOTRDY", "SCR1_MEM_RESP_RDY_OK",
                                                 "SCR1_MEM_RESP_RDY_ER"};
  static constexpr std::string_view kDecompiles[3] = {"2'b00", "2'b01", "2'b10"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::EnumConst *const ec = e->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr) << "enum const index " << i;
    EXPECT_EQ(ec->getName(), kNames[i]) << "enum const index " << i;
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "enum const index " << i;
    EXPECT_EQ(value->getConstType(), vpiBinaryConst) << "enum const index " << i;
    EXPECT_EQ(value->getSize(), 2) << "enum const index " << i;
    EXPECT_EQ(value->getDecompile(), kDecompiles[i]) << "enum const index " << i;
  }
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(AllBindingTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
