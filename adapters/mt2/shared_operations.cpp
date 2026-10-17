#include "shared_operations.h"
namespace mt2fw::mt2 {
namespace {std::atomic<std::shared_ptr<NativeRoutes>> routes;}
void configure_shared_operations(std::shared_ptr<NativeRoutes> value){routes.store(std::move(value),std::memory_order_release);}
void* dispatch_create(const MT2FW_MT2_CreateCall& call,NativeRoutes::CreateOriginal original){
    if(auto value=routes.load(std::memory_order_acquire))return value->create(call,original);
    return original(call.manager,call.type,call.variant,call.pose,call.a,call.b,call.c,call.flag!=0);
}
void dispatch_remove(const MT2FW_MT2_RemoveCall& call,NativeRoutes::RemoveOriginal original){
    if(auto value=routes.load(std::memory_order_acquire)){value->remove(call,original);return;}original(call.manager,call.array);
}
}
