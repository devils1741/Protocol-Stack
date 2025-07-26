#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP
#include <rte_mbuf.h>
#include <memory>
struct packet
{
    struct rte_mbuf *mbuf;
};

class Processor
{
public:
    virtual int setNextProcessor(std::shared_ptr<Processor> nextProcessor) = 0;

private:
    std::shared_ptr<Processor> _nextProcessor;
};

#endif // PROCESSOR_HPP