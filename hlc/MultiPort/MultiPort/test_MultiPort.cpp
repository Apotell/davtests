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

// Tests for dut.sv (tags: MultiPort)
//   package my_pkg;
//       typedef struct {
//           logic[2:0] first;
//           int second;
//       } variable_type;
//   endpackage
//   module top import my_pkg::variable_type;
//   (
//       input variable_type a, b
//   );
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 7.2: package 'my_pkg' declares typedef 'variable_type' as an unpacked
//     struct (no 'packed' keyword) with members 'first' (logic [2:0]) and
//     'second' (int).
//   - 26.4 / 23.2.1: the package import in the module header
//     ("module top import my_pkg::variable_type;") is recorded on 'top' and
//     names the imported item 'variable_type', making it visible in the
//     port list (no COMP_UNDEFINED_TYPE for 'variable_type').
//   - 23.2.2.3: the port list is ANSI style (the first port has a direction
//     and data type). Module 'top' has exactly two ports 'a' and 'b'.
//     "For subsequent ports in an ANSI style port list: If the direction,
//     port kind and data type are all omitted, then they shall be inherited
//     from the previous port." So 'b' is an input of type 'variable_type',
//     exactly like 'a'.
//   - 23.2.2.3: "If the port kind is omitted: For input and inout ports, the
//     port shall default to a net of default net type" -- both ports'
//     internal objects are nets of type 'wire' (no `default_nettype here),
//     carrying the data type 'variable_type'.
//   - each port's low connection binds to its internal object.
//
// What is NOT checked and why:
//   - 6.7.1 lists the valid data types for a net: 4-state integral types or
//     unpacked aggregates whose every element is valid for a net. Member
//     'second' is 'int' (2-state), so a literal reading of 23.2.2.3 + 6.7.1
//     makes the implicit 'input wire variable_type' ports illegal. Tools
//     commonly accept such ports (treating them leniently), and this file
//     is not about net data-type validity, so no diagnostic is asserted
//     either way.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiPort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("my_pkg", m_design->getAllPackages()); }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *getTypedef() {
    const hldb::Package *const pkg = getPkg();
    return (pkg == nullptr) ? nullptr : hldb::findByName<hldb::Typedef>("variable_type", pkg->getTypedefs());
  }

  static const hldb::Port *getPort(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Port>(name, top->getPorts());
  }

  static const hldb::Net *getNet(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Net>(name, top->getNets());
  }

  static void checkPort(std::string_view name) {
    const hldb::Port *const port = getPort(name);
    ASSERT_NE(port, nullptr) << "port '" << name << "' not found";
    EXPECT_EQ(port->getDirection(), vpiInput) << "23.2.2.3: '" << name << "' is an input";
    ASSERT_NE(port->getTypespec(), nullptr) << "23.2.2.3: '" << name << "' must carry data type 'variable_type'";
    ASSERT_NE(port->getTypespec()->getActual(), nullptr);
    const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(port->getTypespec()->getActual());
    ASSERT_NE(tts, nullptr) << "port '" << name << "' must be typed by typedef 'variable_type'";
    EXPECT_EQ(tts->getName(), "variable_type");
    EXPECT_EQ(tts->getTypedef(), getTypedef()) << "must resolve to my_pkg::variable_type";
  }

  static void checkNet(std::string_view name) {
    const hldb::Net *const net = getNet(name);
    ASSERT_NE(net, nullptr) << "23.2.2.3: input port '" << name << "' defaults to a net";
    EXPECT_EQ(net->getNetType(), vpiWire) << "default net type is wire";
    ASSERT_NE(net->getTypespec(), nullptr);
    ASSERT_NE(net->getTypespec()->getActual(), nullptr) << "net '" << name << "' must carry data type 'variable_type'";
    const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(net->getTypespec()->getActual());
    ASSERT_NE(tts, nullptr);
    EXPECT_EQ(tts->getName(), "variable_type");

    const hldb::Port *const port = getPort(name);
    ASSERT_NE(port, nullptr);
    const hldb::RefObj *const low = any_cast<hldb::RefObj>(port->getLowConn());
    ASSERT_NE(low, nullptr);
    EXPECT_EQ(low->getName(), name);
    EXPECT_EQ(low->getActual(), net) << "port '" << name << "' low connection binds to its internal net";
  }
};

// ===========================================================================
// 7.2: package my_pkg / typedef struct { ... } variable_type
// ===========================================================================

TEST_F(MultiPortTest, PackageAndTypedefExist) {
  ASSERT_NE(getPkg(), nullptr);
  EXPECT_NE(getTypedef(), nullptr);
}

TEST_F(MultiPortTest, VariableTypeIsUnpackedStructWithTwoMembers) {
  const hldb::Typedef *const td = getTypedef();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const sts = any_cast<hldb::StructTypespec>(td->getAlias()->getActual());
  ASSERT_NE(sts, nullptr);
  const hldb::Struct *const st = sts->getStruct();
  ASSERT_NE(st, nullptr);
  EXPECT_FALSE(st->getPacked()) << "7.2: no 'packed' keyword -> unpacked struct";
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);

  const hldb::TypespecMember *const first = st->getMembers()->at(0);
  EXPECT_EQ(first->getName(), "first");
  ASSERT_NE(first->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(first->getTypespec()->getActual());
  ASSERT_NE(lt, nullptr) << "'first' is logic [2:0]";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);

  const hldb::TypespecMember *const second = st->getMembers()->at(1);
  EXPECT_EQ(second->getName(), "second");
  ASSERT_NE(second->getTypespec(), nullptr);
  ASSERT_NE(second->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(second->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "'second' is int";
}

// ===========================================================================
// 26.4: import in module header
// ===========================================================================

TEST_F(MultiPortTest, HeaderImportRecorded) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::ImportTypespec *imp = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) {
      imp = it;
      break;
    }
  }
  ASSERT_NE(imp, nullptr) << "'import my_pkg::variable_type;' in module header";
  EXPECT_EQ(imp->getName(), "my_pkg");
  ASSERT_NE(imp->getItem(), nullptr);
  EXPECT_EQ(imp->getItem()->getDecompile(), "variable_type");
}

TEST_F(MultiPortTest, ImportedTypeResolves) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "variable_type"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_PACKAGE, "my_pkg"), nullptr);
}

// ===========================================================================
// 23.2.2.3: ports a, b
// ===========================================================================

TEST_F(MultiPortTest, TopHasTwoPortsInOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  EXPECT_EQ(top->getPorts()->at(0)->getName(), "a");
  EXPECT_EQ(top->getPorts()->at(1)->getName(), "b");
}

TEST_F(MultiPortTest, PortAIsInputVariableType) { checkPort("a"); }

TEST_F(MultiPortTest, PortBInheritsInputVariableType) { checkPort("b"); }

TEST_F(MultiPortTest, NetAIsWireOfVariableType) { checkNet("a"); }

TEST_F(MultiPortTest, NetBIsWireOfVariableType) { checkNet("b"); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
