/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "engine_internal.h"
#include "workload_interface.h"

#define HEAVY_DEFAULT_BLOCK (128 * 1024)   /* 128KB */
#define HEAVY_FILE_SIZE (50 * 1024 * 1024) /* 50MB */

struct HeavyData
{
    char path[MAX_PATH_LEN];
    uint32 block_size;
};

static BOOL Setup_Heavy(const char *path, uint32 block_size, void **data)
{
    struct HeavyData *hd = IExec->AllocVecTags(sizeof(struct HeavyData), AVT_Type, MEMF_SHARED, TAG_DONE);
    if (!hd)
        return FALSE;

    snprintf(hd->path, sizeof(hd->path), "%s", path);
    hd->block_size = block_size ? block_size : HEAVY_DEFAULT_BLOCK;
    *data = hd;
    return TRUE;
}

static BOOL Run_Heavy(void *data, uint32 *bytes_processed, uint32 *op_count)
{
    struct HeavyData *hd = (struct HeavyData *)data;
    char temp_file[512];
    uint32 total_bytes = 0;

    snprintf(temp_file, sizeof(temp_file), "%sbench_heavy.tmp", hd->path);
    total_bytes = WriteDummyFile(temp_file, HEAVY_FILE_SIZE, hd->block_size);
    IDOS->Delete(temp_file);

    *bytes_processed = total_bytes;
    *op_count = (hd->block_size > 0) ? (total_bytes / hd->block_size) : 1;
    return (total_bytes > 0);
}

static void Cleanup_Heavy(void *data)
{
    if (data) {
        IExec->FreeVec(data);
    }
}

static void GetDefaultSettings_Heavy(uint32 *block_size, uint32 *passes)
{
    *block_size = HEAVY_DEFAULT_BLOCK;
    *passes = 1;
}

const BenchWorkload Workload_Legacy_Heavy = {
    .type = TEST_HEAVY_LIFTER,
    .name = "Heavy Lifter (Legacy)",
    .description = "Throughput: 50MB file with 128KB chunks",
    .detailed_info =
        "Heavy Lifter (Legacy)\n"
        "\n"
        "Measures sustained write throughput by writing a single 50 MB\n"
        "file using moderately large 128 KB chunks.\n"
        "\n"
        "  File size:      50 MB\n"
        "  Block size:     Fixed (128 KB)\n"
        "  Metric:         MB/s (megabytes per second)\n"
        "  Default passes: 1\n"
        "\n"
        "The 128 KB chunk size represents a common buffer size used by\n"
        "many real-world applications. This test shows how well the\n"
        "drive and filesystem handle sequential writes with moderate\n"
        "I/O request sizes.\n"
        "\n"
        "Good for: Comparing raw drive throughput.\n"
        "Simulates: File downloads, data extraction.\n",
    .Setup = Setup_Heavy,
    .Run = Run_Heavy,
    .Cleanup = Cleanup_Heavy,
    .GetDefaultSettings = GetDefaultSettings_Heavy};
