import { h, render } from "preact";

import { loadSiteData } from "./lib/kain_site_data";
import { AppShellIsland } from "./islands/AppShellIsland";
import { ChatLabIsland } from "./islands/ChatLabIsland";
import { RealtimeChannelsIsland } from "./islands/RealtimeChannelsIsland";
import { SceneViewportIsland } from "./islands/SceneViewportIsland";

type IslandKind = "app-shell" | "chat" | "realtime" | "scene";

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
  if (target.kind === "chat") {
    render(<ChatLabIsland seed={siteData.chat_seed || []} />, target.node);
  }
}

async function mountAll() {
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
