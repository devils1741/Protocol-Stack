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
    virtual ~Processor() = default;
    void handlePacket();
    void setNextProcessor(std::shared_ptr<Processor> nextProcessor);
    void handlePacket(struct rte_mbuf *mbuf);

protected:
    virtual bool canProcess(struct rte_mbuf *mbuf) = 0;
    virtual void process(struct rte_mbuf *mbuf) = 0;

private:
    std::shared_ptr<Processor> _processor;
};

#endif // PROCESSOR_HPP