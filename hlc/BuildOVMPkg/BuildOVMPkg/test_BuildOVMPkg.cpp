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

// Tests for ovm-2.1.2/src/ovm_pkg.sv (third_party/UVM) -- a complete, unmodified, real-world
// SystemVerilog class library (not a synthetic dut.sv), compiled standalone via
// BuildOVMPkg.hlc. Unlike StaticElaboration/test_StaticElaboration.cpp (which exercises every
// parameterizable definition kind and every scoped-lookup corner case against a small, purpose
// -built dut.sv), this file's job is to confirm that the same static-elaboration (definition
// -level parameter specialization, see memory note project_static_elaboration_plan) and binding
// logic holds up against a large, independently-authored package that was never written with
// HLC's own object model in mind.
//
// Concrete anchor used throughout: ovm_registry.svh declares a single, un-parameterized-at-its
// -own-use-site generic proxy class,
//     class ovm_object_registry #(type T=ovm_object, string Tname="<unknown>") extends
//     ovm_object_wrapper;
// and two real (not macro-expanded) classes inside ovm_objection.svh each use it with their own,
// distinct, concrete #(...) override via a class-scoped typedef:
//     ovm_objection::type_id          -> typedef ovm_object_registry#(ovm_objection,"ovm_objection") type_id;
//     ovm_test_done_objection::type_id -> typedef ovm_object_registry#(ovm_test_done_objection,"ovm_test_done") type_id;
// IEEE 1800-2023 Sec 8.25 (Parameterized classes) requires each distinct #(...) override to
// resolve against its own, independently specialized definition -- never the generic definition
// itself, and never collapsed together with a different override of the same base. These two
// real use sites, sharing one base class but differing in both the class-typed parameter T and
// the string-typed parameter Tname, are an end-to-end check of exactly that requirement.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>

namespace hlc {

class BuildOVMPkgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "BuildOVMPkg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getOvmPkg() {
    return hldb::findByName<hldb::Package>("ovm_pkg", m_design->getAllPackages());
  }
  static const hldb::ClassDefn *getClass(std::string_view defName) {
    return hldb::findByDefName<hldb::ClassDefn>(defName, m_design->getAllClasses());
  }
  static const hldb::Typedef *getTypedef(const hldb::ClassDefn *owner, std::string_view name) {
    if ((owner == nullptr) || (owner->getTypedefs() == nullptr)) return nullptr;
    for (const hldb::Typedef *const td : *owner->getTypedefs()) {
      if (td->getName() == name) return td;
    }
    return nullptr;
  }

  // Normalizes a Typedef's own aliased type down to the ClassTypespec it resolves to, verifying
  // along the way that the chain is shaped exactly as Phase3ModelBuilder::bindClassTypespec /
  // resolveUnsupportedTypespec are expected to leave it (see those functions' own comments in
  // src/DesignCompile/Phase3ModelBuilder.cpp): Typedef -> RefTypespec (getAlias()) -> actual
  // ClassTypespec. Uses the non-templated getActual()/getAnyType() pair rather than a single
  // templated getActual<ClassTypespec>() call, per davtests' own templated-getXX<T>() pitfall
  // (a templated downcast conflates "genuinely null" with "non-null but a different type").
  static const hldb::ClassTypespec *resolveAliasClassTypespec(const hldb::Typedef *td) {
    if (td == nullptr) return nullptr;
    const hldb::RefTypespec *const alias = td->getAlias();
    if (alias == nullptr) return nullptr;
    if (alias->getActual() == nullptr) return nullptr;
    if (alias->getActual()->getAnyType() != hldb::AnyType::ClassTypespec) return nullptr;
    return alias->getActual<hldb::ClassTypespec>();
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::ClassTypespec *ct, std::string_view lhsName) {
    if ((ct == nullptr) || (ct->getParamAssigns() == nullptr)) return nullptr;
    return hldb::findByName<hldb::ParamAssign>(lhsName, ct->getParamAssigns());
  }
};

// ---- Sanity: the real, unmodified OVM library compiles and binds through this pipeline at all.
TEST_F(BuildOVMPkgTest, OvmPkgCompilesAndBindsCoreClasses) {
  const hldb::Package *const pkg = getOvmPkg();
  ASSERT_NE(pkg, nullptr) << "package 'ovm_pkg' not found";
  ASSERT_NE(pkg->getClassDefns(), nullptr);
  EXPECT_NE(pkg->getClassDefns()->size(), 0U);

  EXPECT_EQ(findEvent(EventType::CompFailedToBind, "ovm_objection"), nullptr);
  EXPECT_EQ(findEvent(EventType::CompFailedToBind, "ovm_object_registry"), nullptr);
  EXPECT_EQ(findEvent(EventType::CompFailedToBind, "ovm_test_done_objection"), nullptr);

  ASSERT_NE(getClass("ovm_objection"), nullptr);
  ASSERT_NE(getClass("ovm_test_done_objection"), nullptr);
  ASSERT_NE(getClass("ovm_object_registry"), nullptr);
  ASSERT_NE(getClass("ovm_object_wrapper"), nullptr);
}

// ---- The generic base definition itself must never be mutated into/mistaken for a
// ---- specialization, regardless of how many distinct use sites specialize it elsewhere.
TEST_F(BuildOVMPkgTest, OvmObjectRegistryGenericBaseIsNotASpecialization) {
  const hldb::ClassDefn *const base = getClass("ovm_object_registry");
  ASSERT_NE(base, nullptr);
  EXPECT_FALSE(base->getIsSpecialization());
  EXPECT_EQ(base->getSpecializedFrom(), nullptr);

  // extends ovm_object_wrapper (itself not parameterized) -- confirms the extends chain still
  // resolves through the ordinary RefTypespec/ClassTypespec shape even though ovm_object_registry
  // is this class's own `extends`, i.e. a parameterized class deriving from a non-parameterized
  // one, with no #(...) override needed on the base clause.
  ASSERT_NE(base->getExtends(), nullptr);
  ASSERT_NE(base->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(base->getExtends()->getClassTypespecs()->size(), 1U);
  const hldb::RefTypespec *const baseExtendsRt = base->getExtends()->getClassTypespecs()->front();
  ASSERT_NE(baseExtendsRt, nullptr);
  ASSERT_NE(baseExtendsRt->getActual(), nullptr);
  ASSERT_EQ(baseExtendsRt->getActual()->getAnyType(), hldb::AnyType::ClassTypespec);
  const hldb::ClassTypespec *const baseExtendsCt = baseExtendsRt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(baseExtendsCt->getClassDefn(), nullptr);
  EXPECT_EQ(baseExtendsCt->getClassDefn()->getDefName(), std::string_view("ovm_object_wrapper"));
  EXPECT_FALSE(baseExtendsCt->getClassDefn()->getIsSpecialization());
}

// ---- ovm_objection::type_id resolves to a real, distinct specialization of
// ---- ovm_object_registry#(T=ovm_objection, Tname="ovm_objection").
TEST_F(BuildOVMPkgTest, OvmObjectionTypeIdSpecializesOvmObjectRegistry) {
  const hldb::ClassDefn *const ovmObjection = getClass("ovm_objection");
  ASSERT_NE(ovmObjection, nullptr);

  const hldb::Typedef *const typeId = getTypedef(ovmObjection, "type_id");
  ASSERT_NE(typeId, nullptr) << "'type_id' typedef not found on ovm_objection";

  const hldb::ClassTypespec *const ct = resolveAliasClassTypespec(typeId);
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(ct->getClassDefn(), nullptr);

  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  EXPECT_EQ(specialized->getDefName(), std::string_view("ovm_object_registry"));
  EXPECT_TRUE(specialized->getIsSpecialization());

  ASSERT_NE(specialized->getSpecializedFrom(), nullptr);
  EXPECT_EQ(specialized->getSpecializedFrom(), getClass("ovm_object_registry"));
  EXPECT_FALSE(specialized->getSpecializedFrom()->getIsSpecialization());
  EXPECT_NE(specialized, specialized->getSpecializedFrom())
      << "a specialization must be a distinct definition object from its generic base "
         "(IEEE 1800-2023 Sec 8.25) -- static elaboration must never mutate the shared base";

  // T=ovm_objection -- a class-typed parameter, so its rhs is itself a RefTypespec resolving to
  // a ClassTypespec whose ClassDefn is the (non-specialized -- plain class reference) ovm_objection.
  const hldb::ParamAssign *const paramT = getParamAssign(ct, "T");
  ASSERT_NE(paramT, nullptr) << "no ParamAssign found for formal parameter 'T'";
  ASSERT_NE(paramT->getRhs(), nullptr);
  ASSERT_EQ(paramT->getRhs()->getAnyType(), hldb::AnyType::RefTypespec);
  const hldb::RefTypespec *const paramTRt = paramT->getRhs<hldb::RefTypespec>();
  ASSERT_NE(paramTRt->getActual(), nullptr);
  ASSERT_EQ(paramTRt->getActual()->getAnyType(), hldb::AnyType::ClassTypespec);
  const hldb::ClassTypespec *const paramTCt = paramTRt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(paramTCt->getClassDefn(), nullptr);
  EXPECT_EQ(paramTCt->getClassDefn(), ovmObjection);
  EXPECT_FALSE(paramTCt->getClassDefn()->getIsSpecialization());

  // Tname="ovm_objection" -- a string-typed parameter, so its rhs is a plain string Constant.
  const hldb::ParamAssign *const paramTname = getParamAssign(ct, "Tname");
  ASSERT_NE(paramTname, nullptr) << "no ParamAssign found for formal parameter 'Tname'";
  ASSERT_NE(paramTname->getRhs(), nullptr);
  ASSERT_EQ(paramTname->getRhs()->getAnyType(), hldb::AnyType::Constant);
  const hldb::Constant *const paramTnameConst = paramTname->getRhs<hldb::Constant>();
  EXPECT_EQ(paramTnameConst->getConstType(), vpiStringConst);
  EXPECT_EQ(paramTnameConst->getDecompile(), std::string_view("\"ovm_objection\""));
}

// ---- ovm_test_done_objection::type_id specializes the SAME generic base with DIFFERENT
// ---- parameter values, and must resolve to a DIFFERENT specialized definition than
// ---- ovm_objection::type_id's -- static elaboration's own memoization must key on the actual
// ---- override values, not merely on which base definition is being specialized.
TEST_F(BuildOVMPkgTest, OvmTestDoneObjectionTypeIdIsADistinctSpecialization) {
  const hldb::ClassDefn *const ovmTestDoneObjection = getClass("ovm_test_done_objection");
  ASSERT_NE(ovmTestDoneObjection, nullptr);

  const hldb::Typedef *const typeId = getTypedef(ovmTestDoneObjection, "type_id");
  ASSERT_NE(typeId, nullptr) << "'type_id' typedef not found on ovm_test_done_objection";

  const hldb::ClassTypespec *const ct = resolveAliasClassTypespec(typeId);
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(ct->getClassDefn(), nullptr);

  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  EXPECT_EQ(specialized->getDefName(), std::string_view("ovm_object_registry"));
  EXPECT_TRUE(specialized->getIsSpecialization());
  EXPECT_EQ(specialized->getSpecializedFrom(), getClass("ovm_object_registry"));

  const hldb::ParamAssign *const paramT = getParamAssign(ct, "T");
  ASSERT_NE(paramT, nullptr);
  ASSERT_NE(paramT->getRhs(), nullptr);
  ASSERT_EQ(paramT->getRhs()->getAnyType(), hldb::AnyType::RefTypespec);
  const hldb::RefTypespec *const paramTRt = paramT->getRhs<hldb::RefTypespec>();
  ASSERT_NE(paramTRt->getActual(), nullptr);
  ASSERT_EQ(paramTRt->getActual()->getAnyType(), hldb::AnyType::ClassTypespec);
  EXPECT_EQ(paramTRt->getActual<hldb::ClassTypespec>()->getClassDefn(), ovmTestDoneObjection);

  const hldb::ParamAssign *const paramTname = getParamAssign(ct, "Tname");
  ASSERT_NE(paramTname, nullptr);
  ASSERT_NE(paramTname->getRhs(), nullptr);
  ASSERT_EQ(paramTname->getRhs()->getAnyType(), hldb::AnyType::Constant);
  EXPECT_EQ(paramTname->getRhs<hldb::Constant>()->getDecompile(), std::string_view("\"ovm_test_done\""));

  // The crux of this test: two different #(...) overrides of the same base must never collapse
  // onto the same specialized object.
  const hldb::ClassDefn *const ovmObjection = getClass("ovm_objection");
  ASSERT_NE(ovmObjection, nullptr);
  const hldb::ClassTypespec *const otherCt = resolveAliasClassTypespec(getTypedef(ovmObjection, "type_id"));
  ASSERT_NE(otherCt, nullptr);
  ASSERT_NE(otherCt->getClassDefn(), nullptr);
  EXPECT_NE(specialized, otherCt->getClassDefn())
      << "ovm_objection::type_id and ovm_test_done_objection::type_id specialize "
         "ovm_object_registry with different T/Tname overrides and must not share one definition";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
