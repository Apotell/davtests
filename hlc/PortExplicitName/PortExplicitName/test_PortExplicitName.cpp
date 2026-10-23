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

// Explicitly named ports (tests/PortExplicitName/dut.sv):
//
//   IEEE 1800-2023 Sec 37.14   ports -> bool vpiExplicitName
//   IEEE 1800-2023 Sec 23.2.2.1 non-ANSI port ::= . port_identifier ( [ port_expression ] )
//   IEEE 1800-2023 Sec 23.2.2.2 ANSI ansi_port_declaration ::=
//                               [ port_direction ] . port_identifier ( [ expression ] )

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>

#include <gtest/gtest.h>

namespace hlc {

class PortExplicitNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PortExplicitName.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Port *findPort(std::string_view moduleName, std::string_view portName) const {
    const hldb::Module *const m = hldb::findByName<hldb::Module>(moduleName, m_design->getAllModules());
    if ((m == nullptr) || (m->getPorts() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Port>(portName, m->getPorts());
  }
};

TEST_F(PortExplicitNameTest, NonAnsiPlainPortIsNotExplicit) {
  const hldb::Port *const p = findPort("nonansi_ports", "a");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getExplicitName());
}

TEST_F(PortExplicitNameTest, NonAnsiExplicitPortWithExpression) {
  const hldb::Port *const p = findPort("nonansi_ports", "b");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getExplicitName());
}

TEST_F(PortExplicitNameTest, NonAnsiExplicitPortWithoutExpression) {
  const hldb::Port *const p = findPort("nonansi_ports", "d");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getExplicitName());
}

TEST_F(PortExplicitNameTest, AnsiPlainPortsAreNotExplicit) {
  const hldb::Port *const e = findPort("ansi_ports", "e");
  ASSERT_NE(e, nullptr);
  EXPECT_FALSE(e->getExplicitName());
  const hldb::Port *const i = findPort("ansi_ports", "i");
  ASSERT_NE(i, nullptr);
  EXPECT_FALSE(i->getExplicitName());
}

TEST_F(PortExplicitNameTest, AnsiExplicitPortWithExpression) {
  const hldb::Port *const p = findPort("ansi_ports", "f");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getExplicitName());
}

TEST_F(PortExplicitNameTest, AnsiExplicitPortWithoutExpression) {
  const hldb::Port *const p = findPort("ansi_ports", "h");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getExplicitName());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
