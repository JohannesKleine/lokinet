#include <llarp/dns/server.hpp>

namespace llarp
{
  namespace dns
  {
    Proxy::Proxy(llarp_ev_loop* loop, IQueryHandler* h)
        : m_Loop(loop), m_QueryHandler(h)
    {
      m_UDP.user     = this;
      m_UDP.tick     = &HandleTick;
      m_UDP.recvfrom = &HandleUDPRecv;
    }

    void
    Proxy::Stop()
    {
    }

    bool
    Proxy::Start(const llarp::Addr& addr,
                 const std::vector< llarp::Addr >& resolvers)
    {
      m_Resolvers.clear();
      m_Resolvers = resolvers;
      if(m_Resolvers.size() == 0)
      {
        llarp::LogError("no upstream dns provide specified");
        return false;
      }
      return llarp_ev_add_udp(m_Loop, &m_UDP, addr) == 0;
    }

    void
    Proxy::HandleUDPRecv(llarp_udp_io* u, const sockaddr* from,
                         llarp_buffer_t buf)
    {
      static_cast< Proxy* >(u->user)->HandlePkt(*from, &buf);
    }

    llarp::Addr
    Proxy::PickRandomResolver() const
    {
      size_t sz = m_Resolvers.size();
      if(sz == 0)
        return llarp::Addr("1.1.1.1", 52);
      if(sz == 1)
        return m_Resolvers[0];
      auto itr = m_Resolvers.begin();
      std::advance(itr, llarp_randint() % sz);
      return *itr;
    }

    void
    Proxy::HandleTick(llarp_udp_io*)
    {
    }

    void
    Proxy::SendMessageTo(llarp::Addr to, Message msg)
    {
      byte_t tmp[1500] = {0};
      auto buf         = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(msg.Encode(&buf))
      {
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        llarp_ev_udp_sendto(&m_UDP, to, buf);
      }
      else
        llarp::LogWarn("failed to encode dns message when sending");
    }

    void
    Proxy::HandlePkt(llarp::Addr from, llarp_buffer_t* pkt)
    {
      MessageHeader hdr;
      if(!hdr.Decode(pkt))
      {
        llarp::LogWarn("failed to parse dns header from ", from);
        return;
      }
      TX tx    = {hdr.id, from};
      auto itr = m_Forwarded.find(tx);
      if(itr != m_Forwarded.end())
      {
        llarp_buffer_t buf;
        buf.sz   = pkt->sz;
        buf.base = pkt->base;
        buf.cur  = buf.base;
        // forward
        llarp_ev_udp_sendto(&m_UDP, itr->second, buf);
        // remove pending
        m_Forwarded.erase(itr);
        return;
      }

      Message msg(hdr);
      if(!msg.Decode(pkt))
      {
        llarp::LogWarn("failed to parse dns message from ", from);
        return;
      }

      if(m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
      {
        if(!m_QueryHandler->HandleHookedDNSMessage(
               std::move(msg),
               std::bind(&Proxy::SendMessageTo, this, from,
                         std::placeholders::_1)))
        {
          llarp::LogWarn("failed to handle hooked dns");
        }
        return;
      }
      else if(itr == m_Forwarded.end())
      {
        // new forwarded query
        tx.from         = PickRandomResolver();
        m_Forwarded[tx] = from;
        llarp_buffer_t buf;
        buf.sz   = pkt->sz;
        buf.base = pkt->base;
        buf.cur  = buf.base;
        // do query
        llarp_ev_udp_sendto(&m_UDP, tx.from, buf);
      }
      else
      {
        // drop (?)
      }
    }

  }  // namespace dns
}  // namespace llarp
