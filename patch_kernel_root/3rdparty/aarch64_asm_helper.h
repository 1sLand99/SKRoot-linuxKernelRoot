﻿#pragma once
#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>

#define ASMJIT_STATIC
#include "asmjit2-src/src/asmjit/asmjit.h"
#include "asmjit2-src/src/asmjit/arm/a64assembler.h"

class MyAsmJitErrorHandler : public asmjit::ErrorHandler {
public:
	void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
		std::cerr << "[发生错误] " << asmjit::DebugUtils::errorAsString(err) << ": " << message << std::endl;
		isErr = true;
	}
	bool isError() { return isErr; }
private:
	bool isErr = false;
};

struct aarch64_asm_info {
	std::unique_ptr<asmjit::CodeHolder> codeHolder;
	std::unique_ptr<asmjit::a64::Assembler> a;
	std::unique_ptr<asmjit::StringLogger> logger;
	std::unique_ptr<MyAsmJitErrorHandler> err;
};

static aarch64_asm_info init_aarch64_asm() {
	asmjit::Environment env(asmjit::Arch::kAArch64);
	auto code = std::make_unique<asmjit::CodeHolder>();
	code->init(env);
	auto logger = std::make_unique<asmjit::StringLogger>();
	logger->addFlags(asmjit::FormatFlags::kNone);
	code->setLogger(logger.get());
	auto a = std::make_unique<asmjit::a64::Assembler>(code.get());
	auto err = std::make_unique<MyAsmJitErrorHandler>();
	a->setErrorHandler(err.get());
	return { std::move(code), std::move(a), std::move(logger), std::move(err)};
}

static std::string aarch64_asm_to_bytes(const aarch64_asm_info & asm_info) {
	if (asm_info.err->isError()) {
		return {};
	}
	asm_info.codeHolder->flatten();
	asmjit::Section* sec = asm_info.codeHolder->sectionById(0);
	const asmjit::CodeBuffer& buf = sec->buffer();
	const uint8_t* data = buf.data();
	size_t dataSize = buf.size();
	std::ostringstream oss;
	for (size_t i = 0; i < dataSize; i++) {
		oss << std::uppercase
			<< std::hex
			<< std::setw(2)
			<< std::setfill('0')
			<< static_cast<int>(data[i]);
	}
	return oss.str();
}

static bool aarch64_asm_b(std::unique_ptr<asmjit::a64::Assembler> &a, int32_t b_value) {
	if (b_value % 4 != 0) {
		std::cout << "[发生错误] The B instruction offset must be a multiple of 4" << std::endl;
		return false;
	}
	int64_t imm26 = b_value >> 2;
	if (imm26 < -(1LL << 25) || imm26 >= (1LL << 25)) {
		std::cout << "[发生错误] B instruction offset exceeds ± 128MB range" << std::endl;
		return false;
	}
	int32_t offset = b_value >> 2;
	uint32_t instr = 0x14000000 | (offset & 0x03FFFFFF);
	a->embed((const uint8_t*)&instr, sizeof(instr));
	return true;
}

static bool aarch64_asm_bl(std::unique_ptr<asmjit::a64::Assembler>& a, int32_t bl_value) {
	if (bl_value % 4 != 0) {
		std::cout << "[发生错误] The BL instruction offset must be a multiple of 4" << std::endl;
		return false;
	}
	int64_t imm26 = bl_value >> 2;
	if (imm26 < -(1LL << 25) || imm26 >= (1LL << 25)) {
		std::cout << "[发生错误] BL instruction offset exceeds ±128MB range" << std::endl;
		return false;
	}
	int32_t offset26 = bl_value >> 2;
	uint32_t instr = 0x94000000u | (static_cast<uint32_t>(offset26) & 0x03FFFFFFu);
	a->embed(reinterpret_cast<const uint8_t*>(&instr), sizeof(instr));
	return true;
}

static bool aarch64_asm_adr_x(std::unique_ptr<asmjit::a64::Assembler>& a, asmjit::a64::GpX x, int32_t adr_value) {
	if (adr_value % 4 != 0) {
		std::cout << "[发生错误] The ADR instruction offset must be a multiple of 4" << std::endl;
		return false;
	}
	int64_t immSigned = adr_value >> 2;
	if (immSigned < -(1LL << 20) || immSigned >((1LL << 20) - 1)) {
		std::cout << "[发生错误] ADR instruction offset exceeds ± 1MB range" << std::endl;
		return false;
	}
	aarch64_asm_info asm_info = init_aarch64_asm();
	auto& a2 = asm_info.a;
	asmjit::Label label_statr = a2->newLabel();
	int32_t size = abs(adr_value);
	std::vector<uint8_t> filler(size, 0x00);
	if (adr_value > 0) {
		a2->adr(x, label_statr);
		a2->embed(filler.data(), size);
		a2->bind(label_statr);
		a2->nop();
	} else {
		a2->bind(label_statr);
		a2->embed(filler.data(), size);
		a2->adr(x, label_statr);
	}
	asm_info.codeHolder->flatten();
	asmjit::Section* sec = asm_info.codeHolder->sectionById(0);
	const asmjit::CodeBuffer& buf = sec->buffer();
	uint8_t* data = (uint8_t*)buf.data();
	size_t dataSize = buf.size();
	if (adr_value < 0) {
		data += size;
	}
	a->embed((const uint8_t*)data, 4);
	return true;
}