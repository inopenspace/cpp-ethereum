/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file BlockChainSync.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <mutex>
#include <unordered_map>

#include <libdevcore/Guards.h>
#include <libethcore/Common.h>
#include <libp2p/Common.h>
#include "CommonNet.h"
#include "DownloadMan.h"

namespace dev
{

class RLPStream;

namespace eth
{

class EthereumHost;
class BlockQueue;
class EthereumPeer;

/**
 * @brief Base BlockChain synchronization strategy class.
 * Syncs to peers and keeps up to date. Base class handles blocks downloading but does not contain any details on state transfer logic.
 */
class BlockChainSync: public HasInvariants
{
public:
	BlockChainSync(EthereumHost& _host);
	~BlockChainSync();
	void abortSync(); ///< Abort all sync activity

	/// @returns true is Sync is in progress
	bool isSyncing() const;

	/// Restart sync
	void restartSync();

	/// Called by peer to report status
	void onPeerStatus(std::shared_ptr<EthereumPeer> _peer);

	/// Called by peer once it has new block headers during sync
	void onPeerBlockHeaders(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	/// Called by peer once it has new block bodies
	void onPeerBlockBodies(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	/// Called by peer once it has new block bodies
	void onPeerNewBlock(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	void onPeerNewHashes(std::shared_ptr<EthereumPeer> _peer, std::vector<std::pair<h256, u256>> const& _hashes);


	/// Called by peer when it is disconnecting
	void onPeerAborting();

	/// @returns Synchonization status
	SyncStatus status() const;

	static char const* stateName(SyncState _s) { return s_stateNames[static_cast<int>(_s)]; }

private:

	/// Resume downloading after witing state
	void continueSync();

	/// Called after all blocks have been donloaded
	void completeSync();

	/// Enter waiting state
	void pauseSync();

	EthereumHost& host() { return m_host; }
	EthereumHost const& host() const { return m_host; }

	/// Estimates max number of hashes peers can give us.
	unsigned estimatedHashes() const;

	void resetSync();
	void syncPeer(std::shared_ptr<EthereumPeer> _peer, bool _force);
	void requestBlocks(std::shared_ptr<EthereumPeer> _peer);
	void clearPeerDownload(std::shared_ptr<EthereumPeer> _peer);
	void clearPeerDownload();
	void collectBlocks();

private:
	EthereumHost& m_host;

protected:
	Handler<> m_bqRoomAvailable;				///< Triggered once block queue has space for more blocks
	mutable RecursiveMutex x_sync;
	SyncState m_state = SyncState::NotSynced;	///< Current sync state
	unsigned m_estimatedHashes = 0;				///< Number of estimated hashes for the last peer over PV60. Used for status reporting only.
	h256Hash m_knownNewHashes; 					///< New hashes we know about use for logging only
	unsigned m_startingBlock = 0;      	    	///< Last block number for the start of sync
	unsigned m_highestBlock = 0;       	     	///< Highest block number seen
	std::weak_ptr<EthereumPeer> m_chainPeer;	///< Peer that provides subchain headers. TODO: all we actually need to store here is node id to apply reward/penalty
	
	struct Header
	{
		bytes data;	///< Header data
		h256 hash;	///< Cached hash
	};

	struct HeaderId
	{
		h256 transactionsRoot;
		h256 uncles;

		bool operator==(HeaderId const& _other) const
		{
			return transactionsRoot == _other.transactionsRoot && uncles == _other.uncles;
		}
	};

	struct HeaderIdHash
	{
		std::size_t operator()(const HeaderId& _k) const
		{
			size_t seed = 0;
			h256::hash hasher;
			boost::hash_combine(seed, hasher(_k.transactionsRoot));
			boost::hash_combine(seed, hasher(_k.uncles));
			return seed;
		}
	};

	std::unordered_set<unsigned> m_downloadingHeaders;
	std::unordered_set<unsigned> m_downloadingBodies;
	std::map<unsigned, std::vector<Header>> m_headers;	    ///< Downloaded headers
	std::map<unsigned, std::vector<bytes>> m_bodies;	    ///< Downloaded block bodies
	std::map<std::weak_ptr<EthereumPeer>, std::vector<unsigned>, std::owner_less<std::weak_ptr<EthereumPeer>>> m_headerSyncPeers; ///< Peers to m_downloadingSubchain number map
	std::map<std::weak_ptr<EthereumPeer>, std::vector<unsigned>, std::owner_less<std::weak_ptr<EthereumPeer>>> m_bodySyncPeers; ///< Peers to m_downloadingSubchain number map
	std::unordered_map<HeaderId, unsigned, HeaderIdHash> m_headerIdToNumber;
	bool m_haveCommonHeader = false;
	unsigned m_lastImportedBlock = 0; ///< Last imported block number
	u256 m_syncingTotalDifficulty;

private:
	static char const* const s_stateNames[static_cast<int>(SyncState::Size)];
	bool invariants() const override;
	void logNewBlock(h256 const& _h);
};

std::ostream& operator<<(std::ostream& _out, SyncStatus const& _sync);

}
}
