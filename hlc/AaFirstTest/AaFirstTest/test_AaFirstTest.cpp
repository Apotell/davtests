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

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/port.h>

namespace SURELOG {
class AaFirstTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "AaFirstTest.hlc", "-d", "uhdm"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(AaFirstTest, default) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Port *const port = uhdm::findByName<uhdm::Port>("a", module->getPorts());
  ASSERT_NE(port, nullptr) << "Port is null";
}
}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
