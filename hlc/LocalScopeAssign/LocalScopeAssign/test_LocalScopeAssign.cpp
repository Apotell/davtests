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

// Tests for tests/LocalScopeAssign/dut.sv:
//
//   `default_nettype none
//
//   module module_scope_Example(o1, o2);
//      output wire [31:0] o1, o2;
//      //assign module_scope_Example.o1 = module_scope_Example.v1;
//      //assign module_scope_Example.o2 = module_scope_Example.v2;
//      assign module_scope_Example.o2 = v2;
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 22.8 "`default_nettype": with '`default_nettype none'
//   "all nets shall be explicitly declared. If a net is not explicitly
//   declared, an error is generated." The module's default net type is
//   therefore 'none' (vpiNone).
// IEEE 1800-2023 Sec 23.2.2.1 (non-ANSI port declarations): the list-of-
//   ports header '(o1, o2)' plus 'output wire [31:0] o1, o2;' declares two
//   output ports whose kind is an explicit 'wire' net with packed range
//   [31:0] (Sec 6.7, 7.4.1).
// IEEE 1800-2023 Sec 23.8 "Upwards name referencing": an
//   upward_name_reference 'module_identifier.item_name' -- here
//   'module_scope_Example.o2', where the module name is the enclosing
//   module itself -- identifies the port/net 'o2' of that module. The
//   continuous assignment (Sec 10.3) therefore drives net 'o2'.
// IEEE 1800-2023 Sec 6.10 / 22.8: 'v2' on the RHS is never declared. An
//   implicit net is only created for an undeclared identifier on the LHS of
//   a continuous assignment or in a port expression (Sec 6.10), and
//   '`default_nettype none' forbids even that -- so the reference to 'v2'
//   is an error and no net 'v2' may be created.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LocalScopeAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LocalScopeAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("module_scope_Example", m_design->getAllModules());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getMod();
    if (m == nullptr || m->getContAssigns() == nullptr || m->getContAssigns()->size() != 1) return nullptr;
    return m->getContAssigns()->at(0);
  }

  static void expectOutputWire31To0(std::string_view name) {
    const hldb::Module *const m = getMod();
    ASSERT_NE(m, nullptr);
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, m->getPorts());
    ASSERT_NE(port, nullptr) << "port '" << name << "' not found";
    EXPECT_EQ(port->getDirection(), vpiOutput);
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, m->getNets());
    ASSERT_NE(net, nullptr) << "'output wire ... " << name << "' must be a Net";
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, m->getVariables()), nullptr);
    EXPECT_EQ(net->getNetType(), vpiWire);
    ASSERT_NE(net->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lts = net->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lts, nullptr) << "'wire [31:0]' has implicit logic data type (Sec 6.7.1)";
    ASSERT_NE(lts->getRanges(), nullptr);
    ASSERT_EQ(lts->getRanges()->size(), 1u);
    const hldb::Constant *const left = lts->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = lts->getRanges()->at(0)->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), std::string_view("31"));
    EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  }
};

// ---------------------------------------------------------------------------
// Existence and default net type -- Sec 22.8
// ---------------------------------------------------------------------------

TEST_F(LocalScopeAssignTest, ModuleExists) {
  EXPECT_NE(getMod(), nullptr) << "module 'module_scope_Example' not found";
}

TEST_F(LocalScopeAssignTest, DefaultNetTypeIsNone) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getDefNetType(), vpiNone) << "'`default_nettype none' is in effect for this module";
}

// ---------------------------------------------------------------------------
// Ports -- Sec 23.2.2.1, 6.7
// ---------------------------------------------------------------------------

TEST_F(LocalScopeAssignTest, ModuleHasTwoPortsInOrder) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), std::string_view("o1"));
  EXPECT_EQ(m->getPorts()->at(1)->getName(), std::string_view("o2"));
}

TEST_F(LocalScopeAssignTest, O1IsOutputWire31To0) { expectOutputWire31To0("o1"); }

TEST_F(LocalScopeAssignTest, O2IsOutputWire31To0) { expectOutputWire31To0("o2"); }

// ---------------------------------------------------------------------------
// 'assign module_scope_Example.o2 = v2;' -- Sec 10.3, 23.8
// ---------------------------------------------------------------------------

TEST_F(LocalScopeAssignTest, ModuleHasSingleContAssign) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  EXPECT_EQ(m->getContAssigns()->size(), 1u) << "the two commented-out assigns must not appear";
}

TEST_F(LocalScopeAssignTest, LhsUpwardNameResolvesToNetO2) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getLhs(), nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS should be a reference";
  ASSERT_NE(lhs->getActual(), nullptr) << "'module_scope_Example.o2' must bind (Sec 23.8)";
  ASSERT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net);
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Net>("o2", m->getNets()));
}

TEST_F(LocalScopeAssignTest, LhsPathElemsAreModuleThenO2) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr) << "upward name reference should keep its path components";
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const first = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->getName(), std::string_view("module_scope_Example"));
  ASSERT_NE(first->getActual(), nullptr);
  EXPECT_EQ(first->getActual(), getMod()) << "first component names the enclosing module (Sec 23.8)";
  EXPECT_EQ(second->getName(), std::string_view("o2"));
  ASSERT_NE(second->getActual(), nullptr);
  EXPECT_EQ(second->getActual()->getAnyType(), hldb::AnyType::Net);
}

TEST_F(LocalScopeAssignTest, RhsIsReferenceToV2) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("v2"));
}

// ---------------------------------------------------------------------------
// Undeclared 'v2' -- Sec 6.10, 22.8
// ---------------------------------------------------------------------------

TEST_F(LocalScopeAssignTest, NoImplicitNetV2IsCreated) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("v2", m->getNets()), nullptr)
      << "'`default_nettype none' forbids implicit nets (Sec 22.8)";
  EXPECT_EQ(hldb::findByName<hldb::Variable>("v2", m->getVariables()), nullptr);
}

TEST_F(LocalScopeAssignTest, UndeclaredV2IsReported) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "v2", 7, 37), nullptr)
      << "reference to undeclared 'v2' with '`default_nettype none' is an error (Sec 22.8, 6.10)";
}

TEST_F(LocalScopeAssignTest, UpwardReferenceIsNotReportedUnbound) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "module_scope_Example"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "o2"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
