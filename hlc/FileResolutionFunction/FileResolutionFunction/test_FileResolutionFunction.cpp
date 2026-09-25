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

// Tests for tests/FileResolutionFunction/dut.sv:
//
//   nettype real my_real with my_function2;
//
//   function automatic real my_function2(input real driver []);
//   endfunction
//
// IEEE 1800-2023 Sec 6.6.9 "User-defined nettypes with resolution
// functions": 'nettype real my_real with my_function2;' declares a new net
// type 'my_real' whose base type is 'real' and whose resolution function is
// 'my_function2'. Per the same section, a resolution function must have:
// a return type matching (or castable to) the nettype's base type, exactly
// one formal argument, and that argument must be an unpacked array of the
// nettype's base type. 'my_function2' is declared AFTER the 'nettype'
// declaration that references it, so this also exercises forward
// reference resolution within the same compilation unit.
//
// UHDM models a nettype_declaration as a Typedef with getIsNettype() true
// and getResolutionFunc() populated with a RefObj to the resolution
// function (hldb/typedef.h), rather than as a dedicated "Nettype" class.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/real_typespec.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class FileResolutionFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileResolutionFunction.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Typedef *getMyReal() {
    if (m_design->getTypedefs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("my_real", m_design->getTypedefs());
  }
  static const hldb::Function *getMyFunction2() {
    if (m_design->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("my_function2", m_design->getTaskFuncs());
  }
};

TEST_F(FileResolutionFunctionTest, NettypeMyRealExists) {
  ASSERT_NE(getMyReal(), nullptr) << "'nettype real my_real with my_function2;' not found";
}

// 6.6.9: this Typedef must be flagged as a nettype declaration, not a plain
// typedef.
TEST_F(FileResolutionFunctionTest, MyRealIsFlaggedAsNettype) {
  const hldb::Typedef *const td = getMyReal();
  ASSERT_NE(td, nullptr);
  EXPECT_TRUE(td->getIsNettype()) << "6.6.9: 'nettype ...' must set getIsNettype()";
}

// 6.6.9: the nettype's base type is 'real'.
TEST_F(FileResolutionFunctionTest, MyRealBaseTypeIsReal) {
  const hldb::Typedef *const td = getMyReal();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr) << "nettype base type reference must be non-null";
  ASSERT_NE(td->getAlias()->getActual(), nullptr) << "'real' base type must resolve to an actual typespec";
  EXPECT_NE(td->getAlias()->getActual<hldb::RealTypespec>(), nullptr) << "6.6.9: base type of 'my_real' must be 'real'";
}

// 6.6.9: 'with my_function2' -- the resolution function reference.
TEST_F(FileResolutionFunctionTest, MyRealResolutionFuncIsMyFunction2) {
  const hldb::Typedef *const td = getMyReal();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getResolutionFunc(), nullptr) << "6.6.9: 'with my_function2' must populate getResolutionFunc()";
  EXPECT_EQ(td->getResolutionFunc()->getName(), std::string_view{"my_function2"});
}

TEST_F(FileResolutionFunctionTest, FunctionMyFunction2Exists) {
  ASSERT_NE(getMyFunction2(), nullptr) << "'function automatic real my_function2(...)' not found";
}

// 6.6.9: a resolution function's return type must match the nettype's base
// type ('real' here).
TEST_F(FileResolutionFunctionTest, MyFunction2ReturnsReal) {
  const hldb::Function *const func = getMyFunction2();
  ASSERT_NE(func, nullptr);
  ASSERT_NE(func->getReturn(), nullptr);
  ASSERT_NE(func->getReturn()->getActual(), nullptr) << "return type must resolve to an actual typespec";
  EXPECT_NE(func->getReturn()->getActual<hldb::RealTypespec>(), nullptr) << "6.6.9: resolution function must return 'real'";
}

// 6.6.9: exactly one formal argument, an (unpacked array) 'real' input.
TEST_F(FileResolutionFunctionTest, MyFunction2HasSingleRealArrayInput) {
  const hldb::Function *const func = getMyFunction2();
  ASSERT_NE(func, nullptr);
  ASSERT_NE(func->getIODecls(), nullptr);
  ASSERT_EQ(func->getIODecls()->size(), 1u) << "6.6.9: a resolution function must have exactly one formal argument";
  const hldb::IODecl *const driver = func->getIODecls()->at(0);
  EXPECT_EQ(driver->getName(), std::string_view{"driver"});
  EXPECT_EQ(driver->getDirection(), vpiInput);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
