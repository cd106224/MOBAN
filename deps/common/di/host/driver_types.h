#pragma once

#include <cstdlib>

// clang-format off

#define devHostAllocPortable    0x01    /**< Pinned memory accessible by all CUDA contexts */

// error types
enum devError {
    /**
     * The API call returned with no errors. In the case of query calls, this
     * also means that the operation being queried is complete (see
     * ::cudaEventQuery() and ::cudaStreamQuery()).
     */
    devSuccess                           =      0,

    /**
     * The API call failed because it was unable to allocate enough memory to
     * perform the requested operation.
     */
    devErrorMemoryAllocation             =      2,
};

typedef devError    devError_t;

// other types
typedef int32_t*    devStream_t;
typedef int32_t*    devEvent_t;

struct devPointerAttribute_t {
    int device;
    void* devicePointer;
    void* hostPointer;
    int type;
};

// stream handles
#define devStreamDefault                   0x00  /**< Default stream flag */
#define devStreamNonBlocking               0x01  /**< Stream does not synchronize with stream 0 (the NULL stream) */
#define devStreamLegacy                    ((devStream_t)0x1)
#define devStreamPerThread                 ((devStream_t)0x2)

// event flags
#define devEventDefault                    0x00  /**< Default event flag */
#define devEventBlockingSync               0x01  /**< Event uses blocking synchronization */
#define devEventDisableTiming              0x02  /**< Event will not record timing data */
#define devEventInterprocess               0x04  /**< Event is suitable for interprocess use. devEventDisableTiming must be set */

// memory copy types
enum devMemcpyKind {
    devMemcpyHostToHost         = 0,    /**< Host   -> Host */
    devMemcpyHostToDevice       = 1,    /**< Host   -> Device */
    devMemcpyDeviceToHost       = 2,    /**< Device -> Host */
    devMemcpyDeviceToDevice     = 3,    /**< Device -> Device */
    devMemcpyDefault            = 4     /**< Direction of the transfer is inferred from the pointer values. Requires unified virtual addressing */
};

// memory types
enum devMemoryKind {
    devMemoryTypeHost       = 1,  /**< Memory is physically located on host. */
    devMemoryTypeDevice     = 2,  /**< Memory is physically located on device. */
    devMemoryTypeManaged    = 3,  /**< Managed memory, automaticallly managed by the unified memory system */
};

// device attributes
enum devDeviceAttr {
    devDevAttrMaxSharedMemoryPerBlock       = 8,  /**< Maximum shared memory available per block in bytes */
};

// clang-format on
