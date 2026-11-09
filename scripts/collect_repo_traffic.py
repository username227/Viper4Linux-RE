#!/usr/bin/env python3
"""Capture authenticated GitHub traffic and published release download counters.

All requested API calls must succeed before writing any output. Never prints or
stores the access token. Daily history is committed only to traffic-stats.
"""
import datetime as dt
import json
import os
import pathlib
import sys
import urllib.error
import urllib.request

API_ROOT = "https://api.github.com"


def api_get(path, token):
    request = urllib.request.Request(
        API_ROOT + path,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "viper4linux-re-traffic-collector",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        # Never display authorization headers or token values.
        raise RuntimeError(f"GitHub API returned HTTP {exc.code} for {path}") from exc


def validate_traffic(payload, category):
    if not isinstance(payload, dict) or not isinstance(payload.get(category), list):
        raise ValueError(f"Invalid or incomplete {category} response")
    for entry in payload[category]:
        if not isinstance(entry, dict) or not isinstance(entry.get("timestamp"), str) or not all(
            isinstance(entry.get(k), int) and entry[k] >= 0 for k in ("count", "uniques")
        ):
            raise ValueError(f"Invalid {category} daily entry")
    for key in ("count", "uniques"):
        if not isinstance(payload.get(key), int) or payload[key] < 0:
            raise ValueError(f"Invalid {category} summary")
    return payload[category]


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: collect_repo_traffic.py /path/to/traffic.json")
    token = os.environ.get("VIPER_TRAFFIC_TOKEN")
    repo = os.environ.get("GITHUB_REPOSITORY")
    if not token or repo != "username227/Viper4Linux-RE":
        raise RuntimeError("Missing token or unexpected GITHUB_REPOSITORY")

    clones_payload = api_get(f"/repos/{repo}/traffic/clones?per=day", token)
    views_payload = api_get(f"/repos/{repo}/traffic/views?per=day", token)
    referrers = api_get(f"/repos/{repo}/traffic/popular/referrers", token)
    paths = api_get(f"/repos/{repo}/traffic/popular/paths", token)
    repo_info = api_get(f"/repos/{repo}", token)
    releases = api_get(f"/repos/{repo}/releases?per_page=100", token)

    clones = validate_traffic(clones_payload, "clones")
    views = validate_traffic(views_payload, "views")
    if not isinstance(referrers, list) or not isinstance(paths, list):
        raise ValueError("Missing referrer/path traffic response")
    if not isinstance(repo_info, dict) or not isinstance(repo_info.get("stargazers_count"), int):
        raise ValueError("Missing repository star count")
    if not isinstance(releases, list) or any(
        not isinstance(r, dict) or not isinstance(r.get("assets"), list)
        for r in releases
    ):
        raise ValueError("Invalid releases response")

    utc_now = dt.datetime.now(dt.timezone.utc)
    # GitHub traffic is bucketed by UTC date; the current UTC day is incomplete.
    current_date = utc_now.date().isoformat()
    clones_by_day = {x["timestamp"][:10]: x for x in clones}
    views_by_day = {x["timestamp"][:10]: x for x in views}
    all_days = sorted(set(clones_by_day) | set(views_by_day))

    path = pathlib.Path(sys.argv[1])
    if path.is_file():
        doc = json.loads(path.read_text(encoding="utf-8"))
        if doc.get("schema_version") != 1 or doc.get("repository") != repo:
            raise ValueError("Existing statistics file has unexpected schema/repository")
        if not isinstance(doc.get("daily"), dict) or not isinstance(doc.get("snapshots"), dict):
            raise ValueError("Existing statistics file has invalid history")
    else:
        doc = {"schema_version": 1, "repository": repo, "daily": {}, "snapshots": {}}

    for day in all_days:
        if day >= current_date:
            continue
        c = clones_by_day.get(day, {"count": 0, "uniques": 0})
        v = views_by_day.get(day, {"count": 0, "uniques": 0})
        doc["daily"][day] = {
            "clones": c["count"],
            "unique_cloners": c["uniques"],
            "views": v["count"],
            "unique_visitors": v["uniques"],
            "source": "github_utc_day",
        }

    release_counts = {}
    for release in releases:
        tag = release.get("tag_name")
        if not isinstance(tag, str) or not tag:
            continue
        assets = release["assets"]
        if any(not isinstance(a.get("name"), str) or not isinstance(a.get("download_count"), int) for a in assets):
            raise ValueError(f"Invalid assets for release {tag}")
        release_counts[tag] = {
            "published_at": release.get("published_at"),
            "assets": {a["name"]: a["download_count"] for a in assets},
        }

    doc["snapshots"][current_date] = {
        "collected_at_utc": utc_now.isoformat(timespec="seconds"),
        "stars": repo_info["stargazers_count"],
        "releases": release_counts,
        "top_referrers": referrers,
        "top_paths": paths,
        "rolling_14_day_traffic": {
            "clones": clones_payload["count"],
            "unique_cloners": clones_payload["uniques"],
            "views": views_payload["count"],
            "unique_visitors": views_payload["uniques"],
        },
    }
    doc["last_success_utc"] = utc_now.isoformat(timespec="seconds")
    path.parent.mkdir(parents=True, exist_ok=True)
    # Write only after ALL API responses have passed validation.
    temp_path = path.with_suffix(path.suffix + ".tmp")
    temp_path.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temp_path.replace(path)
    print(f"Collected GitHub statistics for {repo} at {doc['last_success_utc']}")


if __name__ == "__main__":
    main()
