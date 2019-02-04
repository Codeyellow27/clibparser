//
// Project: CMiniLang
// Author: bajdcc
//

#ifndef CMINILANG_VM_H
#define CMINILANG_VM_H

#include <memory>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <deque>
#include "types.h"
#include "memory.h"
#include "cmem.h"
#include "cvfs.h"

namespace clib {

// Website: https://github.com/bajdcc/MiniOS

/* virtual memory management */
// 虚存分配（二级页表分配方式）

// 参考：http://wiki.osdev.org/Paging

// 对于一个32位虚拟地址（virtual address）
// 32-22: 页目录号 | 21-12: 页表号 | 11-0: 页内偏移
// http://www.360doc.com/content/11/0804/10/7204565_137844381.shtml

/* 4k per page */
#define PAGE_SIZE 4096

/* 页掩码，取高20位 */
#define PAGE_MASK 0xfffff000

/* 地址对齐 */
#define PAGE_ALIGN_DOWN(x) ((x) & PAGE_MASK)
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)

/* 分析地址 */
#define PDE_INDEX(x) (((x) >> 22) & 0x3ff)  // 获得地址对应的页目录号
#define PTE_INDEX(x) (((x) >> 12) & 0x3ff)  // 获得页表号
#define OFFSET_INDEX(x) ((x) & 0xfff)       // 获得页内偏移

// 页目录项、页表项用uint32表示即可
    typedef uint32_t pde_t;
    typedef uint32_t pte_t;

/* 页目录大小 1024 */
#define PDE_SIZE (PAGE_SIZE/sizeof(pte_t))
/* 页表大小 1024 */
#define PTE_SIZE (PAGE_SIZE/sizeof(pde_t))
/* 页表总数 1024*PTE_SIZE*PAGE_SIZE = 4G */
#define PTE_COUNT 1024

/* CPU */
#define CR0_PG  0x80000000

/* pde&pdt attribute */
#define PTE_P   0x1     // 有效位 Present
#define PTE_R   0x2     // 读写位 Read/Write, can be read&write when set
#define PTE_U   0x4     // 用户位 User / Kern
#define PTE_K   0x0     // 内核位 User / Kern
#define PTE_W   0x8     // 写回 Write through
#define PTE_D   0x10    // 不缓存 Cache disable
#define PTE_A   0x20    // 可访问 Accessed
#define PTE_S   0x40    // Page size, 0 for 4kb pre page
#define PTE_G   0x80    // Ignored

/* 用户代码段基址 */
#define USER_BASE 0xc0000000
/* 用户数据段基址 */
#define DATA_BASE 0xd0000000
/* 用户栈基址 */
#define STACK_BASE 0xe0000000
/* 用户堆基址 */
#define HEAP_BASE 0xf0000000
/* 段掩码 */
#define SEGMENT_MASK 0x0fffffff

/* 物理内存(单位：16B)，越多越好！ */
#define PHY_MEM (256 * 1024)

#define PE_MAGIC "ccos"

#define U2K(addr) ((uint) ((addr) << 20) & 0x0ff00000)
#define K2U(addr) ((uint) ((addr) & 0x000fffff))

#define TASK_NUM 256
#define HANDLE_NUM 1024

    class cvm : public imem, public vfs_func_t, public vfs_stream_call {
    public:
        cvm();
        ~cvm();

        cvm(const cvm &) = delete;
        cvm &operator=(const cvm &) = delete;

        int load(const string_t &path, const std::vector<byte> &file, const std::vector<string_t> &args);
        bool run(int cycle, int &cycles);

        void map_page(uint32_t addr, uint32_t id) override;
        void as_root(bool flag);
        bool read_vfs(const string_t &path, std::vector<byte> &data) const;
        bool write_vfs(const string_t &path, const std::vector<byte> &data);

        vfs_stream_t stream_type(const string_t &path) const override;
        string_t stream_callback(const string_t &path) override;
        vfs_node_dec *stream_create(const vfs_mod_query *mod, vfs_stream_t type) override;
        int stream_index(vfs_stream_t type) override;

    private:
        // 申请页框
        uint32_t pmm_alloc(bool reusable = true);
        // 初始化页表
        void vmm_init();
        // 虚页映射
        void vmm_map(uint32_t va, uint32_t pa, uint32_t flags);
        // 解除映射
        void vmm_unmap(uint32_t va);
        // 查询分页情况
        int vmm_ismap(uint32_t va, uint32_t *pa) const;

        template<class T = int>
        T vmm_get(uint32_t va);
        string_t vmm_getstr(uint32_t va);
        template<class T = int>
        T vmm_set(uint32_t va, T);
        void vmm_setstr(uint32_t va, const string_t &str);
        uint32_t vmm_malloc(uint32_t size);
        uint32_t vmm_free(uint32_t addr);
        uint32_t vmm_memset(uint32_t va, uint32_t value, uint32_t count);
        uint32_t vmm_memcmp(uint32_t src, uint32_t dst, uint32_t count);
        template<class T = int>
        void vmm_pushstack(uint32_t &sp, T value);
        template<class T = int>
        T vmm_popstack(uint32_t &sp);

        void error(const string_t &);
        void exec(int cycle, int &cycles);
        void destroy(int id);
        int exec_file(const string_t &path);
        int fork();

        bool interrupt();

        void init_fs();

        enum handle_type {
            h_none,
            h_file,
        };

        int new_pid();
        int new_handle(handle_type);
        int destroy_handle(int handle);

    private:
        /* 内核页表 = PTE_SIZE*PAGE_SIZE */
        pde_t *pgd_kern;
        /* 内核页表内容 = PTE_COUNT*PTE_SIZE*PAGE_SIZE */
        pde_t *pte_kern;
        /* 物理内存(1 block=16B) */
        memory_pool<PHY_MEM> memory;
        /* 页表 */
        pde_t *pgdir{nullptr};
        int pids{0};

        enum ctx_flag_t {
            CTX_VALID = 1 << 0,
            CTX_KERNEL = 1 << 1,
            CTX_USER_MODE = 1 << 2,
            CTX_FOREGROUND = 1 << 3,
        };

        enum ctx_state_t {
            CTS_RUNNING,
            CTS_WAIT,
            CTS_ZOMBIE,
            CTS_DEAD,
        };

        struct context_t {
            uint flag;
            int id;
            int parent;
            std::unordered_set<int> child;
            ctx_state_t state;
            string_t path;
            uint mask;
            uint entry;
            uint poolsize;
            uint stack;
            uint data;
            uint base;
            uint heap;
            uint pc;
            int ax;
            int bx;
            uint bp;
            uint sp;
            bool debug;
            std::vector<byte> file;
            std::vector<uint32_t> allocation;
            std::vector<uint32_t> data_mem;
            std::vector<uint32_t> text_mem;
            std::vector<uint32_t> stack_mem;
            std::unique_ptr<cmem> pool;
            // SYSTEM CALL
            std::chrono::system_clock::time_point record_now;
            decimal waiting_ms;
            int input_redirect;
            int output_redirect;
            bool input_stop;
            std::deque<char> input_queue;
            std::unordered_set<int> handles;
        };
        context_t *ctx{nullptr};
        int available_tasks{0};
        std::array<context_t, TASK_NUM> tasks;
        cvfs fs;

        struct handle_t {
            handle_type type;
            string_t name;
            union {
                vfs_node_dec *file;
            } data;
        };
        int handle_ids{0};
        int available_handles{0};
        int set_cycle_id{-1};
        int set_resize_id{-1};
        std::array<handle_t, HANDLE_NUM> handles;

    public:
        static struct global_state_t {
            bool interrupt{false};
            int input_lock{-1};
            std::vector<int> input_waiting_list;
            std::string input_content;
            bool input_success{false};
            int input_read_ptr{-1};
            string_t hostname{"ccos"};
        } global_state;
    };
}

#endif //CMINILANG_VM_H
