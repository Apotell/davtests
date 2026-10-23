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

// Type parameter properties (tests/TypeParameterProps/dut.sv):
//
//   IEEE 1800-2023 Sec 37.28 (diagram)  vpiLocalParam is TRUE for a localparam. Per
//                                       Sec 6.20.1/6.20.4 a 'parameter' is an implicit localparam
//                                       in a package, a class body, and the body of a
//                                       module/interface/program that has a parameter_port_list;
//                                       it is an ordinary parameter in a parameter_port_list and
//                                       in the body of a module without one.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/type_parameter.h>

#include <gtest/gtest.h>

namespace hlc {

class TypeParameterPropsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TypeParameterProps.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::TypeParameter *paramIn(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::TypeParameter>(name, scope->getParameters());
  }

  // Looked up by vpiDefName: a parameterized definition's vpiName also carries its parameters.
  template <typename T>
  static const T *findByDefName(std::string_view defName, const std::vector<T *> *collection) {
    if (collection == nullptr) return nullptr;
    for (const T *item : *collection) {
      if (item->getDefName() == defName) return item;
    }
    return nullptr;
  }

  const hldb::TypeParameter *moduleParam(std::string_view module, std::string_view name) const {
    return paramIn(findByDefName(module, m_design->getAllModules()), name);
  }

  const hldb::TypeParameter *packageParam(std::string_view name) const {
    return paramIn(hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()), name);
  }

  const hldb::TypeParameter *classParam(std::string_view name) const {
    return paramIn(findByDefName("C", m_design->getAllClasses()), name);
  }
};

TEST_F(TypeParameterPropsTest, ExplicitLocalparamIsLocal) {
  const hldb::TypeParameter *const portList = moduleParam("with_ports", "LP");
  ASSERT_NE(portList, nullptr);
  EXPECT_TRUE(portList->getLocalParam());
  const hldb::TypeParameter *const body = moduleParam("with_ports", "L");
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(body->getLocalParam());
}

TEST_F(TypeParameterPropsTest, ImplicitLocalparams) {
  const hldb::TypeParameter *const body = moduleParam("with_ports", "BODY");
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(body->getLocalParam());
  const hldb::TypeParameter *const pkg = packageParam("PT");
  ASSERT_NE(pkg, nullptr);
  EXPECT_TRUE(pkg->getLocalParam());
  const hldb::TypeParameter *const cls = classParam("CB");
  ASSERT_NE(cls, nullptr);
  EXPECT_TRUE(cls->getLocalParam());
}

TEST_F(TypeParameterPropsTest, OrdinaryParametersAreNotLocal) {
  const hldb::TypeParameter *const port = moduleParam("with_ports", "T");
  ASSERT_NE(port, nullptr);
  EXPECT_FALSE(port->getLocalParam());
  const hldb::TypeParameter *const body = moduleParam("no_ports", "P");
  ASSERT_NE(body, nullptr);
  EXPECT_FALSE(body->getLocalParam());
  const hldb::TypeParameter *const classPort = classParam("CT");
  ASSERT_NE(classPort, nullptr);
  EXPECT_FALSE(classPort->getLocalParam());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
