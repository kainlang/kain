export type KainChatSeedMessage = {
  role: string;
  text: string;
};

export type KainAppModule = {
  name: string;
  route?: string | null;
  summary?: string | null;
  tags?: string[] | null;
};

export type KainRealtimeChannel = {
  name: string;
  protocol?: string | null;
  cadence?: string | null;
  summary?: string | null;
  producer?: string | null;
  tags?: string[] | null;
};

export type KainSceneLayer = {
  title?: string | null;
  summary?: string | null;
  color?: string | null;
};

export type KainSceneDescriptor = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  layers?: KainSceneLayer[] | null;
};

export type KainSiteData = {
  brand?: string;
  nav?: { label: string; href: string }[];
  hero?: {
    kicker?: string;
    title?: string;
    body?: string;
    actions?: { label: string; href: string; style?: string }[];
  };
  app_modules?: KainAppModule[];
  realtime_channels?: KainRealtimeChannel[];
  chat_seed?: KainChatSeedMessage[];
  scene?: KainSceneDescriptor;
};

export function resolveAgainstLocation(relativeOrAbsolute: string): string {
  try {
    return new URL(relativeOrAbsolute, window.location.href).toString();
  } catch {
    return relativeOrAbsolute;
  }
}

export async function fetchJson<T>(url: string): Promise<T> {
  const response = await fetch(url, { headers: { accept: "application/json" } });
  if (!response.ok) {
    throw new Error(`fetch failed: ${response.status} ${response.statusText}`);
  }
  return (await response.json()) as T;
}

export async function loadSiteData(siteDataPath: string): Promise<KainSiteData> {
  return fetchJson<KainSiteData>(resolveAgainstLocation(siteDataPath));
}

