/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "GpuService.h"

#include <android-base/stringprintf.h>
#include <binder/IPCThreadState.h>
#include <binder/IResultReceiver.h>
#include <binder/Parcel.h>
#include <binder/PermissionCache.h>
#include <private/android_filesystem_config.h>
#include <utils/String8.h>
#include <utils/Trace.h>

#include <vkjson.h>

#include "gpustats/GpuStats.h"

namespace android {

using base::StringAppendF;

namespace {
status_t cmdHelp(int out);
status_t cmdVkjson(int out, int err);
} // namespace

const String16 sDump("android.permission.DUMP");

const char* const GpuService::SERVICE_NAME = "gpu";

GpuService::GpuService() : mGpuStats(std::make_unique<GpuStats>()){};

void GpuService::setGpuStats(const std::string& driverPackageName,
                             const std::string& driverVersionName, uint64_t driverVersionCode,
                             int64_t driverBuildTime, const std::string& appPackageName,
                             GraphicsEnv::Driver driver, bool isDriverLoaded,
                             int64_t driverLoadingTime) {
    ATRACE_CALL();

    mGpuStats->insert(driverPackageName, driverVersionName, driverVersionCode, driverBuildTime,
                      appPackageName, driver, isDriverLoaded, driverLoadingTime);
}

status_t GpuService::shellCommand(int /*in*/, int out, int err, std::vector<String16>& args) {
    ATRACE_CALL();

    ALOGV("shellCommand");
    for (size_t i = 0, n = args.size(); i < n; i++)
        ALOGV("  arg[%zu]: '%s'", i, String8(args[i]).string());

    if (args.size() >= 1) {
        if (args[0] == String16("vkjson")) return cmdVkjson(out, err);
        if (args[0] == String16("help")) return cmdHelp(out);
    }
    // no command, or unrecognized command
    cmdHelp(err);
    return BAD_VALUE;
}

status_t GpuService::doDump(int fd, const Vector<String16>& args, bool /*asProto*/) {
    std::string result;

    IPCThreadState* ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();

    if ((uid != AID_SHELL) && !PermissionCache::checkPermission(sDump, pid, uid)) {
        StringAppendF(&result, "Permission Denial: can't dump gpu from pid=%d, uid=%d\n", pid, uid);
    } else {
        bool dumpAll = true;
        size_t index = 0;
        size_t numArgs = args.size();

        if (numArgs) {
            if ((index < numArgs) && (args[index] == String16("--gpustats"))) {
                index++;
                mGpuStats->dump(args, &result);
                dumpAll = false;
            }
        }

        if (dumpAll) {
            mGpuStats->dump(Vector<String16>(), &result);
        }
    }

    write(fd, result.c_str(), result.size());
    return NO_ERROR;
}

namespace {

status_t cmdHelp(int out) {
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        ALOGE("vkjson: failed to create out stream: %s (%d)", strerror(errno), errno);
        return BAD_VALUE;
    }
    fprintf(outs,
            "GPU Service commands:\n"
            "  vkjson   dump Vulkan properties as JSON\n");
    fclose(outs);
    return NO_ERROR;
}

void vkjsonPrint(FILE* out) {
    std::string json = VkJsonInstanceToJson(VkJsonGetInstance());
    fwrite(json.data(), 1, json.size(), out);
    fputc('\n', out);
}

status_t cmdVkjson(int out, int /*err*/) {
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        int errnum = errno;
        ALOGE("vkjson: failed to create output stream: %s", strerror(errnum));
        return -errnum;
    }
    vkjsonPrint(outs);
    fclose(outs);
    return NO_ERROR;
}

} // anonymous namespace

} // namespace android
