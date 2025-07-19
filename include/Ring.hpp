#ifndef RING__H__
#define RING__H__
#include <rte_malloc.h>

struct inout_ring
{
    struct rte_ring *in = nullptr;  ///< 输入环形缓冲区指针
    struct rte_ring *out = nullptr; ///< 输出环形缓冲区指针
};

class Ring
{
public:
    static Ring &getSingleton()
    {
        static Ring instance;
        return instance;
    }
    
    int setRingSize(size_t size)
    {
        if (size == 0 || (size & (size - 1)) != 0)
        {
            SPDLOG_ERROR("Ring size must be a power of 2: {}", size);
            return -1;
        }
        _RING_SIZE = size;
        reSetRing();
        return 0;
    }

    struct inout_ring *getRing()
    {
        if (_ring == nullptr)
        {
            _ring = static_cast<struct inout_ring *>(rte_malloc("in/out ring", sizeof(struct inout_ring), 0));
            if (_ring == nullptr)
            {
                SPDLOG_ERROR("Failed to allocate memory for ring buffer");
                rte_exit(EXIT_FAILURE, "ring buffer init failed\n");
            }
            memset(_ring, 0, sizeof(struct inout_ring));
            _ring->in = rte_ring_create("in ring", _RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
            _ring->out = rte_ring_create("out ring", _RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

            if (!_ring->in || !_ring->out)
            {
                rte_free(_ring);
                _ring = nullptr;
                SPDLOG_ERROR("Failed to allocate memory for ring in/out");
                rte_exit(EXIT_FAILURE, "ring in/out create failed\n");
            }
        }
        return _ring;
    }

private:
    Ring() = default;
    ~Ring()
    {
        reSetRing();
    }
    int reSetRing()
    {
        if (_ring)
        {
            rte_ring_free(_ring->in);
            rte_ring_free(_ring->out);
            rte_free(_ring);
            _ring = nullptr;
        }
        return 0; // 成功
    }

private:
    struct inout_ring *_ring = nullptr; ///< 内部使用的环形缓冲区结构体指针
    size_t _RING_SIZE = 1024;           ///< 默认环形缓冲区大小
};

#endif