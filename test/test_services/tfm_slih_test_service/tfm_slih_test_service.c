/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "psa/service.h"
#include "psa_manifest/tfm_slih_test_service.h"
#include "tfm_slih_test_service_types.h"
#include "tfm_plat_test.h"
#include "tfm_sp_log.h"

/* The execution flow ensures there are no race conditions for test_type */
static int32_t test_type = TFM_SLIH_TEST_CASE_INVALID;
static uint32_t *test_case_2_signal_wait;

static void timer0_handler(void)
{
    tfm_plat_test_secure_timer_clear_intr();
    psa_eoi(TFM_TIMER0_IRQ_SIGNAL);

    tfm_plat_test_secure_timer_stop();
    psa_irq_disable(TFM_TIMER0_IRQ_SIGNAL);
}

static void slih_test_case_1(psa_msg_t *msg) {
    psa_irq_enable(TFM_TIMER0_IRQ_SIGNAL);

    tfm_plat_test_secure_timer_start();

    if (psa_wait(TFM_TIMER0_IRQ_SIGNAL, PSA_BLOCK) != TFM_TIMER0_IRQ_SIGNAL) {
        psa_panic();
    }
    timer0_handler();

    psa_reply(msg->handle, PSA_SUCCESS);
}

static void slih_test_case_2(psa_msg_t *msg) {
    if (msg->in_size[0] != sizeof(test_case_2_signal_wait)) {
        psa_reply(msg->handle, PSA_ERROR_INVALID_ARGUMENT);
    }

    psa_read(msg->handle, 0, &test_case_2_signal_wait,
             sizeof(test_case_2_signal_wait));

    psa_irq_enable(TFM_TIMER0_IRQ_SIGNAL);

    tfm_plat_test_secure_timer_start();

    psa_reply(msg->handle, PSA_SUCCESS);

    if (psa_wait(TFM_TIMER0_IRQ_SIGNAL, PSA_BLOCK) != TFM_TIMER0_IRQ_SIGNAL) {
            psa_panic();
    }
    timer0_handler();

    *test_case_2_signal_wait = 1;
}

static void slih_test_get_msg(psa_signal_t signal, psa_msg_t *msg) {
    psa_status_t status;

    status = psa_get(signal, msg);
    if (status != PSA_SUCCESS) {
        psa_panic();
    }
}

void tfm_slih_test_service_entry(void)
{
    psa_signal_t signals = 0;
    psa_msg_t msg;

    while (1) {
        signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);
        if (signals & TFM_SLIH_TEST_CASE_SIGNAL) {
            slih_test_get_msg(TFM_SLIH_TEST_CASE_SIGNAL, &msg);
            test_type = msg.type;
            switch (test_type) {
            case TFM_SLIH_TEST_CASE_1:
                slih_test_case_1(&msg);
                break;
            case TFM_SLIH_TEST_CASE_2:
                slih_test_case_2(&msg);
                break;
            default:
                LOG_ERRFMT("SLIH test service: Invalid message type: 0x%x\r\n",
                   msg.type);
                psa_panic();
                break;
            }
        }
    }
}
