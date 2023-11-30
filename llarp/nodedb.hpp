#pragma once

#include "crypto/crypto.hpp"
#include "dht/key.hpp"
#include "router_contact.hpp"
#include "router_id.hpp"
#include "util/common.hpp"
#include "util/fs.hpp"
#include "util/thread/threading.hpp"

#include <llarp/router/router.hpp>

#include <algorithm>
#include <atomic>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace llarp
{
  struct Router;

  inline constexpr size_t ROUTER_ID_SOURCE_COUNT{12};
  inline constexpr size_t MIN_RID_FETCHES{8};
  inline constexpr size_t MIN_ACTIVE_RIDS{24};
  inline constexpr size_t MAX_RID_ERRORS{ROUTER_ID_SOURCE_COUNT - MIN_RID_FETCHES};
  inline constexpr int MAX_FETCH_ATTEMPTS{10};
  inline constexpr int MAX_BOOTSTRAP_FETCH_ATTEMPTS{3};
  inline constexpr size_t BOOTSTRAP_SOURCE_COUNT{50};

  inline constexpr auto FLUSH_INTERVAL{5min};

  class NodeDB
  {
    Router& _router;
    const fs::path _root;
    const std::function<void(std::function<void()>)> _disk;

    llarp_time_t _next_flush_time;

    /******** RouterID/RouterContacts ********/

    /** RouterID mappings
        Both the following are populated in NodeDB startup with RouterID's stored on disk.
        - active_client_routers: meant to persist between lokinet sessions, and is only
          populated during startup and RouterID fetching. This is meant to represent the
          client instance's perspective of the network and which RouterID's are "active"
        - known_rcs: populated during startup and when RC's are updated both during gossip
          and periodic RC fetching
    */
    std::unordered_set<RouterID> active_client_routers;
    std::unordered_map<RouterID, RemoteRC> known_rcs;

    /** RouterID lists
        - white: active routers
        - gray: fully funded, but decommissioned routers
        - green: registered, but not fully-staked routers
    */
    std::unordered_set<RouterID> router_whitelist;
    std::unordered_set<RouterID> router_greylist;
    std::unordered_set<RouterID> router_greenlist;

    // All registered relays (service nodes)
    std::unordered_set<RouterID> registered_routers;
    // timing
    std::unordered_map<RouterID, rc_time> last_rc_update_times;
    rc_time last_rc_update_relay_timestamp;
    // only ever use to specific edges as path first-hops
    std::unordered_set<RouterID> _pinned_edges;
    // source of "truth" for RC updating. This relay will also mediate requests to the
    // 12 selected active RID's for RID fetching
    RouterID fetch_source;
    // set of 12 randomly selected RID's from the client's set of routers
    std::unordered_set<RouterID> rid_sources{};
    // logs the RID's that resulted in an error during RID fetching
    std::unordered_set<RouterID> fail_sources{};
    // stores all RID fetch responses for greedy comprehensive processing
    std::unordered_map<RouterID, std::unordered_set<RouterID>> fetch_rid_responses;
    /** Failure counters:
        - fetch_failures: tracks errors fetching RC's from the RC node and requesting RID's
          from the 12 RID sources. Errors in the individual RID sets are NOT counted towards
          this, their performance as a group is evaluated wholistically
        - bootstrap_failures: tracks errors fetching both RC's from bootstrasps and RID requests
          they mediate. This is a different counter as we only bootstrap in problematic cases
    */
    std::atomic<int> fetch_failures{0}, bootstrap_failures{0};

    std::atomic<bool> is_fetching_rids{false}, is_fetching_rcs{false},
        using_bootstrap_fallback{false};

    bool
    want_rc(const RouterID& rid) const;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    remove_many_from_disk_async(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    get_path_by_pubkey(RouterID pk) const;

    std::unique_ptr<BootstrapList> _bootstraps;

   public:
    explicit NodeDB(
        fs::path rootdir, std::function<void(std::function<void()>)> diskCaller, Router* r);

    /// in memory nodedb
    NodeDB();

    bool
    process_fetched_rcs(RouterID source, std::vector<RemoteRC> rcs, rc_time timestamp);

    void
    ingest_rid_fetch_responses(const RouterID& source, std::unordered_set<RouterID> ids = {});

    bool
    process_fetched_rids();

    void
    fetch_initial();

    //  RouterContact fetching
    void
    fetch_rcs(bool initial = false);
    void
    post_fetch_rcs(bool initial = false);
    void
    fetch_rcs_result(bool initial = false, bool error = false);

    //  RouterID fetching
    void
    fetch_rids(bool initial = false);
    void
    post_fetch_rids(bool initial = false);
    void
    fetch_rids_result(bool initial = false);

    //  Bootstrap fallback
    void
    fallback_to_bootstrap();

    void
    select_router_id_sources(std::unordered_set<RouterID> excluded = {});

    // /// If we receive a bad set of RCs from our current RC source relay, we consider
    // /// that relay to be a bad source of RCs and we randomly choose a new one.
    // ///
    // /// When using a new RC fetch relay, we first re-fetch the full RC list and, if
    // /// that aligns with our RouterID list, we go back to periodic updates from that relay.
    // ///
    // /// This will respect edge-pinning and attempt to use a relay we already have
    // /// a connection with.
    // void
    // rotate_rc_source();

    // /// This function is called during startup and initial fetching. When a lokinet client
    // /// instance performs its initial RC/RID fetching, it may need to randomly select a
    // /// node from its list of stale RC's to relay its requests. If there is a failure in
    // /// mediating these request, the client will randomly select another RC source
    // ///
    // /// Returns:
    // ///   true - a new startup RC source was selected
    // ///   false - a new startup RC source was NOT selected
    // bool
    // rotate_startup_rc_source();

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& greenlist);

    std::optional<RouterID>
    get_random_whitelist_router() const;

    // client:
    //   if pinned edges were specified, connections are allowed only to those and
    //   to the configured bootstrap nodes.  otherwise, always allow.
    //
    // relay:
    //   outgoing connections are allowed only to other registered, funded relays
    //   (whitelist and greylist, respectively).
    bool
    is_connection_allowed(const RouterID& remote) const;

    // client:
    //   same as is_connection_allowed
    //
    // server:
    //   we only build new paths through registered, not decommissioned relays
    //   (i.e. whitelist)
    bool
    is_path_allowed(const RouterID& remote) const
    {
      return router_whitelist.count(remote);
    }

    // if pinned edges were specified, the remote must be in that set, else any remote
    // is allowed as first hop.
    bool
    is_first_hop_allowed(const RouterID& remote) const;

    std::unordered_set<RouterID>&
    pinned_edges()
    {
      return _pinned_edges;
    }

    std::unique_ptr<BootstrapList>&
    bootstrap_list()
    {
      return _bootstraps;
    }

    void
    set_bootstrap_routers(std::unique_ptr<BootstrapList> from_router);

    const std::unordered_set<RouterID>&
    whitelist() const
    {
      return router_whitelist;
    }

    const std::unordered_set<RouterID>&
    greylist() const
    {
      return router_greylist;
    }

    const std::unordered_set<RouterID>&
    get_registered_routers() const
    {
      return registered_routers;
    }

    const std::unordered_map<RouterID, RemoteRC>&
    get_rcs() const
    {
      return known_rcs;
    }

    const std::unordered_map<RouterID, rc_time>&
    get_last_rc_update_times() const
    {
      return last_rc_update_times;
    }

    /// load all known_rcs from disk syncrhonously
    void
    load_from_disk();

    /// explicit save all RCs to disk synchronously
    void
    save_to_disk() const;

    /// the number of RCs that are loaded from disk
    size_t
    num_loaded() const;

    /// do periodic tasks like flush to disk and expiration
    void
    Tick(llarp_time_t now);

    /// find the absolute closets router to a dht location
    RemoteRC
    find_closest_to(dht::Key_t location) const;

    /// find many routers closest to dht key
    std::vector<RemoteRC>
    find_many_closest_to(dht::Key_t location, uint32_t numRouters) const;

    /// return true if we have an rc by its ident pubkey
    bool
    has_rc(RouterID pk) const;

    /// maybe get an rc by its ident pubkey
    std::optional<RemoteRC>
    get_rc(RouterID pk) const;

    template <typename Filter>
    std::optional<RemoteRC>
    GetRandom(Filter visit) const
    {
      return _router.loop()->call_get([visit]() -> std::optional<RemoteRC> {
        std::vector<const decltype(known_rcs)::value_type*> known_rcs;
        for (const auto& entry : known_rcs)
          known_rcs.push_back(entry);

        std::shuffle(known_rcs.begin(), known_rcs.end(), llarp::csrng);

        for (const auto entry : known_rcs)
        {
          if (visit(entry->second))
            return entry->second;
        }

        return std::nullopt;
      });
    }

    /// visit all known_rcs
    template <typename Visit>
    void
    VisitAll(Visit visit) const
    {
      _router.loop()->call([this, visit]() {
        for (const auto& item : known_rcs)
          visit(item.second);
      });
    }

    /// remove an entry via its ident pubkey
    void
    remove_router(RouterID pk);

    /// remove an entry given a filter that inspects the rc
    template <typename Filter>
    void
    RemoveIf(Filter visit)
    {
      _router.loop()->call([this, visit]() {
        std::unordered_set<RouterID> removed;
        auto itr = known_rcs.begin();
        while (itr != known_rcs.end())
        {
          if (visit(itr->second))
          {
            removed.insert(itr->second.router_id());
            itr = known_rcs.erase(itr);
          }
          else
            ++itr;
        }
        if (not removed.empty())
          remove_many_from_disk_async(std::move(removed));
      });
    }

    /// remove rcs that are older than we want to keep.  For relays, this is when
    /// they  become "outdated" (i.e. 12hrs).  Clients will hang on to them until
    /// they are fully "expired" (i.e. 30 days), as the client may go offline for
    /// some time and can still try to use those RCs to re-learn the network.
    void
    remove_stale_rcs();

    /// put (or replace) the RC if we consider it valid (want_rc).  returns true if put.
    bool
    put_rc(RemoteRC rc, rc_time now = time_point_now());

    /// if we consider it valid (want_rc),
    /// put this rc into the cache if it is not there or is newer than the one there already
    /// returns true if the rc was inserted
    bool
    put_rc_if_newer(RemoteRC rc, rc_time now = time_point_now());
  };
}  // namespace llarp
