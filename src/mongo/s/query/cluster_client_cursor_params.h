/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "mongo/bson/bsonobj.h"
#include "mongo/client/read_preference.h"
#include "mongo/db/auth/user_name.h"
#include "mongo/db/cursor_id.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/pipeline/pipeline.h"
#include "mongo/db/query/cursor_response.h"
#include "mongo/db/query/tailable_mode.h"
#include "mongo/s/client/shard.h"
#include "mongo/util/net/hostandport.h"

namespace mongo {
namespace executor {
class TaskExecutor;
}

class OperationContext;
class RouterExecStage;

/**
 * The resulting ClusterClientCursor will take ownership of the existing remote cursor, generating
 * results based on the cursor's current state.
 *
 * Note that any results already generated from this cursor will not be returned by the resulting
 * ClusterClientCursor. The caller is responsible for ensuring that results previously generated by
 * this cursor have been processed.
 */
struct ClusterClientCursorParams {
    struct RemoteCursor {
        RemoteCursor(ShardId shardId, HostAndPort hostAndPort, CursorResponse cursorResponse)
            : shardId(std::move(shardId)),
              hostAndPort(std::move(hostAndPort)),
              cursorResponse(std::move(cursorResponse)) {}

        // The shardId of the shard on which the cursor resides.
        ShardId shardId;

        // The exact host (within the shard) on which the cursor resides.
        HostAndPort hostAndPort;

        // Encompasses the state of the established cursor.
        CursorResponse cursorResponse;
    };

    ClusterClientCursorParams(NamespaceString nss,
                              boost::optional<ReadPreferenceSetting> readPref = boost::none)
        : nsString(std::move(nss)) {
        if (readPref) {
            readPreference = std::move(readPref.get());
        }
    }

    // Namespace against which the cursors exist.
    NamespaceString nsString;

    // Per-remote node data.
    std::vector<RemoteCursor> remotes;

    // The sort specification. Leave empty if there is no sort.
    BSONObj sort;

    // When 'compareWholeSortKey' is true, $sortKey is a scalar value, rather than an object. We
    // extract the sort key {$sortKey: <value>}. The sort key pattern is verified to be {$sortKey:
    // 1}.
    bool compareWholeSortKey = false;

    // The number of results to skip. Optional. Should not be forwarded to the remote hosts in
    // 'cmdObj'.
    boost::optional<long long> skip;

    // The number of results per batch. Optional. If specified, will be specified as the batch for
    // each getMore.
    boost::optional<long long> batchSize;

    // Limits the number of results returned by the ClusterClientCursor to this many. Optional.
    // Should be forwarded to the remote hosts in 'cmdObj'.
    boost::optional<long long> limit;

    // If set, we use this pipeline to merge the output of aggregations on each remote.
    std::unique_ptr<Pipeline, PipelineDeleter> mergePipeline;

    // Whether this cursor is tailing a capped collection, and whether it has the awaitData option
    // set.
    TailableMode tailableMode = TailableMode::kNormal;

    // Set if a readPreference must be respected throughout the lifetime of the cursor.
    boost::optional<ReadPreferenceSetting> readPreference;

    // If valid, is called to return the RouterExecStage which becomes the initial source in this
    // cursor's execution plan. Otherwise, a RouterStageMerge is used.
    stdx::function<std::unique_ptr<RouterExecStage>(
        OperationContext*, executor::TaskExecutor*, ClusterClientCursorParams*)>
        createCustomCursorSource;

    // Whether the client indicated that it is willing to receive partial results in the case of an
    // unreachable host.
    bool isAllowPartialResults = false;
};

}  // mongo
