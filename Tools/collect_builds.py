#!/usr/bin/env python3
"""Download builds shared through the "Share your build" issue form into a dataset folder.

Only submissions with every consent box ticked are kept. Python 3 standard library only.

    python Tools/collect_builds.py --repo tongchenliu/BlockBuildKit --out dataset

Set GITHUB_TOKEN to a personal access token for a higher API rate limit (60 requests/hour without one).
"""
import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
LABEL = "shared-build"
TITLE_PREFIX = "[Build]"
ATTACH_RE = re.compile(r"https://github\.com/(?:user-attachments/files|[^/\s]+/[^/\s]+/files)/\d+/[^\s)\]>\"']+?\.json", re.I)
SECTION_KEYS = {
    "build file": "file",
    "what did you build": "description",
    "kind": "kind",
    "which pieces did you use": "pieces",
    "pack names": "packs",
    "consent": "consent",
}


def request(url, token=None, accept="application/vnd.github+json"):
    headers = {"User-Agent": "BlockBuildKit-collector", "Accept": accept}
    if token and url.startswith(API):
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def list_issues(repo, token):
    page = 1
    while True:
        data = json.loads(request(f"{API}/repos/{repo}/issues?state=all&per_page=100&page={page}", token))
        if not data:
            return
        for issue in data:
            if "pull_request" in issue:
                continue
            labels = {l.get("name", "") for l in issue.get("labels", [])}
            if LABEL in labels or (issue.get("title") or "").startswith(TITLE_PREFIX):
                yield issue
        page += 1


def parse_sections(body):
    """Issue forms render as '### <label>' headings followed by the answer."""
    out, key, buf = {}, None, []
    for line in (body or "").splitlines():
        if line.startswith("### "):
            if key:
                out[key] = "\n".join(buf).strip()
            title = line[4:].strip().lower()
            key = next((v for k, v in SECTION_KEYS.items() if title.startswith(k)), None)
            buf = []
        elif key:
            buf.append(line)
    if key:
        out[key] = "\n".join(buf).strip()
    for k, v in list(out.items()):
        if v == "_No response_":
            out[k] = ""
    return out


def consent_ok(text):
    ticked = len(re.findall(r"^- \[[xX]\]", text or "", re.M))
    unticked = len(re.findall(r"^- \[ \]", text or "", re.M))
    return ticked >= 3 and unticked == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default="tongchenliu/BlockBuildKit", help="owner/name of the GitHub repository")
    ap.add_argument("--out", default="dataset", help="output folder")
    args = ap.parse_args()
    token = os.environ.get("GITHUB_TOKEN")

    builds_dir = os.path.join(args.out, "builds")
    os.makedirs(builds_dir, exist_ok=True)
    rows, skipped = [], []
    try:
        issues = list(list_issues(args.repo, token))
    except urllib.error.URLError as e:
        print(f"Could not reach the GitHub API ({e}). Check your internet connection, or set GITHUB_TOKEN if you hit the rate limit.")
        return 1
    for issue in issues:
        n = issue["number"]
        sec = parse_sections(issue.get("body"))
        if not consent_ok(sec.get("consent")):
            skipped.append((n, "consent boxes not all ticked"))
            continue
        urls = ATTACH_RE.findall(sec.get("file", "")) or ATTACH_RE.findall(issue.get("body") or "")
        if not urls:
            skipped.append((n, "no .json attachment"))
            continue
        path = os.path.join(builds_dir, f"{n}.json")
        if not os.path.exists(path):
            try:
                raw = request(urls[0], accept="*/*")
                doc = json.loads(raw.decode("utf-8-sig"))
            except (urllib.error.URLError, ValueError) as e:
                skipped.append((n, f"download/parse failed: {e}"))
                continue
            if not str(doc.get("schema", "")).startswith("blockbuildkit.share/"):
                skipped.append((n, "not a Block Build Kit share file"))
                continue
            with open(path, "w", encoding="utf-8") as f:
                json.dump(doc, f, ensure_ascii=False)
        with open(path, encoding="utf-8") as f:
            doc = json.load(f)
        rows.append({
            "issue": n,
            "url": issue.get("html_url"),
            "created_at": issue.get("created_at"),
            "description": sec.get("description", ""),
            "kind": sec.get("kind", ""),
            "pieces_used": [p.strip() for p in sec.get("pieces", "").split(",") if p.strip()],
            "packs": sec.get("packs", ""),
            "file": f"builds/{n}.json",
            "piece_count": doc.get("piece_count"),
            "session_count": doc.get("session_count"),
            "license": "CC0-1.0",
        })

    rows.sort(key=lambda r: r["issue"])
    with open(os.path.join(args.out, "index.jsonl"), "w", encoding="utf-8") as f:
        for r in rows:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    print(f"{len(rows)} builds in {args.out}/index.jsonl")
    for n, why in skipped:
        print(f"  skipped #{n}: {why}")


if __name__ == "__main__":
    sys.exit(main())
