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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {
class AaFirstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AaFirstTest.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AaFirstTest, default) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Port *const port = hldb::findByName<hldb::Port>("a", module->getPorts());
  ASSERT_NE(port, nullptr) << "Port is null";

  // Per IEEE 1800-2023 Sec 23.2.2.3, an ANSI "input" port always defaults to
  // a net (the explicit data type "logic" does not change this -- only an
  // "output" port with an explicit data type defaults to a variable). With
  // no `default_nettype` override in effect, that net is a wire.
  const hldb::Net *const net = hldb::findByName<hldb::Net>("a", module->getNets());
  ASSERT_NE(net, nullptr) << "Net is null";
  EXPECT_EQ(net->getNetType(), vpiWire);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("a", module->getVariables()), nullptr)
      << "'a' is an input port -- it must not also appear in vpiVariables";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
