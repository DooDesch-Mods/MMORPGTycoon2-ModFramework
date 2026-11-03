#include "sdk/native_services.h"
#include <stddef.h>
typedef char v1_size_unchanged[sizeof(MT2FW_MT2_ServiceV1)==144?1:-1];
typedef char v1_frame_slot_unchanged[offsetof(MT2FW_MT2_ServiceV1,frame_subscribe)==56?1:-1];
typedef char v1_last_slot_unchanged[offsetof(MT2FW_MT2_ServiceV1,section_read)==136?1:-1];
typedef char v2_prefix[offsetof(MT2FW_MT2_ServiceV2,v1)==0?1:-1];
typedef char v2_bind_slot[offsetof(MT2FW_MT2_ServiceV2,character_preview_int_bind)==144?1:-1];
typedef char v2_unbind_slot[offsetof(MT2FW_MT2_ServiceV2,character_preview_int_unbind)==152?1:-1];
typedef char v2_size[sizeof(MT2FW_MT2_ServiceV2)==160?1:-1];
typedef char call_size[sizeof(MT2FW_MT2_CharacterPreviewIntCall)==40?1:-1];
typedef char selection_size[sizeof(MT2FW_MT2_CharacterPreviewIntSelection)==176?1:-1];
typedef char selection_cause_offset[offsetof(MT2FW_MT2_CharacterPreviewIntSelection,cause)==16?1:-1];
int main(void){return MT2FW_MT2_SERVICE_VERSION==1u&&MT2FW_MT2_PREVIEW_SERVICE_VERSION==2u?0:1;}
