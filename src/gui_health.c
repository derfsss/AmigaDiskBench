/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * Drive Health UI Implementation
 */

#include "engine_internal.h"
#include "gui_internal.h"
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>
#include <stdio.h>

void UpdateHealthUI(const char *volume)
{
    if (!volume || !ui.window)
        return;

    char device[64];
    uint32 unit;

    /* Resolve volume to physical device */
    if (GetDeviceFromVolume(volume, device, sizeof(device), &unit)) {
        LOG_DEBUG("UpdateHealthUI: Querying %s unit %u for S.M.A.R.T.", device, (unsigned int)unit);

        /* For now, we fetch synchronously to keep implementation simple,
           but usually S.M.A.R.T. queries are fast.
           In a future update, this can be moved to the worker thread. */
        if (GetSmartData(device, unit, &ui.current_health)) {
            RefreshHealthTab();
        } else {
            const char *err_msg = "S.M.A.R.T. Not Supported or Query Failed";
            if (!ui.current_health.driver_supported) {
                err_msg = "Driver (a1ide.device?) does not support S.M.A.R.T. PT";
            }

            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_status_label, ui.window, NULL, GA_Text,
                                              (uint32)err_msg, TAG_DONE);
            /* Clear list and other labels */
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels,
                                              (uint32)-1, TAG_DONE);
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_temp_label, ui.window, NULL, GA_Text,
                                              (uint32) "Temp: N/A", TAG_DONE);
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_power_label, ui.window, NULL, GA_Text,
                                              (uint32) "Power-on: N/A", TAG_DONE);

            struct Node *n, *nxt;
            for (n = IExec->GetHead(&ui.health_labels); n; n = nxt) {
                nxt = IExec->GetSucc(n);
                IListBrowser->FreeListBrowserNode(n);
            }
            IExec->NewList(&ui.health_labels);
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels,
                                              (uint32)&ui.health_labels, TAG_DONE);
        }
    }
}

void RefreshHealthTab(void)
{
    if (!ui.window)
        return;

    /* Update Status Labels */
    char temp_str[32], power_str[64];
    const char *status_text = "Unknown";

    switch (ui.current_health.overall_status) {
    case SMART_STATUS_OK:
        status_text = "DRIVE HEALTH: OK";
        break;
    case SMART_STATUS_WARNING:
        status_text = "DRIVE HEALTH: WARNING (Attention Required)";
        break;
    case SMART_STATUS_CRITICAL:
        status_text = "DRIVE HEALTH: CRITICAL (Imminent Failure!)";
        break;
    default:
        status_text = "DRIVE HEALTH: UNKNOWN";
        break;
    }

    if (ui.current_health.supported) {
        snprintf(temp_str, sizeof(temp_str), "Temp: %u C", (unsigned int)ui.current_health.temperature);
        snprintf(power_str, sizeof(power_str), "Power-on: %u Hours", (unsigned int)ui.current_health.power_on_hours);
    } else {
        snprintf(temp_str, sizeof(temp_str), "Temp: N/A");
        snprintf(power_str, sizeof(power_str), "Power-on: N/A");
    }

    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_status_label, ui.window, NULL, GA_Text,
                                      (uint32)status_text, TAG_DONE);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_temp_label, ui.window, NULL, GA_Text, (uint32)temp_str,
                                      TAG_DONE);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_power_label, ui.window, NULL, GA_Text,
                                      (uint32)power_str, TAG_DONE);

    /* Update ListBrowser */
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels, (uint32)-1,
                                      TAG_DONE);

    /* Free existing labels */
    struct Node *n, *nxt;
    for (n = IExec->GetHead(&ui.health_labels); n; n = nxt) {
        nxt = IExec->GetSucc(n);
        IListBrowser->FreeListBrowserNode(n);
    }
    IExec->NewList(&ui.health_labels);

    /* Populate with new data */
    for (uint32 i = 0; i < ui.current_health.attribute_count; i++) {
        SmartAttribute *attr = &ui.current_health.attributes[i];
        char id_str[8], val_str[16], worst_str[16], thresh_str[16], raw_str[32];
        const char *attr_status = "OK";

        snprintf(id_str, sizeof(id_str), "%u", attr->id);
        snprintf(val_str, sizeof(val_str), "%u", attr->value);
        snprintf(worst_str, sizeof(worst_str), "%u", attr->worst);
        snprintf(thresh_str, sizeof(thresh_str), "%u", attr->threshold);
        snprintf(raw_str, sizeof(raw_str), "0x%llX", attr->raw_value);

        if (attr->status == SMART_STATUS_WARNING)
            attr_status = "Warning";
        if (attr->status == SMART_STATUS_CRITICAL)
            attr_status = "Critical";

        struct Node *node = IListBrowser->AllocListBrowserNode(
            7, LBNA_Column, 0, LBNCA_Text, id_str, LBNA_Column, 1, LBNCA_Text, attr->name, LBNA_Column, 2, LBNCA_Text,
            val_str, LBNA_Column, 3, LBNCA_Text, worst_str, LBNA_Column, 4, LBNCA_Text, thresh_str, LBNA_Column, 5,
            LBNCA_Text, raw_str, LBNA_Column, 6, LBNCA_Text, attr_status, TAG_DONE);

        if (node)
            IExec->AddTail(&ui.health_labels, node);
    }

    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels,
                                      (uint32)&ui.health_labels, TAG_DONE);
}
