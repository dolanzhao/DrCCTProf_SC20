#include <iostream>
#include <string.h>
#include <sstream>
#include <algorithm>
#include <climits>
#include <iterator>
#include <unistd.h>
#include <vector>
#include <map>

#include <sys/resource.h>
#include <sys/mman.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"

using namespace std;

#define DRCCTLIB_PRINTF(format, args...)                                            \
    do {                                                                            \
        char name[MAXIMUM_PATH] = "";                                               \
        gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));              \
        pid_t pid = getpid();                                                       \
        dr_printf("[(%s%d)drcctlib_cct_only_no_cache msg]====" format "\n", name, pid, \
                  ##args);                                                          \
    } while (0)

#define DRCCTLIB_EXIT_PROCESS(format, args...)                                       \
    do {                                                                             \
        char name[MAXIMUM_PATH] = "";                                                \
        gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));               \
        pid_t pid = getpid();                                                        \
        dr_printf("[(%s%d)drcctlib_cct_only_no_cache(%s%d) msg]====" format "\n", name, \
                  pid, ##args);                                                      \
    } while (0);                                                                     \
    dr_exit_process(-1)


static int tls_idx;
#define MINSERT instrlist_meta_preinsert

#ifdef ARM_CCTLIB
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT
#else
#    ifdef CCTLIB_64
#        define OPND_CREATE_CCT_INT OPND_CREATE_INT64
#    else
#        define OPND_CREATE_CCT_INT OPND_CREATE_INT32
#    endif
#endif

#ifdef CCTLIB_64
#    define OPND_CREATE_CTXT_HNDL_MEM OPND_CREATE_MEM64
#else
#    define OPND_CREATE_CTXT_HNDL_MEM OPND_CREATE_MEM32
#endif

typedef struct _per_thread_t{
    aligned_ctxt_hndl_t cur_ctxt_hndl;
} per_thread_t;

#define TLS_MEM_REF_BUFF_SIZE 100


// client want to do
void
DoWhatClientWantTodo(void* drcontext, context_handle_t cur_ctxt_hndl)
{
    // use {cur_ctxt_hndl}
}

// dr clean call
void 
InsertCleancall()
{
    void* drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    DoWhatClientWantTodo(drcontext, pt->cur_ctxt_hndl);
}

// insert
static void
InstrumentIns(void *drcontext, instrlist_t *bb, instr_t *instr, int32_t slot)
{
    reg_id_t reg_ctxt_hndl, reg_temp;
    if (drreg_reserve_register(drcontext, bb, instr, NULL, &reg_ctxt_hndl) != DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, bb, instr, NULL, &reg_temp) != DRREG_SUCCESS) {
        DRCCTLIB_EXIT_PROCESS("InstrumentInsCallback drreg_reserve_register != DRREG_SUCCESS");
    }
    drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_ctxt_hndl, reg_temp);
    drmgr_insert_read_tls_field(drcontext, tls_idx, bb, instr, reg_temp);

    MINSERT(bb, instr,
            XINST_CREATE_store(
                drcontext,
                OPND_CREATE_CTXT_HNDL_MEM(reg_temp, offsetof(per_thread_t, cur_ctxt_hndl)),
                opnd_create_reg(reg_ctxt_hndl)));

    if (drreg_unreserve_register(drcontext, bb, instr, reg_ctxt_hndl) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, instr, reg_temp) != DRREG_SUCCESS) {
        DRCCTLIB_EXIT_PROCESS("InstrumentInsCallback drreg_unreserve_register != DRREG_SUCCESS");
    }
}

// analysis
void
InstrumentInsCallback(void *drcontext, instr_instrument_msg_t *instrument_msg, void *data)
{
    
    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    InstrumentIns(drcontext, bb, instr, slot);
    dr_insert_clean_call(drcontext, bb, instr, (void *)InsertCleancall, false, 0);
}



static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if(pt == NULL){
        DRCCTLIB_EXIT_PROCESS("pt == NULL");
    }
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    pt->cur_ctxt_hndl = 0;
}

static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
}




static void
ClientInit(int argc, const char *argv[])
{
}

static void
ClientExit(void)
{
    drcctlib_exit();

    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        DRCCTLIB_PRINTF("ERROR: drcctlib_cct_only_no_cache failed to unregister in ClientExit");
    }
    drmgr_exit();
    if (drreg_exit() != DRREG_SUCCESS) {
        DRCCTLIB_PRINTF("failed to exit drreg");
    }
    drutil_exit();
}


#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'drcctlib_cct_only_no_cache'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if (!drmgr_init()) {
        DRCCTLIB_EXIT_PROCESS("ERROR: drcctlib_cct_only_no_cache unable to initialize drmgr");
    }
    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        DRCCTLIB_EXIT_PROCESS("ERROR: drcctlib_cct_only_no_cache unable to initialize drreg");
    }
    if (!drutil_init()) {
        DRCCTLIB_EXIT_PROCESS("ERROR: drcctlib_cct_only_no_cache unable to initialize drutil");
    }
    drmgr_register_thread_init_event(ClientThreadStart);
    drmgr_register_thread_exit_event(ClientThreadEnd);

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        DRCCTLIB_EXIT_PROCESS("ERROR: drcctlib_cct_only_no_cache drmgr_register_tls_field fail");
    }

    drcctlib_init_ex(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, NULL,
                    NULL, NULL, NULL, NULL, DRCCTLIB_DEFAULT);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif