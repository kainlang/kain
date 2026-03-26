export type KainChatSeedMessage = {
  role: string;
  text: string;
};

export type KainCardEntry = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  summary?: string | null;
};

export type KainProcessStep = {
  title?: string | null;
  body?: string | null;
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

export type KainAuthMethod = {
  label: string;
  detail?: string | null;
  scope?: string | null;
  status?: string | null;
};

export type KainAuthDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  session_title?: string | null;
  session_body?: string | null;
  methods?: KainAuthMethod[] | null;
};

export type KainCommerceOffer = {
  kicker?: string | null;
  cadence?: string | null;
  name: string;
  price?: string | null;
  summary?: string | null;
  features?: string[] | null;
  actions?: { label: string; href: string; style?: string }[] | null;
};

export type KainCommerceDescriptor = {
  offers?: KainCommerceOffer[] | null;
};

export type KainUploadsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
};

export type KainAnalyticsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
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
  chat_personas?: KainCardEntry[];
  chat_modes?: KainCardEntry[];
  actor_playbooks?: KainProcessStep[];
  actor_tools?: KainCardEntry[];
  scene?: KainSceneDescriptor;
  auth?: KainAuthDescriptor | null;
  commerce?: KainCommerceDescriptor | null;
  uploads?: KainUploadsDescriptor | null;
  analytics?: KainAnalyticsDescriptor | null;
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
