#include <win_kernel/kernel_objects.h>
#include <win_kernel/memory_manager.h>
#include <debug.h>
#include <vmm/vm_operations.h>
#include <vmm/vmcs.h>

BYTE_PTR GetCurrent_KPRCB()
{
    return vmread(GUEST_GS_BASE) + KPRC_KPRCB;
}

STATUS ObjGetCurrent_ETHREAD(OUT BYTE_PTR* ethread)
{
    return WinMmCopyGuestMemory(ethread, GetCurrent_KPRCB() + KPRCB_KTHREAD, sizeof(QWORD));
}

STATUS ObjGetCurrent_EPROCESS(OUT BYTE_PTR* eprocess)
{
    BYTE_PTR ethread;

    if(ObjGetCurrent_ETHREAD(&ethread) != STATUS_SUCCESS)
        return STATUS_ETHREAD_NOT_AVAILABLE;
    return ObjGetObjectField(ETHREAD, ethread, ETHREAD_KPROCESS, eprocess);
}

/* 
    This function is based on the assembly code of ExpLookupHandleTableEntry.
    To have better a understanding, see ReactOS' implementation before reading this.
*/
STATUS ObjTranslateHandleToObject(IN HANDLE handle, IN BYTE_PTR handleTable, OUT BYTE_PTR* object)
{
    DWORD nextHandleNeedingPool;
    QWORD tableBase, tableLevel, res, tableResult, objectHeader;
    HANDLE handleBackup;
    
    if(handle == -1ULL)
    {
        ObjGetCurrent_EPROCESS(object);
        return STATUS_SUCCESS;
    }
    // Zero the 2 last bits (tag bits)
    handle &= ~(3ULL);
    if(WinMmCopyGuestMemory(&nextHandleNeedingPool, handleTable, sizeof(DWORD)) != STATUS_SUCCESS)
        return STATUS_COULD_NOT_TRANSLATE_HANDLE;
    if(handle >= nextHandleNeedingPool)
        return STATUS_COULD_NOT_TRANSLATE_HANDLE;
    
    if(WinMmCopyGuestMemory(&tableBase, handleTable + 0x8, sizeof(QWORD)) != STATUS_SUCCESS)
        return STATUS_COULD_NOT_TRANSLATE_HANDLE;
    tableLevel = tableBase & 3;
    // mov rax,rdx (case 1) OR mov rcx,rdx (case 2)
    handleBackup = handle;
    switch(tableLevel)
    {
        case 2:
        {    
            // rcx = handleBackup, rdx = handle, rax = handleBackup >> 0xa (res)
            // shr rcx,0Ah
            handleBackup >>= 0xa;
            // mov rax,rcx
            res = handleBackup;
            // and ecx,1FFh
            handleBackup &= 0x1ff;
            // shr rax,9
            res >>= 9;
            // and edx,3FFh
            handle &= 0x3ff;
            // mov rax,qword ptr [r8+rax*8-2]
            if(WinMmCopyGuestMemory(&res, tableBase + res * 8 - 2, sizeof(QWORD)) != STATUS_SUCCESS)
                return STATUS_COULD_NOT_TRANSLATE_HANDLE;
            // mov rax,qword ptr [rax+rcx*8]
            if(WinMmCopyGuestMemory(&res, res + handleBackup * 8, sizeof(QWORD)) != STATUS_SUCCESS)
                return STATUS_COULD_NOT_TRANSLATE_HANDLE;
            // lea rax,[rax+rdx*4]
            tableResult = res + handle * 4;
            break;
        }
        case 1:
        {
            // shr rax,0Ah
            handleBackup >>= 0xa;
            // and edx,3FFh
            handle &= 0x3ff;
            // mov rax,qword ptr [r8+rax*8-1]
            if(WinMmCopyGuestMemory(&handleBackup, tableBase + handleBackup * 8 - 1, sizeof(QWORD)) != STATUS_SUCCESS)
                return STATUS_COULD_NOT_TRANSLATE_HANDLE;
            // lea rax,[rax+rdx*4]
            tableResult = handleBackup + handle * 4;
            break;             
        }
        case 0:
        {
            // lea rax,[r8+rdx*4]
            tableResult = tableBase + handle * 4;
            break;
        }
        default:
        {
            Print("Wrong table level when translating handle to kernel object");
            ASSERT(FALSE);
        }
    }
    if(WinMmCopyGuestMemory(&objectHeader, tableResult, sizeof(QWORD)))
        return STATUS_COULD_NOT_TRANSLATE_HANDLE;
    objectHeader >>= 20;
    objectHeader *= 0x10;
    objectHeader += 0xffff000000000000ULL;
    *object = objectHeader + OBJECT_HEADER_BODY;
    return STATUS_SUCCESS;
}

STATUS ObjGet_ETHREAD_field(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case ETHREAD_THREAD_ID:
        case ETHREAD_KPROCESS:
            return WinMmCopyGuestMemory(value, object + field, sizeof(QWORD));
    }
}
STATUS ObjGet_EPROCESS_field(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case EPROCESS_EXE_NAME:
            return WinMmCopyGuestMemory(value, object + field, 15 * sizeof(CHAR));
        case EPROCESS_PID:
        case EPROCESS_OBJECT_TABLE:
            return WinMmCopyGuestMemory(value, object + field, sizeof(QWORD));
    }
}

STATUS ObjGetDeviceObjectField(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case DEVICE_OBJECT_DRIVER_OBJECT:
            return WinMmCopyGuestMemory(value, object + field, sizeof(QWORD));
    }
}
STATUS ObjGetDriverObjectField(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case DRIVER_OBJECT_NAME:
            return WinMmCopyGuestMemory(value, object + field, sizeof(WIN_KERNEL_UNICODE_STRING));
    }
}
STATUS ObjGet_VPB_field(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case VPB_DEVICE_OBJECT:
            return WinMmCopyGuestMemory(value, object + field, sizeof(QWORD));
    }
}

STATUS ObjGet_FILE_OBJECT_field(IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(field)
    {
        case FILE_OBJECT_TYPE:
        case FILE_OBJECT_VPB:
        case FILE_OBJECT_SCB:
            return WinMmCopyGuestMemory(value, object + field, sizeof(QWORD));
        case FILE_OBJECT_FILE_NAME:
            return WinMmCopyGuestMemory(value, object + field, sizeof(WIN_KERNEL_UNICODE_STRING));
    }
}

STATUS ObjGetObjectField(IN BYTE objectType, IN QWORD object, IN QWORD field, OUT PVOID value)
{
    switch(objectType)
    {
        case ETHREAD:
            return ObjGet_ETHREAD_field(object, field, value);
        case EPROCESS:
            return ObjGet_EPROCESS_field(object, field, value);
        case FILE_OBJECT:
            return ObjGet_FILE_OBJECT_field(object, field, value);
        case DEVICE_OBJECT:
            return ObjGetDeviceObjectField(object, field, value);
        case DRIVER_OBJECT:
            return ObjGetDriverObjectField(object, field, value);
        case VPB:
            return ObjGet_VPB_field(object, field, value);
        default:
        {
            Print("Unsupported object type!\n");
            ASSERT(FALSE);
        }
    }

    return STATUS_OBJECT_FIELD_NOT_FOUND;
}
