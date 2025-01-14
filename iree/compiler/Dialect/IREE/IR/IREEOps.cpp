// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iree/compiler/Dialect/IREE/IR/IREEOps.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/SMLoc.h"
#include "mlir/Dialect/StandardOps/Ops.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Support/STLExtras.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {

//===----------------------------------------------------------------------===//
// iree.return
//===----------------------------------------------------------------------===//

static ParseResult parseReturnOp(OpAsmParser &parser, OperationState &state) {
  SmallVector<OpAsmParser::OperandType, 2> opInfo;
  SmallVector<Type, 2> types;
  llvm::SMLoc loc = parser.getCurrentLocation();
  return failure(parser.parseOperandList(opInfo) ||
                 (!opInfo.empty() && parser.parseColonTypeList(types)) ||
                 parser.resolveOperands(opInfo, types, loc, state.operands));
}

static void printReturnOp(OpAsmPrinter &p, ReturnOp op) {
  p << "iree.return";
  if (op.getNumOperands() > 0) {
    p << ' ';
    p.printOperands(op.operand_begin(), op.operand_end());
    p << " : ";
    interleaveComma(op.getOperandTypes(), p);
  }
}

//===----------------------------------------------------------------------===//
// iree.load_input
//===----------------------------------------------------------------------===//

ParseResult parseLoadInputOp(OpAsmParser &parser, OperationState &state) {
  OpAsmParser::OperandType operand;
  Type argType;
  if (parser.parseLParen() || parser.parseOperand(operand) ||
      parser.parseColonType(argType) || parser.parseRParen() ||
      parser.resolveOperand(operand, argType, state.operands) ||
      parser.parseOptionalAttrDict(state.attributes)) {
    return failure();
  }
  Type outputType;
  if (parser.parseColonType(outputType) ||
      parser.addTypeToList(outputType, state.types)) {
    return failure();
  }
  return success();
}

void printLoadInputOp(OpAsmPrinter &printer, Operation *op) {
  auto inputValue = op->getOperand(0);
  auto outputValue = op->getResult(0);
  printer << op->getName() << '(';
  printer.printOperand(inputValue);
  printer << " : ";
  printer.printType(inputValue.getType());
  printer << ") ";
  printer.printOptionalAttrDict(op->getAttrs());
  printer << " : ";
  printer.printType(outputValue.getType());
}

//===----------------------------------------------------------------------===//
// iree.store_output
//===----------------------------------------------------------------------===//

ParseResult parseStoreOutputOp(OpAsmParser &parser, OperationState &state) {
  OpAsmParser::OperandType op0, op1;
  Type argType0, argType1;
  if (parser.parseLParen() || parser.parseOperand(op0) ||
      parser.parseColonType(argType0) || parser.parseComma() ||
      parser.resolveOperand(op0, argType0, state.operands) ||
      parser.parseOperand(op1) || parser.parseColonType(argType1) ||
      parser.parseRParen() ||
      parser.resolveOperand(op1, argType1, state.operands) ||
      parser.parseOptionalAttrDict(state.attributes)) {
    return failure();
  }
  return success();
}

void printStoreOutputOp(OpAsmPrinter &printer, Operation *op) {
  auto inputValue = op->getOperand(0);
  auto outputValue = op->getOperand(1);
  printer << op->getName() << '(';
  printer.printOperand(inputValue);
  printer << " : ";
  printer.printType(inputValue.getType());
  printer << ", ";
  printer.printOperand(outputValue);
  printer << " : ";
  printer.printType(outputValue.getType());
  printer << ") ";
  printer.printOptionalAttrDict(op->getAttrs());
}

//===----------------------------------------------------------------------===//
// iree.store_reduce
//===----------------------------------------------------------------------===//

ParseResult parseStoreReduceOp(OpAsmParser &parser, OperationState &state) {
  OpAsmParser::OperandType src, dst;
  Type srcType, dstType;
  SymbolRefAttr reductionFn;
  if (parser.parseLParen() || parser.parseOperand(src) ||
      parser.parseColonType(srcType) ||
      parser.resolveOperand(src, srcType, state.operands) ||
      parser.parseComma() || parser.parseOperand(dst) ||
      parser.parseColonType(dstType) ||
      parser.resolveOperand(dst, dstType, state.operands) ||
      parser.parseComma() ||
      parser.parseAttribute(reductionFn, "reduction_fn", state.attributes) ||
      parser.parseRParen() || parser.parseOptionalAttrDict(state.attributes)) {
    return failure();
  }
  return success();
}

void printStoreReduceOp(OpAsmPrinter &printer, Operation *op) {
  auto storeReduceOp = cast<IREE::StoreReduceOp>(op);
  printer << op->getName() << '(';
  printer.printOperand(storeReduceOp.src());
  printer << " : ";
  printer.printType(storeReduceOp.src().getType());
  printer << ", ";
  printer.printOperand(storeReduceOp.dst());
  printer << " : ";
  printer.printType(storeReduceOp.dst().getType());
  printer << ", ";
  printer.printAttribute(storeReduceOp.reduction_fnAttr());
  printer << ") ";
  printer.printOptionalAttrDict(op->getAttrs(), StringRef("reduction_fn"));
}

//===----------------------------------------------------------------------===//
// iree.do_not_optimize
//===----------------------------------------------------------------------===//

void DoNotOptimizeOp::build(Builder *builder, OperationState &state,
                            ValueRange operands,
                            ArrayRef<NamedAttribute> attributes) {
  state.addOperands(operands);
  state.addTypes(llvm::to_vector<2>(operands.getTypes()));
  state.addAttributes(attributes);
}

ParseResult parseDoNotOptimizeOp(OpAsmParser &parser, OperationState &state) {
  SmallVector<OpAsmParser::OperandType, 2> args;
  // Operands and results have the same types.
  auto &operandTypes = state.types;

  if (failed(parser.parseLParen()) || failed(parser.parseOperandList(args)) ||
      failed(parser.parseRParen()) ||
      failed(parser.parseOptionalAttrDict(state.attributes)) ||
      failed(parser.parseOptionalColonTypeList(state.types)) ||
      failed(parser.resolveOperands(
          args, operandTypes, parser.getCurrentLocation(), state.operands))) {
    return failure();
  }

  return success();
}

void printDoNotOptimizeOp(OpAsmPrinter &p, Operation *op) {
  p << "iree.do_not_optimize";
  p << "(";
  p.printOperands(op->getOperands());
  p << ")";
  p.printOptionalAttrDict(op->getAttrs());

  if (op->getNumOperands() != 0) {
    p << " : ";
    interleaveComma(op->getOperandTypes(), p);
  }
}

static LogicalResult verifyDoNotOptimizeOp(DoNotOptimizeOp op) {
  if (op.getNumOperands() != op.getNumResults()) {
    return op.emitOpError()
           << "must have same number of operands and results, but has "
           << op.getNumOperands() << " and " << op.getNumResults()
           << ", respectively";
  }

  for (int i = 0, e = op.getNumOperands(); i < e; ++i) {
    if (op.getOperand(i).getType() != op.getResult(i).getType()) {
      op.emitOpError() << "must have same operand and result types, but they "
                          "differ at index "
                       << i;
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// iree.unfoldable_constant
//===----------------------------------------------------------------------===//

// Parsing/printing copied from std.constant

ParseResult parseUnfoldableConstantOp(OpAsmParser &parser,
                                      OperationState &state) {
  Attribute valueAttr;
  if (parser.parseOptionalAttrDict(state.attributes) ||
      parser.parseAttribute(valueAttr, "value", state.attributes))
    return failure();

  // If the attribute is a symbol reference, then we expect a trailing type.
  Type type;
  if (!valueAttr.isa<SymbolRefAttr>())
    type = valueAttr.getType();
  else if (parser.parseColonType(type))
    return failure();

  // Add the attribute type to the list.
  return parser.addTypeToList(type, state.types);
}

void printUnfoldableConstantOp(OpAsmPrinter &p, Operation *op) {
  auto constOp = cast<IREE::UnfoldableConstantOp>(op);
  p << "iree.unfoldable_constant ";
  p.printOptionalAttrDict(constOp.getAttrs(), /*elidedAttrs=*/{"value"});

  if (constOp.getAttrs().size() > 1) p << ' ';
  p << constOp.value();

  // If the value is a symbol reference, print a trailing type.
  if (constOp.value().isa<SymbolRefAttr>()) p << " : " << constOp.getType();
}

namespace {

struct ExpandUnfoldableConstantOp
    : public OpRewritePattern<UnfoldableConstantOp> {
  using OpRewritePattern<IREE::UnfoldableConstantOp>::OpRewritePattern;
  PatternMatchResult matchAndRewrite(UnfoldableConstantOp op,
                                     PatternRewriter &rewriter) const override {
    auto stdConst = rewriter.create<ConstantOp>(op.getLoc(), op.value());
    rewriter.replaceOpWithNewOp<DoNotOptimizeOp>(op, stdConst.getResult());
    return matchSuccess();
  }
};

}  // namespace

void UnfoldableConstantOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<ExpandUnfoldableConstantOp>(context);
}

#define GET_OP_CLASSES
#include "iree/compiler/Dialect/IREE/IR/IREEOps.cpp.inc"

}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir
