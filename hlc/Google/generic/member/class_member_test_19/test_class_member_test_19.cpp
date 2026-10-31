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

// Validates the HLDB model built for
// tests/Google/generic/member/class_member_test_19.sv. Lines 1-7 are
// one-line "//" license comments, lines 8-9 are blank, lines 10-14 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   15  class myclass;
//   16  typedef logic bool;
//   17  localparam int N = 2;
//   18  extern function void subr(bool x[N]);
//   19  endclass
//   20
//   21  function void myclass::subr(bool x[N]); endfunction
//   22
//   23  module test;
//   24  endmodule
//
// The fixture is tagged 8.3 (class declarations).
//
// class myclass; ... endclass (8.3, A.1.2)
//   - A class named myclass, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list (8.25), no 'extends', no 'implements', no
//     property, no constraint, no nested class and no end label.
//   - Exactly three class items: the typedef bool, the local parameter N
//     and the extern method prototype subr.
//
// typedef logic bool; (6.18, 8.3)
//   - A type declaration inside the class body, so bool is a typedef of
//     myclass, not of the compilation unit. It aliases logic (6.11), a
//     single bit (no packed dimension).
//
// localparam int N = 2; (6.20.4, 8.3)
//   - A local parameter of myclass (class_item ::= local_parameter_declaration)
//     named N, of type int (6.11), with value 2. It is a parameter of the
//     class, not of the compilation unit.
//
// extern function void subr(bool x[N]); (8.24, 13.4, 7.4.2, A.1.9)
//   - A function prototype of myclass, a class method (8.6) with no
//     'virtual' (8.20) or 'pure' (8.21) qualifier, returning void (13.4.1).
//   - One argument, x. No direction is written, so it is an input (13.3:
//     "The default direction of a task argument is input"; 13.4 applies
//     the same to functions). 'bool x[N]' declares x as a fixed-size
//     unpacked array (7.4.2: '[N]' is the C-style '[0:N-1]') of N = 2
//     elements of the class typedef bool.
//
// function void myclass::subr(bool x[N]); endfunction (8.24)
//   - 8.24: the out-of-block declaration is the body of the extern method
//     subr of myclass ('myclass::' binds it to the class). It is a class
//     method, not a compilation-unit function.
//   - 8.24: after 'myclass::subr' the declaration is in the scope of
//     myclass, so 'bool' and 'N' in its argument list resolve to the class
//     typedef bool and the class parameter N (only a return type would need
//     'myclass::'; 'void' needs none).
//   - It is a function returning void, automatic (8.6: "Methods declared as
//     part of a class type have automatic lifetime"), not virtual, not
//     pure, with the prototype's argument list (8.24), no statement and no
//     end label.
//   - The prototype's implementation is this body.
//
// module test; endmodule (23.2)
//   - A module named test, declared once, with no port list and no module
//     items. test is never instantiated, so it is a top-level module (23.3.1).
//     The instance tree and the top-module flag only exist after elaboration.
//
// The file is legal, so it compiles without errors. The .hlc also puts UVM
// 1800.2-2017 on the include path ('-I') but adds no UVM source file, and
// passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal, syntax or error diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - Exactly one class myclass: user-defined, not an interface class, not
//     virtual, with no base class, implemented class, property, constraint,
//     nested class or end label.
//   - myclass has exactly one typedef, bool, aliasing a single-bit logic.
//   - myclass has exactly one parameter, N: a local parameter of type int
//     whose value is 2.
//   - myclass has exactly one method prototype, subr: a function, a class
//     method, not virtual, not pure, returning void, with one input argument
//     x that is a fixed-size unpacked array of 2 elements of the class
//     typedef bool.
//   - myclass has exactly one method with a body, subr: a function, a class
//     method, automatic, not virtual, not pure, returning void, with the
//     same argument x, no statement and no end label.
//   - The prototype's implementation is that body.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no other compilation-unit declaration (in
//     particular, neither bool, N nor the out-of-block body is a
//     compilation-unit declaration).
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// KNOWN FAILURE, not a defect in this test (HLC 577):
//   - Out-of-block body not flagged as a method: HLC records the body
//     'function void myclass::subr' in myclass's getMethods(), links the
//     prototype to it and resolves its argument type, but getMethod()
//     (vpiMethod) is false on that body, although 8.24 makes it the body
//     of a method of myclass (the prototype itself is flagged). The
//     getMethod() check of MyClassHasOutOfBlockSubrBody fails.
//   - Array size not computed: on both the prototype's and the body's
//     argument x, the unpacked dimension '[N]' (N = 2) has
//     Range::getSize() == 0 instead of 2 (7.4.2). The element type, bool,
//     and N's own value (2) are modeled correctly. This model is not
//     elaborated, and no other test in the suite asserts Range::getSize(),
//     so the size may only be filled in at elaboration; the check is kept
//     because 6.20/7.4.2 fix N = 2 at compile time. The size checks of
//     MyClassHasOneExternVoidFunctionPrototype and
//     MyClassHasOutOfBlockSubrBody fail.
//   Both assert the LRM behavior and are intentionally not skipped or
//   relaxed.
//
// Model note: HLDB records method prototypes as TaskFuncDecl (here a
// FunctionDecl) in ClassDefn::getTaskFuncDecls(); getMethods() holds
// methods with a body, and TaskFuncDecl::getTaskFunc() links a prototype to
// its body. A written 'void' return type is a VoidTypespec.
//
// What is NOT checked, and why
//   - How the 'myclass::' class_scope is encoded on the body (prefix or
//     otherwise): the binding it expresses is checked through getMethods()
//     and getTaskFunc() instead.
//   - The prototype's lifetime flag: a prototype has no body whose
//     variables a lifetime could govern; the body's lifetime is checked.
//   - subr's access (local/protected): none is written, so it is
//     public (8.18); not the subject of this fixture.
//   - The array's left/right bound expressions: '[N]' means '[0:N-1]'
//     (7.4.2), but whether HLDB keeps 'N' or the folded bounds is a model
//     convention; the size, 2, which the LRM fixes, is checked instead.
//   - getArrayType(): '[N]' is a fixed-size array (7.4.2), but the VPI
//     encoding of that kind is a model convention; getPacked() is checked.
//   - The license and metadata comments (5.4): comments carry no meaning.
//   - The SourceFile time unit and precision: they come from the
//     -timescale command-line option, not from the SystemVerilog text.
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/function.h>
#include <hldb/function_decl.h>
#include <hldb/include_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/let_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/task_func_decl.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_member_test_19.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  if (text.size() < suffix.size()) {
    return false;
  }
  return text.substr(text.size() - suffix.size()) == suffix;
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  if (collection == nullptr) {
    return 0;
  }
  return collection->size();
}

// Number of objects in 'collection' whose source location is the file under test.
template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) {
        ++count;
      }
    }
  }
  return count;
}

// Number of objects in 'collection' named 'name'.
template <typename T>
size_t countNamed(const std::vector<T *> *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() == name) {
        ++count;
      }
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &error) {
  if (error.getArguments().empty()) {
    return 0;
  }
  return error.getArguments().front().m_line;
}

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const EventListener *eventListener, EventSeverity severity) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if ((errorInfo == nullptr) || (errorInfo->m_severity != severity)) {
      continue;
    }
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->m_lines[0].m_text) + "]";
  }
  return description;
}

// Checks that 'returnType' is the written return type 'void' (13.4.1).
void expectVoidReturn(const hldb::RefTypespec *returnType, std::string_view where) {
  ASSERT_NE(returnType, nullptr) << where << ": 13.4.1: 'void' is written as the return type";
  EXPECT_NE(returnType->getActual<hldb::VoidTypespec>(), nullptr) << where << ": 13.4.1: subr returns void";
}
}  // namespace

class ClassMemberTest19Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_19.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design == nullptr) {
      return sourceFiles;
    }
    const hldb::SourceFileCollection *const collection = m_design->getSourceFiles();
    if (collection == nullptr) {
      return sourceFiles;
    }
    for (const hldb::SourceFile *sourceFile : *collection) {
      if (endsWith(sourceFile->getName(), kSourceFileName)) {
        sourceFiles.emplace_back(sourceFile);
      }
    }
    return sourceFiles;
  }

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    if (sourceFiles.size() != 1) {
      return nullptr;
    }
    return sourceFiles.front();
  }

  // The class myclass, or nullptr.
  static const hldb::ClassDefn *getMyClass() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  // The class typedef bool of myclass (line 16), or nullptr.
  static const hldb::Typedef *getBool() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Typedef>("bool", myClass->getTypedefs());
  }

  // The extern prototype subr of myclass (line 18), or nullptr.
  static const hldb::TaskFuncDecl *getSubrPrototype() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFuncDecl>("subr", myClass->getTaskFuncDecls());
  }

  // The out-of-block body of subr (line 21), a method of myclass, or nullptr.
  static const hldb::TaskFunc *getSubrBody() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFunc>("subr", myClass->getMethods());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }

  // The class parameter N of myclass (line 17), or nullptr.
  static const hldb::Parameter *getN() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Parameter>("N", myClass->getParameters());
  }

  // Checks that 'arguments' is exactly '(bool x[N])': one argument named x,
  // an input (13.3 default direction), whose type is a fixed-size unpacked
  // array (7.4.2) of N = 2 elements of the class typedef bool of myclass
  // (line 16).
  static void expectSingleBoolArrayX(const hldb::IODeclCollection *arguments, std::string_view where) {
    ASSERT_EQ(sizeOf(arguments), 1u) << where << " declares one argument";
    const hldb::IODecl *const x = arguments->at(0);
    EXPECT_EQ(x->getName(), std::string_view("x")) << where;
    EXPECT_EQ(x->getDirection(), vpiInput) << where << ": 13.3: the default direction of an argument is input";

    const hldb::RefTypespec *const type = x->getTypespec();
    ASSERT_NE(type, nullptr) << where << ": 'bool x[N]' has an explicit data type";
    const hldb::ArrayTypespec *const arrayType = type->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(arrayType, nullptr) << where << ": 7.4.2: '[N]' after x makes it an unpacked array";
    EXPECT_FALSE(arrayType->getPacked()) << where << ": 7.4: a dimension after the name is unpacked";
    ASSERT_NE(arrayType->getRange(), nullptr) << where << ": 'x[N]' has one dimension";
    EXPECT_EQ(arrayType->getRange()->getSize(), 2) << where << ": 7.4.2: '[N]' with N = 2 has 2 elements";

    const hldb::RefTypespec *const elemType = arrayType->getElemTypespec();
    ASSERT_NE(elemType, nullptr) << where << ": the array has an element type";
    const hldb::TypedefTypespec *const typedefTypespec = elemType->getActual<hldb::TypedefTypespec>();
    ASSERT_NE(typedefTypespec, nullptr) << where << ": 6.18: the element type is the typedef bool";
    EXPECT_EQ(typedefTypespec->getName(), std::string_view("bool")) << where;
    const hldb::Typedef *const boolType = typedefTypespec->getTypedef();
    ASSERT_NE(boolType, nullptr) << where;
    EXPECT_EQ(boolType, getBool()) << where << ": 8.24: bool resolves to the typedef declared in myclass";
  }
};

// Lines 15-24 are a legal class with a typedef, a local parameter and an extern method (8.24),
// its out-of-block body and a module (23.2), so nothing is fatal, a syntax
// error or an error.
TEST_F(ClassMemberTest19Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest19Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest19Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 15: 'class myclass;' declares the class once. It is written in the
// source, so it is user-defined (37.29 vpiClassType); it is neither an
// interface class (8.26) nor an abstract 'virtual class' (8.21).
TEST_F(ClassMemberTest19Test, MyClassIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 15 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); lines 16-18 are a typedef, a local parameter and a
// method prototype, so myclass has no property, constraint or nested class;
// line 19 is 'endclass' without ': myclass'.
TEST_F(ClassMemberTest19Test, MyClassHasNoOtherItems) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);

  EXPECT_EQ(myClass->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(myClass->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(myClass->getVariables()), 0u) << "8.5: no class property is declared";
  EXPECT_EQ(sizeOf(myClass->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(myClass->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(myClass->getEndLabelObj(), nullptr) << "'endclass' is written without ': myclass'";
}

// Line 16: 'typedef logic bool;' inside the class body declares one typedef
// of myclass (6.18, 8.3) aliasing logic (6.11) with no packed dimension.
TEST_F(ClassMemberTest19Test, BoolIsClassTypedefOfLogic) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(sizeOf(myClass->getTypedefs()), 1u) << "line 16 is the only type declaration inside myclass";

  const hldb::Typedef *const boolType = getBool();
  ASSERT_NE(boolType, nullptr) << "bool is a typedef of myclass";
  const hldb::RefTypespec *const alias = boolType->getAlias();
  ASSERT_NE(alias, nullptr) << "bool aliases a data type";
  const hldb::LogicTypespec *const logicType = alias->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicType, nullptr) << "6.11: bool aliases logic";
  EXPECT_EQ(sizeOf(logicType->getRanges()), 0u) << "'typedef logic bool' has no packed dimension";
}

// Line 17: 'localparam int N = 2;' declares the only parameter of myclass
// (no parameter port list is written, 8.25): a local parameter (6.20.4) of
// type int (6.11) whose value is 2.
TEST_F(ClassMemberTest19Test, NIsClassLocalParamIntEqualTo2) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(sizeOf(myClass->getParameters()), 1u) << "line 17 declares the only parameter of myclass";

  const hldb::Parameter *const n = getN();
  ASSERT_NE(n, nullptr) << "N is a parameter of myclass";
  EXPECT_TRUE(n->getLocalParam()) << "6.20.4: N is declared 'localparam'";
  const hldb::RefTypespec *const type = n->getTypespec();
  ASSERT_NE(type, nullptr) << "'localparam int N' has an explicit data type";
  EXPECT_NE(type->getActual<hldb::IntTypespec>(), nullptr) << "6.11: N is an int";

  const hldb::ParamAssign *value = nullptr;
  if (myClass->getParamAssigns() != nullptr) {
    for (const hldb::ParamAssign *paramAssign : *myClass->getParamAssigns()) {
      if ((paramAssign->getLhs() != nullptr) && (paramAssign->getLhs()->getName() == "N")) {
        value = paramAssign;
      }
    }
  }
  ASSERT_NE(value, nullptr) << "6.20.4: '= 2' assigns N its value";
  const hldb::Constant *const two = ::any_cast<hldb::Constant>(value->getRhs());
  ASSERT_NE(two, nullptr) << "N is assigned a literal";
  EXPECT_EQ(two->getDecompile(), std::string_view("2"));
}

// Line 18: 'extern function void subr(bool x[N]);' is the only method
// prototype of myclass (8.24): a function returning void (13.4.1), a class
// method (8.6), not virtual (8.20), not pure (8.21), with one input
// argument x, an unpacked array of N = 2 elements of the class type bool.
TEST_F(ClassMemberTest19Test, MyClassHasOneExternVoidFunctionPrototype) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  const hldb::TaskFuncDeclCollection *const prototypes = myClass->getTaskFuncDecls();
  ASSERT_EQ(sizeOf(prototypes), 1u) << "8.24: line 18 declares the only method prototype";

  const hldb::TaskFuncDecl *const prototype = prototypes->at(0);
  EXPECT_EQ(prototype->getName(), std::string_view("subr"));
  EXPECT_TRUE(prototype->getMethod()) << "8.6: subr is declared in the body of myclass";
  EXPECT_FALSE(prototype->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(prototype->getPure()) << "8.21: 'pure' is not written";
  expectSingleBoolArrayX(prototype->getIODecls(), "prototype subr");

  const hldb::FunctionDecl *const functionDecl = ::any_cast<hldb::FunctionDecl>(prototype);
  ASSERT_NE(functionDecl, nullptr) << "13.4: subr is declared 'function'";
  expectVoidReturn(functionDecl->getReturn(), "prototype subr");
}

// Line 21: 'function void myclass::subr(bool x[N]); endfunction'
// is the out-of-block body of the extern method (8.24), so it is the only
// method of myclass with a body: a function returning void, a class method,
// automatic (8.6), not virtual, not pure, with the prototype's argument list
// (8.24; bool and N resolve in myclass's scope), no statement and no end label.
TEST_F(ClassMemberTest19Test, MyClassHasOutOfBlockSubrBody) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  const hldb::TaskFuncCollection *const methods = myClass->getMethods();
  ASSERT_EQ(sizeOf(methods), 1u) << "8.24: line 21 is the body of myclass's extern method";

  const hldb::TaskFunc *const body = methods->at(0);
  EXPECT_EQ(body->getName(), std::string_view("subr"));
  EXPECT_TRUE(body->getMethod()) << "8.24: 'myclass::subr' is a method of myclass";
  EXPECT_TRUE(body->getAutomatic()) << "8.6: methods of a class have automatic lifetime";
  EXPECT_FALSE(body->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(body->getPure()) << "8.21: an extern method with a body is not pure";
  EXPECT_EQ(body->getStmt(), nullptr) << "13.4: the function body holds no statement";
  EXPECT_EQ(body->getEndLabelObj(), nullptr) << "'endfunction' is written without ': subr'";
  expectSingleBoolArrayX(body->getIODecls(), "body subr");

  const hldb::Function *const function = ::any_cast<hldb::Function>(body);
  ASSERT_NE(function, nullptr) << "13.4: subr is declared 'function'";
  expectVoidReturn(function->getReturn(), "body subr");
}

// 8.24: the out-of-block declaration on line 21 implements the extern
// prototype on line 18.
TEST_F(ClassMemberTest19Test, PrototypeIsImplementedByOutOfBlockBody) {
  const hldb::TaskFuncDecl *const prototype = getSubrPrototype();
  ASSERT_NE(prototype, nullptr);
  const hldb::TaskFunc *const body = getSubrBody();
  ASSERT_NE(body, nullptr);

  ASSERT_NE(prototype->getTaskFunc(), nullptr) << "8.24: extern subr is defined on line 21";
  EXPECT_EQ(prototype->getTaskFunc(), body) << "8.24: the prototype's implementation is myclass::subr";
}

// Lines 23-24: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest19Test, TestModuleIsEmpty) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u) << "the file declares exactly one module";

  const hldb::Module *const test = getTestModule();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getEndLabelObj(), nullptr) << "'endmodule' is written without ': test'";
  EXPECT_EQ(sizeOf(test->getPorts()), 0u) << "'module test;' has no port list";
  EXPECT_EQ(sizeOf(test->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(test->getNets()), 0u);
  EXPECT_EQ(sizeOf(test->getVariables()), 0u);
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
  EXPECT_EQ(sizeOf(test->getTaskFuncs()), 0u) << "subr is a method of myclass, not of test";
  EXPECT_EQ(sizeOf(test->getTypedefs()), 0u) << "bool is declared in myclass, not in test";
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "myclass is declared outside module test";
}

// Besides myclass and test the file declares no design element (3.2-3.12):
// myclass is the only class, and it is in no package.
TEST_F(ClassMemberTest19Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "myclass is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than myclass is declared: the
// out-of-block body on line 21 is a method of myclass (8.24), bool is a
// typedef and N a parameter of myclass (lines 16-17), and the file declares
// no let, parameter or variable.
TEST_F(ClassMemberTest19Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "8.24: myclass::subr is a method of myclass";
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u) << "bool is declared inside myclass";
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u) << "N is a parameter of myclass";
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// 23.3.1: test is never instantiated, so it is a top-level module,
// implicitly instantiated once under its own name. The instance tree and
// the top-module flag only exist after elaboration; in a non-elaborated
// model no module is flagged top.
TEST_F(ClassMemberTest19Test, TestIsTheTopLevelModule) {
  const hldb::Module *const test = getTestModule();
  ASSERT_NE(test, nullptr);

  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u) << "23.3.1: test is the only top-level module";
    const hldb::Module *const topInstance = hldb::findByName<hldb::Module>("test", m_design->getTopModules());
    ASSERT_NE(topInstance, nullptr) << "23.3.1: the top instance is named after module test";
    EXPECT_TRUE(topInstance->getTopModule()) << "23.3.1: test is never instantiated, so it is a top-level module";
  } else {
    EXPECT_FALSE(test->getTopModule()) << "all modules are non-top modules in non-elaborated model";
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
