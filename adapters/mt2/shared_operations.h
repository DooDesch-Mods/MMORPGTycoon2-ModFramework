#pragma once
#include "native_routes.h"
namespace mt2fw::mt2 {
void configure_shared_operations(std::shared_ptr<NativeRoutes>);
void* dispatch_create(const MT2FW_MT2_CreateCall&,NativeRoutes::CreateOriginal);
void dispatch_remove(const MT2FW_MT2_RemoveCall&,NativeRoutes::RemoveOriginal);
}
