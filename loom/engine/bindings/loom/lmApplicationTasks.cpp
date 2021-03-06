/*
 * ===========================================================================
 * Loom SDK
 * Copyright 2011, 2012, 2013
 * The Game Engine Company, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ===========================================================================
 */

#include "loom/common/platform/platformTime.h"
#include "loom/common/platform/platformHttp.h"
#include "loom/common/platform/platformThread.h"
#include "loom/common/platform/platformNetwork.h"
#include "loom/common/core/performance.h"
#include "loom/common/assets/assets.h"
#include "loom/script/native/lsNativeDelegate.h"
#include "lmApplication.h"
#include "loom/common/config/applicationConfig.h"
#include "loom/engine/loom2d/l2dStage.h"
#include "loom/graphics/gfxTexture.h"

lmDefineLogGroup(gTickLogGroup, "tick", true, LoomLogInfo)

extern "C"
{

atomic_int_t gLoomTicking = 1;
atomic_int_t gLoomPaused = 0;

void loom_tick()
{
    if (atomic_load32(&gLoomTicking) < 1)
    {

        // Signal that the app has really stopped execution
        if (atomic_load32(&gLoomPaused) == 0)
        {
            atomic_store32(&gLoomPaused, 1);
        }

        // Sleep for longer while paused.
        // Since graphics aren't running in a paused state, there is no yielding
        // we would otherwise run in a busy loop without sleeping.
        loom_thread_sleep(30);

        return;
    }

    atomic_store32(&gLoomPaused, 0);

    LOOM_PROFILE_START(loom_tick);

    LSLuaState *vm = NULL;

    vm = LoomApplication::getReloadQueued() ? NULL : LoomApplication::getRootVM();

    // Mark the main thread for NativeDelegates. On some platforms this
    // may change so we remark every frame.
    NativeDelegate::markMainThread();
    if (vm) NativeDelegate::executeDeferredCalls(vm->VM());

    performance_tick();

    profilerBlock_t p = { "loom_tick", platform_getMilliseconds(), 17 };
    
    if (LoomApplication::getReloadQueued())
    {
        LoomApplication::reloadMainAssembly();
    }
    else
    {
        if (vm)
        {
            // https://theengineco.atlassian.net/browse/LOOM-468
            // decouple debugger enabled from connection time
            // as the debugger matures this may change a bit
            if (LoomApplicationConfig::waitForDebugger() > 0)
            {
                vm->invokeStaticMethod("system.debugger.DebuggerClient", "update");
            }

            LoomApplication::ticks.invoke();
        }
    }
    
    loom_asset_pump();
    loom_net_pump();
    
    platform_HTTPUpdate();
    
    GFX::Texture::tick();
    
    if (Loom2D::Stage::smMainStage) Loom2D::Stage::smMainStage->invokeRenderStage();
    
    finishProfilerBlock(&p);
    
    LOOM_PROFILE_END(loom_tick);

}
}

