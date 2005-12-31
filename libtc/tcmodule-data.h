/*
 * tcmodule-common.h - transcode module system, take two: data types
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

/*
 * this header file contains basic data types declarations for transcode's
 * new module system (1.1.x and later).
 * Should not be included directly, but doing this will not harm anything.
 */
#ifndef TCMODULE_DATA_H
#define TCMODULE_DATA_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <stdlib.h>

#include "framebuffer.h"
#include "transcode.h"
#include "tcmodule-info.h"

/*
 * Data structure private for each instance.
 * This is an almost-opaque structure.
 *
 * The main purpose of this structure is to let each module (class)
 * to have it's private data, totally opaque to loader and to the
 * client code.
 * This structure also keep some accounting informations useful
 * both for module code and for loader. Those informations are
 * a module id, which identifies uniquely a given module instance
 * in a given timespan, and a string representing the module 'type',
 * a composition of it's class and specific name.
 */
typedef struct tcmoduleinstance_ TCModuleInstance;
struct tcmoduleinstance_ {
    int id; /* instance id; */
    const char *type; /* packed class + name of module */

    void *userdata; /* opaque to factory, used by each module */
};

/* can be shared between _all_ instances */
typedef struct tcmoduleclass_ TCModuleClass;
struct tcmoduleclass_ {
    int id; /* opaque internal handle */

    const TCModuleInfo *info;

    /* mandatory operations: */
    int (*init)(TCModuleInstance *self);
    int (*fini)(TCModuleInstance *self);
    int (*configure)(TCModuleInstance *self, const char *options, vob_t *vob);
    int (*stop)(TCModuleInstance *self);
    const char* (*inspect)(TCModuleInstance *self, const char *param);

    /*
     * not-mandatory operations, a module doing something useful implements
     * at least one.
     */
    int (*encode_audio)(TCModuleInstance *self,
                        aframe_list_t *inframe, aframe_list_t *outframe);
    int (*encode_video)(TCModuleInstance *self,
                        vframe_list_t *inframe, vframe_list_t *outframe);
    int (*decode_audio)(TCModuleInstance *self,
                        aframe_list_t *inframe, aframe_list_t *outframe);
    int (*decode_video)(TCModuleInstance *self,
                        vframe_list_t *inframe, vframe_list_t *outframe);
    int (*filter_audio)(TCModuleInstance *self, aframe_list_t *frame);
    int (*filter_video)(TCModuleInstance *self, vframe_list_t *frame);
    int (*multiplex)(TCModuleInstance *self,
                     vframe_list_t *vframe, aframe_list_t *aframe);
    int (*demultiplex)(TCModuleInstance *self,
                       vframe_list_t *vframe, aframe_list_t *aframe);
};

/**************************************************************************
 * TCModuleClass operations documentation:                                *
 **************************************************************************
 *
 * init:
 *      initialize a module, acquiring all needed resources.
 *      If module have options, init operation MUST set sensible defaults;
 *      an initialized, but unconfigured, module MUST be give
 *      a proper result when a specific operation (encode, demultiplex)
 *      is requested.
 * Parameters:
 *      self: pointer to module instance to initialize.
 * Return Value:
 *      0  succesfull.
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*()
 * Side effects:
 *      None.
 * Preconditions:
 *      None
 * Postconditions:
 *      Given module is ready to perform any supported operation.
 *
 *
 * fini:
 *      finalize an initialized module, releasing all acquired resources.
 *      A finalized module MUST be re-initialized before any new usage.
 * Parameters:
 *      self: pointer to module instance to finalize.
 * Return Value:
 *      0  succesfull.
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*()
 * Side effects:
 *      None.
 * Preconditions:
 *      module was already initialized. To finalize a uninitialized module
 *      will cause an undefined behaviour.
 * Postconditions:
 *      all resources acquired by given module are released.
 *
 *
 * configure:
 *      change settings for current initialized module, and return current
 *      ones.
 *      All module classes MUST support a special "help" option. If this
 *      option is given, this operation must return a textual,
 *      human-readable description of module parameters. An overview
 *      of what module can do SHOULD also be returned.
 *      After reconfiguration, a module MUST be able to perform
 *      any supported operation immediately.
 *      If reconfiguration doesn't make sense for a module, the module
 *      should ignore the novel parameter (accpeting it but without
 *      changing it's state) and should send a proper message to user
 *      via tc_log*().
 * Parameters:
 *      self: pointer to module instance to configure.
 *      options: string contaning module options
 * Return Value:
 *      0  succesfull.
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*()
 * Side effects:
 *      None.
 * Preconditions:
 *      Given module was already initialized. Try to (re)configure
 *      an unitialized module will cause an undefined behaviour.
 * Postconditions:
 *      Given module is ready to perform any supported operation.
 *
 *
 * stop:
 *      reset a module and prepare for reconfiguration or finalization.
 *      This means to flush buffers, close open files and so on,
 *      but NOT release the reseource needed by a module to work.
 *      Please note that this operation can do actions similar, but
 *      not equal, to `fini'. Also note that `stop' can be invoked
 *      zero or multiple times during the module lifetime, but
 *      `fini' WILL be invkoed one and only one time.
 * Parameters:
 *      self: pointer to module instance to stop.
 * Return Value:
 *      0  succesfull.
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*()
 * Side effects:
 *      None.
 * Preconditions:
 *      Given module was already initialized. Try to (re)stop
 *      an unitialized module will cause an undefined behaviour.
 *      Module doesn't need to be configured before to be stooped.
 * Postconditions:
 *      Given module is ready to be reconfigure safely.
 *
 *
 * filter_{audio,video}:
 *      apply an in-place transformation to a given audio/video frame.
 *      Specific module loaded determines the action performend on
 *      given frame.
 * Parameters:
 *      self: pointer to module instance to use.
 *      frame: pointer to {audio,video} frame data to elaborate.
 * Return Value:
 *      0  succesfull.
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*()
 * Side effects:
 *      None.
 * Preconditions:
 *      module was already initialized. To use a uninitialized module
 *      for filter will cause an undefined behaviour.
 * Postconditions:
 *      None
 *
 *
 *
 * multiplex:
 *      merge given encoded frames in output stream.
 * Parameters:
 *      self: pointer to module instance to use.
 *      vframe: pointer to video frame to multiplex.
 *              if NULL, don't multiplex video for this invokation.
 *      aframe: pointer to audio frame to multiplex
 *              if NULL, don't multiplex audio for this invokation.
 * Return value:
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*().
 *      >0 number of bytes writed for multiplexed frame(s). Can be
 *         (and usually is) different from the plain sum of sizes of
 *         encoded frames.
 * Side effects:
 *      None
 * Preconditions:
 *      module was already initialized. To use a uninitialized module
 *      for multiplex will cause an undefined behaviour.
 * Postconditions:
 *      None
 *
 * demultiplex:
 *      extract given encoded frames from input stream.
 * Parameters:
 *      self: pointer to module instance to use.
 *      vframe: if not NULL, extract next video frame from input stream
 *              and store it here.
 *      aframe: if not NULL, extract next audio frame from input strema
 *              and store it here.
 * Return value:
 *      -1 error occurred. A proper message should be sent to user using
 *         tc_log*().
 *      >0 number of bytes readed for demultiplexed frame(s). Can be
 *         (and usually is) different from the plain sum of sizes of
 *         encoded frames.
 * Side effects:
 *      None
 * Preconditions:
 *      module was already initialized. To use a uninitialized module
 *      for demultiplex will cause an undefined behaviour.
 * Postconditions:
 *      None
 *
 */

#endif /* TCMODULE_DATA_H */
