/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/compiler/passes/value_reduction_pass.h>

#include <alloy/backend/backend.h>
#include <alloy/compiler/compiler.h>
#include <alloy/runtime/runtime.h>

#include <bitset>

using namespace alloy;
using namespace alloy::backend;
using namespace alloy::compiler;
using namespace alloy::compiler::passes;
using namespace alloy::frontend;
using namespace alloy::hir;
using namespace alloy::runtime;


ValueReductionPass::ValueReductionPass() :
    CompilerPass() {
}

ValueReductionPass::~ValueReductionPass() {
}

void ValueReductionPass::ComputeLastUse(Value* value) {
  uint32_t max_ordinal = 0;
  Value::Use* last_use = NULL;
  auto use = value->use_head;
  while (use) {
    if (!last_use || use->instr->ordinal >= max_ordinal) {
      last_use = use;
      max_ordinal = use->instr->ordinal;
    }
    use = use->next;
  }
  value->tag = last_use->instr;
}

int ValueReductionPass::Run(HIRBuilder* builder) {
  // Walk each block and reuse variable ordinals as much as possible.

  // Let's hope this is enough.
  std::bitset<1024> ordinals;

  auto block = builder->first_block();
  while (block) {
    // Reset used ordinals.
    ordinals.reset();

    // Renumber all instructions to make liveness tracking easier.
    uint32_t instr_ordinal = 0;
    auto instr = block->instr_head;
    while (instr) {
      instr->ordinal = instr_ordinal++;
      instr = instr->next;
    }

    instr = block->instr_head;
    while (instr) {
      const OpcodeInfo* info = instr->opcode;
      OpcodeSignatureType dest_type = GET_OPCODE_SIG_TYPE_DEST(info->signature);
      OpcodeSignatureType src1_type = GET_OPCODE_SIG_TYPE_SRC1(info->signature);
      OpcodeSignatureType src2_type = GET_OPCODE_SIG_TYPE_SRC2(info->signature);
      OpcodeSignatureType src3_type = GET_OPCODE_SIG_TYPE_SRC3(info->signature);
      if (src1_type == OPCODE_SIG_TYPE_V && !instr->src1.value->IsConstant()) {
        auto v = instr->src1.value;
        if (!v->tag) {
          ComputeLastUse(v);
        }
        if (v->tag == instr) {
          // Available.
          ordinals.set(v->ordinal, false);
        }
      }
      if (src2_type == OPCODE_SIG_TYPE_V && !instr->src2.value->IsConstant()) {
        auto v = instr->src2.value;
        if (!v->tag) {
          ComputeLastUse(v);
        }
        if (v->tag == instr) {
          // Available.
          ordinals.set(v->ordinal, false);
        }
      }
      if (src3_type == OPCODE_SIG_TYPE_V && !instr->src3.value->IsConstant()) {
        auto v = instr->src3.value;
        if (!v->tag) {
          ComputeLastUse(v);
        }
        if (v->tag == instr) {
          // Available.
          ordinals.set(v->ordinal, false);
        }
      }
      if (dest_type == OPCODE_SIG_TYPE_V) {
        // Dest values are processed last, as they may be able to reuse a
        // source value ordinal.
        auto v = instr->dest;
        // Find a lower ordinal.
        for (auto n = 0; n < ordinals.size(); n++) {
          if (!ordinals.test(n)) {
            ordinals.set(n);
            v->ordinal = n;
            break;
          }
        }
      }

      instr = instr->next;
    }

    block = block->next;
  }

  return 0;
}
