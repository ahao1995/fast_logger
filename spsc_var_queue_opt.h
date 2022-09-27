#pragma once
#include <stdint.h>

template <uint32_t Bytes>
class SPSCVarQueueOPT
{
  public:
    struct MsgHeader
    {
        // size of this msg, including header itself
        // auto set by lib, can be read by user
        uint16_t size;
        uint16_t msg_type;
        // userdata can be used by caller, e.g. save timestamp or other stuff
        // we assume that user_msg is 8 types alligned so there'll be 4 bytes padding anyway, otherwise we can choose to
        // eliminate userdata
        uint32_t userdata;
    };
    static constexpr uint32_t BLK_CNT = Bytes / sizeof(MsgHeader);

    MsgHeader *alloc(uint16_t size_)
    {
        size            = size_ + sizeof(MsgHeader);
        uint32_t blk_sz = (size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        if (blk_sz >= free_write_cnt) {
            asm volatile("" : "=m"(read_idx) : :); // force read memory
            uint32_t read_idx_cache = read_idx;
            if (read_idx_cache <= write_idx) {
                free_write_cnt = BLK_CNT - write_idx;
                if (blk_sz >= free_write_cnt && read_idx_cache != 0) { // wrap around
                    blk[0].size = 0;
                    asm volatile("" : : "m"(blk) :); // memory fence
                    blk[write_idx].size = 1;
                    write_idx           = 0;
                    free_write_cnt      = read_idx_cache;
                }
            } else {
                free_write_cnt = read_idx_cache - write_idx;
            }
            if (free_write_cnt <= blk_sz) {
                return nullptr;
            }
        }
        return &blk[write_idx];
    }

    void push()
    {
        uint32_t blk_sz              = (size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        blk[write_idx + blk_sz].size = 0;

        asm volatile("" : : "m"(blk) :); // memory fence
        blk[write_idx].size = size;
        write_idx += blk_sz;
        free_write_cnt -= blk_sz;
    }

    template <typename Writer>
    bool tryPush(uint16_t size, Writer writer)
    {
        MsgHeader *header = alloc(size);
        if (!header)
            return false;
        writer(header);
        push();
        return true;
    }

    MsgHeader *front()
    {
        asm volatile("" : "=m"(blk) : :); // force read memory
        uint16_t size = blk[read_idx].size;
        if (size == 1) { // wrap around
            read_idx = 0;
            size     = blk[0].size;
        }
        if (size == 0)
            return nullptr;
        return &blk[read_idx];
    }

    void pop()
    {
        asm volatile("" : "=m"(blk) : "m"(read_idx) :); // memory fence
        uint32_t blk_sz = (blk[read_idx].size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        read_idx += blk_sz;
        asm volatile("" : : "m"(read_idx) :); // force write memory
    }

    template <typename Reader>
    bool tryPop(Reader reader)
    {
        MsgHeader *header = front();
        if (!header)
            return false;
        reader(header);
        pop();
        return true;
    }

  private:
    alignas(64) MsgHeader blk[BLK_CNT] = {};

    alignas(128) uint32_t write_idx = 0;
    uint32_t free_write_cnt         = BLK_CNT;
    uint16_t size;

    alignas(128) uint32_t read_idx = 0;
};