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

// Generic interface ports (tests/GenericInterfacePort/dut.sv):
//
//   IEEE 1800-2023 Sec 37.15 detail 5  ref obj -> bool vpiGeneric: TRUE if the ref obj
//                                      references a generic interface, FALSE otherwise.
//   IEEE 1800-2023 Sec 37.14 detail 5  vpiLowConn of an interface port is a ref obj.
//   IEEE 1800-2023 Sec 25.3.3          generic interface reference ("interface" keyword).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>

#include <gtest/gtest.h>

namespace hlc {

class GenericInterfacePortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenericInterfacePort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::RefObj *findLowConn(std::string_view portName) const {
    const hldb::Module *const m = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((m == nullptr) || (m->getPorts() == nullptr)) return nullptr;
    const hldb::Port *const p = hldb::findByName<hldb::Port>(portName, m->getPorts());
    if ((p == nullptr) || (p->getLowConn() == nullptr)) return nullptr;
    EXPECT_EQ(p->getLowConn()->getAnyType(), hldb::AnyType::RefObj) << portName;
    return p->getLowConn<hldb::RefObj>();
  }
};

TEST_F(GenericInterfacePortTest, GenericInterfacePortIsGeneric) {
  const hldb::RefObj *const ro = findLowConn("gen");
  ASSERT_NE(ro, nullptr);
  EXPECT_TRUE(ro->getGeneric());
}

TEST_F(GenericInterfacePortTest, GenericModportPortIsGeneric) {
  const hldb::RefObj *const ro = findLowConn("gen_mp");
  ASSERT_NE(ro, nullptr);
  EXPECT_TRUE(ro->getGeneric());
}

TEST_F(GenericInterfacePortTest, NamedModportPortIsNotGeneric) {
  const hldb::RefObj *const ro = findLowConn("named_mp");
  ASSERT_NE(ro, nullptr);
  EXPECT_FALSE(ro->getGeneric());
}

TEST_F(GenericInterfacePortTest, NamedInterfacePortIsNotGeneric) {
  const hldb::RefObj *const ro = findLowConn("named");
  ASSERT_NE(ro, nullptr);
  EXPECT_FALSE(ro->getGeneric());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
