import { h, render } from "preact";

import { loadSiteData } from "./lib/kain_site_data";
import { kainScriptTagline } from "./lib/kain_script_bridge.ks";
import { AgentStudioIsland } from "./islands/AgentStudioIsland";
import { AppShellIsland } from "./islands/AppShellIsland";
import { AnalyticsLabIsland } from "./islands/AnalyticsLabIsland";
import { ActorOpsIsland } from "./islands/ActorOpsIsland";
import { AuthSessionIsland } from "./islands/AuthSessionIsland";
import { ChatLabIsland } from "./islands/ChatLabIsland";
import { ExperienceCatalogIsland } from "./islands/ExperienceCatalogIsland";
import { RealtimeChannelsIsland } from "./islands/RealtimeChannelsIsland";
import { SceneViewportIsland } from "./islands/SceneViewportIsland";
import { StatusWatchIsland } from "./islands/StatusWatchIsland";
import { SystemContractIsland } from "./islands/SystemContractIsland";
import { UiStacksIsland } from "./islands/UiStacksIsland";
import { UiKitIsland } from "./islands/UiKitIsland";
import { UploadsLabIsland } from "./islands/UploadsLabIsland";

type IslandKind =
  | "actor-ops"
  | "agent-studio"
  | "app-shell"
  | "chat"
  | "experience-catalog"
  | "realtime"
  | "scene"
  | "status"
  | "auth-session"
  | "uploads"
  | "analytics"
  | "system-contract"
  | "ui-stacks"
  | "ui-kit";

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
  if (target.kind === "experience-catalog") {
    render(<ExperienceCatalogIsland entries={siteData.experience_catalog || []} />, target.node);
    return;
  }
  if (target.kind === "actor-ops") {
    render(
      <ActorOpsIsland
        routes={siteData.routes || []}
        actors={siteData.actors || []}
        topology={siteData.actor_topology || null}
        policies={siteData.actor_policies || []}
        metrics={siteData.actor_metrics || []}
        supervision={siteData.actor_supervision || []}
        queues={siteData.actor_queues || []}
        jobs={siteData.actor_jobs || []}
        schedules={siteData.actor_schedules || []}
        hosts={siteData.actor_hosts || []}
        runtime={siteData.actor_runtime || []}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "agent-studio") {
    render(
      <AgentStudioIsland
        agents={siteData.ai_agents?.agents || []}
        workflows={[...(siteData.agent_workflows || []), ...(siteData.ai_agents?.workflows || [])]}
        tools={[...(siteData.tool_registry || []), ...(siteData.ai_agents?.tools || [])]}
        knowledge={siteData.knowledge_sources || []}
        memory={siteData.memory_stores || []}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "ui-kit") {
    render(
      <UiKitIsland
        components={siteData.ui_components || []}
        layouts={siteData.ui_layouts || []}
        tokens={siteData.ui_tokens || []}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "ui-stacks") {
    render(
      <UiStacksIsland
        uiRuntime={siteData.ui_runtime || []}
        kainUi={siteData.kain_ui_stack || []}
        uiState={siteData.ui_state_stack || []}
        uiRouting={siteData.ui_routing_stack || []}
        uiData={siteData.ui_data_stack || []}
        uiForms={siteData.ui_form_stack || []}
        uiMotion={siteData.ui_motion_stack || []}
        uiTesting={siteData.ui_testing_stack || []}
        uiTooling={siteData.ui_tooling_stack || []}
      />,
      target.node
    );
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
    const runtimeRoutes = siteData.runtime?.routes || {};
    render(
      <ChatLabIsland
        seed={siteData.chat_seed || []}
        personas={siteData.chat_personas || []}
        modes={siteData.chat_modes || []}
        agents={siteData.ai_agents?.agents || []}
        playbooks={siteData.chat_playbooks || []}
        tools={siteData.chat_tools || []}
        memory={siteData.chat_memory || []}
        chatEndpoint={runtimeRoutes.chat}
        streamEndpoint={runtimeRoutes.chat_stream}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "auth-session") {
    render(<AuthSessionIsland />, target.node);
    return;
  }
  if (target.kind === "uploads") {
    const runtimeRoutes = siteData.runtime?.routes || {};
    render(
      <UploadsLabIsland
        uploadEndpoint={runtimeRoutes.uploads}
        servePrefix={runtimeRoutes.uploads_prefix}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "analytics") {
    const runtimeRoutes = siteData.runtime?.routes || {};
    render(
      <AnalyticsLabIsland
        eventEndpoint={runtimeRoutes.analytics_event}
        eventsEndpoint={runtimeRoutes.analytics_events}
      />,
      target.node
    );
    return;
  }
  if (target.kind === "system-contract") {
    render(<SystemContractIsland />, target.node);
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
