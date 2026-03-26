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

export type KainStatusService = {
  name: string;
  status?: string | null;
  detail?: string | null;
  uptime?: string | null;
};

export type KainStatusIncident = {
  phase?: string | null;
  title?: string | null;
  body?: string | null;
  started_at?: string | null;
  resolved_at?: string | null;
};

export type KainStatusDescriptor = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  updated_at?: string | null;
  services?: KainStatusService[] | null;
  incidents?: KainStatusIncident[] | null;
};

export type KainRoadmapItem = {
  phase?: string | null;
  title?: string | null;
  body?: string | null;
  eta?: string | null;
};

export type KainTeamMember = {
  name: string;
  role?: string | null;
  summary?: string | null;
  focus?: string | null;
};

export type KainSupportChannel = {
  name: string;
  detail?: string | null;
  availability?: string | null;
  href?: string | null;
};

export type KainLegalLink = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  href?: string | null;
  label?: string | null;
};

export type KainSecurityControl = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainSecurityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  controls?: KainSecurityControl[] | null;
};

export type KainCareerRole = {
  title?: string | null;
  location?: string | null;
  type?: string | null;
  summary?: string | null;
  href?: string | null;
  tags?: string[] | null;
};

export type KainCareersDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  roles?: KainCareerRole[] | null;
};

export type KainPartner = {
  name: string;
  category?: string | null;
  detail?: string | null;
  href?: string | null;
};

export type KainPressAsset = {
  label?: string | null;
  detail?: string | null;
  href?: string | null;
};

export type KainPressContact = {
  name?: string | null;
  role?: string | null;
  email?: string | null;
};

export type KainPressKit = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  assets?: KainPressAsset[] | null;
  contacts?: KainPressContact[] | null;
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
  status?: KainStatusDescriptor | null;
  roadmap?: KainRoadmapItem[] | null;
  team_members?: KainTeamMember[] | null;
  support_channels?: KainSupportChannel[] | null;
  legal?: KainLegalLink[] | null;
  security?: KainSecurityDescriptor | null;
  careers?: KainCareersDescriptor | null;
  partners?: KainPartner[] | null;
  press_kit?: KainPressKit | null;
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
