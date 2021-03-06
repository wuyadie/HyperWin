#include <win_kernel/process.h>
#include <win_kernel/memory_manager.h>
#include <win_kernel/kernel_objects.h>
#include <debug.h>
#include <vmm/vmm.h>
#include <utils/allocation.h>
#include <utils/utils.h>

QWORD_MAP g_groupsData;

VOID PspInit()
{
    MapCreate(&g_groupsData, BasicHashFunction, BASIC_HASH_LEN, DefaultEqualityFunction);
}

STATUS PspMarkProcessProtected(IN HANDLE processHandle, IN BYTE protectionLevel, IN BYTE signLevel, 
    IN BYTE sectionSignLevel)
{
    QWORD pid, eprocess, handleTable, protectedProcessEprocess;
    STATUS status;

    ObjGetCurrent_EPROCESS(&eprocess);
    ObjGetObjectField(EPROCESS, eprocess, EPROCESS_OBJECT_TABLE, &handleTable);
    SUCCESS_OR_RETURN(ObjTranslateHandleToObject(processHandle, handleTable, &protectedProcessEprocess));
    SUCCESS_OR_RETURN(WinMmCopyMemoryToGuest(protectedProcessEprocess + EPROCESS_SIGN_LEVEL, &signLevel, sizeof(BYTE)));
    SUCCESS_OR_RETURN(WinMmCopyMemoryToGuest(protectedProcessEprocess + EPROCESS_SECTION_SIGN_LEVEL, 
        &sectionSignLevel, sizeof(BYTE)));
    SUCCESS_OR_RETURN(WinMmCopyMemoryToGuest(protectedProcessEprocess + EPROCESS_PROTECTION, 
        &protectionLevel, sizeof(BYTE)));
    
    return STATUS_SUCCESS;
}

STATUS PspCreateNewGroup(IN QWORD groupId, IN BOOL includeSelf)
{
    PPROCESS_GROUP groupData;
    PHEAP heap;
    QWORD eprocess, pid;
    STATUS status;

    if(MapGet(&g_groupsData, groupId) != MAP_KEY_NOT_FOUND)
        return STATUS_GROUP_ALREADY_EXISTS;

    heap = &VmmGetVmmStruct()->currentCPU->sharedData->heap;
    SUCCESS_OR_RETURN(heap->allocate(heap, sizeof(PROCESS_GROUP), &groupData));
    SUCCESS_OR_RETURN(heap->allocate(heap, sizeof(QWORD_SET), &groupData->processes));
    SUCCESS_OR_RETURN(SetInit(&groupData->processes, BASIC_HASH_LEN, BasicHashFunction));
    groupData->groupId = groupId;
    // Should the current process be included?
    if(includeSelf)
    {
        ObjGetCurrent_EPROCESS(&eprocess);
        SUCCESS_OR_RETURN(ObjGetObjectField(EPROCESS, eprocess, EPROCESS_PID, &pid));
        SetInsert(&groupData->processes, pid);
    }
    MapSet(&g_groupsData, groupId, groupData);

    return STATUS_SUCCESS;
}