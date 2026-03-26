import { h, render } from "preact";

import { loadSiteData } from "./lib/kain_site_data";
import { kainScriptTagline } from "./lib/kain_script_bridge.ks";
import { AppShellIsland } from "./islands/AppShellIsland";
import { AnalyticsLabIsland } from "./islands/AnalyticsLabIsland";
import { AuthSessionIsland } from "./islands/AuthSessionIsland";
import { ChatLabIsland } from "./islands/ChatLabIsland";
import { RealtimeChannelsIsland } from "./islands/RealtimeChannelsIsland";
import { SceneViewportIsland } from "./islands/SceneViewportIsland";
import { StatusWatchIsland } from "./islands/StatusWatchIsland";
import { UploadsLabIsland } from "./islands/UploadsLabIsland";

type IslandKind =
  | "app-shell"
  | "chat"
  | "realtime"
  | "scene"
  | "status"
  | "auth-session"
  | "uploads"
  | "analytics";

type IslandTarget = {
  node: HTMLElement;
  kind: IslandKind;
  siteDataPath: string;
};

function getIslandTargets(): IslandTarget[] {
  const targets: IslandTarget[] = [];
  for (const node of document.querySelectorAll<HTMLElement>("[data-kain-island]")) {
    const kind = (node.getAttribute("data-kain-island") || "").trim() as IslandKind;
    if (!kind) continue;
    targets.push({
      node,
      kind,
      siteDataPath: node.getAttribute("data-site-data") || "site.data.json"
    });
  }
  return targets;
}

async function mountTarget(target: IslandTarget) {
  const siteData = await loadSiteData(target.siteDataPath);
  if (target.kind === "app-shell") {
    render(<AppShellIsland modules={siteData.app_modules || []} />, target.node);
    return;
  }
  if (target.kind === "realtime") {
    render(<RealtimeChannelsIsland channels={siteData.realtime_channels || []} />, target.node);
    return;
  }
  if (target.kind === "scene") {
    render(<SceneViewportIsland scene={siteData.scene || null} />, target.node);
    return;
  }
  if (target.kind === "status") {
    render(<StatusWatchIsland status={siteData.status || null} />, target.node);
    return;
  }
  if (target.kind === "chat") {
    render(<ChatLabIsland seed={siteData.chat_seed || []} />, target.node);
    return;
  }
  if (target.kind === "auth-session") {
    render(<AuthSessionIsland />, target.node);
    return;
  }
  if (target.kind === "uploads") {
    render(<UploadsLabIsland />, target.node);
    return;
  }
  if (target.kind === "analytics") {
    render(<AnalyticsLabIsland />, target.node);
  }
}

async function mountAll() {
  const root = document.documentElement;
  if (root) {
    root.setAttribute("data-kain-script", kainScriptTagline());
  }
  const targets = getIslandTargets();
  if (!targets.length) return;
  await Promise.allSettled(targets.map((target) => mountTarget(target)));
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", () => {
    void mountAll();
  });
} else {
  void mountAll();
}
