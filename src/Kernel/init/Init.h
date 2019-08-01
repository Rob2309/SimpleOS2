#pragma once

#define INIT_STAGE_DEVDRIVERS 1
#define INIT_STAGE_FSDRIVERS 2
#define INIT_STAGE_EXECHANDLERS 3

#define REGISTER_INIT_FUNC(func, stage) static uint64 func##_kinit_array_entry[] __attribute__((used)) __attribute__((section(".kinit_array"))) = { (uint64)&func, stage }