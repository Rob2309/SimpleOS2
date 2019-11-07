#pragma once

typedef void (*KillHandler)();

void setkillhandler(KillHandler handler);