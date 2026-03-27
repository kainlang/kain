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

export type KainCommunityChannel = {
  name?: string | null;
  platform?: string | null;
  summary?: string | null;
  members?: string | null;
  cadence?: string | null;
  href?: string | null;
};

export type KainCommunityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  channels?: KainCommunityChannel[] | null;
};

export type KainEventEntry = {
  title?: string | null;
  date?: string | null;
  format?: string | null;
  summary?: string | null;
  focus?: string | null;
  href?: string | null;
};

export type KainEventsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  rsvp_form_id?: string | null;
  upcoming?: KainEventEntry[] | null;
};

export type KainNewsletterDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  cadence?: string | null;
  topics?: string[] | null;
  form_id?: string | null;
};

export type KainComplianceControl = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainComplianceDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  controls?: KainComplianceControl[] | null;
};

export type KainObservabilitySignal = {
  title?: string | null;
  detail?: string | null;
  owner?: string | null;
  cadence?: string | null;
};

export type KainObservabilityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  signals?: KainObservabilitySignal[] | null;
};

export type KainInfrastructureItem = {
  title?: string | null;
  detail?: string | null;
  tier?: string | null;
  status?: string | null;
};

export type KainInfrastructureDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  stack?: KainInfrastructureItem[] | null;
};

export type KainLocalizationLanguage = {
  name?: string | null;
  coverage?: string | null;
  status?: string | null;
};

export type KainLocalizationRegion = {
  name?: string | null;
  timezone?: string | null;
  status?: string | null;
};

export type KainLocalizationDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  languages?: KainLocalizationLanguage[] | null;
  regions?: KainLocalizationRegion[] | null;
};

export type KainAccessibilityCheck = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainAccessibilityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  checks?: KainAccessibilityCheck[] | null;
};

export type KainPerformanceTarget = {
  title?: string | null;
  detail?: string | null;
  target?: string | null;
};

export type KainPerformanceDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  targets?: KainPerformanceTarget[] | null;
};

export type KainGrowthCampaign = {
  title?: string | null;
  channel?: string | null;
  summary?: string | null;
  status?: string | null;
};

export type KainGrowthFunnel = {
  stage?: string | null;
  metric?: string | null;
  owner?: string | null;
};

export type KainGrowthDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  campaigns?: KainGrowthCampaign[] | null;
  funnels?: KainGrowthFunnel[] | null;
};

export type KainExperimentTest = {
  name?: string | null;
  hypothesis?: string | null;
  status?: string | null;
  metric?: string | null;
  owner?: string | null;
};

export type KainExperimentDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  tests?: KainExperimentTest[] | null;
};

export type KainServiceEntry = {
  name?: string | null;
  tier?: string | null;
  summary?: string | null;
  sla?: string | null;
  owner?: string | null;
};

export type KainServiceCatalog = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  services?: KainServiceEntry[] | null;
};

export type KainSuccessPlaybook = {
  title?: string | null;
  goal?: string | null;
  owner?: string | null;
  cadence?: string | null;
};

export type KainSuccessDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  playbooks?: KainSuccessPlaybook[] | null;
};

export type KainNotificationChannel = {
  name?: string | null;
  purpose?: string | null;
  owner?: string | null;
  cadence?: string | null;
  transport?: string | null;
};

export type KainNotificationDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  channels?: KainNotificationChannel[] | null;
};

export type KainActorTopologyNode = {
  id?: string | null;
  name?: string | null;
  role?: string | null;
  channel?: string | null;
};

export type KainActorTopologyEdge = {
  from?: string | null;
  to?: string | null;
  relation?: string | null;
  detail?: string | null;
};

export type KainActorTopology = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  nodes?: KainActorTopologyNode[] | null;
  edges?: KainActorTopologyEdge[] | null;
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
  chat_playbooks?: KainProcessStep[];
  chat_tools?: KainCardEntry[];
  chat_memory?: KainCardEntry[];
  actor_playbooks?: KainProcessStep[];
  actor_tools?: KainCardEntry[];
  actor_topology?: KainActorTopology | null;
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
  community?: KainCommunityDescriptor | null;
  events?: KainEventsDescriptor | null;
  newsletter?: KainNewsletterDescriptor | null;
  compliance?: KainComplianceDescriptor | null;
  observability?: KainObservabilityDescriptor | null;
  infrastructure?: KainInfrastructureDescriptor | null;
  localization?: KainLocalizationDescriptor | null;
  accessibility?: KainAccessibilityDescriptor | null;
  performance?: KainPerformanceDescriptor | null;
  growth?: KainGrowthDescriptor | null;
  experiments?: KainExperimentDescriptor | null;
  service_catalog?: KainServiceCatalog | null;
  success?: KainSuccessDescriptor | null;
  notifications?: KainNotificationDescriptor | null;
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
