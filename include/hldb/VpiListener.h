// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

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

/*
 * File:   VpiListener.h
 * Author: alaindargelas
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_VPILISTENER_H
#define UHDM_VPILISTENER_H

#include <uhdm/any.h>
#include <uhdm/containers.h>
#include <uhdm/uhdm_types.h>
#include <uhdm/vpi_user.h>

#include <ostream>
#include <set>
#include <vector>

#define TRACE_CONTEXT                                                                                   \
  "[" << ((const Any *)object)->getStartLine() << "," << ((const Any *)object)->getStartColumn() << ":" \
      << ((const Any *)object)->getEndLine() << "," << ((const Any *)object)->getEndColumn() << "]"

#define TRACE_ENTER strm << std::string(++indent * 2, ' ') << __func__ << ": " << TRACE_CONTEXT << std::endl
#define TRACE_LEAVE strm << std::string(2 * indent--, ' ') << __func__ << ": " << TRACE_CONTEXT << std::endl

namespace uhdm {
class Serializer;

class VpiListener {
 protected:
  using visited_t = AnySet;
  using any_stack_t = std::vector<const Any *>;

 public:
  // Use implicit constructor to initialize all members
  // VpiListener()

  virtual ~VpiListener() = default;

 public:
  void listenAny(vpiHandle handle);
  void listenDesigns(const std::vector<vpiHandle> &designs);
  void listenAlias(vpiHandle handle);
  void listenAlways(vpiHandle handle);
  void listenAnyPattern(vpiHandle handle);
  void listenArrayExpr(vpiHandle handle);
  void listenArrayTypespec(vpiHandle handle);
  void listenAssert(vpiHandle handle);
  void listenAssignStmt(vpiHandle handle);
  void listenAssignment(vpiHandle handle);
  void listenAssume(vpiHandle handle);
  void listenAttribute(vpiHandle handle);
  void listenBegin(vpiHandle handle);
  void listenBindDirective(vpiHandle handle);
  void listenBitSelect(vpiHandle handle);
  void listenBitTypespec(vpiHandle handle);
  void listenBreakStmt(vpiHandle handle);
  void listenByteTypespec(vpiHandle handle);
  void listenCaseItem(vpiHandle handle);
  void listenCaseProperty(vpiHandle handle);
  void listenCasePropertyItem(vpiHandle handle);
  void listenCaseStmt(vpiHandle handle);
  void listenChandleTypespec(vpiHandle handle);
  void listenCheckerDecl(vpiHandle handle);
  void listenCheckerInst(vpiHandle handle);
  void listenCheckerInstPort(vpiHandle handle);
  void listenCheckerPort(vpiHandle handle);
  void listenClassDefn(vpiHandle handle);
  void listenClassObj(vpiHandle handle);
  void listenClassTypespec(vpiHandle handle);
  void listenClause(vpiHandle handle);
  void listenClockedProperty(vpiHandle handle);
  void listenClockedSeq(vpiHandle handle);
  void listenClockingBlock(vpiHandle handle);
  void listenClockingIODecl(vpiHandle handle);
  void listenComment(vpiHandle handle);
  void listenConfigDecl(vpiHandle handle);
  void listenConfigRule(vpiHandle handle);
  void listenConstant(vpiHandle handle);
  void listenConstrForeach(vpiHandle handle);
  void listenConstrIf(vpiHandle handle);
  void listenConstrIfElse(vpiHandle handle);
  void listenConstraint(vpiHandle handle);
  void listenConstraintOrdering(vpiHandle handle);
  void listenContAssign(vpiHandle handle);
  void listenContAssignBit(vpiHandle handle);
  void listenContinueStmt(vpiHandle handle);
  void listenCover(vpiHandle handle);
  void listenCoverBin(vpiHandle handle);
  void listenCoverCross(vpiHandle handle);
  void listenCoverGroup(vpiHandle handle);
  void listenCoverPoint(vpiHandle handle);
  void listenCoverageOption(vpiHandle handle);
  void listenDeassign(vpiHandle handle);
  void listenDefParam(vpiHandle handle);
  void listenDelayControl(vpiHandle handle);
  void listenDelayTerm(vpiHandle handle);
  void listenDesign(vpiHandle handle);
  void listenDisable(vpiHandle handle);
  void listenDisableFork(vpiHandle handle);
  void listenDistItem(vpiHandle handle);
  void listenDistribution(vpiHandle handle);
  void listenDoWhile(vpiHandle handle);
  void listenEnumConst(vpiHandle handle);
  void listenEnumTypespec(vpiHandle handle);
  void listenEventControl(vpiHandle handle);
  void listenEventStmt(vpiHandle handle);
  void listenEventTypespec(vpiHandle handle);
  void listenExpectStmt(vpiHandle handle);
  void listenExtends(vpiHandle handle);
  void listenFinalStmt(vpiHandle handle);
  void listenForStmt(vpiHandle handle);
  void listenForce(vpiHandle handle);
  void listenForeachStmt(vpiHandle handle);
  void listenForeverStmt(vpiHandle handle);
  void listenForkStmt(vpiHandle handle);
  void listenFuncCall(vpiHandle handle);
  void listenFunction(vpiHandle handle);
  void listenFunctionDecl(vpiHandle handle);
  void listenGate(vpiHandle handle);
  void listenGateArray(vpiHandle handle);
  void listenGenCase(vpiHandle handle);
  void listenGenFor(vpiHandle handle);
  void listenGenIf(vpiHandle handle);
  void listenGenIfElse(vpiHandle handle);
  void listenGenRegion(vpiHandle handle);
  void listenGenScopeArray(vpiHandle handle);
  void listenHierPath(vpiHandle handle);
  void listenIODecl(vpiHandle handle);
  void listenIdentifier(vpiHandle handle);
  void listenIfElse(vpiHandle handle);
  void listenIfStmt(vpiHandle handle);
  void listenImmediateAssert(vpiHandle handle);
  void listenImmediateAssume(vpiHandle handle);
  void listenImmediateCover(vpiHandle handle);
  void listenImplements(vpiHandle handle);
  void listenImplication(vpiHandle handle);
  void listenImportTypespec(vpiHandle handle);
  void listenIncludeStmt(vpiHandle handle);
  void listenIndexedPartSelect(vpiHandle handle);
  void listenInitial(vpiHandle handle);
  void listenIntTypespec(vpiHandle handle);
  void listenIntegerTypespec(vpiHandle handle);
  void listenInterface(vpiHandle handle);
  void listenInterfaceArray(vpiHandle handle);
  void listenInterfaceTFDecl(vpiHandle handle);
  void listenInterfaceTypespec(vpiHandle handle);
  void listenLetDecl(vpiHandle handle);
  void listenLetExpr(vpiHandle handle);
  void listenLibrary(vpiHandle handle);
  void listenLogicTypespec(vpiHandle handle);
  void listenLongIntTypespec(vpiHandle handle);
  void listenMethodFuncCall(vpiHandle handle);
  void listenMethodTaskCall(vpiHandle handle);
  void listenModPath(vpiHandle handle);
  void listenModport(vpiHandle handle);
  void listenModule(vpiHandle handle);
  void listenModuleArray(vpiHandle handle);
  void listenModuleTypespec(vpiHandle handle);
  void listenMulticlockSequenceExpr(vpiHandle handle);
  void listenNamedEvent(vpiHandle handle);
  void listenNamedEventArray(vpiHandle handle);
  void listenNet(vpiHandle handle);
  void listenNullStmt(vpiHandle handle);
  void listenOperation(vpiHandle handle);
  void listenOrderedWait(vpiHandle handle);
  void listenPackage(vpiHandle handle);
  void listenPackageTypespec(vpiHandle handle);
  void listenParamAssign(vpiHandle handle);
  void listenParameter(vpiHandle handle);
  void listenPartSelect(vpiHandle handle);
  void listenPathTerm(vpiHandle handle);
  void listenPort(vpiHandle handle);
  void listenPortBit(vpiHandle handle);
  void listenPreprocMacroDefinition(vpiHandle handle);
  void listenPreprocMacroInstance(vpiHandle handle);
  void listenPrimTerm(vpiHandle handle);
  void listenProgram(vpiHandle handle);
  void listenProgramArray(vpiHandle handle);
  void listenProgramTypespec(vpiHandle handle);
  void listenPropFormalDecl(vpiHandle handle);
  void listenPropertyDecl(vpiHandle handle);
  void listenPropertyInst(vpiHandle handle);
  void listenPropertySpec(vpiHandle handle);
  void listenPropertyTypespec(vpiHandle handle);
  void listenRange(vpiHandle handle);
  void listenRealTypespec(vpiHandle handle);
  void listenRefInstance(vpiHandle handle);
  void listenRefObj(vpiHandle handle);
  void listenRefTypespec(vpiHandle handle);
  void listenReg(vpiHandle handle);
  void listenRegArray(vpiHandle handle);
  void listenRelease(vpiHandle handle);
  void listenRepeat(vpiHandle handle);
  void listenRepeatControl(vpiHandle handle);
  void listenRestrict(vpiHandle handle);
  void listenReturnStmt(vpiHandle handle);
  void listenSeqFormalDecl(vpiHandle handle);
  void listenSequenceDecl(vpiHandle handle);
  void listenSequenceInst(vpiHandle handle);
  void listenSequenceTypespec(vpiHandle handle);
  void listenShortIntTypespec(vpiHandle handle);
  void listenShortRealTypespec(vpiHandle handle);
  void listenSoftDisable(vpiHandle handle);
  void listenSourceFile(vpiHandle handle);
  void listenSpecParam(vpiHandle handle);
  void listenStringTypespec(vpiHandle handle);
  void listenStructPattern(vpiHandle handle);
  void listenStructTypespec(vpiHandle handle);
  void listenSwitchArray(vpiHandle handle);
  void listenSwitchTran(vpiHandle handle);
  void listenSysFuncCall(vpiHandle handle);
  void listenSysTaskCall(vpiHandle handle);
  void listenTableEntry(vpiHandle handle);
  void listenTaggedPattern(vpiHandle handle);
  void listenTask(vpiHandle handle);
  void listenTaskCall(vpiHandle handle);
  void listenTaskDecl(vpiHandle handle);
  void listenTchk(vpiHandle handle);
  void listenTchkTerm(vpiHandle handle);
  void listenThread(vpiHandle handle);
  void listenTimeTypespec(vpiHandle handle);
  void listenTypeParameter(vpiHandle handle);
  void listenTypedefTypespec(vpiHandle handle);
  void listenTypespecMember(vpiHandle handle);
  void listenUdp(vpiHandle handle);
  void listenUdpArray(vpiHandle handle);
  void listenUdpDefn(vpiHandle handle);
  void listenUdpDefnTypespec(vpiHandle handle);
  void listenUnionTypespec(vpiHandle handle);
  void listenUniqueness(vpiHandle handle);
  void listenUnsupportedExpr(vpiHandle handle);
  void listenUnsupportedStmt(vpiHandle handle);
  void listenUnsupportedTypespec(vpiHandle handle);
  void listenUserSystf(vpiHandle handle);
  void listenVarSelect(vpiHandle handle);
  void listenVariable(vpiHandle handle);
  void listenVoidTypespec(vpiHandle handle);
  void listenWaitFork(vpiHandle handle);
  void listenWaitStmt(vpiHandle handle);
  void listenWhileStmt(vpiHandle handle);

  virtual void enterAny(const Any *object, vpiHandle handle) {}
  virtual void leaveAny(const Any *object, vpiHandle handle) {}

  virtual void enterAlias(const Alias* object, vpiHandle handle) {}
  virtual void leaveAlias(const Alias* object, vpiHandle handle) {}

  virtual void enterAlways(const Always* object, vpiHandle handle) {}
  virtual void leaveAlways(const Always* object, vpiHandle handle) {}

  virtual void enterAnyPattern(const AnyPattern* object, vpiHandle handle) {}
  virtual void leaveAnyPattern(const AnyPattern* object, vpiHandle handle) {}

  virtual void enterArrayExpr(const ArrayExpr* object, vpiHandle handle) {}
  virtual void leaveArrayExpr(const ArrayExpr* object, vpiHandle handle) {}

  virtual void enterArrayTypespec(const ArrayTypespec* object, vpiHandle handle) {}
  virtual void leaveArrayTypespec(const ArrayTypespec* object, vpiHandle handle) {}

  virtual void enterAssert(const Assert* object, vpiHandle handle) {}
  virtual void leaveAssert(const Assert* object, vpiHandle handle) {}

  virtual void enterAssignStmt(const AssignStmt* object, vpiHandle handle) {}
  virtual void leaveAssignStmt(const AssignStmt* object, vpiHandle handle) {}

  virtual void enterAssignment(const Assignment* object, vpiHandle handle) {}
  virtual void leaveAssignment(const Assignment* object, vpiHandle handle) {}

  virtual void enterAssume(const Assume* object, vpiHandle handle) {}
  virtual void leaveAssume(const Assume* object, vpiHandle handle) {}

  virtual void enterAttribute(const Attribute* object, vpiHandle handle) {}
  virtual void leaveAttribute(const Attribute* object, vpiHandle handle) {}

  virtual void enterBegin(const Begin* object, vpiHandle handle) {}
  virtual void leaveBegin(const Begin* object, vpiHandle handle) {}

  virtual void enterBindDirective(const BindDirective* object, vpiHandle handle) {}
  virtual void leaveBindDirective(const BindDirective* object, vpiHandle handle) {}

  virtual void enterBitSelect(const BitSelect* object, vpiHandle handle) {}
  virtual void leaveBitSelect(const BitSelect* object, vpiHandle handle) {}

  virtual void enterBitTypespec(const BitTypespec* object, vpiHandle handle) {}
  virtual void leaveBitTypespec(const BitTypespec* object, vpiHandle handle) {}

  virtual void enterBreakStmt(const BreakStmt* object, vpiHandle handle) {}
  virtual void leaveBreakStmt(const BreakStmt* object, vpiHandle handle) {}

  virtual void enterByteTypespec(const ByteTypespec* object, vpiHandle handle) {}
  virtual void leaveByteTypespec(const ByteTypespec* object, vpiHandle handle) {}

  virtual void enterCaseItem(const CaseItem* object, vpiHandle handle) {}
  virtual void leaveCaseItem(const CaseItem* object, vpiHandle handle) {}

  virtual void enterCaseProperty(const CaseProperty* object, vpiHandle handle) {}
  virtual void leaveCaseProperty(const CaseProperty* object, vpiHandle handle) {}

  virtual void enterCasePropertyItem(const CasePropertyItem* object, vpiHandle handle) {}
  virtual void leaveCasePropertyItem(const CasePropertyItem* object, vpiHandle handle) {}

  virtual void enterCaseStmt(const CaseStmt* object, vpiHandle handle) {}
  virtual void leaveCaseStmt(const CaseStmt* object, vpiHandle handle) {}

  virtual void enterChandleTypespec(const ChandleTypespec* object, vpiHandle handle) {}
  virtual void leaveChandleTypespec(const ChandleTypespec* object, vpiHandle handle) {}

  virtual void enterCheckerDecl(const CheckerDecl* object, vpiHandle handle) {}
  virtual void leaveCheckerDecl(const CheckerDecl* object, vpiHandle handle) {}

  virtual void enterCheckerInst(const CheckerInst* object, vpiHandle handle) {}
  virtual void leaveCheckerInst(const CheckerInst* object, vpiHandle handle) {}

  virtual void enterCheckerInstPort(const CheckerInstPort* object, vpiHandle handle) {}
  virtual void leaveCheckerInstPort(const CheckerInstPort* object, vpiHandle handle) {}

  virtual void enterCheckerPort(const CheckerPort* object, vpiHandle handle) {}
  virtual void leaveCheckerPort(const CheckerPort* object, vpiHandle handle) {}

  virtual void enterClassDefn(const ClassDefn* object, vpiHandle handle) {}
  virtual void leaveClassDefn(const ClassDefn* object, vpiHandle handle) {}

  virtual void enterClassObj(const ClassObj* object, vpiHandle handle) {}
  virtual void leaveClassObj(const ClassObj* object, vpiHandle handle) {}

  virtual void enterClassTypespec(const ClassTypespec* object, vpiHandle handle) {}
  virtual void leaveClassTypespec(const ClassTypespec* object, vpiHandle handle) {}

  virtual void enterClause(const Clause* object, vpiHandle handle) {}
  virtual void leaveClause(const Clause* object, vpiHandle handle) {}

  virtual void enterClockedProperty(const ClockedProperty* object, vpiHandle handle) {}
  virtual void leaveClockedProperty(const ClockedProperty* object, vpiHandle handle) {}

  virtual void enterClockedSeq(const ClockedSeq* object, vpiHandle handle) {}
  virtual void leaveClockedSeq(const ClockedSeq* object, vpiHandle handle) {}

  virtual void enterClockingBlock(const ClockingBlock* object, vpiHandle handle) {}
  virtual void leaveClockingBlock(const ClockingBlock* object, vpiHandle handle) {}

  virtual void enterClockingIODecl(const ClockingIODecl* object, vpiHandle handle) {}
  virtual void leaveClockingIODecl(const ClockingIODecl* object, vpiHandle handle) {}

  virtual void enterComment(const Comment* object, vpiHandle handle) {}
  virtual void leaveComment(const Comment* object, vpiHandle handle) {}

  virtual void enterConfigDecl(const ConfigDecl* object, vpiHandle handle) {}
  virtual void leaveConfigDecl(const ConfigDecl* object, vpiHandle handle) {}

  virtual void enterConfigRule(const ConfigRule* object, vpiHandle handle) {}
  virtual void leaveConfigRule(const ConfigRule* object, vpiHandle handle) {}

  virtual void enterConstant(const Constant* object, vpiHandle handle) {}
  virtual void leaveConstant(const Constant* object, vpiHandle handle) {}

  virtual void enterConstrForeach(const ConstrForeach* object, vpiHandle handle) {}
  virtual void leaveConstrForeach(const ConstrForeach* object, vpiHandle handle) {}

  virtual void enterConstrIf(const ConstrIf* object, vpiHandle handle) {}
  virtual void leaveConstrIf(const ConstrIf* object, vpiHandle handle) {}

  virtual void enterConstrIfElse(const ConstrIfElse* object, vpiHandle handle) {}
  virtual void leaveConstrIfElse(const ConstrIfElse* object, vpiHandle handle) {}

  virtual void enterConstraint(const Constraint* object, vpiHandle handle) {}
  virtual void leaveConstraint(const Constraint* object, vpiHandle handle) {}

  virtual void enterConstraintOrdering(const ConstraintOrdering* object, vpiHandle handle) {}
  virtual void leaveConstraintOrdering(const ConstraintOrdering* object, vpiHandle handle) {}

  virtual void enterContAssign(const ContAssign* object, vpiHandle handle) {}
  virtual void leaveContAssign(const ContAssign* object, vpiHandle handle) {}

  virtual void enterContAssignBit(const ContAssignBit* object, vpiHandle handle) {}
  virtual void leaveContAssignBit(const ContAssignBit* object, vpiHandle handle) {}

  virtual void enterContinueStmt(const ContinueStmt* object, vpiHandle handle) {}
  virtual void leaveContinueStmt(const ContinueStmt* object, vpiHandle handle) {}

  virtual void enterCover(const Cover* object, vpiHandle handle) {}
  virtual void leaveCover(const Cover* object, vpiHandle handle) {}

  virtual void enterCoverBin(const CoverBin* object, vpiHandle handle) {}
  virtual void leaveCoverBin(const CoverBin* object, vpiHandle handle) {}

  virtual void enterCoverCross(const CoverCross* object, vpiHandle handle) {}
  virtual void leaveCoverCross(const CoverCross* object, vpiHandle handle) {}

  virtual void enterCoverGroup(const CoverGroup* object, vpiHandle handle) {}
  virtual void leaveCoverGroup(const CoverGroup* object, vpiHandle handle) {}

  virtual void enterCoverPoint(const CoverPoint* object, vpiHandle handle) {}
  virtual void leaveCoverPoint(const CoverPoint* object, vpiHandle handle) {}

  virtual void enterCoverageOption(const CoverageOption* object, vpiHandle handle) {}
  virtual void leaveCoverageOption(const CoverageOption* object, vpiHandle handle) {}

  virtual void enterDeassign(const Deassign* object, vpiHandle handle) {}
  virtual void leaveDeassign(const Deassign* object, vpiHandle handle) {}

  virtual void enterDefParam(const DefParam* object, vpiHandle handle) {}
  virtual void leaveDefParam(const DefParam* object, vpiHandle handle) {}

  virtual void enterDelayControl(const DelayControl* object, vpiHandle handle) {}
  virtual void leaveDelayControl(const DelayControl* object, vpiHandle handle) {}

  virtual void enterDelayTerm(const DelayTerm* object, vpiHandle handle) {}
  virtual void leaveDelayTerm(const DelayTerm* object, vpiHandle handle) {}

  virtual void enterDesign(const Design* object, vpiHandle handle) {}
  virtual void leaveDesign(const Design* object, vpiHandle handle) {}

  virtual void enterDisable(const Disable* object, vpiHandle handle) {}
  virtual void leaveDisable(const Disable* object, vpiHandle handle) {}

  virtual void enterDisableFork(const DisableFork* object, vpiHandle handle) {}
  virtual void leaveDisableFork(const DisableFork* object, vpiHandle handle) {}

  virtual void enterDistItem(const DistItem* object, vpiHandle handle) {}
  virtual void leaveDistItem(const DistItem* object, vpiHandle handle) {}

  virtual void enterDistribution(const Distribution* object, vpiHandle handle) {}
  virtual void leaveDistribution(const Distribution* object, vpiHandle handle) {}

  virtual void enterDoWhile(const DoWhile* object, vpiHandle handle) {}
  virtual void leaveDoWhile(const DoWhile* object, vpiHandle handle) {}

  virtual void enterEnumConst(const EnumConst* object, vpiHandle handle) {}
  virtual void leaveEnumConst(const EnumConst* object, vpiHandle handle) {}

  virtual void enterEnumTypespec(const EnumTypespec* object, vpiHandle handle) {}
  virtual void leaveEnumTypespec(const EnumTypespec* object, vpiHandle handle) {}

  virtual void enterEventControl(const EventControl* object, vpiHandle handle) {}
  virtual void leaveEventControl(const EventControl* object, vpiHandle handle) {}

  virtual void enterEventStmt(const EventStmt* object, vpiHandle handle) {}
  virtual void leaveEventStmt(const EventStmt* object, vpiHandle handle) {}

  virtual void enterEventTypespec(const EventTypespec* object, vpiHandle handle) {}
  virtual void leaveEventTypespec(const EventTypespec* object, vpiHandle handle) {}

  virtual void enterExpectStmt(const ExpectStmt* object, vpiHandle handle) {}
  virtual void leaveExpectStmt(const ExpectStmt* object, vpiHandle handle) {}

  virtual void enterExtends(const Extends* object, vpiHandle handle) {}
  virtual void leaveExtends(const Extends* object, vpiHandle handle) {}

  virtual void enterFinalStmt(const FinalStmt* object, vpiHandle handle) {}
  virtual void leaveFinalStmt(const FinalStmt* object, vpiHandle handle) {}

  virtual void enterForStmt(const ForStmt* object, vpiHandle handle) {}
  virtual void leaveForStmt(const ForStmt* object, vpiHandle handle) {}

  virtual void enterForce(const Force* object, vpiHandle handle) {}
  virtual void leaveForce(const Force* object, vpiHandle handle) {}

  virtual void enterForeachStmt(const ForeachStmt* object, vpiHandle handle) {}
  virtual void leaveForeachStmt(const ForeachStmt* object, vpiHandle handle) {}

  virtual void enterForeverStmt(const ForeverStmt* object, vpiHandle handle) {}
  virtual void leaveForeverStmt(const ForeverStmt* object, vpiHandle handle) {}

  virtual void enterForkStmt(const ForkStmt* object, vpiHandle handle) {}
  virtual void leaveForkStmt(const ForkStmt* object, vpiHandle handle) {}

  virtual void enterFuncCall(const FuncCall* object, vpiHandle handle) {}
  virtual void leaveFuncCall(const FuncCall* object, vpiHandle handle) {}

  virtual void enterFunction(const Function* object, vpiHandle handle) {}
  virtual void leaveFunction(const Function* object, vpiHandle handle) {}

  virtual void enterFunctionDecl(const FunctionDecl* object, vpiHandle handle) {}
  virtual void leaveFunctionDecl(const FunctionDecl* object, vpiHandle handle) {}

  virtual void enterGate(const Gate* object, vpiHandle handle) {}
  virtual void leaveGate(const Gate* object, vpiHandle handle) {}

  virtual void enterGateArray(const GateArray* object, vpiHandle handle) {}
  virtual void leaveGateArray(const GateArray* object, vpiHandle handle) {}

  virtual void enterGenCase(const GenCase* object, vpiHandle handle) {}
  virtual void leaveGenCase(const GenCase* object, vpiHandle handle) {}

  virtual void enterGenFor(const GenFor* object, vpiHandle handle) {}
  virtual void leaveGenFor(const GenFor* object, vpiHandle handle) {}

  virtual void enterGenIf(const GenIf* object, vpiHandle handle) {}
  virtual void leaveGenIf(const GenIf* object, vpiHandle handle) {}

  virtual void enterGenIfElse(const GenIfElse* object, vpiHandle handle) {}
  virtual void leaveGenIfElse(const GenIfElse* object, vpiHandle handle) {}

  virtual void enterGenRegion(const GenRegion* object, vpiHandle handle) {}
  virtual void leaveGenRegion(const GenRegion* object, vpiHandle handle) {}

  virtual void enterGenScopeArray(const GenScopeArray* object, vpiHandle handle) {}
  virtual void leaveGenScopeArray(const GenScopeArray* object, vpiHandle handle) {}

  virtual void enterHierPath(const HierPath* object, vpiHandle handle) {}
  virtual void leaveHierPath(const HierPath* object, vpiHandle handle) {}

  virtual void enterIODecl(const IODecl* object, vpiHandle handle) {}
  virtual void leaveIODecl(const IODecl* object, vpiHandle handle) {}

  virtual void enterIdentifier(const Identifier* object, vpiHandle handle) {}
  virtual void leaveIdentifier(const Identifier* object, vpiHandle handle) {}

  virtual void enterIfElse(const IfElse* object, vpiHandle handle) {}
  virtual void leaveIfElse(const IfElse* object, vpiHandle handle) {}

  virtual void enterIfStmt(const IfStmt* object, vpiHandle handle) {}
  virtual void leaveIfStmt(const IfStmt* object, vpiHandle handle) {}

  virtual void enterImmediateAssert(const ImmediateAssert* object, vpiHandle handle) {}
  virtual void leaveImmediateAssert(const ImmediateAssert* object, vpiHandle handle) {}

  virtual void enterImmediateAssume(const ImmediateAssume* object, vpiHandle handle) {}
  virtual void leaveImmediateAssume(const ImmediateAssume* object, vpiHandle handle) {}

  virtual void enterImmediateCover(const ImmediateCover* object, vpiHandle handle) {}
  virtual void leaveImmediateCover(const ImmediateCover* object, vpiHandle handle) {}

  virtual void enterImplements(const Implements* object, vpiHandle handle) {}
  virtual void leaveImplements(const Implements* object, vpiHandle handle) {}

  virtual void enterImplication(const Implication* object, vpiHandle handle) {}
  virtual void leaveImplication(const Implication* object, vpiHandle handle) {}

  virtual void enterImportTypespec(const ImportTypespec* object, vpiHandle handle) {}
  virtual void leaveImportTypespec(const ImportTypespec* object, vpiHandle handle) {}

  virtual void enterIncludeStmt(const IncludeStmt* object, vpiHandle handle) {}
  virtual void leaveIncludeStmt(const IncludeStmt* object, vpiHandle handle) {}

  virtual void enterIndexedPartSelect(const IndexedPartSelect* object, vpiHandle handle) {}
  virtual void leaveIndexedPartSelect(const IndexedPartSelect* object, vpiHandle handle) {}

  virtual void enterInitial(const Initial* object, vpiHandle handle) {}
  virtual void leaveInitial(const Initial* object, vpiHandle handle) {}

  virtual void enterIntTypespec(const IntTypespec* object, vpiHandle handle) {}
  virtual void leaveIntTypespec(const IntTypespec* object, vpiHandle handle) {}

  virtual void enterIntegerTypespec(const IntegerTypespec* object, vpiHandle handle) {}
  virtual void leaveIntegerTypespec(const IntegerTypespec* object, vpiHandle handle) {}

  virtual void enterInterface(const Interface* object, vpiHandle handle) {}
  virtual void leaveInterface(const Interface* object, vpiHandle handle) {}

  virtual void enterInterfaceArray(const InterfaceArray* object, vpiHandle handle) {}
  virtual void leaveInterfaceArray(const InterfaceArray* object, vpiHandle handle) {}

  virtual void enterInterfaceTFDecl(const InterfaceTFDecl* object, vpiHandle handle) {}
  virtual void leaveInterfaceTFDecl(const InterfaceTFDecl* object, vpiHandle handle) {}

  virtual void enterInterfaceTypespec(const InterfaceTypespec* object, vpiHandle handle) {}
  virtual void leaveInterfaceTypespec(const InterfaceTypespec* object, vpiHandle handle) {}

  virtual void enterLetDecl(const LetDecl* object, vpiHandle handle) {}
  virtual void leaveLetDecl(const LetDecl* object, vpiHandle handle) {}

  virtual void enterLetExpr(const LetExpr* object, vpiHandle handle) {}
  virtual void leaveLetExpr(const LetExpr* object, vpiHandle handle) {}

  virtual void enterLibrary(const Library* object, vpiHandle handle) {}
  virtual void leaveLibrary(const Library* object, vpiHandle handle) {}

  virtual void enterLogicTypespec(const LogicTypespec* object, vpiHandle handle) {}
  virtual void leaveLogicTypespec(const LogicTypespec* object, vpiHandle handle) {}

  virtual void enterLongIntTypespec(const LongIntTypespec* object, vpiHandle handle) {}
  virtual void leaveLongIntTypespec(const LongIntTypespec* object, vpiHandle handle) {}

  virtual void enterMethodFuncCall(const MethodFuncCall* object, vpiHandle handle) {}
  virtual void leaveMethodFuncCall(const MethodFuncCall* object, vpiHandle handle) {}

  virtual void enterMethodTaskCall(const MethodTaskCall* object, vpiHandle handle) {}
  virtual void leaveMethodTaskCall(const MethodTaskCall* object, vpiHandle handle) {}

  virtual void enterModPath(const ModPath* object, vpiHandle handle) {}
  virtual void leaveModPath(const ModPath* object, vpiHandle handle) {}

  virtual void enterModport(const Modport* object, vpiHandle handle) {}
  virtual void leaveModport(const Modport* object, vpiHandle handle) {}

  virtual void enterModule(const Module* object, vpiHandle handle) {}
  virtual void leaveModule(const Module* object, vpiHandle handle) {}

  virtual void enterModuleArray(const ModuleArray* object, vpiHandle handle) {}
  virtual void leaveModuleArray(const ModuleArray* object, vpiHandle handle) {}

  virtual void enterModuleTypespec(const ModuleTypespec* object, vpiHandle handle) {}
  virtual void leaveModuleTypespec(const ModuleTypespec* object, vpiHandle handle) {}

  virtual void enterMulticlockSequenceExpr(const MulticlockSequenceExpr* object, vpiHandle handle) {}
  virtual void leaveMulticlockSequenceExpr(const MulticlockSequenceExpr* object, vpiHandle handle) {}

  virtual void enterNamedEvent(const NamedEvent* object, vpiHandle handle) {}
  virtual void leaveNamedEvent(const NamedEvent* object, vpiHandle handle) {}

  virtual void enterNamedEventArray(const NamedEventArray* object, vpiHandle handle) {}
  virtual void leaveNamedEventArray(const NamedEventArray* object, vpiHandle handle) {}

  virtual void enterNet(const Net* object, vpiHandle handle) {}
  virtual void leaveNet(const Net* object, vpiHandle handle) {}

  virtual void enterNullStmt(const NullStmt* object, vpiHandle handle) {}
  virtual void leaveNullStmt(const NullStmt* object, vpiHandle handle) {}

  virtual void enterOperation(const Operation* object, vpiHandle handle) {}
  virtual void leaveOperation(const Operation* object, vpiHandle handle) {}

  virtual void enterOrderedWait(const OrderedWait* object, vpiHandle handle) {}
  virtual void leaveOrderedWait(const OrderedWait* object, vpiHandle handle) {}

  virtual void enterPackage(const Package* object, vpiHandle handle) {}
  virtual void leavePackage(const Package* object, vpiHandle handle) {}

  virtual void enterPackageTypespec(const PackageTypespec* object, vpiHandle handle) {}
  virtual void leavePackageTypespec(const PackageTypespec* object, vpiHandle handle) {}

  virtual void enterParamAssign(const ParamAssign* object, vpiHandle handle) {}
  virtual void leaveParamAssign(const ParamAssign* object, vpiHandle handle) {}

  virtual void enterParameter(const Parameter* object, vpiHandle handle) {}
  virtual void leaveParameter(const Parameter* object, vpiHandle handle) {}

  virtual void enterPartSelect(const PartSelect* object, vpiHandle handle) {}
  virtual void leavePartSelect(const PartSelect* object, vpiHandle handle) {}

  virtual void enterPathTerm(const PathTerm* object, vpiHandle handle) {}
  virtual void leavePathTerm(const PathTerm* object, vpiHandle handle) {}

  virtual void enterPort(const Port* object, vpiHandle handle) {}
  virtual void leavePort(const Port* object, vpiHandle handle) {}

  virtual void enterPortBit(const PortBit* object, vpiHandle handle) {}
  virtual void leavePortBit(const PortBit* object, vpiHandle handle) {}

  virtual void enterPreprocMacroDefinition(const PreprocMacroDefinition* object, vpiHandle handle) {}
  virtual void leavePreprocMacroDefinition(const PreprocMacroDefinition* object, vpiHandle handle) {}

  virtual void enterPreprocMacroInstance(const PreprocMacroInstance* object, vpiHandle handle) {}
  virtual void leavePreprocMacroInstance(const PreprocMacroInstance* object, vpiHandle handle) {}

  virtual void enterPrimTerm(const PrimTerm* object, vpiHandle handle) {}
  virtual void leavePrimTerm(const PrimTerm* object, vpiHandle handle) {}

  virtual void enterProgram(const Program* object, vpiHandle handle) {}
  virtual void leaveProgram(const Program* object, vpiHandle handle) {}

  virtual void enterProgramArray(const ProgramArray* object, vpiHandle handle) {}
  virtual void leaveProgramArray(const ProgramArray* object, vpiHandle handle) {}

  virtual void enterProgramTypespec(const ProgramTypespec* object, vpiHandle handle) {}
  virtual void leaveProgramTypespec(const ProgramTypespec* object, vpiHandle handle) {}

  virtual void enterPropFormalDecl(const PropFormalDecl* object, vpiHandle handle) {}
  virtual void leavePropFormalDecl(const PropFormalDecl* object, vpiHandle handle) {}

  virtual void enterPropertyDecl(const PropertyDecl* object, vpiHandle handle) {}
  virtual void leavePropertyDecl(const PropertyDecl* object, vpiHandle handle) {}

  virtual void enterPropertyInst(const PropertyInst* object, vpiHandle handle) {}
  virtual void leavePropertyInst(const PropertyInst* object, vpiHandle handle) {}

  virtual void enterPropertySpec(const PropertySpec* object, vpiHandle handle) {}
  virtual void leavePropertySpec(const PropertySpec* object, vpiHandle handle) {}

  virtual void enterPropertyTypespec(const PropertyTypespec* object, vpiHandle handle) {}
  virtual void leavePropertyTypespec(const PropertyTypespec* object, vpiHandle handle) {}

  virtual void enterRange(const Range* object, vpiHandle handle) {}
  virtual void leaveRange(const Range* object, vpiHandle handle) {}

  virtual void enterRealTypespec(const RealTypespec* object, vpiHandle handle) {}
  virtual void leaveRealTypespec(const RealTypespec* object, vpiHandle handle) {}

  virtual void enterRefInstance(const RefInstance* object, vpiHandle handle) {}
  virtual void leaveRefInstance(const RefInstance* object, vpiHandle handle) {}

  virtual void enterRefObj(const RefObj* object, vpiHandle handle) {}
  virtual void leaveRefObj(const RefObj* object, vpiHandle handle) {}

  virtual void enterRefTypespec(const RefTypespec* object, vpiHandle handle) {}
  virtual void leaveRefTypespec(const RefTypespec* object, vpiHandle handle) {}

  virtual void enterReg(const Reg* object, vpiHandle handle) {}
  virtual void leaveReg(const Reg* object, vpiHandle handle) {}

  virtual void enterRegArray(const RegArray* object, vpiHandle handle) {}
  virtual void leaveRegArray(const RegArray* object, vpiHandle handle) {}

  virtual void enterRelease(const Release* object, vpiHandle handle) {}
  virtual void leaveRelease(const Release* object, vpiHandle handle) {}

  virtual void enterRepeat(const Repeat* object, vpiHandle handle) {}
  virtual void leaveRepeat(const Repeat* object, vpiHandle handle) {}

  virtual void enterRepeatControl(const RepeatControl* object, vpiHandle handle) {}
  virtual void leaveRepeatControl(const RepeatControl* object, vpiHandle handle) {}

  virtual void enterRestrict(const Restrict* object, vpiHandle handle) {}
  virtual void leaveRestrict(const Restrict* object, vpiHandle handle) {}

  virtual void enterReturnStmt(const ReturnStmt* object, vpiHandle handle) {}
  virtual void leaveReturnStmt(const ReturnStmt* object, vpiHandle handle) {}

  virtual void enterSeqFormalDecl(const SeqFormalDecl* object, vpiHandle handle) {}
  virtual void leaveSeqFormalDecl(const SeqFormalDecl* object, vpiHandle handle) {}

  virtual void enterSequenceDecl(const SequenceDecl* object, vpiHandle handle) {}
  virtual void leaveSequenceDecl(const SequenceDecl* object, vpiHandle handle) {}

  virtual void enterSequenceInst(const SequenceInst* object, vpiHandle handle) {}
  virtual void leaveSequenceInst(const SequenceInst* object, vpiHandle handle) {}

  virtual void enterSequenceTypespec(const SequenceTypespec* object, vpiHandle handle) {}
  virtual void leaveSequenceTypespec(const SequenceTypespec* object, vpiHandle handle) {}

  virtual void enterShortIntTypespec(const ShortIntTypespec* object, vpiHandle handle) {}
  virtual void leaveShortIntTypespec(const ShortIntTypespec* object, vpiHandle handle) {}

  virtual void enterShortRealTypespec(const ShortRealTypespec* object, vpiHandle handle) {}
  virtual void leaveShortRealTypespec(const ShortRealTypespec* object, vpiHandle handle) {}

  virtual void enterSoftDisable(const SoftDisable* object, vpiHandle handle) {}
  virtual void leaveSoftDisable(const SoftDisable* object, vpiHandle handle) {}

  virtual void enterSourceFile(const SourceFile* object, vpiHandle handle) {}
  virtual void leaveSourceFile(const SourceFile* object, vpiHandle handle) {}

  virtual void enterSpecParam(const SpecParam* object, vpiHandle handle) {}
  virtual void leaveSpecParam(const SpecParam* object, vpiHandle handle) {}

  virtual void enterStringTypespec(const StringTypespec* object, vpiHandle handle) {}
  virtual void leaveStringTypespec(const StringTypespec* object, vpiHandle handle) {}

  virtual void enterStructPattern(const StructPattern* object, vpiHandle handle) {}
  virtual void leaveStructPattern(const StructPattern* object, vpiHandle handle) {}

  virtual void enterStructTypespec(const StructTypespec* object, vpiHandle handle) {}
  virtual void leaveStructTypespec(const StructTypespec* object, vpiHandle handle) {}

  virtual void enterSwitchArray(const SwitchArray* object, vpiHandle handle) {}
  virtual void leaveSwitchArray(const SwitchArray* object, vpiHandle handle) {}

  virtual void enterSwitchTran(const SwitchTran* object, vpiHandle handle) {}
  virtual void leaveSwitchTran(const SwitchTran* object, vpiHandle handle) {}

  virtual void enterSysFuncCall(const SysFuncCall* object, vpiHandle handle) {}
  virtual void leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) {}

  virtual void enterSysTaskCall(const SysTaskCall* object, vpiHandle handle) {}
  virtual void leaveSysTaskCall(const SysTaskCall* object, vpiHandle handle) {}

  virtual void enterTableEntry(const TableEntry* object, vpiHandle handle) {}
  virtual void leaveTableEntry(const TableEntry* object, vpiHandle handle) {}

  virtual void enterTaggedPattern(const TaggedPattern* object, vpiHandle handle) {}
  virtual void leaveTaggedPattern(const TaggedPattern* object, vpiHandle handle) {}

  virtual void enterTask(const Task* object, vpiHandle handle) {}
  virtual void leaveTask(const Task* object, vpiHandle handle) {}

  virtual void enterTaskCall(const TaskCall* object, vpiHandle handle) {}
  virtual void leaveTaskCall(const TaskCall* object, vpiHandle handle) {}

  virtual void enterTaskDecl(const TaskDecl* object, vpiHandle handle) {}
  virtual void leaveTaskDecl(const TaskDecl* object, vpiHandle handle) {}

  virtual void enterTchk(const Tchk* object, vpiHandle handle) {}
  virtual void leaveTchk(const Tchk* object, vpiHandle handle) {}

  virtual void enterTchkTerm(const TchkTerm* object, vpiHandle handle) {}
  virtual void leaveTchkTerm(const TchkTerm* object, vpiHandle handle) {}

  virtual void enterThread(const Thread* object, vpiHandle handle) {}
  virtual void leaveThread(const Thread* object, vpiHandle handle) {}

  virtual void enterTimeTypespec(const TimeTypespec* object, vpiHandle handle) {}
  virtual void leaveTimeTypespec(const TimeTypespec* object, vpiHandle handle) {}

  virtual void enterTypeParameter(const TypeParameter* object, vpiHandle handle) {}
  virtual void leaveTypeParameter(const TypeParameter* object, vpiHandle handle) {}

  virtual void enterTypedefTypespec(const TypedefTypespec* object, vpiHandle handle) {}
  virtual void leaveTypedefTypespec(const TypedefTypespec* object, vpiHandle handle) {}

  virtual void enterTypespecMember(const TypespecMember* object, vpiHandle handle) {}
  virtual void leaveTypespecMember(const TypespecMember* object, vpiHandle handle) {}

  virtual void enterUdp(const Udp* object, vpiHandle handle) {}
  virtual void leaveUdp(const Udp* object, vpiHandle handle) {}

  virtual void enterUdpArray(const UdpArray* object, vpiHandle handle) {}
  virtual void leaveUdpArray(const UdpArray* object, vpiHandle handle) {}

  virtual void enterUdpDefn(const UdpDefn* object, vpiHandle handle) {}
  virtual void leaveUdpDefn(const UdpDefn* object, vpiHandle handle) {}

  virtual void enterUdpDefnTypespec(const UdpDefnTypespec* object, vpiHandle handle) {}
  virtual void leaveUdpDefnTypespec(const UdpDefnTypespec* object, vpiHandle handle) {}

  virtual void enterUnionTypespec(const UnionTypespec* object, vpiHandle handle) {}
  virtual void leaveUnionTypespec(const UnionTypespec* object, vpiHandle handle) {}

  virtual void enterUniqueness(const Uniqueness* object, vpiHandle handle) {}
  virtual void leaveUniqueness(const Uniqueness* object, vpiHandle handle) {}

  virtual void enterUnsupportedExpr(const UnsupportedExpr* object, vpiHandle handle) {}
  virtual void leaveUnsupportedExpr(const UnsupportedExpr* object, vpiHandle handle) {}

  virtual void enterUnsupportedStmt(const UnsupportedStmt* object, vpiHandle handle) {}
  virtual void leaveUnsupportedStmt(const UnsupportedStmt* object, vpiHandle handle) {}

  virtual void enterUnsupportedTypespec(const UnsupportedTypespec* object, vpiHandle handle) {}
  virtual void leaveUnsupportedTypespec(const UnsupportedTypespec* object, vpiHandle handle) {}

  virtual void enterUserSystf(const UserSystf* object, vpiHandle handle) {}
  virtual void leaveUserSystf(const UserSystf* object, vpiHandle handle) {}

  virtual void enterVarSelect(const VarSelect* object, vpiHandle handle) {}
  virtual void leaveVarSelect(const VarSelect* object, vpiHandle handle) {}

  virtual void enterVariable(const Variable* object, vpiHandle handle) {}
  virtual void leaveVariable(const Variable* object, vpiHandle handle) {}

  virtual void enterVoidTypespec(const VoidTypespec* object, vpiHandle handle) {}
  virtual void leaveVoidTypespec(const VoidTypespec* object, vpiHandle handle) {}

  virtual void enterWaitFork(const WaitFork* object, vpiHandle handle) {}
  virtual void leaveWaitFork(const WaitFork* object, vpiHandle handle) {}

  virtual void enterWaitStmt(const WaitStmt* object, vpiHandle handle) {}
  virtual void leaveWaitStmt(const WaitStmt* object, vpiHandle handle) {}

  virtual void enterWhileStmt(const WhileStmt* object, vpiHandle handle) {}
  virtual void leaveWhileStmt(const WhileStmt* object, vpiHandle handle) {}

  bool isInUhdmAllIterator() const { return uhdmAllIterator; }
  bool inCallstackOfType(UhdmType type);
  Design *currentDesign() { return m_currentDesign; }

  void requestAbort() { m_abortRequested = true; }

 protected:
  visited_t m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
  bool uhdmAllIterator = false;
  Design *m_currentDesign = nullptr;

 private:
  void listenAny_(vpiHandle handle);
  void listenAttribute_(vpiHandle handle);
  void listenIdentifier_(vpiHandle handle);
  void listenComment_(vpiHandle handle);
  void listenLetDecl_(vpiHandle handle);
  void listenConcurrentAssertions_(vpiHandle handle);
  void listenProcess_(vpiHandle handle);
  void listenAlways_(vpiHandle handle);
  void listenFinalStmt_(vpiHandle handle);
  void listenInitial_(vpiHandle handle);
  void listenAtomicStmt_(vpiHandle handle);
  void listenDelayControl_(vpiHandle handle);
  void listenDelayTerm_(vpiHandle handle);
  void listenEventControl_(vpiHandle handle);
  void listenRepeatControl_(vpiHandle handle);
  void listenScope_(vpiHandle handle);
  void listenBegin_(vpiHandle handle);
  void listenForkStmt_(vpiHandle handle);
  void listenForStmt_(vpiHandle handle);
  void listenIfStmt_(vpiHandle handle);
  void listenEventStmt_(vpiHandle handle);
  void listenThread_(vpiHandle handle);
  void listenForeverStmt_(vpiHandle handle);
  void listenWaits_(vpiHandle handle);
  void listenWaitStmt_(vpiHandle handle);
  void listenWaitFork_(vpiHandle handle);
  void listenOrderedWait_(vpiHandle handle);
  void listenDisables_(vpiHandle handle);
  void listenDisable_(vpiHandle handle);
  void listenDisableFork_(vpiHandle handle);
  void listenContinueStmt_(vpiHandle handle);
  void listenBreakStmt_(vpiHandle handle);
  void listenReturnStmt_(vpiHandle handle);
  void listenWhileStmt_(vpiHandle handle);
  void listenRepeat_(vpiHandle handle);
  void listenDoWhile_(vpiHandle handle);
  void listenIfElse_(vpiHandle handle);
  void listenCaseStmt_(vpiHandle handle);
  void listenForce_(vpiHandle handle);
  void listenAssignStmt_(vpiHandle handle);
  void listenDeassign_(vpiHandle handle);
  void listenRelease_(vpiHandle handle);
  void listenNullStmt_(vpiHandle handle);
  void listenExpectStmt_(vpiHandle handle);
  void listenForeachStmt_(vpiHandle handle);
  void listenGenScope_(vpiHandle handle);
  void listenGenScopeArray_(vpiHandle handle);
  void listenAssert_(vpiHandle handle);
  void listenCover_(vpiHandle handle);
  void listenAssume_(vpiHandle handle);
  void listenRestrict_(vpiHandle handle);
  void listenImmediateAssert_(vpiHandle handle);
  void listenImmediateAssume_(vpiHandle handle);
  void listenImmediateCover_(vpiHandle handle);
  void listenExpr_(vpiHandle handle);
  void listenCaseItem_(vpiHandle handle);
  void listenAssignment_(vpiHandle handle);
  void listenAnyPattern_(vpiHandle handle);
  void listenTaggedPattern_(vpiHandle handle);
  void listenStructPattern_(vpiHandle handle);
  void listenUnsupportedExpr_(vpiHandle handle);
  void listenUnsupportedStmt_(vpiHandle handle);
  void listenPreprocMacroDefinition_(vpiHandle handle);
  void listenPreprocMacroInstance_(vpiHandle handle);
  void listenSourceFile_(vpiHandle handle);
  void listenSequenceInst_(vpiHandle handle);
  void listenSeqFormalDecl_(vpiHandle handle);
  void listenSequenceDecl_(vpiHandle handle);
  void listenPropFormalDecl_(vpiHandle handle);
  void listenPropertyInst_(vpiHandle handle);
  void listenPropertySpec_(vpiHandle handle);
  void listenPropertyDecl_(vpiHandle handle);
  void listenClockedProperty_(vpiHandle handle);
  void listenCasePropertyItem_(vpiHandle handle);
  void listenCaseProperty_(vpiHandle handle);
  void listenMulticlockSequenceExpr_(vpiHandle handle);
  void listenClockedSeq_(vpiHandle handle);
  void listenSimpleExpr_(vpiHandle handle);
  void listenConstant_(vpiHandle handle);
  void listenLetExpr_(vpiHandle handle);
  void listenOperation_(vpiHandle handle);
  void listenRefObj_(vpiHandle handle);
  void listenRefInstance_(vpiHandle handle);
  void listenRefTypespec_(vpiHandle handle);
  void listenSelect_(vpiHandle handle);
  void listenPartSelect_(vpiHandle handle);
  void listenIndexedPartSelect_(vpiHandle handle);
  void listenVarSelect_(vpiHandle handle);
  void listenBitSelect_(vpiHandle handle);
  void listenVariable_(vpiHandle handle);
  void listenHierPath_(vpiHandle handle);
  void listenArrayExpr_(vpiHandle handle);
  void listenRegArray_(vpiHandle handle);
  void listenReg_(vpiHandle handle);
  void listenTaskFunc_(vpiHandle handle);
  void listenTask_(vpiHandle handle);
  void listenFunction_(vpiHandle handle);
  void listenTaskFuncDecl_(vpiHandle handle);
  void listenTaskDecl_(vpiHandle handle);
  void listenFunctionDecl_(vpiHandle handle);
  void listenModport_(vpiHandle handle);
  void listenInterfaceTFDecl_(vpiHandle handle);
  void listenContAssign_(vpiHandle handle);
  void listenContAssignBit_(vpiHandle handle);
  void listenPorts_(vpiHandle handle);
  void listenPort_(vpiHandle handle);
  void listenPortBit_(vpiHandle handle);
  void listenCheckerPort_(vpiHandle handle);
  void listenCheckerInstPort_(vpiHandle handle);
  void listenPrimitive_(vpiHandle handle);
  void listenGate_(vpiHandle handle);
  void listenSwitchTran_(vpiHandle handle);
  void listenUdp_(vpiHandle handle);
  void listenModPath_(vpiHandle handle);
  void listenTchk_(vpiHandle handle);
  void listenRange_(vpiHandle handle);
  void listenUdpDefn_(vpiHandle handle);
  void listenTableEntry_(vpiHandle handle);
  void listenIODecl_(vpiHandle handle);
  void listenAlias_(vpiHandle handle);
  void listenClockingBlock_(vpiHandle handle);
  void listenClockingIODecl_(vpiHandle handle);
  void listenCoverageOption_(vpiHandle handle);
  void listenCoverBin_(vpiHandle handle);
  void listenCoverPoint_(vpiHandle handle);
  void listenCoverCross_(vpiHandle handle);
  void listenCoverGroup_(vpiHandle handle);
  void listenParamAssign_(vpiHandle handle);
  void listenInstanceArray_(vpiHandle handle);
  void listenInterfaceArray_(vpiHandle handle);
  void listenProgramArray_(vpiHandle handle);
  void listenModuleArray_(vpiHandle handle);
  void listenPrimitiveArray_(vpiHandle handle);
  void listenGateArray_(vpiHandle handle);
  void listenSwitchArray_(vpiHandle handle);
  void listenUdpArray_(vpiHandle handle);
  void listenTypespec_(vpiHandle handle);
  void listenPrimTerm_(vpiHandle handle);
  void listenPathTerm_(vpiHandle handle);
  void listenTchkTerm_(vpiHandle handle);
  void listenNet_(vpiHandle handle);
  void listenEventTypespec_(vpiHandle handle);
  void listenNamedEvent_(vpiHandle handle);
  void listenNamedEventArray_(vpiHandle handle);
  void listenParameter_(vpiHandle handle);
  void listenDefParam_(vpiHandle handle);
  void listenSpecParam_(vpiHandle handle);
  void listenClassTypespec_(vpiHandle handle);
  void listenExtends_(vpiHandle handle);
  void listenImplements_(vpiHandle handle);
  void listenClassDefn_(vpiHandle handle);
  void listenClassObj_(vpiHandle handle);
  void listenInstance_(vpiHandle handle);
  void listenInterface_(vpiHandle handle);
  void listenProgram_(vpiHandle handle);
  void listenPackage_(vpiHandle handle);
  void listenModule_(vpiHandle handle);
  void listenCheckerDecl_(vpiHandle handle);
  void listenCheckerInst_(vpiHandle handle);
  void listenBindDirective_(vpiHandle handle);
  void listenShortRealTypespec_(vpiHandle handle);
  void listenRealTypespec_(vpiHandle handle);
  void listenByteTypespec_(vpiHandle handle);
  void listenShortIntTypespec_(vpiHandle handle);
  void listenIntTypespec_(vpiHandle handle);
  void listenLongIntTypespec_(vpiHandle handle);
  void listenIntegerTypespec_(vpiHandle handle);
  void listenTimeTypespec_(vpiHandle handle);
  void listenEnumTypespec_(vpiHandle handle);
  void listenStringTypespec_(vpiHandle handle);
  void listenTypedefTypespec_(vpiHandle handle);
  void listenChandleTypespec_(vpiHandle handle);
  void listenModuleTypespec_(vpiHandle handle);
  void listenProgramTypespec_(vpiHandle handle);
  void listenUdpDefnTypespec_(vpiHandle handle);
  void listenStructTypespec_(vpiHandle handle);
  void listenUnionTypespec_(vpiHandle handle);
  void listenLogicTypespec_(vpiHandle handle);
  void listenArrayTypespec_(vpiHandle handle);
  void listenPackageTypespec_(vpiHandle handle);
  void listenVoidTypespec_(vpiHandle handle);
  void listenUnsupportedTypespec_(vpiHandle handle);
  void listenSequenceTypespec_(vpiHandle handle);
  void listenPropertyTypespec_(vpiHandle handle);
  void listenInterfaceTypespec_(vpiHandle handle);
  void listenTypeParameter_(vpiHandle handle);
  void listenTypespecMember_(vpiHandle handle);
  void listenEnumConst_(vpiHandle handle);
  void listenBitTypespec_(vpiHandle handle);
  void listenTFCall_(vpiHandle handle);
  void listenUserSystf_(vpiHandle handle);
  void listenSysFuncCall_(vpiHandle handle);
  void listenSysTaskCall_(vpiHandle handle);
  void listenMethodFuncCall_(vpiHandle handle);
  void listenMethodTaskCall_(vpiHandle handle);
  void listenFuncCall_(vpiHandle handle);
  void listenTaskCall_(vpiHandle handle);
  void listenConstraintExpr_(vpiHandle handle);
  void listenConstraintOrdering_(vpiHandle handle);
  void listenConstraint_(vpiHandle handle);
  void listenImportTypespec_(vpiHandle handle);
  void listenDistItem_(vpiHandle handle);
  void listenDistribution_(vpiHandle handle);
  void listenImplication_(vpiHandle handle);
  void listenConstrIf_(vpiHandle handle);
  void listenConstrIfElse_(vpiHandle handle);
  void listenConstrForeach_(vpiHandle handle);
  void listenSoftDisable_(vpiHandle handle);
  void listenUniqueness_(vpiHandle handle);
  void listenGenStmt_(vpiHandle handle);
  void listenGenIf_(vpiHandle handle);
  void listenGenIfElse_(vpiHandle handle);
  void listenGenFor_(vpiHandle handle);
  void listenGenCase_(vpiHandle handle);
  void listenGenRegion_(vpiHandle handle);
  void listenClause_(vpiHandle handle);
  void listenConfigRule_(vpiHandle handle);
  void listenConfigDecl_(vpiHandle handle);
  void listenLibrary_(vpiHandle handle);
  void listenIncludeStmt_(vpiHandle handle);
  void listenDesign_(vpiHandle handle);
};

class VpiListenerTracer : public VpiListener {
 public:
  VpiListenerTracer(std::ostream &strm) : strm(strm) {}
  ~VpiListenerTracer() final = default;

    void enterAttribute(const Attribute* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAttribute(const Attribute* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIdentifier(const Identifier* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIdentifier(const Identifier* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterComment(const Comment* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveComment(const Comment* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterLetDecl(const LetDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveLetDecl(const LetDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAlways(const Always* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAlways(const Always* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterFinalStmt(const FinalStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveFinalStmt(const FinalStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterInitial(const Initial* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveInitial(const Initial* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDelayControl(const DelayControl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDelayControl(const DelayControl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDelayTerm(const DelayTerm* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDelayTerm(const DelayTerm* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterEventControl(const EventControl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveEventControl(const EventControl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRepeatControl(const RepeatControl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRepeatControl(const RepeatControl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterBegin(const Begin* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveBegin(const Begin* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterForkStmt(const ForkStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveForkStmt(const ForkStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterForStmt(const ForStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveForStmt(const ForStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIfStmt(const IfStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIfStmt(const IfStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterEventStmt(const EventStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveEventStmt(const EventStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterThread(const Thread* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveThread(const Thread* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterForeverStmt(const ForeverStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveForeverStmt(const ForeverStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterWaitStmt(const WaitStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveWaitStmt(const WaitStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterWaitFork(const WaitFork* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveWaitFork(const WaitFork* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterOrderedWait(const OrderedWait* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveOrderedWait(const OrderedWait* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDisable(const Disable* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDisable(const Disable* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDisableFork(const DisableFork* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDisableFork(const DisableFork* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterContinueStmt(const ContinueStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveContinueStmt(const ContinueStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterBreakStmt(const BreakStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveBreakStmt(const BreakStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterReturnStmt(const ReturnStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveReturnStmt(const ReturnStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterWhileStmt(const WhileStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveWhileStmt(const WhileStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRepeat(const Repeat* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRepeat(const Repeat* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDoWhile(const DoWhile* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDoWhile(const DoWhile* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIfElse(const IfElse* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIfElse(const IfElse* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCaseStmt(const CaseStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCaseStmt(const CaseStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterForce(const Force* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveForce(const Force* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAssignStmt(const AssignStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAssignStmt(const AssignStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDeassign(const Deassign* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDeassign(const Deassign* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRelease(const Release* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRelease(const Release* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterNullStmt(const NullStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveNullStmt(const NullStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterExpectStmt(const ExpectStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveExpectStmt(const ExpectStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterForeachStmt(const ForeachStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveForeachStmt(const ForeachStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenScopeArray(const GenScopeArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenScopeArray(const GenScopeArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAssert(const Assert* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAssert(const Assert* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCover(const Cover* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCover(const Cover* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAssume(const Assume* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAssume(const Assume* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRestrict(const Restrict* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRestrict(const Restrict* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImmediateAssert(const ImmediateAssert* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImmediateAssert(const ImmediateAssert* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImmediateAssume(const ImmediateAssume* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImmediateAssume(const ImmediateAssume* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImmediateCover(const ImmediateCover* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImmediateCover(const ImmediateCover* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCaseItem(const CaseItem* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCaseItem(const CaseItem* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAssignment(const Assignment* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAssignment(const Assignment* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAnyPattern(const AnyPattern* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAnyPattern(const AnyPattern* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTaggedPattern(const TaggedPattern* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTaggedPattern(const TaggedPattern* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterStructPattern(const StructPattern* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveStructPattern(const StructPattern* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUnsupportedExpr(const UnsupportedExpr* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUnsupportedExpr(const UnsupportedExpr* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUnsupportedStmt(const UnsupportedStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUnsupportedStmt(const UnsupportedStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPreprocMacroDefinition(const PreprocMacroDefinition* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePreprocMacroDefinition(const PreprocMacroDefinition* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPreprocMacroInstance(const PreprocMacroInstance* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePreprocMacroInstance(const PreprocMacroInstance* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSourceFile(const SourceFile* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSourceFile(const SourceFile* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSequenceInst(const SequenceInst* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSequenceInst(const SequenceInst* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSeqFormalDecl(const SeqFormalDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSeqFormalDecl(const SeqFormalDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSequenceDecl(const SequenceDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSequenceDecl(const SequenceDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPropFormalDecl(const PropFormalDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePropFormalDecl(const PropFormalDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPropertyInst(const PropertyInst* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePropertyInst(const PropertyInst* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPropertySpec(const PropertySpec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePropertySpec(const PropertySpec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPropertyDecl(const PropertyDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePropertyDecl(const PropertyDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClockedProperty(const ClockedProperty* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClockedProperty(const ClockedProperty* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCasePropertyItem(const CasePropertyItem* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCasePropertyItem(const CasePropertyItem* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCaseProperty(const CaseProperty* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCaseProperty(const CaseProperty* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterMulticlockSequenceExpr(const MulticlockSequenceExpr* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveMulticlockSequenceExpr(const MulticlockSequenceExpr* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClockedSeq(const ClockedSeq* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClockedSeq(const ClockedSeq* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstant(const Constant* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstant(const Constant* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterLetExpr(const LetExpr* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveLetExpr(const LetExpr* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterOperation(const Operation* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveOperation(const Operation* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRefObj(const RefObj* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRefObj(const RefObj* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRefInstance(const RefInstance* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRefInstance(const RefInstance* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRefTypespec(const RefTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRefTypespec(const RefTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPartSelect(const PartSelect* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePartSelect(const PartSelect* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIndexedPartSelect(const IndexedPartSelect* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIndexedPartSelect(const IndexedPartSelect* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterVarSelect(const VarSelect* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveVarSelect(const VarSelect* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterBitSelect(const BitSelect* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveBitSelect(const BitSelect* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterVariable(const Variable* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveVariable(const Variable* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterHierPath(const HierPath* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveHierPath(const HierPath* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterArrayExpr(const ArrayExpr* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveArrayExpr(const ArrayExpr* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRegArray(const RegArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRegArray(const RegArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterReg(const Reg* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveReg(const Reg* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTask(const Task* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTask(const Task* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterFunction(const Function* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveFunction(const Function* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTaskDecl(const TaskDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTaskDecl(const TaskDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterFunctionDecl(const FunctionDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveFunctionDecl(const FunctionDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterModport(const Modport* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveModport(const Modport* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterInterfaceTFDecl(const InterfaceTFDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveInterfaceTFDecl(const InterfaceTFDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterContAssign(const ContAssign* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveContAssign(const ContAssign* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterContAssignBit(const ContAssignBit* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveContAssignBit(const ContAssignBit* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPort(const Port* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePort(const Port* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPortBit(const PortBit* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePortBit(const PortBit* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCheckerPort(const CheckerPort* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCheckerPort(const CheckerPort* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCheckerInstPort(const CheckerInstPort* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCheckerInstPort(const CheckerInstPort* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGate(const Gate* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGate(const Gate* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSwitchTran(const SwitchTran* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSwitchTran(const SwitchTran* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUdp(const Udp* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUdp(const Udp* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterModPath(const ModPath* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveModPath(const ModPath* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTchk(const Tchk* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTchk(const Tchk* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRange(const Range* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRange(const Range* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUdpDefn(const UdpDefn* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUdpDefn(const UdpDefn* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTableEntry(const TableEntry* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTableEntry(const TableEntry* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIODecl(const IODecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIODecl(const IODecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterAlias(const Alias* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveAlias(const Alias* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClockingBlock(const ClockingBlock* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClockingBlock(const ClockingBlock* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClockingIODecl(const ClockingIODecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClockingIODecl(const ClockingIODecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCoverageOption(const CoverageOption* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCoverageOption(const CoverageOption* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCoverBin(const CoverBin* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCoverBin(const CoverBin* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCoverPoint(const CoverPoint* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCoverPoint(const CoverPoint* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCoverCross(const CoverCross* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCoverCross(const CoverCross* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCoverGroup(const CoverGroup* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCoverGroup(const CoverGroup* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterParamAssign(const ParamAssign* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveParamAssign(const ParamAssign* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterInterfaceArray(const InterfaceArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveInterfaceArray(const InterfaceArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterProgramArray(const ProgramArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveProgramArray(const ProgramArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterModuleArray(const ModuleArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveModuleArray(const ModuleArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGateArray(const GateArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGateArray(const GateArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSwitchArray(const SwitchArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSwitchArray(const SwitchArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUdpArray(const UdpArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUdpArray(const UdpArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPrimTerm(const PrimTerm* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePrimTerm(const PrimTerm* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPathTerm(const PathTerm* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePathTerm(const PathTerm* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTchkTerm(const TchkTerm* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTchkTerm(const TchkTerm* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterNet(const Net* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveNet(const Net* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterEventTypespec(const EventTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveEventTypespec(const EventTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterNamedEvent(const NamedEvent* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveNamedEvent(const NamedEvent* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterNamedEventArray(const NamedEventArray* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveNamedEventArray(const NamedEventArray* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterParameter(const Parameter* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveParameter(const Parameter* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDefParam(const DefParam* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDefParam(const DefParam* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSpecParam(const SpecParam* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSpecParam(const SpecParam* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClassTypespec(const ClassTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClassTypespec(const ClassTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterExtends(const Extends* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveExtends(const Extends* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImplements(const Implements* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImplements(const Implements* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClassDefn(const ClassDefn* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClassDefn(const ClassDefn* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClassObj(const ClassObj* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClassObj(const ClassObj* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterInterface(const Interface* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveInterface(const Interface* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterProgram(const Program* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveProgram(const Program* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPackage(const Package* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePackage(const Package* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterModule(const Module* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveModule(const Module* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCheckerDecl(const CheckerDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCheckerDecl(const CheckerDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterCheckerInst(const CheckerInst* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveCheckerInst(const CheckerInst* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterBindDirective(const BindDirective* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveBindDirective(const BindDirective* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterShortRealTypespec(const ShortRealTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveShortRealTypespec(const ShortRealTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterRealTypespec(const RealTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveRealTypespec(const RealTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterByteTypespec(const ByteTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveByteTypespec(const ByteTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterShortIntTypespec(const ShortIntTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveShortIntTypespec(const ShortIntTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIntTypespec(const IntTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIntTypespec(const IntTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterLongIntTypespec(const LongIntTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveLongIntTypespec(const LongIntTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIntegerTypespec(const IntegerTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIntegerTypespec(const IntegerTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTimeTypespec(const TimeTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTimeTypespec(const TimeTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterEnumTypespec(const EnumTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveEnumTypespec(const EnumTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterStringTypespec(const StringTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveStringTypespec(const StringTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTypedefTypespec(const TypedefTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTypedefTypespec(const TypedefTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterChandleTypespec(const ChandleTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveChandleTypespec(const ChandleTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterModuleTypespec(const ModuleTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveModuleTypespec(const ModuleTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterProgramTypespec(const ProgramTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveProgramTypespec(const ProgramTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUdpDefnTypespec(const UdpDefnTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUdpDefnTypespec(const UdpDefnTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterStructTypespec(const StructTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveStructTypespec(const StructTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUnionTypespec(const UnionTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUnionTypespec(const UnionTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterLogicTypespec(const LogicTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveLogicTypespec(const LogicTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterArrayTypespec(const ArrayTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveArrayTypespec(const ArrayTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPackageTypespec(const PackageTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePackageTypespec(const PackageTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterVoidTypespec(const VoidTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveVoidTypespec(const VoidTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUnsupportedTypespec(const UnsupportedTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUnsupportedTypespec(const UnsupportedTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSequenceTypespec(const SequenceTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSequenceTypespec(const SequenceTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterPropertyTypespec(const PropertyTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leavePropertyTypespec(const PropertyTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterInterfaceTypespec(const InterfaceTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveInterfaceTypespec(const InterfaceTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTypeParameter(const TypeParameter* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTypeParameter(const TypeParameter* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTypespecMember(const TypespecMember* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTypespecMember(const TypespecMember* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterEnumConst(const EnumConst* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveEnumConst(const EnumConst* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterBitTypespec(const BitTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveBitTypespec(const BitTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUserSystf(const UserSystf* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUserSystf(const UserSystf* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSysFuncCall(const SysFuncCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSysTaskCall(const SysTaskCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSysTaskCall(const SysTaskCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterMethodFuncCall(const MethodFuncCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveMethodFuncCall(const MethodFuncCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterMethodTaskCall(const MethodTaskCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveMethodTaskCall(const MethodTaskCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterFuncCall(const FuncCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveFuncCall(const FuncCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterTaskCall(const TaskCall* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveTaskCall(const TaskCall* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstraintOrdering(const ConstraintOrdering* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstraintOrdering(const ConstraintOrdering* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstraint(const Constraint* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstraint(const Constraint* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImportTypespec(const ImportTypespec* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImportTypespec(const ImportTypespec* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDistItem(const DistItem* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDistItem(const DistItem* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDistribution(const Distribution* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDistribution(const Distribution* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterImplication(const Implication* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveImplication(const Implication* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstrIf(const ConstrIf* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstrIf(const ConstrIf* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstrIfElse(const ConstrIfElse* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstrIfElse(const ConstrIfElse* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConstrForeach(const ConstrForeach* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConstrForeach(const ConstrForeach* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterSoftDisable(const SoftDisable* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveSoftDisable(const SoftDisable* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterUniqueness(const Uniqueness* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveUniqueness(const Uniqueness* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenIf(const GenIf* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenIf(const GenIf* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenIfElse(const GenIfElse* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenIfElse(const GenIfElse* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenFor(const GenFor* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenFor(const GenFor* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenCase(const GenCase* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenCase(const GenCase* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterGenRegion(const GenRegion* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveGenRegion(const GenRegion* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterClause(const Clause* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveClause(const Clause* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConfigRule(const ConfigRule* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConfigRule(const ConfigRule* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterConfigDecl(const ConfigDecl* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveConfigDecl(const ConfigDecl* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterLibrary(const Library* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveLibrary(const Library* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterIncludeStmt(const IncludeStmt* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveIncludeStmt(const IncludeStmt* object, vpiHandle handle) final { TRACE_LEAVE; }

    void enterDesign(const Design* object, vpiHandle handle) final { TRACE_ENTER; }
    void leaveDesign(const Design* object, vpiHandle handle) final { TRACE_LEAVE; }

 protected:
  std::ostream &strm;
  int32_t indent = -1;
};
}  // namespace uhdm

#endif  // UHDM_VPILISTENER_H
