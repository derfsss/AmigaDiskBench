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

    char device[128];
    uint32 unit = 0;

    /* 1. Reset UI immediately to clear stale data from previous drive */
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_status_label, ui.window, NULL, GA_Text,
                                      (uint32) "Querying drive health...", TAG_DONE);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_temp_label, ui.window, NULL, GA_Text,
                                      (uint32) "Temp: ...", TAG_DONE);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_power_label, ui.window, NULL, GA_Text,
                                      (uint32) "Power-on: ...", TAG_DONE);

    /* Clear the listbrowser */
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels, (uint32)-1,
                                      TAG_DONE);
    struct Node *n, *nxt;
    for (n = IExec->GetHead(&ui.health_labels); n; n = nxt) {
        nxt = IExec->GetSucc(n);
        IListBrowser->FreeListBrowserNode(n);
    }
    IExec->NewList(&ui.health_labels);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL, LISTBROWSER_Labels,
                                      (uint32)&ui.health_labels, TAG_DONE);

    /* 2. Resolve volume and query S.M.A.R.T. */
    if (GetDeviceFromVolume(volume, device, sizeof(device), &unit)) {
        LOG_DEBUG("UpdateHealthUI: Querying %s unit %u for S.M.A.R.T.", device, (unsigned int)unit);

        if (GetSmartData(device, unit, &ui.current_health)) {
            RefreshHealthTab();
        } else {
            /* Error message is now populated in health_summary by GetSmartData */
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_status_label, ui.window, NULL, GA_Text,
                                              (uint32)ui.current_health.health_summary, TAG_DONE);

            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_temp_label, ui.window, NULL, GA_Text,
                                              (uint32) "Temp: N/A", TAG_DONE);
            IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_power_label, ui.window, NULL, GA_Text,
                                              (uint32) "Power-on: N/A", TAG_DONE);
        }
    } else {
        IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_status_label, ui.window, NULL, GA_Text,
                                          (uint32) "Could not resolve volume to device.", TAG_DONE);
    }
}

void RefreshHealthTab(void)
{
    if (!ui.window)
        return;

    /* Update Status Labels */
    char temp_str[32], power_str[64];
    char status_buf[128];
    const char *status_text;

    /* Build method suffix for diagnostic display */
    const char *method_suffix = "";
    switch (ui.current_health.method) {
    case SMART_METHOD_CMD_IDE:  method_suffix = " [Direct ATA]";       break;
    case SMART_METHOD_SAT:      method_suffix = " [ATA Pass-Through]"; break;
    case SMART_METHOD_SMARTCTL: method_suffix = " [smartctl]";         break;
    default: break;
    }

    switch (ui.current_health.overall_status) {
    case SMART_STATUS_OK:
        snprintf(status_buf, sizeof(status_buf), "DRIVE HEALTH: OK%s", method_suffix);
        break;
    case SMART_STATUS_WARNING:
        snprintf(status_buf, sizeof(status_buf), "DRIVE HEALTH: WARNING (Attention Required)%s", method_suffix);
        break;
    case SMART_STATUS_CRITICAL:
        snprintf(status_buf, sizeof(status_buf), "DRIVE HEALTH: CRITICAL (Imminent Failure!)%s", method_suffix);
        break;
    default:
        snprintf(status_buf, sizeof(status_buf), "DRIVE HEALTH: UNKNOWN");
        break;
    }
    status_text = status_buf;

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
            7, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)id_str, LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)attr->name,
            LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)val_str, LBNA_Column, 3, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)worst_str, LBNA_Column, 4, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)thresh_str,
            LBNA_Column, 5, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)raw_str, LBNA_Column, 6, LBNCA_CopyText, TRUE, LBNCA_Text, (uint32)attr_status, TAG_DONE);

        if (node)
            IExec->AddTail(&ui.health_labels, node);
    }

    /* Reattach labels — detach first, then reattach with AutoFit so the
     * weighted column recalculates now that rows are present. */
    IIntuition->SetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL,
                               LISTBROWSER_Labels, (uint32)-1, TAG_DONE);
    IIntuition->RefreshSetGadgetAttrs((struct Gadget *)ui.health_list, ui.window, NULL,
                                      LISTBROWSER_Labels, (uint32)&ui.health_labels,
                                      LISTBROWSER_AutoFit, TRUE, TAG_DONE);
}
