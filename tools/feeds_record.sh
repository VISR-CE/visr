#!/usr/bin/env bash
# Push the feeds checked into this checkout's docs/site/packs to their public record, steverice/visr-feeds: one commit
# on its main when anything changed, never a force push (docs/data-packs.md, "Feed hosts"). Run by
# .github/workflows/feeds-record.yml with FEEDS_DEPLOY_KEY, the private half of visr-feeds' write deploy key.
# Each attempt clones afresh, so a push that lost a race with another one records again on top of it.
set -o errexit -o nounset -o pipefail

if [ -z "${FEEDS_DEPLOY_KEY:-}" ]; then
    echo "feeds_record: FEEDS_DEPLOY_KEY is not set" >&2
    exit 2
fi
remote=${FEEDS_REMOTE:-git@github.com:steverice/visr-feeds.git}
attempts=${FEEDS_ATTEMPTS:-3}
delay=${FEEDS_RETRY_SECONDS:-20}
work=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/feeds-record.XXXXXX")
trap 'rm -R -- "$work"' EXIT
printf '%s\n' "$FEEDS_DEPLOY_KEY" > "$work/key"
chmod 600 "$work/key"
echo 'github.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl' > "$work/known_hosts"
export GIT_SSH_COMMAND="ssh -i $work/key -o IdentitiesOnly=yes -o StrictHostKeyChecking=yes -o UserKnownHostsFile=$work/known_hosts"
source_sha=$(git rev-parse HEAD)

record_once() {
    local feeds="$work/visr-feeds"
    rm -R -f -- "$feeds"
    local heads
    heads=$(git ls-remote --heads "$remote" main) || return 1
    if [ -n "$heads" ]; then
        git -c core.hooksPath=/dev/null clone --quiet --depth 1 --branch main "$remote" "$feeds" || return 1
    else
        # an empty repository: the first record creates main
        git init --quiet "$feeds"
        git -C "$feeds" remote add origin "$remote"
        git -C "$feeds" switch --quiet --orphan main
    fi
    ${FEEDS_PYTHON:-python3} tools/feeds_store.py record --checkout "$feeds" || return 3
    git -C "$feeds" add --all .
    if git -C "$feeds" diff --cached --quiet; then
        echo "visr-feeds already holds the feeds of steverice/visr@$source_sha"
        return 0
    fi
    git -C "$feeds" -c user.name='github-actions[bot]' \
        -c user.email='41898282+github-actions[bot]@users.noreply.github.com' \
        -c commit.gpgsign=false -c core.hooksPath=/dev/null \
        commit --quiet --message "Feeds from steverice/visr@$source_sha"
    git -C "$feeds" push --quiet origin HEAD:refs/heads/main || return 1
    echo "Recorded the feeds of steverice/visr@$source_sha in visr-feeds ($(git -C "$feeds" rev-parse --short HEAD))"
}

for attempt in $(seq 1 "$attempts"); do
    status=0
    record_once || status=$?
    # 3: the feeds themselves failed validation; retrying cannot help
    if [ "$status" -eq 0 ] || [ "$status" -eq 3 ]; then
        exit "$status"
    fi
    echo "feeds_record: attempt $attempt of $attempts could not reach visr-feeds" >&2
    [ "$attempt" -lt "$attempts" ] && sleep $((attempt * delay))
done
exit 1
