// cuda_driver_example.cu
// 演示CUDA Driver API的核心用法：初始化、模块加载、内核启动

#include <cuda.h>
#include <iostream>
#include <vector>
#include <cstring>

// CUDA内核的PTX代码（实际项目中通常从文件加载）
// 这是简单的向量加法内核的PTX
const char* ptxCode = R"(
.version 7.0
.target sm_50
.address_size 64

.visible .entry vecAdd(
    .param .u64 vecAdd_param_0,  // float* c
    .param .u64 vecAdd_param_1,  // const float* a
    .param .u64 vecAdd_param_2,  // const float* b
    .param .u32 vecAdd_param_3   // int n
)
{
    .reg .pred      %p<2>;
    .reg .f32       %f<4>;
    .reg .b32       %r<6>;
    .reg .b64       %rd<11>;

    ld.param.u64    %rd1, [vecAdd_param_0];
    ld.param.u64    %rd2, [vecAdd_param_1];
    ld.param.u64    %rd3, [vecAdd_param_2];
    ld.param.u32    %r2, [vecAdd_param_3];

    mov.u32         %r3, %ctaid.x;
    mov.u32         %r4, %ntid.x;
    mov.u32         %r5, %tid.x;
    mad.lo.s32      %r1, %r4, %r3, %r5;

    setp.ge.s32     %p1, %r1, %r2;
    @%p1 bra        BB0_2;

    cvta.to.global.u64      %rd4, %rd2;
    mul.lo.s64      %rd5, %rd1, 4;
    add.s64         %rd6, %rd4, %rd5;
    ld.global.f32   %f1, [%rd6];
    cvta.to.global.u64      %rd7, %rd3;
    add.s64         %rd8, %rd7, %rd5;
    ld.global.f32   %f2, [%rd8];
    add.f32         %f3, %f1, %f2;
    cvta.to.global.u64      %rd9, %rd1;
    add.s64         %rd10, %rd9, %rd5;
    st.global.f32   [%rd10], %f3;

BB0_2:
    ret;
}
)";

// 错误检查宏
#define CHECK_CUDA_DRIVER(call) \
    do { \
        CUresult result = call; \
        if (result != CUDA_SUCCESS) { \
            const char* errStr; \
            cuGetErrorString(result, &errStr); \
            std::cerr << "CUDA Driver Error at " << __FILE__ << ":" << __LINE__ \
                      << " - " << errStr << " (" << result << ")" << std::endl; \
            exit(1); \
        } \
    } while(0)

int main() {
    // ========================================
    // 1. 初始化CUDA驱动
    // ========================================
    std::cout << "=== 初始化CUDA驱动 ===" << std::endl;
    
    // 必须首先调用cuInit(0)
    CHECK_CUDA_DRIVER(cuInit(0));

    // ========================================
    // 2. 获取并选择设备
    // ========================================
    std::cout << "\n=== 设备枚举 ===" << std::endl;
    
    int deviceCount = 0;
    CHECK_CUDA_DRIVER(cuDeviceGetCount(&deviceCount));
    std::cout << "发现 " << deviceCount << " 个CUDA设备" << std::endl;

    // 获取第一个设备
    CUdevice device;
    CHECK_CUDA_DRIVER(cuDeviceGet(&device, 0));

    // 获取设备属性
    char deviceName[256];
    CHECK_CUDA_DRIVER(cuDeviceGetName(deviceName, sizeof(deviceName), device));
    
    int major = 0, minor = 0;
    CHECK_CUDA_DRIVER(cuDeviceComputeCapability(&major, &minor, device));
    
    size_t totalMem = 0;
    CHECK_CUDA_DRIVER(cuDeviceTotalMem(&totalMem, device));

    std::cout << "设备 0: " << deviceName << std::endl;
    std::cout << "  计算能力: " << major << "." << minor << std::endl;
    std::cout << "  总显存: " << (totalMem / 1024 / 1024) << " MB" << std::endl;

    // ========================================
    // 3. 创建上下文 (Context)
    // ========================================
    std::cout << "\n=== 创建CUDA上下文 ===" << std::endl;
    
    CUcontext context;
    // CU_CTX_SCHED_AUTO: 自动选择线程调度策略
    // CU_CTX_MAP_HOST: 支持主机内存映射
    unsigned int flags = CU_CTX_SCHED_AUTO;
    CHECK_CUDA_DRIVER(cuCtxCreate(&context, flags, device));
    
    std::cout << "上下文创建成功" << std::endl;

    // ========================================
    // 4. 加载模块（PTX或cubin）
    // ========================================
    std::cout << "\n=== 加载CUDA模块 ===" << std::endl;
    
    CUmodule module;
    
    // 方法1: 直接从字符串加载PTX
    CHECK_CUDA_DRIVER(cuModuleLoadData(&module, ptxCode));
    
    // 方法2: 从文件加载（更常用）
    // CHECK_CUDA_DRIVER(cuModuleLoad(&module, "kernel.ptx"));
    // CHECK_CUDA_DRIVER(cuModuleLoad(&module, "kernel.cubin"));
    
    std::cout << "模块加载成功" << std::endl;

    // 获取内核函数
    CUfunction kernel;
    CHECK_CUDA_DRIVER(cuModuleGetFunction(&kernel, module, "vecAdd"));
    std::cout << "获取内核函数 'vecAdd' 成功" << std::endl;

    // ========================================
    // 5. 准备数据并分配设备内存
    // ========================================
    std::cout << "\n=== 内存管理 ===" << std::endl;
    
    const int N = 1024 * 1024;  // 1M元素
    const size_t size = N * sizeof(float);
    
    // 主机数据
    std::vector<float> h_a(N, 1.0f);
    std::vector<float> h_b(N, 2.0f);
    std::vector<float> h_c(N, 0.0f);

    // 分配设备内存
    CUdeviceptr d_a, d_b, d_c;
    CHECK_CUDA_DRIVER(cuMemAlloc(&d_a, size));
    CHECK_CUDA_DRIVER(cuMemAlloc(&d_b, size));
    CHECK_CUDA_DRIVER(cuMemAlloc(&d_c, size));
    
    std::cout << "分配设备内存: " << (size * 3 / 1024 / 1024) << " MB" << std::endl;

    // 创建流（异步操作）
    CUstream stream;
    CHECK_CUDA_DRIVER(cuStreamCreate(&stream, CU_STREAM_DEFAULT));

    // 主机到设备拷贝（异步）
    CHECK_CUDA_DRIVER(cuMemcpyHtoDAsync(d_a, h_a.data(), size, stream));
    CHECK_CUDA_DRIVER(cuMemcpyHtoDAsync(d_b, h_b.data(), size, stream));
    
    std::cout << "数据拷贝到设备完成" << std::endl;

    // ========================================
    // 6. 配置并启动内核
    // ========================================
    std::cout << "\n=== 启动内核 ===" << std::endl;
    
    // 设置内核参数
    // Driver API使用数组传递参数，注意必须是void*数组
    void* kernelParams[] = {
        &d_c,    // float* c
        &d_a,    // const float* a
        &d_b,    // const float* b
        &N       // int n
    };

    // 启动配置
    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;
    
    std::cout << "网格配置: " << blocksPerGrid << " blocks, " 
              << threadsPerBlock << " threads/block" << std::endl;

    // 启动内核（异步）
    CHECK_CUDA_DRIVER(cuLaunchKernel(
        kernel,                          // 内核函数
        blocksPerGrid, 1, 1,            // 网格维度 (x, y, z)
        threadsPerBlock, 1, 1,          // 块维度 (x, y, z)
        0,                              // 共享内存大小（字节）
        stream,                         // CUDA流
        kernelParams,                   // 内核参数
        nullptr                         // 额外参数（通常为nullptr）
    ));

    std::cout << "内核启动成功" << std::endl;

    // ========================================
    // 7. 同步与结果获取
    // ========================================
    
    // 等待流完成
    CHECK_CUDA_DRIVER(cuStreamSynchronize(stream));
    
    // 设备到主机拷贝
    CHECK_CUDA_DRIVER(cuMemcpyDtoH(h_c.data(), d_c, size));
    
    std::cout << "结果拷贝回主机" << std::endl;

    // 验证结果
    bool success = true;
    for (int i = 0; i < 10; i++) {
        std::cout << "c[" << i << "] = " << h_c[i] << " (expected 3.0)" << std::endl;
        if (std::abs(h_c[i] - 3.0f) > 1e-5) success = false;
    }
    std::cout << (success ? "✓ 验证通过" : "✗ 验证失败") << std::endl;

    // ========================================
    // 8. 清理资源
    // ========================================
    std::cout << "\n=== 清理资源 ===" << std::endl;
    
    CHECK_CUDA_DRIVER(cuStreamDestroy(stream));
    CHECK_CUDA_DRIVER(cuMemFree(d_a));
    CHECK_CUDA_DRIVER(cuMemFree(d_b));
    CHECK_CUDA_DRIVER(cuMemFree(d_c));
    CHECK_CUDA_DRIVER(cuModuleUnload(module));
    CHECK_CUDA_DRIVER(cuCtxDestroy(context));
    
    std::cout << "资源清理完成" << std::endl;

    return 0;
}