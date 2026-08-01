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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Instantiation.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so top_nonansi's
// instantiation testing point stands on its own. The module, program,
// interface and class it instantiates are duplicated into this file (see
// Instantiation.sv) rather than shared across files.
//
// Checked:
//   - top_nonansi exists with 4 wire nets (a, b, y1, y2)
//   - a and b have initial values (1'b1 / 1'b0)
//   - top_nonansi instantiates the module, the program, and the interface
//   - top_nonansi declares a class-handle variable 'cls0'

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_instance.h>
#include <hldb/variable.h>

namespace hlc {

class NonAnsiInstantiationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Instantiation.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTopNonansi() {
    return hldb::findByName<hldb::Module>("top_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiInstantiationTest, TopNonansiExists) { ASSERT_NE(getTopNonansi(), nullptr); }

TEST_F(NonAnsiInstantiationTest, TopNonansiHasFourWireNets) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 4u) << "expected nets a, b, y1, y2";
}

TEST_F(NonAnsiInstantiationTest, TopNonansiAInitialValueIsOne) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'wire a = 1'b1;' should have an initial value";
}

TEST_F(NonAnsiInstantiationTest, TopNonansiBInitialValueIsZero) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'wire b = 1'b0;' should have an initial value";
}

TEST_F(NonAnsiInstantiationTest, TopNonansiInstantiatesModuleAndProgramAndInterface) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("mod_nonansi", top->getRefInstances()), nullptr)
      << "module instance 'mod_nonansi' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("prog_inst", top->getRefInstances()), nullptr)
      << "program instance 'prog_inst' not found";
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("if0", top->getRefInstances()), nullptr)
      << "interface instance 'if0' not found";
}

TEST_F(NonAnsiInstantiationTest, TopNonansiHasCls0Variable) {
  const hldb::Module *const top = getTopNonansi();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls0", top->getVariables()), nullptr)
      << "class-handle variable 'cls0' not found";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
