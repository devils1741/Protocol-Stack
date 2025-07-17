#ifndef RING__H__
#define RING__H__
#include <rte_malloc.h>

struct inout_ring
{
    struct rte_ring *in;
    struct rte_ring *out;
};

class Ring
{
public:
    struct inout_ring *getRing()
    {
        if (_ring == nullptr)
        {
            _ring = static_cast<struct inout_ring *>(rte_malloc("in/out ring", sizeof(struct inout_ring), 0));
            memset(_ring, 0, sizeof(struct inout_ring));
        }
        return _ring;
    }
    static Ring &getSingleton()
    {
        static Ring instance; ///< 静态实例，确保只创建一次
        return instance;
    }

private:
    Ring() = default;  ///< 私有构造函数，禁止外部实例化
    ~Ring() = default; ///< 私有析构函数，禁止外部删除
private:
    struct inout_ring *_ring = nullptr; ///< 内部使用的环形缓冲区结构体指针
};

#endif