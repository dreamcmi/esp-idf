/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

/**
 * This file will be included in `tasks.c` file, thus, it must NOT be included
 * by any (other) file.
 * The functions below only consist in getters for the static variables in
 * `tasks.c` file.
 * The only source files that should call these functions are the ones in
 * `/additions` directory.
 */

/* -------------------------------------------------- Task Snapshot ----------------------------------------------------
 *
 * ------------------------------------------------------------------------------------------------------------------ */

#if CONFIG_FREERTOS_ENABLE_TASK_SNAPSHOT

#include "task_snapshot.h"

/**
 * @brief List of all task lists in FreeRTOS
 *
 * @note There are currently differing number of task list between SMP FreeRTOS and ESP-IDF FreeRTOS
 */
static List_t *non_ready_task_lists[] = {
    #ifdef CONFIG_FREERTOS_SMP
        &xPendingReadyList,
    #else
        &xPendingReadyList[0],
        #ifndef CONFIG_FREERTOS_UNICORE
            &xPendingReadyList[1],
        #endif // CONFIG_FREERTOS_UNICORE
    #endif //CONFIG_FREERTOS_SMP
    &xDelayedTaskList1,
    &xDelayedTaskList2,
    #if( INCLUDE_vTaskDelete == 1 )
        &xTasksWaitingTermination,
    #endif
    #if( INCLUDE_vTaskSuspend == 1 )
        &xSuspendedTaskList,
    #endif
};

/**
 * @brief Get the next task list to traverse
 *
 * - Given a particular task list, this function returns the next task to traverse.
 * - The task lists are returned in the following precedence
 *      - Ready lists (highest to lowers priority)
 *      - Pending ready list(s)
 *      - Delayed list 1
 *      - Delayed list 2
 *      - Waiting termination list
 *      - Suspended list
 *
 * @param pxCurTaskList Previously traversed task list (or NULL if obtaining the first task list)
 * @return List_t* The next task list to traverse (or NULL of all task lists have been traversed)
 */
static List_t *pxGetNextTaskList(List_t *pxCurTaskList)
{
    List_t *pxNextTaskList = NULL;

    // No Current List. Start from the highest priority ready task list
    if (pxCurTaskList == NULL)
    {
        pxNextTaskList = &pxReadyTasksLists[configMAX_PRIORITIES - 1];
    }
    // Current list is one of the ready task lists. Find the current priority, and return the next lower priority ready task list
    else if (pxCurTaskList >= &pxReadyTasksLists[0] && pxCurTaskList <= &pxReadyTasksLists[configMAX_PRIORITIES - 1] )
    {
        // Find the current priority
        int cur_priority;
        for (cur_priority = configMAX_PRIORITIES - 1; cur_priority >= 0; cur_priority--) {
            if (pxCurTaskList == &pxReadyTasksLists[cur_priority]) {
                break;
            }
        }
        // Return the ready task list at (cur_priority - 1), or the pending ready task list
        if (cur_priority > 0)
        {
            pxNextTaskList = &pxReadyTasksLists[cur_priority - 1];
        }
        // We've reached the end of the Ready Task Lists.  We get the next list from the non-ready task lists
        else if (cur_priority == 0)
        {
            pxNextTaskList = non_ready_task_lists[0];
        }
        else
        {
            abort();    // This should never occur
        }
    }

    // Current list is one of the non-ready task lists. Fetch the next non-ready task list
    if (pxNextTaskList == NULL) {
        int cur_list_idx;
        const int num_non_ready_task_lists = (sizeof(non_ready_task_lists) / sizeof(List_t *));
        // Note: - 1 so that if the current list is the last on non_ready_task_lists[], the next list will return NULL
        for (cur_list_idx = 0; cur_list_idx < num_non_ready_task_lists - 1; cur_list_idx++) {
            if (pxCurTaskList == non_ready_task_lists[cur_list_idx]) {
                pxNextTaskList = non_ready_task_lists[cur_list_idx + 1];
                break;
            }
        }
    }

    return pxNextTaskList;
}

TaskHandle_t pxTaskGetNext( TaskHandle_t pxTask )
{
    TCB_t *pxTCB = (TCB_t *)pxTask;
    // Check current task is valid
    if (pxTCB != NULL && !portVALID_TCB_MEM(pxTCB)) {
        return NULL;
    }

    List_t *pxCurTaskList;
    const ListItem_t *pxCurListItem;
    if (pxTCB == NULL) {
        // Starting traversal for the first time
        pxCurTaskList = pxGetNextTaskList(NULL);
        pxCurListItem = listGET_END_MARKER(pxCurTaskList);
    } else {
        // Continuing traversal
        pxCurTaskList = listLIST_ITEM_CONTAINER(&pxTCB->xStateListItem);
        pxCurListItem = &pxTCB->xStateListItem;
    }

    ListItem_t *pxNextListItem = NULL;
    if (pxCurListItem->pxNext == listGET_END_MARKER(pxCurTaskList)) {
        List_t *pxNextTaskList = pxGetNextTaskList(pxCurTaskList);
        while (pxNextTaskList != NULL) {
            if (!listLIST_IS_EMPTY(pxNextTaskList)) {
                // Get the first item in the next task list
                pxNextListItem = listGET_HEAD_ENTRY(pxNextTaskList);
                break;
            }
            // Task list is empty. Get the next task list
            pxNextTaskList = pxGetNextTaskList(pxNextTaskList);
        }
    } else {
        //There are still more items in the current task list. Get the next item
        pxNextListItem = listGET_NEXT(pxCurListItem);
    }

    TCB_t *pxNextTCB;
    if (pxNextListItem == NULL) {
        pxNextTCB = NULL;
    } else {
        pxNextTCB = (TCB_t *)listGET_LIST_ITEM_OWNER(pxNextListItem);
    }

    return pxNextTCB;
}

BaseType_t vTaskGetSnapshot( TaskHandle_t pxTask, TaskSnapshot_t *pxTaskSnapshot )
{
    if (portVALID_TCB_MEM(pxTask) == false || pxTaskSnapshot == NULL) {
        return pdFALSE;
    }

    TCB_t *pxTCB = (TCB_t *)pxTask;
    pxTaskSnapshot->pxTCB = pxTCB;
    pxTaskSnapshot->pxTopOfStack = (StackType_t *)pxTCB->pxTopOfStack;
    pxTaskSnapshot->pxEndOfStack = (StackType_t *)pxTCB->pxEndOfStack;
    return pdTRUE;
}

UBaseType_t uxTaskGetSnapshotAll( TaskSnapshot_t * const pxTaskSnapshotArray, const UBaseType_t uxArrayLength, UBaseType_t * const pxTCBSize )
{
    UBaseType_t uxArrayNumFilled = 0;

    //Traverse all of the tasks lists
    List_t *pxCurTaskList = pxGetNextTaskList(NULL);    //Get the first task list
    while (pxCurTaskList != NULL && uxArrayNumFilled < uxArrayLength) {
        if (!listLIST_IS_EMPTY(pxCurTaskList)) {
            const ListItem_t *pxCurListItem;
            //Walk each task on the current task list
            pxCurListItem = listGET_HEAD_ENTRY(pxCurTaskList);
            while (pxCurListItem != listGET_END_MARKER(pxCurTaskList)) {
                TCB_t *pxTCB = (TCB_t *)listGET_LIST_ITEM_OWNER(pxCurListItem);
                vTaskGetSnapshot((TaskHandle_t)pxTCB, &pxTaskSnapshotArray[uxArrayNumFilled]);
                uxArrayNumFilled++;
                if (!(uxArrayNumFilled < uxArrayLength)) {
                    break;
                }
                pxCurListItem = listGET_NEXT(pxCurListItem);
            }
        }
        //Get the next task list
        pxCurTaskList = pxGetNextTaskList(pxCurTaskList);
    }

    *pxTCBSize = sizeof(TCB_t);
    return uxArrayNumFilled;
}
#endif // CONFIG_FREERTOS_ENABLE_TASK_SNAPSHOT

/* ----------------------------------------------------- OpenOCD -------------------------------------------------------
 *
 * ------------------------------------------------------------------------------------------------------------------ */

#if ( configENABLE_FREERTOS_DEBUG_OCDAWARE == 1 )

/**
 * Debug param indexes. DO NOT change the order. OpenOCD uses the same indexes
 * Entries in FreeRTOS_openocd_params must match the order of these indexes
 */
enum {
    ESP_FREERTOS_DEBUG_TABLE_SIZE = 0,
    ESP_FREERTOS_DEBUG_TABLE_VERSION,
    ESP_FREERTOS_DEBUG_KERNEL_VER_MAJOR,
    ESP_FREERTOS_DEBUG_KERNEL_VER_MINOR,
    ESP_FREERTOS_DEBUG_KERNEL_VER_BUILD,
    ESP_FREERTOS_DEBUG_UX_TOP_USED_PIORITY,
    ESP_FREERTOS_DEBUG_PX_TOP_OF_STACK,
    ESP_FREERTOS_DEBUG_PC_TASK_NAME,
    /* New entries must be inserted here */
    ESP_FREERTOS_DEBUG_TABLE_END,
};

const DRAM_ATTR uint8_t FreeRTOS_openocd_params[ESP_FREERTOS_DEBUG_TABLE_END]  = {
    ESP_FREERTOS_DEBUG_TABLE_END,       /* table size */
    1,                                  /* table version */
    tskKERNEL_VERSION_MAJOR,
    tskKERNEL_VERSION_MINOR,
    tskKERNEL_VERSION_BUILD,
    configMAX_PRIORITIES - 1,           /* uxTopUsedPriority */
    offsetof(TCB_t, pxTopOfStack),      /* thread_stack_offset; */
    offsetof(TCB_t, pcTaskName),        /* thread_name_offset; */
};

#endif // configENABLE_FREERTOS_DEBUG_OCDAWARE == 1

/* -------------------------------------------- FreeRTOS IDF API Additions ---------------------------------------------
 * FreeRTOS related API that were added by IDF
 * ------------------------------------------------------------------------------------------------------------------ */

#if CONFIG_FREERTOS_SMP
_Static_assert(tskNO_AFFINITY == CONFIG_FREERTOS_NO_AFFINITY, "CONFIG_FREERTOS_NO_AFFINITY must be the same as tskNO_AFFINITY");

BaseType_t xTaskCreatePinnedToCore( TaskFunction_t pxTaskCode,
                                    const char * const pcName,
                                    const uint32_t usStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    TaskHandle_t * const pxCreatedTask,
                                    const BaseType_t xCoreID)
{
    BaseType_t ret;
    #if ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) )
        {
            // Convert xCoreID into an affinity mask
            UBaseType_t uxCoreAffinityMask;
            if (xCoreID == tskNO_AFFINITY) {
                uxCoreAffinityMask = tskNO_AFFINITY;
            } else {
                uxCoreAffinityMask = (1 << xCoreID);
            }
            ret = xTaskCreateAffinitySet(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, uxCoreAffinityMask, pxCreatedTask);
        }
    #else /* ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) ) */
        {
            ret = xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
        }
    #endif /* ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) ) */
    return ret;
}

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
TaskHandle_t xTaskCreateStaticPinnedToCore( TaskFunction_t pxTaskCode,
                                            const char * const pcName,
                                            const uint32_t ulStackDepth,
                                            void * const pvParameters,
                                            UBaseType_t uxPriority,
                                            StackType_t * const puxStackBuffer,
                                            StaticTask_t * const pxTaskBuffer,
                                            const BaseType_t xCoreID)
{
    TaskHandle_t ret;
    #if ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) )
        {
            // Convert xCoreID into an affinity mask
            UBaseType_t uxCoreAffinityMask;
            if (xCoreID == tskNO_AFFINITY) {
                uxCoreAffinityMask = tskNO_AFFINITY;
            } else {
                uxCoreAffinityMask = (1 << xCoreID);
            }
            ret = xTaskCreateStaticAffinitySet(pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, puxStackBuffer, pxTaskBuffer, uxCoreAffinityMask);
        }
    #else /* ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) ) */
        {
            ret = xTaskCreateStatic(pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, puxStackBuffer, pxTaskBuffer);
        }
    #endif /* ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUM_CORES > 1 ) ) */
    return ret;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */

TaskHandle_t xTaskGetCurrentTaskHandleForCPU( BaseType_t xCoreID )
{
    TaskHandle_t xTaskHandleTemp;
    assert(xCoreID >= 0 && xCoreID < configNUM_CORES);
    taskENTER_CRITICAL();
    xTaskHandleTemp = (TaskHandle_t) pxCurrentTCBs[xCoreID];
    taskEXIT_CRITICAL();
    return xTaskHandleTemp;
}

TaskHandle_t xTaskGetIdleTaskHandleForCPU( BaseType_t xCoreID )
{
    assert(xCoreID >= 0 && xCoreID < configNUM_CORES);
    return (TaskHandle_t) xIdleTaskHandle[xCoreID];
}

BaseType_t xTaskGetAffinity( TaskHandle_t xTask )
{
    taskENTER_CRITICAL();
    UBaseType_t uxCoreAffinityMask;
#if ( configUSE_CORE_AFFINITY == 1 && configNUM_CORES > 1 )
    TCB_t *pxTCB = prvGetTCBFromHandle( xTask );
    uxCoreAffinityMask = pxTCB->uxCoreAffinityMask;
#else
    uxCoreAffinityMask = tskNO_AFFINITY;
#endif
    taskEXIT_CRITICAL();
    BaseType_t ret;
    // If the task is not pinned to a particular core, treat it as tskNO_AFFINITY
    if (uxCoreAffinityMask & (uxCoreAffinityMask - 1)) {    // If more than one bit set
        ret = tskNO_AFFINITY;
    } else {
        int index_plus_one = __builtin_ffs(uxCoreAffinityMask);
        assert(index_plus_one >= 1);
        ret = index_plus_one - 1;
    }
    return ret;
}

#if ( CONFIG_FREERTOS_TLSP_DELETION_CALLBACKS )
void vTaskSetThreadLocalStoragePointerAndDelCallback( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue, TlsDeleteCallbackFunction_t pvDelCallback)
    {
        // Verify that the offsets of pvThreadLocalStoragePointers and pvDummy15 match.
        // pvDummy15 is part of the StaticTask_t struct and is used to access the TLSPs
        // while deletion.
        _Static_assert(offsetof( StaticTask_t, pvDummy15 ) == offsetof( TCB_t, pvThreadLocalStoragePointers ), "Offset of pvDummy15 must match the offset of pvThreadLocalStoragePointers");

        //Set the local storage pointer first
        vTaskSetThreadLocalStoragePointer( xTaskToSet, xIndex, pvValue );

        //Set the deletion callback at an offset of configNUM_THREAD_LOCAL_STORAGE_POINTERS/2
        vTaskSetThreadLocalStoragePointer( xTaskToSet, ( xIndex + ( configNUM_THREAD_LOCAL_STORAGE_POINTERS / 2 ) ), pvDelCallback );
    }
#endif // CONFIG_FREERTOS_TLSP_DELETION_CALLBACKS

#endif // CONFIG_FREERTOS_SMP
