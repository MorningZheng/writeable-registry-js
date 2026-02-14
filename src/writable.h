#pragma once

#define STRICT
#define UNICODE
#define _UNICODE

#include <napi.h>
#include <windows.h>
#include <string>
#include <memory>
#include <iostream>
#include <cassert>

void test(const Napi::CallbackInfo& info);

Napi::Value ReadFromKey(const Napi::CallbackInfo& info);
Napi::Value DeleteFromPath(const Napi::CallbackInfo& info);